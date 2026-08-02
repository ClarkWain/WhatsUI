#include "visual_qa_page.h"

#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include "view/components/page_header.h"
#include "view/components/preview_surface.h"
#include "view/components/responsive_layouts.h"
#include "wui/theme.h"
#include "wui/declarative.h"
#include "wui/ui_inspector.h"

using namespace wui;

namespace whatsui::gallery::view::pages {
namespace {

std::string scaleLabel(float scale)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << scale << "x actual window scale";
    return stream.str();
}

class WindowScaleProbe final : public wui::Node {
public:
    WindowScaleProbe(wui::UiWindow& window, VisualQaViewModel& viewModel)
        : window_(&window)
        , viewModel_(&viewModel)
    {
    }

    [[nodiscard]] wui::SizeF measure(const wui::Constraints&) const override { return {}; }

    void prepare(wui::PaintContext& context) override
    {
        wui::Node::prepare(context);
        viewModel_->setActualScaleFactor(window_->platformWindow().metrics().scaleFactor);
    }

    void paint(wui::PaintContext&) override {}

private:
    wui::UiWindow* window_;
    VisualQaViewModel* viewModel_;
};

void setInteractionState(wui::ButtonNode& button, InteractionPreview interaction)
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

void bindInteraction(wui::Node& owner, wui::ButtonNode& button, VisualQaViewModel& viewModel)
{
    setInteractionState(button, viewModel.selectedInteraction().get());
    const auto id = viewModel.selectedInteraction().subscribe(
        [&button](InteractionPreview value) { setInteractionState(button, value); });
    owner.addTeardown([&viewModel, id] { viewModel.selectedInteraction().unsubscribe(id); });
}

std::unique_ptr<wui::Node> buildProfileControls(VisualQaViewModel& viewModel)
{
    
    constexpr std::array<std::pair<const char*, DpiProfile>, 5> profiles{{
        {"System", DpiProfile::System}, {"100%", DpiProfile::Dpi100},
        {"125%", DpiProfile::Dpi125}, {"150%", DpiProfile::Dpi150},
        {"200%", DpiProfile::Dpi200},
    }};
    auto row = std::make_unique<view::components::ResponsiveFlow>();
    auto buttons = std::make_shared<std::vector<std::pair<DpiProfile, wui::ToggleButtonNode*>>>();
    row->gap(6.0f);
    for (const auto& [label, profile] : profiles) {
        auto button = std::make_unique<wui::ToggleButtonNode>(
            label, profile == viewModel.selectedDpi().get());
        button->setAppearance(wui::ButtonAppearance::Subtle);
        auto* raw = button.get();
        buttons->push_back({profile, raw});
        button->onChange([&viewModel, profile, buttons, raw](bool checked) {
            if (!checked) {
                raw->setChecked(true);
                return;
            }
            viewModel.selectDpi(profile);
            for (const auto& [candidate, item] : *buttons) {
                item->setChecked(candidate == profile);
            }
        });
        row->appendChild(std::move(button));
    }
    return row;
}

std::unique_ptr<wui::Node> buildActiveDpi(VisualQaViewModel& viewModel)
{
    return Card()
        .appearance(wui::CardAppearance::FilledAlternative)
        .children(
            Column()
            .gap(10.0f)
            .align(wui::Alignment::Stretch)
            .children(
                [] {
                    auto heading = std::make_unique<view::components::ResponsiveRow>();
                    heading->align(wui::Alignment::Center);
                    heading->appendChild(Text("DPI profile").size(16.0f).weight(600).build());
                    heading->appendChild(Spacer().flex(1.0f).build());
                    heading->appendChild(Badge("LIVE WINDOW VALUE")
                        .appearance(wui::BadgeAppearance::Tint)
                        .color(wui::BadgeColor::Brand)
                        .build());
                    return heading;
                }(),
                Text().bind(viewModel.actualScaleFactor(), scaleLabel)
                    .size(13.0f)
                    .color(wui::theme().colors.textMuted),
                Text("Marks the review target only; the native window DPR is unchanged.")
                    .size(11.0f)
                    .wrap()
                    .color(wui::theme().colors.textMuted),
                buildProfileControls(viewModel)
            )
        )
        .build();
}

std::unique_ptr<wui::Node> buildThemeControls(
    VisualQaViewModel& viewModel,
    const ApplyVisualQaThemeHandler& applyTheme)
{
    auto row = std::make_unique<view::components::ResponsiveFlow>();
    row->gap(6.0f);
    for (const auto& [label, theme] : std::array<std::pair<const char*, ThemePreview>, 2>{{
             {"Light preview", ThemePreview::Light}, {"Dark preview", ThemePreview::Dark}}}) {
        auto button = std::make_unique<wui::ToggleButtonNode>(
            label, theme == viewModel.selectedTheme().get());
        button->setAppearance(wui::ButtonAppearance::Subtle);
        auto* raw = button.get();
        button->onChange([&viewModel, theme, applyTheme, raw](bool checked) {
            if (!checked) {
                raw->setChecked(true);
                return;
            }
            viewModel.selectTheme(theme);
            if (applyTheme) applyTheme(theme);
        });
        row->appendChild(std::move(button));
    }
    return row;
}

std::unique_ptr<wui::Node> buildInteractionControls(VisualQaViewModel& viewModel)
{
    const std::array<std::pair<const char*, InteractionPreview>, 5> states{{
        {"Rest", InteractionPreview::Rest}, {"Hover", InteractionPreview::Hovered},
        {"Pressed", InteractionPreview::Pressed}, {"Focused", InteractionPreview::Focused},
        {"Disabled", InteractionPreview::Disabled},
    }};
    auto row = std::make_unique<view::components::ResponsiveFlow>();
    auto buttons = std::make_shared<
        std::vector<std::pair<InteractionPreview, wui::ToggleButtonNode*>>>();
    row->gap(6.0f);
    for (const auto& [label, state] : states) {
        auto button = std::make_unique<wui::ToggleButtonNode>(
            label, state == viewModel.selectedInteraction().get());
        button->setAppearance(wui::ButtonAppearance::Subtle);
        auto* raw = button.get();
        buttons->push_back({state, raw});
        button->onChange([&viewModel, state, buttons, raw](bool checked) {
            if (!checked) {
                raw->setChecked(true);
                return;
            }
            viewModel.selectInteraction(state);
            for (const auto& [candidate, item] : *buttons) {
                item->setChecked(candidate == state);
            }
        });
        row->appendChild(std::move(button));
    }
    return row;
}

std::unique_ptr<wui::Node> buildStateMatrix(VisualQaViewModel& viewModel, wui::ButtonNode*& preview)
{
    auto button = std::make_unique<wui::ButtonNode>("Review state");
    button->setAppearance(wui::ButtonAppearance::Primary);
    preview = button.get();
    return view::components::buildPreviewSurface(
        {"Interaction state matrix", "Choose a state to apply to the real Button", "LIVE STATE", 210.0f, true},
        Column()
        .gap(18.0f)
        .align(wui::Alignment::Center)
        .children(std::move(button), buildInteractionControls(viewModel))
        .build());
}

std::unique_ptr<wui::DialogNode> buildInspectorDialog(wui::UiWindow& window)
{
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
                Button("Close").onClick([&window] { (void)window.dismissTopDialog(); }))
            )
        )
        .build();
}

