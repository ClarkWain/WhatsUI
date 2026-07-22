#include "visual_qa_page.h"

#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include "view/components/page_header.h"
#include "view/components/preview_surface.h"
#include "wui/theme.h"
#include "wui/ui.h"
#include "wui/ui_inspector.h"

namespace whatsui::gallery::view::pages {
namespace {

std::string scaleLabel(float scale)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << scale << "x actual window scale";
    return stream.str();
}

void setInteractionState(wui::Button& button, InteractionPreview interaction)
{
    for (const auto state : {wui::ControlVisualState::Hovered,
                             wui::ControlVisualState::Pressed,
                             wui::ControlVisualState::Focused,
                             wui::ControlVisualState::FocusVisible}) {
        button.setVisualState(state, false);
    }
    button.setEnabled(interaction != InteractionPreview::Disabled);
    if (interaction == InteractionPreview::Hovered) {
        button.setVisualState(wui::ControlVisualState::Hovered, true);
    } else if (interaction == InteractionPreview::Pressed) {
        button.setVisualState(wui::ControlVisualState::Pressed, true);
    } else if (interaction == InteractionPreview::Focused) {
        button.setVisualState(wui::ControlVisualState::Focused, true);
        button.setVisualState(wui::ControlVisualState::FocusVisible, true);
    }
}

void bindInteraction(wui::Node& owner, wui::Button& button, VisualQaViewModel& viewModel)
{
    setInteractionState(button, viewModel.selectedInteraction().get());
    const auto id = viewModel.selectedInteraction().subscribe(
        [&button](InteractionPreview value) { setInteractionState(button, value); });
    owner.addTeardown([&viewModel, id] { viewModel.selectedInteraction().unsubscribe(id); });
}

std::unique_ptr<wui::Node> buildProfileControls(VisualQaViewModel& viewModel)
{
    using namespace wui::ui;
    constexpr std::array<std::pair<const char*, DpiProfile>, 5> profiles{{
        {"System", DpiProfile::System}, {"100%", DpiProfile::Dpi100},
        {"125%", DpiProfile::Dpi125}, {"150%", DpiProfile::Dpi150},
        {"200%", DpiProfile::Dpi200},
    }};
    auto row = std::make_unique<wui::Row>();
    auto buttons = std::make_shared<std::vector<std::pair<DpiProfile, wui::Button*>>>();
    row->setGap(6.0f);
    row->setAlign(wui::Alignment::Center);
    for (const auto& [label, profile] : profiles) {
        auto button = std::make_unique<wui::Button>(label);
        button->setAppearance(profile == viewModel.selectedDpi().get()
            ? wui::ButtonAppearance::Primary
            : wui::ButtonAppearance::Subtle);
        auto* raw = button.get();
        buttons->push_back({profile, raw});
        button->onClick([&viewModel, profile, buttons] {
            viewModel.selectDpi(profile);
            for (const auto& [candidate, item] : *buttons) {
                item->setAppearance(candidate == profile
                    ? wui::ButtonAppearance::Primary
                    : wui::ButtonAppearance::Subtle);
            }
        });
        row->appendChild(std::move(button));
    }
    return row;
}

std::unique_ptr<wui::Node> buildActiveDpi(VisualQaViewModel& viewModel)
{
    using namespace wui::ui;
    return Card()
        .appearance(wui::CardAppearance::FilledAlternative)
        .children(
            Column()
                .gap(10.0f)
                .align(wui::Alignment::Stretch)
                .children(
                    Row().align(wui::Alignment::Center).children(
                        Text("DPI profile").size(16.0f).weight(600),
                        Spacer().flex(1.0f),
                        Badge("LIVE WINDOW VALUE").appearance(wui::BadgeAppearance::Tint).color(wui::BadgeColor::Brand)),
                    Text().bind(viewModel.actualScaleFactor(), scaleLabel)
                        .size(13.0f)
                        .color(wui::theme().colors.textMuted),
                    Text("Profile buttons select a review target; they do not pretend to change the native window DPR.")
                        .size(11.0f)
                        .wrap()
                        .color(wui::theme().colors.textMuted),
                    buildProfileControls(viewModel)))
        .intoNode();
}

std::unique_ptr<wui::Node> buildThemeControls(VisualQaViewModel& viewModel)
{
    auto row = std::make_unique<wui::Row>();
    row->setGap(6.0f);
    auto buttons = std::make_shared<std::vector<std::pair<ThemePreview, wui::Button*>>>();
    for (const auto& [label, theme] : std::array<std::pair<const char*, ThemePreview>, 2>{{
             {"Light preview", ThemePreview::Light}, {"Dark preview", ThemePreview::Dark}}}) {
        auto button = std::make_unique<wui::Button>(label);
        button->setAppearance(theme == viewModel.selectedTheme().get()
            ? wui::ButtonAppearance::Primary
            : wui::ButtonAppearance::Subtle);
        auto* raw = button.get();
        buttons->push_back({theme, raw});
        button->onClick([&viewModel, theme, buttons] {
            viewModel.selectTheme(theme);
            for (const auto& [candidate, item] : *buttons) {
                item->setAppearance(candidate == theme
                    ? wui::ButtonAppearance::Primary
                    : wui::ButtonAppearance::Subtle);
            }
        });
        row->appendChild(std::move(button));
    }
    return row;
}

std::unique_ptr<wui::Node> buildInteractionControls(VisualQaViewModel& viewModel)
{
    using namespace wui::ui;
    const std::array<std::pair<const char*, InteractionPreview>, 5> states{{
        {"Rest", InteractionPreview::Rest}, {"Hover", InteractionPreview::Hovered},
        {"Pressed", InteractionPreview::Pressed}, {"Focused", InteractionPreview::Focused},
        {"Disabled", InteractionPreview::Disabled},
    }};
    auto row = std::make_unique<wui::Row>();
    auto buttons = std::make_shared<std::vector<std::pair<InteractionPreview, wui::Button*>>>();
    row->setGap(6.0f);
    for (const auto& [label, state] : states) {
        auto button = std::make_unique<wui::Button>(label);
        button->setAppearance(state == viewModel.selectedInteraction().get()
            ? wui::ButtonAppearance::Primary
            : wui::ButtonAppearance::Subtle);
        auto* raw = button.get();
        buttons->push_back({state, raw});
        button->onClick([&viewModel, state, buttons] {
            viewModel.selectInteraction(state);
            for (const auto& [candidate, item] : *buttons) {
                item->setAppearance(candidate == state
                    ? wui::ButtonAppearance::Primary
                    : wui::ButtonAppearance::Subtle);
            }
        });
        row->appendChild(std::move(button));
    }
    return row;
}

std::unique_ptr<wui::Node> buildStateMatrix(VisualQaViewModel& viewModel, wui::Button*& preview)
{
    using namespace wui::ui;
    auto button = std::make_unique<wui::Button>("Review state");
    button->setAppearance(wui::ButtonAppearance::Primary);
    preview = button.get();
    return view::components::buildPreviewSurface(
        {"Interaction state matrix", "Choose a state to apply to the real Button", "LIVE STATE", 210.0f, true},
        Column()
            .gap(18.0f)
            .align(wui::Alignment::Center)
            .children(std::move(button), buildInteractionControls(viewModel))
            .intoNode());
}

std::unique_ptr<wui::Dialog> buildInspectorDialog(wui::UiWindow& window)
{
    using namespace wui::ui;
    std::size_t nodeCount = 0;
    std::size_t dirtyCount = 0;
    if (const auto* root = window.root()) {
        const auto summary = wui::inspectUiDirty(*root);
        nodeCount = summary.nodeCount;
        dirtyCount = summary.dirtyNodeCount;
    }
    return Dialog().maxWidth(420.0f).dismissOnBackdrop().content(
        Box().width(380.0f).padding(20.0f).children(
            Column().gap(12.0f).align(wui::Alignment::Stretch).children(
                Text("Current page inspector").size(18.0f).weight(600),
                Text(std::to_string(nodeCount) + " nodes · " + std::to_string(dirtyCount) + " dirty")
                    .size(12.0f).color(wui::theme().colors.textMuted),
                Text("Snapshot captured when this dialog opened.").size(11.0f),
                Button("Close").onClick([&window] { (void)window.dismissTopDialog(); }))))
        .intoDialog();
}

std::unique_ptr<wui::Node> buildInspectorEntry(wui::UiWindow& window)
{
    using namespace wui::ui;
    return Card().appearance(wui::CardAppearance::Outline).children(
        Row().gap(14.0f).align(wui::Alignment::Center).children(
            Icon(wui::IconName::TaskList).color(wui::theme().colors.accent),
            Column().gap(3.0f).children(
                Text("Inspector snapshot").size(15.0f).weight(600),
                Text("Capture the currently installed Visual QA tree on demand.")
                    .size(11.0f).color(wui::theme().colors.textMuted)),
            Spacer().flex(1.0f),
            Button("Inspect now").onClick([&window] {
                (void)window.showDialog(buildInspectorDialog(window));
            }))).intoNode();
}

std::unique_ptr<wui::Node> buildSampleRuns()
{
    using namespace wui::ui;
    auto run = [](std::string name, std::string detail) {
        return Row()
            .gap(12.0f)
            .align(wui::Alignment::Center)
            .children(
                Icon(wui::IconName::CheckmarkCircle).color(wui::theme().colors.success),
                Column().gap(2.0f).children(
                    Text(std::move(name)).size(13.0f).weight(600),
                    Text(std::move(detail)).size(10.0f).color(wui::theme().colors.textMuted)),
                Spacer().flex(1.0f),
                Badge("SAMPLE").appearance(wui::BadgeAppearance::Outline).color(wui::BadgeColor::Neutral));
    };
    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            Column()
                .gap(12.0f)
                .align(wui::Alignment::Stretch)
                .children(
                    Row().align(wui::Alignment::Center).children(
                        Text("Recent runs").size(16.0f).weight(600),
                        Spacer().flex(1.0f),
                        Badge("DEMO DATA").appearance(wui::BadgeAppearance::Tint).color(wui::BadgeColor::Warning)),
                    Text("Illustrative local snapshots only; this page is not connected to CI or CTest history.")
                        .size(11.0f).wrap().color(wui::theme().colors.textMuted),
                    run("Fluent component matrix", "Software · 100% and 150%"),
                    Divider(),
                    run("Button geometry review", "OpenGL · 100%, 125%, 150%, 200%"),
                    Divider(),
                    run("Text baseline acceptance", "Software · fractional DPI")))
        .intoNode();
}

} // namespace

std::unique_ptr<wui::Node> buildVisualQaPage(
    VisualQaViewModel& viewModel,
    wui::UiWindow& window)
{
    using namespace wui::ui;
    wui::Button* preview = nullptr;
    auto root = ScrollView()
        .children(
            Column()
                .gap(20.0f)
                .padding({32.0f, 32.0f, 40.0f, 32.0f})
                .align(wui::Alignment::Stretch)
                .children(
                    view::components::buildPageHeader({"QUALITY", "Visual QA", "Inspect active scale, control states, and clearly-labelled sample evidence.", {}}),
                    buildActiveDpi(viewModel),
                    buildThemeControls(viewModel),
                    buildStateMatrix(viewModel, preview),
                    buildInspectorEntry(window),
                    buildSampleRuns()))
        .intoNode();
    bindInteraction(*root, *preview, viewModel);
    return root;
}

} // namespace whatsui::gallery::view::pages
