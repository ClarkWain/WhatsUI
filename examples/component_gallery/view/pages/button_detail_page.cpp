#include "button_detail_page.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "view/components/page_header.h"
#include "view/components/preview_surface.h"
#include "wui/theme.h"
#include "wui/ui.h"

namespace whatsui::gallery::view::pages {
namespace {

wui::ButtonAppearance toAppearance(ButtonAppearanceSample value)
{
    switch (value) {
    case ButtonAppearanceSample::Primary: return wui::ButtonAppearance::Primary;
    case ButtonAppearanceSample::Secondary: return wui::ButtonAppearance::Secondary;
    case ButtonAppearanceSample::Subtle: return wui::ButtonAppearance::Subtle;
    case ButtonAppearanceSample::Transparent: return wui::ButtonAppearance::Transparent;
    case ButtonAppearanceSample::Outline: return wui::ButtonAppearance::Outline;
    }
    return wui::ButtonAppearance::Primary;
}

wui::ButtonSize toSize(ButtonSizeSample value)
{
    switch (value) {
    case ButtonSizeSample::Small: return wui::ButtonSize::Small;
    case ButtonSizeSample::Medium: return wui::ButtonSize::Medium;
    case ButtonSizeSample::Large: return wui::ButtonSize::Large;
    }
    return wui::ButtonSize::Medium;
}

void updatePreview(wui::Button& button, const ButtonDetailViewModel& viewModel)
{
    button.setAppearance(toAppearance(viewModel.appearance().get()));
    button.setSize(toSize(viewModel.size().get()));
    button.setIcon(viewModel.iconVisible().get()
        ? std::optional<wui::IconName>{wui::IconName::Add}
        : std::nullopt);
    button.setEnabled(viewModel.enabled().get());
}

void bindPreview(wui::Node& owner, wui::Button& button, ButtonDetailViewModel& viewModel)
{
    updatePreview(button, viewModel);
    const auto appearanceId = viewModel.appearance().subscribe(
        [&button, &viewModel](const ButtonAppearanceSample&) { updatePreview(button, viewModel); });
    const auto sizeId = viewModel.size().subscribe(
        [&button, &viewModel](const ButtonSizeSample&) { updatePreview(button, viewModel); });
    const auto iconId = viewModel.iconVisible().subscribe(
        [&button, &viewModel](const bool&) { updatePreview(button, viewModel); });
    const auto enabledId = viewModel.enabled().subscribe(
        [&button, &viewModel](const bool&) { updatePreview(button, viewModel); });
    owner.addTeardown([&viewModel, appearanceId, sizeId, iconId, enabledId] {
        viewModel.appearance().unsubscribe(appearanceId);
        viewModel.size().unsubscribe(sizeId);
        viewModel.iconVisible().unsubscribe(iconId);
        viewModel.enabled().unsubscribe(enabledId);
    });
}

std::unique_ptr<wui::Node> buildLivePreview(wui::Button*& previewOut)
{
    auto button = std::make_unique<wui::Button>("Create project");
    previewOut = button.get();
    return view::components::buildPreviewSurface(
        {"Preview", "Properties update this live control", "Interactive", 220.0f, true},
        std::move(button));
}

template <typename Enum>
std::unique_ptr<wui::Node> buildChoiceRow(
    const std::vector<std::pair<const char*, Enum>>& choices,
    Enum selected,
    std::function<void(Enum)> onSelect)
{
    using namespace wui::ui;
    auto row = std::make_unique<wui::Row>();
    auto buttons = std::make_shared<std::vector<std::pair<Enum, wui::Button*>>>();
    row->setGap(6.0f);
    row->setAlign(wui::Alignment::Center);
    for (const auto& [label, value] : choices) {
        auto button = std::make_unique<wui::Button>(label);
        button->setAppearance(value == selected
            ? wui::ButtonAppearance::Primary
            : wui::ButtonAppearance::Subtle);
        auto* raw = button.get();
        buttons->push_back({value, raw});
        button->onClick([onSelect, value, buttons] {
            onSelect(value);
            for (const auto& [candidate, item] : *buttons) {
                item->setAppearance(candidate == value
                    ? wui::ButtonAppearance::Primary
                    : wui::ButtonAppearance::Subtle);
            }
        });
        row->appendChild(std::move(button));
    }
    return row;
}

std::unique_ptr<wui::Node> buildProperties(ButtonDetailViewModel& viewModel)
{
    using namespace wui::ui;
    const std::vector<std::pair<const char*, ButtonAppearanceSample>> appearances{
        {"Primary", ButtonAppearanceSample::Primary},
        {"Secondary", ButtonAppearanceSample::Secondary},
        {"Subtle", ButtonAppearanceSample::Subtle},
        {"Outline", ButtonAppearanceSample::Outline},
    };
    const std::vector<std::pair<const char*, ButtonSizeSample>> sizes{
        {"Small", ButtonSizeSample::Small},
        {"Medium", ButtonSizeSample::Medium},
        {"Large", ButtonSizeSample::Large},
    };
    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            Column()
                .gap(14.0f)
                .align(wui::Alignment::Stretch)
                .children(
                    Text("Properties").size(16.0f).weight(600),
                    Text("Appearance").size(12.0f).color(wui::theme().colors.textMuted),
                    buildChoiceRow<ButtonAppearanceSample>(
                        appearances,
                        viewModel.appearance().get(),
                        [&viewModel](ButtonAppearanceSample value) { viewModel.selectAppearance(value); }),
                    Text("Size").size(12.0f).color(wui::theme().colors.textMuted),
                    buildChoiceRow<ButtonSizeSample>(
                        sizes,
                        viewModel.size().get(),
                        [&viewModel](ButtonSizeSample value) { viewModel.selectSize(value); }),
                    Switch("Show leading icon", viewModel.iconVisible().get())
                        .onChange([&viewModel](bool value) { viewModel.setIconVisible(value); }),
                    Switch("Enabled", viewModel.enabled().get())
                        .onChange([&viewModel](bool value) { viewModel.setEnabled(value); })))
        .intoNode();
}

std::unique_ptr<wui::Node> buildStateButton(
    std::string label,
    std::initializer_list<wui::ControlVisualState> states,
    bool enabled = true)
{
    auto button = std::make_unique<wui::Button>(std::move(label));
    button->setAppearance(wui::ButtonAppearance::Primary);
    button->setEnabled(enabled);
    for (const auto state : states) {
        button->setVisualState(state, true);
    }
    return button;
}

std::unique_ptr<wui::Node> buildStates()
{
    using namespace wui::ui;
    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            Column()
                .gap(14.0f)
                .align(wui::Alignment::Stretch)
                .children(
                    Text("States").size(16.0f).weight(600),
                    Row()
                        .gap(8.0f)
                        .children(
                            buildStateButton("Rest", {}),
                            buildStateButton("Hovered", {wui::ControlVisualState::Hovered}),
                            buildStateButton("Pressed", {wui::ControlVisualState::Pressed}),
                            buildStateButton("Focused", {wui::ControlVisualState::Focused, wui::ControlVisualState::FocusVisible}),
                            buildStateButton("Disabled", {}, false))))
        .intoNode();
}

