// Windows Fluent command-palette reference application.
//
// The same source drives a deterministic Software capture and the GLFW
// window. It demonstrates a practical M2 composition: Dialog provides modal
// isolation, TextInput owns text/IME editing, live filtering rebuilds a
// result list at the frame boundary, and Enter/Escape stay available without
// application-specific event plumbing.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/paint_context.h"
#include "wui/scheduler.h"
#include "wui/theme.h"
#include "wui/ui.h"
#include "wui/whatscanvas_text.h"

#ifdef WUI_COMMAND_PALETTE_INTERACTIVE
#include "wui/glfw_platform.h"
#endif

namespace {

struct PaletteCommand {
    std::string title;
    std::string detail;
    std::string shortcut;

    bool operator==(const PaletteCommand& other) const
    {
        return title == other.title && detail == other.detail && shortcut == other.shortcut;
    }
};

struct PaletteModel {
    wui::State<std::string> query{""};
    wui::State<std::vector<PaletteCommand>> results;
    wui::State<bool> hasResults{true};
    wui::State<bool> hasNoResults{false};
    wui::State<std::string> lastAction{"No command selected"};
};

const std::vector<PaletteCommand>& commands()
{
    static const std::vector<PaletteCommand> value{
        {"Open My Day", "Navigate to your planned work", "Enter"},
        {"Add task", "Create a task in My Day", "A"},
        {"Toggle completed", "Show or hide completed tasks", "T"},
        {"Open settings", "Personalize this workspace", "S"},
        {"Start focus session", "Protect time for your next task", "F"},
        {"Show keyboard shortcuts", "Learn the essential commands", "?"},
    };
    return value;
}

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

void filter(PaletteModel& model, const std::string& query)
{
    const auto needle = lowercase(query);
    std::vector<PaletteCommand> visible;
    for (const auto& command : commands()) {
        const auto haystack = lowercase(command.title + " " + command.detail);
        if (needle.empty() || haystack.find(needle) != std::string::npos) {
            visible.push_back(command);
        }
    }
    model.query.set(query);
    model.results.set(std::move(visible));
    model.hasResults.set(!model.results.get().empty());
    model.hasNoResults.set(model.results.get().empty());
}

std::string resultSummary(const std::vector<PaletteCommand>& results)
{
    const auto count = results.size();
    return std::to_string(count) + (count == 1 ? " command" : " commands");
}

// This panel has a deliberately quiet Fluent rhythm: 4px-derived spacing,
// an elevated neutral surface, one blue focus color, and shortcut labels that
// stay visually secondary to the command name.
std::unique_ptr<wui::Node> buildPaletteContent(PaletteModel& model,
                                               std::function<void(const PaletteCommand&)> execute,
                                               std::function<void()> close,
                                               wui::TextInput** inputOut = nullptr)
{
    using namespace wui::ui;
    const auto& current = wui::theme();
    auto executeFirst = [&model, execute] {
        if (!model.results.get().empty()) {
            execute(model.results.get().front());
        }
    };

    auto search = TextField("Search commands")
        .onChange([&model](const std::string& query) { filter(model, query); })
        .onSubmit(executeFirst)
        .onCancel(close);
    if (inputOut != nullptr) {
        *inputOut = search.get();
    }

    return Box().width(560.0f).padding({24.0f, 20.0f, 24.0f, 22.0f}).children(
        Column().gap(16.0f).align(wui::Alignment::Stretch).children(
            Row().align(wui::Alignment::Center).children(
                Column().gap(2.0f).flex(1.0f).children(
                    Text("Command palette").size(20.0f).lineHeight(28.0f).color(current.colors.text),
                    Text("Search actions, pages, and workspace tools").size(12.0f).lineHeight(18.0f).color(current.colors.textMuted)),
                Text("Esc to close").size(11.0f).lineHeight(16.0f).color(current.colors.textMuted)),
            std::move(search),
            Row().align(wui::Alignment::Center).children(
                Text().bind(model.results, resultSummary).size(11.0f).lineHeight(16.0f).color(current.colors.textMuted),
                Spacer().flex(1.0f),
                Text("Enter runs the first result").size(11.0f).lineHeight(16.0f).color(current.colors.textMuted)),
            If(model.hasResults).then([&model, execute] {
                return Box().background(wui::theme().colors.surfaceAlt).radius(wui::theme().radius.md).padding(6.0f).children(
                    ForEach<PaletteCommand>(model.results, [execute](const PaletteCommand& command) {
                        return Box().background(wui::theme().colors.surface).radius(wui::theme().radius.sm)
                            .padding({12.0f, 9.0f, 10.0f, 9.0f}).children(
                                Row().align(wui::Alignment::Center).gap(12.0f).children(
                                    Column().gap(1.0f).flex(1.0f).children(
                                        Text(command.title).size(14.0f).lineHeight(20.0f).color(wui::theme().colors.text),
                                        Text(command.detail).size(11.0f).lineHeight(16.0f).color(wui::theme().colors.textMuted)),
                                    Button(command.shortcut).variant(wui::ButtonVariant::Ghost)
                                        .onClick([execute, command] { execute(command); })));
                    }).gap(4.0f).align(wui::Alignment::Stretch));
            }),
            // Keep the empty response structurally explicit, rather than an
            // empty list with unexplained blank space.
            If(model.hasNoResults).then([] {
                return Box().background(wui::theme().colors.surfaceAlt).radius(wui::theme().radius.md)
                    .height(92.0f).contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                    .children(Column().gap(3.0f).align(wui::Alignment::Center).children(
                        Text("No matching commands").size(14.0f).lineHeight(20.0f).color(wui::theme().colors.text),
                        Text("Try task, focus, or settings.").size(11.0f).lineHeight(16.0f).color(wui::theme().colors.textMuted)));
            }),
            Text("Tip: use concise words such as focus, settings, or task.").size(11.0f).lineHeight(16.0f).color(current.colors.textMuted)));
}

std::unique_ptr<wui::Node> buildLanding(PaletteModel& model, std::function<void()> open)
{
    using namespace wui::ui;
    const auto& current = wui::theme();
    auto actionCard = Box().background(current.colors.surface).radius(current.radius.lg).padding(24.0f).children(
        Column().gap(14.0f).align(wui::Alignment::Stretch).children(
            Text("Quick actions").size(16.0f).lineHeight(22.0f).color(current.colors.text),
            Text().bind(model.lastAction, [](const std::string& value) { return "Last: " + value; })
                .size(12.0f).lineHeight(18.0f).color(current.colors.textMuted),
            Row().align(wui::Alignment::Center).gap(12.0f).children(
                Button("Open command palette").variant(wui::ButtonVariant::Primary).onClick(open),
                Text("Ctrl+K is the familiar host shortcut.").size(11.0f).lineHeight(16.0f).color(current.colors.textMuted))));
    auto content = Column().gap(20.0f).align(wui::Alignment::Stretch).children(
        Text("Workspace").size(11.0f).lineHeight(16.0f).color(current.colors.accent),
        Text("Find the next thing to do.").size(32.0f).lineHeight(42.0f).color(current.colors.text),
        Text("The command palette keeps navigation and actions within one focused, keyboard-first surface.")
            .wrap().size(15.0f).lineHeight(22.0f).color(current.colors.textMuted),
        std::move(actionCard));
    auto rail = Box().width(720.0f).padding({48.0f, 40.0f, 48.0f, 40.0f}).children(std::move(content));
    return Box().background(current.colors.background).children(
        Row().align(wui::Alignment::Stretch).children(Spacer().flex(1.0f), std::move(rail), Spacer().flex(1.0f)));
}

#ifdef WUI_COMMAND_PALETTE_INTERACTIVE
void showPalette(wui::UiWindow& window, PaletteModel& model)
{
    using namespace wui::ui;
    filter(model, "");
    auto dialog = Dialog().maxWidth(608.0f).dismissOnBackdrop().content(
        buildPaletteContent(model,
            [&window, &model](const PaletteCommand& command) {
                model.lastAction.set(command.title);
                (void)window.dismissTopDialog();
            },
            [&window] { (void)window.dismissTopDialog(); }))
        .intoDialog();
    const auto id = window.showDialog(std::move(dialog));
    (void)id;
    // Dialog owns the keyboard domain. Focusing the first control immediately
    // gives IME and ordinary typing to the search field.
    auto* dialogRoot = window.overlayHost().top()->content.get();
    auto findInput = [](const auto& self, wui::Node* node) -> wui::TextInput* {
        if (auto* input = dynamic_cast<wui::TextInput*>(node)) return input;
        for (const auto& child : node->children()) if (auto* result = self(self, child.get())) return result;
        return nullptr;
    };
    window.focusManager().setFocused(findInput(findInput, dialogRoot));
}
#endif

void render(wui::Node& root, wsc::Canvas& canvas, float scale)
{
    wui::WhatsCanvasTextMeasurer text(canvas, scale);
    wui::setTextMeasurer(&text);
    wui::flushStructuralUpdates();
    root.layout({0.0f, 0.0f, static_cast<float>(canvas.getWidth()) / scale, static_cast<float>(canvas.getHeight()) / scale});
    wui::PaintContext paint(canvas, scale);
    root.prepare(paint);
    for (int pass = 0; pass < 2; ++pass) {
        canvas.beginFrame();
        paint.fillRect(root.bounds(), wui::theme().colors.background);
        root.paint(paint);
        canvas.endFrame();
    }
    wui::setTextMeasurer(nullptr);
}

} // namespace

