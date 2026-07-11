// Windows Fluent Settings reference application.
//
// The source is shared by a deterministic Software capture and an interactive
// GLFW host. It intentionally uses only currently public WhatsUI controls:
// Button, Checkbox, ScrollView, Text and Dialog. The density setting is a
// discrete stepper until M3 introduces a proper pointer-drag Slider.

#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "wsc/Canvas.h"

#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/ui.h"
#include "wui/whatscanvas_text.h"

#ifdef WUI_SETTINGS_INTERACTIVE
#include "wui/glfw_platform.h"
#endif

namespace {

struct SettingsModel {
    wui::State<bool> systemTheme{true};
    wui::State<bool> compactMode{false};
    wui::State<bool> notifications{true};
    wui::State<bool> quietHours{false};
    wui::State<bool> diagnostics{false};
    wui::State<int> density{100};
};

using Action = std::function<void()>;
using DialogRequest = std::function<void(std::string, std::string, Action)>;

std::string densityLabel(int value)
{
    return std::to_string(value) + "%";
}

// A low-emphasis one-pixel divider made from the existing Container primitive.
wui::ui::Box divider()
{
    return wui::ui::Box().height(1.0f).background(wui::theme().colors.border);
}

std::unique_ptr<wui::Node> buildSettingsUi(SettingsModel& model,
                                           DialogRequest requestDialog,
                                           Action showMenu)
{
    using namespace wui::ui;
    const auto canvas = wui::theme().colors.background;
    const auto surface = wui::theme().colors.surface;
    const auto ink = wui::theme().colors.text;
    const auto muted = wui::theme().colors.textMuted;
    const auto accent = wui::theme().colors.accent;

    auto densityDown = [&model] {
        model.density.set(std::max(80, model.density.get() - 10));
    };
    auto densityUp = [&model] {
        model.density.set(std::min(130, model.density.get() + 10));
    };
    auto reset = [&model] {
        model.systemTheme.set(true);
        model.compactMode.set(false);
        model.notifications.set(true);
        model.quietHours.set(false);
        model.diagnostics.set(false);
        model.density.set(100);
    };

    // The navigation rail is visual navigation in M1. Its controls remain in
    // the normal focus order, demonstrating Tab/Shift+Tab across rail, page,
    // checkboxes, stepper, actions, and the scroll viewport.
    auto rail = Box().width(184.0f).background(surface).padding({20.0f, 24.0f, 20.0f, 16.0f})
        .children(Column().gap(8.0f).align(wui::Alignment::Stretch).children(
            Text("WORKSPACE").size(10.0f).lineHeight(14.0f).color(muted),
            Text("Settings").size(22.0f).lineHeight(30.0f).color(ink),
            Box().height(12.0f),
            Button("General").variant(wui::ButtonVariant::Primary),
            Button("Notifications").variant(wui::ButtonVariant::Ghost),
            Button("Privacy").variant(wui::ButtonVariant::Ghost),
            Spacer().flex(1.0f),
            Text("WINDOWS FLUENT").size(10.0f).lineHeight(14.0f).color(muted)));

    auto page = ScrollView().flex(1.0f).children(
        Column().gap(20.0f).padding({32.0f, 28.0f, 32.0f, 28.0f}).align(wui::Alignment::Stretch).children(
            Row().align(wui::Alignment::Center).gap(12.0f).children(
                Column().gap(3.0f).flex(1.0f).children(
                    Text("General").size(28.0f).lineHeight(36.0f).color(ink),
                    Text("Shape the way this workspace feels and behaves.").size(13.0f).lineHeight(19.0f).color(muted)),
                Button("...").variant(wui::ButtonVariant::Ghost).onClick(showMenu)),
            Box().background(surface).radius(8.0f).padding(20.0f).children(
                Column().gap(16.0f).align(wui::Alignment::Stretch).children(
                    Text("Personalization").size(17.0f).lineHeight(24.0f).color(ink),
                    Text("Choose familiar defaults and a comfortable reading density.").size(12.0f).lineHeight(18.0f).color(muted),
                    divider(),
                    Row().align(wui::Alignment::Center).gap(16.0f).children(
                        Column().gap(2.0f).flex(1.0f).children(
                            Text("Use system theme").size(14.0f).lineHeight(20.0f).color(ink),
                            Text("Keep this workspace aligned with Windows.").size(12.0f).lineHeight(18.0f).color(muted)),
                        Checkbox("", model.systemTheme.get()).bind(model.systemTheme)),
                    Row().align(wui::Alignment::Center).gap(16.0f).children(
                        Column().gap(2.0f).flex(1.0f).children(
                            Text("Compact spacing").size(14.0f).lineHeight(20.0f).color(ink),
                            Text("Fit more useful information in each panel.").size(12.0f).lineHeight(18.0f).color(muted)),
                        Checkbox("", model.compactMode.get()).bind(model.compactMode)),
                    divider(),
                    Row().align(wui::Alignment::Center).gap(12.0f).children(
                        Column().gap(2.0f).flex(1.0f).children(
                            Text("Text scale").size(14.0f).lineHeight(20.0f).color(ink),
                            Text("A discrete, accessible slider substitute in M1.").size(12.0f).lineHeight(18.0f).color(muted)),
                        Button("-").variant(wui::ButtonVariant::Ghost).onClick(densityDown),
                        Box().width(54.0f).contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                            .children(Text().bind(model.density, densityLabel).size(13.0f).lineHeight(18.0f).color(accent)),
                        Button("+").variant(wui::ButtonVariant::Ghost).onClick(densityUp)))),
            Box().background(surface).radius(8.0f).padding(20.0f).children(
                Column().gap(16.0f).align(wui::Alignment::Stretch).children(
                    Text("Notifications").size(17.0f).lineHeight(24.0f).color(ink),
                    Text("Stay informed without losing your focus.").size(12.0f).lineHeight(18.0f).color(muted),
                    divider(),
                    Checkbox("Show task reminders", model.notifications.get()).bind(model.notifications),
                    Checkbox("Pause alerts during focus time", model.quietHours.get()).bind(model.quietHours))),
            Box().background(surface).radius(8.0f).padding(20.0f).children(
                Column().gap(16.0f).align(wui::Alignment::Stretch).children(
                    Text("Privacy and diagnostics").size(17.0f).lineHeight(24.0f).color(ink),
                    Text("Keep optional diagnostic data under your control.").size(12.0f).lineHeight(18.0f).color(muted),
                    divider(),
                    Checkbox("Share optional diagnostics", model.diagnostics.get()).bind(model.diagnostics),
                    Row().align(wui::Alignment::Center).gap(12.0f).children(
                        Column().flex(1.0f).gap(2.0f).children(
                            Text("Restore defaults").size(14.0f).lineHeight(20.0f).color(ink),
                            Text("Return every setting in this reference panel to its default.").size(12.0f).lineHeight(18.0f).color(muted)),
                        Button("Reset").variant(wui::ButtonVariant::Ghost).onClick(
                            [requestDialog, reset] {
                                requestDialog("Restore default settings?",
                                              "Your local choices in this reference panel will be reset.", reset);
                            })))),
            Text("WhatsUI Settings reference  |  M1 input and scroll contract").size(11.0f).lineHeight(16.0f).color(muted)));

    return Box().background(canvas).children(Row().align(wui::Alignment::Stretch).children(
        std::move(rail), std::move(page)));
}

#ifdef WUI_SETTINGS_INTERACTIVE
void showResetDialog(wui::UiWindow& window, std::string title, std::string detail, Action confirm)
{
    using namespace wui::ui;
    auto dialog = Dialog().maxWidth(392.0f).content(
        Box().width(392.0f).background(wui::theme().colors.surface).radius(8.0f)
            .padding({24.0f, 22.0f, 20.0f, 22.0f}).children(
                Column().gap(18.0f).align(wui::Alignment::Stretch).children(
                    Column().gap(5.0f).children(
                        Text(std::move(title)).size(20.0f).lineHeight(28.0f).color(wui::theme().colors.text),
                        Text(std::move(detail)).wrap().size(13.0f).lineHeight(19.0f).color(wui::theme().colors.textMuted)),
                    Row().align(wui::Alignment::Center).gap(8.0f).children(
                        Spacer().flex(1.0f),
                        Button("Cancel").variant(wui::ButtonVariant::Ghost)
                            .onClick([&window] { (void)window.dismissTopDialog(); }),
                        Button("Restore").variant(wui::ButtonVariant::Primary)
                            .onClick([&window, confirm = std::move(confirm)]() mutable {
                                (void)window.dismissTopDialog();
                                confirm();
                            })))))
            .intoDialog();
    (void)window.showDialog(std::move(dialog));
}

void showPopupMenu(wui::UiWindow& window, SettingsModel& model)
{
    using namespace wui::ui;
    auto id = std::make_shared<wui::OverlayId>(0);
    auto dismiss = [&window, id] { (void)window.overlayHost().dismiss(*id); };
    auto popup = Box().padding({24.0f, 62.0f, 24.0f, 0.0f}).contentAlign(wui::Alignment::End, wui::Alignment::Start)
        .children(Box().width(224.0f).background(wui::theme().colors.surface).radius(8.0f).padding(8.0f)
            .children(Column().gap(4.0f).align(wui::Alignment::Stretch).children(
                Text("MORE OPTIONS").size(10.0f).lineHeight(14.0f).color(wui::theme().colors.textMuted),
                Button("Use compact density").variant(wui::ButtonVariant::Ghost).onClick([&model, dismiss] {
                    model.compactMode.set(true);
                    dismiss();
                }),
                Button("Restore defaults").variant(wui::ButtonVariant::Ghost).onClick([&window, &model, dismiss] {
                    dismiss();
                    showResetDialog(window, "Restore default settings?", "Your local choices will be reset.", [&model] {
                        model.systemTheme.set(true); model.compactMode.set(false); model.notifications.set(true);
                        model.quietHours.set(false); model.diagnostics.set(false); model.density.set(100);
                    });
                }),
                Button("Close menu").variant(wui::ButtonVariant::Ghost).onClick(dismiss))));
    *id = window.overlayHost().show(std::move(popup));
}
#endif

} // namespace