std::unique_ptr<wui::Node> buildInspectorEntry(wui::UiWindow& window)
{
    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            [&window] {
                auto entry = std::make_unique<view::components::ResponsiveRow>();
                entry->gap(14.0f).align(wui::Alignment::Center);
                entry->appendChild(Icon(wui::IconName::TaskList)
                    .color(wui::theme().colors.accent).build());
                entry->appendChild(Column().gap(3.0f).children(
                    Text("Inspector snapshot").size(15.0f).weight(600),
                    Text("Capture the currently installed Visual QA tree on demand.")
                        .size(11.0f).color(wui::theme().colors.textMuted)).build());
                entry->appendChild(Spacer().flex(1.0f).build());
                entry->appendChild(Button("Inspect now").onClick([&window] {
                    (void)window.showDialog(buildInspectorDialog(window));
                }).build());
                return entry;
            }()
        ).build();
}

std::unique_ptr<wui::Node> buildSampleRuns()
{
    auto run = [](std::string name, std::string detail) {
        auto item = std::make_unique<view::components::ResponsiveRow>();
        item->gap(12.0f).align(wui::Alignment::Center);
        item->appendChild(Icon(wui::IconName::CheckmarkCircle)
            .color(wui::theme().colors.success).build());
        item->appendChild(Column().gap(2.0f).children(
            Text(std::move(name)).size(13.0f).weight(600),
            Text(std::move(detail)).size(10.0f).color(wui::theme().colors.textMuted)).build());
        item->appendChild(Spacer().flex(1.0f).build());
        item->appendChild(Badge("SAMPLE").appearance(wui::BadgeAppearance::Outline)
            .color(wui::BadgeColor::Neutral).build());
        return item;
    };
    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            Column()
            .gap(12.0f)
            .align(wui::Alignment::Stretch)
            .children(
                [] {
                    auto heading = std::make_unique<view::components::ResponsiveRow>();
                    heading->align(wui::Alignment::Center);
                    heading->appendChild(Text("Recent runs").size(16.0f).weight(600).build());
                    heading->appendChild(Spacer().flex(1.0f).build());
                    heading->appendChild(Badge("DEMO DATA").appearance(wui::BadgeAppearance::Tint)
                        .color(wui::BadgeColor::Warning).build());
                    return heading;
                }(),
                Text("Illustrative local snapshots only; this page is not connected to CI or CTest history.")
                    .size(11.0f).wrap().color(wui::theme().colors.textMuted),
                run("Fluent component matrix", "Software · 100% and 150%"),
                Divider(),
                run("Button geometry review", "OpenGL · 100%, 125%, 150%, 200%"),
                Divider(),
                run("Text baseline acceptance", "Software · fractional DPI")
            )
        )
        .build();
}

} // namespace

std::unique_ptr<wui::Node> buildVisualQaPage(
    VisualQaViewModel& viewModel,
    wui::UiWindow& window,
    ApplyVisualQaThemeHandler applyTheme)
{
    wui::ButtonNode* preview = nullptr;
    auto root = ScrollView()
        .content(
            Column()
            .gap(20.0f)
            .padding({32.0f, 32.0f, 40.0f, 32.0f})
            .align(wui::Alignment::Stretch)
            .children(
                view::components::buildPageHeader({"QUALITY", "Visual QA", "Inspect active scale, control states, and clearly-labelled sample evidence.", {}}),
                buildActiveDpi(viewModel),
                buildThemeControls(viewModel, applyTheme),
                buildStateMatrix(viewModel, preview),
                buildInspectorEntry(window),
                buildSampleRuns(),
                std::make_unique<WindowScaleProbe>(window, viewModel)
            )
        )
        .build();
    bindInteraction(*root, *preview, viewModel);
    return root;
}

} // namespace whatsui::gallery::view::pages