int main(int argc, char** argv)
{
#ifdef WUI_COMMAND_PALETTE_INTERACTIVE
    PaletteModel model;
    filter(model, "");
    try {
        return wui::runGlfwApp("WhatsUI Command Palette", {900.0f, 640.0f}, [&model](wui::UiWindow& window) {
            return buildLanding(model, [&window, &model] { showPalette(window, model); });
        });
    } catch (const std::exception& error) {
        std::cerr << "FATAL: " << error.what() << std::endl;
        return 1;
    }
#else
    std::filesystem::path output{"command_palette_visual"};
    for (int index = 1; index < argc; ++index) output = argv[index];
    std::filesystem::create_directories(output);
    constexpr int width = 900;
    constexpr int height = 640;
    constexpr float scale = 2.0f;
    PaletteModel model;
    filter(model, "");
    wui::TextInput* searchInput = nullptr;
    auto root = wui::ui::Dialog().maxWidth(608.0f).content(buildPaletteContent(
        model, [&model](const PaletteCommand& command) { model.lastAction.set(command.title); }, [] {}, &searchInput)).intoNode();
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, static_cast<int>(width * scale), static_cast<int>(height * scale));
    if (!canvas || !canvas->initializeContext()) throw std::runtime_error("failed to create software canvas");
    render(*root, *canvas, scale);
    if (!canvas->savePixelsPPM((output / "command_palette_default.ppm").string())) throw std::runtime_error("failed to save capture");
    searchInput->text("focus");
    render(*root, *canvas, scale);
    if (!canvas->savePixelsPPM((output / "command_palette_filtered.ppm").string())) throw std::runtime_error("failed to save capture");
    std::cout << "wrote command palette captures to " << output << std::endl;
    return 0;
#endif
}