std::unique_ptr<wui::Node> buildTokens()
{
    using namespace wui::ui;
    const auto& current = wui::theme();
    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            Column()
                .gap(10.0f)
                .align(wui::Alignment::Stretch)
                .children(
                    Text("Tokens").size(16.0f).weight(600),
                    Row().children(Text("Control height").size(12.0f), Spacer().flex(1.0f), Text(std::to_string(static_cast<int>(current.controls.height)) + " px").size(12.0f)),
                    Divider(),
                    Row().children(Text("Horizontal padding").size(12.0f), Spacer().flex(1.0f), Text(std::to_string(static_cast<int>(current.controls.horizontalPadding)) + " px").size(12.0f)),
                    Divider(),
                    Row().children(Text("Corner radius").size(12.0f), Spacer().flex(1.0f), Text(std::to_string(static_cast<int>(current.radius.md)) + " px").size(12.0f)),
                    Divider(),
                    Row().children(Text("Focus width").size(12.0f), Spacer().flex(1.0f), Text(std::to_string(static_cast<int>(current.controls.focusWidth)) + " px").size(12.0f))))
        .intoNode();
}

} // namespace

std::unique_ptr<wui::Node> buildButtonDetailPage(
    ButtonDetailViewModel& viewModel,
    std::function<void()> onBack)
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
                    view::components::buildPageHeader({"COMPONENT DETAIL", "Button", "Trigger an immediate action with a Fluent button.", {{"Back", wui::IconName::ChevronLeft, wui::ButtonAppearance::Subtle, std::move(onBack)}}}),
                    buildLivePreview(preview),
                    buildProperties(viewModel),
                    buildStates(),
                    buildTokens()))
        .intoNode();
    bindPreview(*root, *preview, viewModel);
    return root;
}

} // namespace whatsui::gallery::view::pages