int main(int argc, char** argv)
{
#ifdef WUI_SETTINGS_INTERACTIVE
    SettingsModel model;
    try {
        return wui::runGlfwApp("WhatsUI Settings", {900.0f, 680.0f}, [&model](wui::UiWindow& window) {
            return buildSettingsUi(model,
                [&window](std::string title, std::string detail, Action confirm) {
                    showResetDialog(window, std::move(title), std::move(detail), std::move(confirm));
                },
                [&window, &model] { showPopupMenu(window, model); });
        });
    } catch (const std::exception& error) {
        std::cerr << "FATAL: " << error.what() << std::endl;
        return 1;
    }
#else
    int width = 900;
    int height = 680;
    constexpr float scale = 2.0f;
    std::filesystem::path output{"settings_visual"};
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--size") {
            if (++index >= argc || std::sscanf(argv[index], "%dx%d", &width, &height) != 2 || width < 480 || height < 420) {
                std::cerr << "--size expects WIDTHxHEIGHT (minimum 480x420)" << std::endl;
                return 2;
            }
        } else if (argument.rfind("--", 0) == 0) {
            std::cerr << "unknown argument: " << argument << std::endl;
            return 2;
        } else {
            output = argument;
        }
    }
    std::filesystem::create_directories(output);
    SettingsModel model;
    auto root = buildSettingsUi(model, [](std::string, std::string, Action action) { action(); }, [] {});
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, static_cast<int>(width * scale), static_cast<int>(height * scale));
    if (!canvas || !canvas->initializeContext()) throw std::runtime_error("failed to create software canvas");
    wui::WhatsCanvasTextMeasurer text(*canvas, scale);
    wui::setTextMeasurer(&text);
    root->layout({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});
    wui::PaintContext paint(*canvas, scale);
    root->prepare(paint);
    for (int pass = 0; pass < 2; ++pass) {
        canvas->beginFrame();
        paint.fillRect({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, wui::theme().colors.background);
        root->paint(paint);
        canvas->endFrame();
    }
    const auto path = output / "settings_general.ppm";
    if (!canvas->savePixelsPPM(path.string())) throw std::runtime_error("failed to save capture");
    wui::setTextMeasurer(nullptr);
    std::cout << "wrote " << path << std::endl;
    return 0;
#endif
}
