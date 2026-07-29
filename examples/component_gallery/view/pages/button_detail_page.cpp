#include "button_detail_page.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "view/components/page_header.h"
#include "view/components/preview_surface.h"
#include "view/components/responsive_choice_group.h"
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

std::string appearanceChoiceValue(ButtonAppearanceSample value)
{
    switch (value) {
    case ButtonAppearanceSample::Primary: return "primary";
    case ButtonAppearanceSample::Secondary: return "secondary";
    case ButtonAppearanceSample::Subtle: return "subtle";
    case ButtonAppearanceSample::Transparent: return "transparent";
    case ButtonAppearanceSample::Outline: return "outline";
    }
    return "primary";
}

std::string sizeChoiceValue(ButtonSizeSample value)
{
    switch (value) {
    case ButtonSizeSample::Small: return "small";
    case ButtonSizeSample::Medium: return "medium";
    case ButtonSizeSample::Large: return "large";
    }
    return "medium";
}

std::unique_ptr<wui::Node> buildAppearanceChoice(ButtonDetailViewModel& viewModel)
{
    return std::make_unique<view::components::ResponsiveChoiceGroup>(
        std::vector<view::components::ResponsiveChoiceOption>{
            {"primary", "Primary"}, {"secondary", "Secondary"},
            {"subtle", "Subtle"}, {"outline", "Outline"}},
        [&viewModel] { return appearanceChoiceValue(viewModel.appearance().get()); },
        [&viewModel](const std::string& value) {
            if (value == "primary") viewModel.selectAppearance(ButtonAppearanceSample::Primary);
            else if (value == "secondary") viewModel.selectAppearance(ButtonAppearanceSample::Secondary);
            else if (value == "subtle") viewModel.selectAppearance(ButtonAppearanceSample::Subtle);
            else if (value == "outline") viewModel.selectAppearance(ButtonAppearanceSample::Outline);
        },
        "Button appearance");
}

std::unique_ptr<wui::Node> buildSizeChoice(ButtonDetailViewModel& viewModel)
{
    return std::make_unique<view::components::ResponsiveChoiceGroup>(
        std::vector<view::components::ResponsiveChoiceOption>{
            {"small", "Small"}, {"medium", "Medium"}, {"large", "Large"}},
        [&viewModel] { return sizeChoiceValue(viewModel.size().get()); },
        [&viewModel](const std::string& value) {
            if (value == "small") viewModel.selectSize(ButtonSizeSample::Small);
            else if (value == "medium") viewModel.selectSize(ButtonSizeSample::Medium);
            else if (value == "large") viewModel.selectSize(ButtonSizeSample::Large);
        },
        "Button size");
}

std::unique_ptr<wui::Node> buildProperties(ButtonDetailViewModel& viewModel)
{
    using namespace wui::ui;
    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            Column()
            .gap(14.0f)
            .align(wui::Alignment::Stretch)
            .children(
                Text("Properties").size(16.0f).weight(600),
                Text("Appearance").size(12.0f).color(wui::theme().colors.textMuted),
                buildAppearanceChoice(viewModel),
                Text("Size").size(12.0f).color(wui::theme().colors.textMuted),
                buildSizeChoice(viewModel),
                Switch("Show leading icon", viewModel.iconVisible().get())
                    .onChange([&viewModel](bool value) { viewModel.setIconVisible(value); }),
                Switch("Enabled", viewModel.enabled().get())
                    .onChange([&viewModel](bool value) { viewModel.setEnabled(value); })
            )
        )
        .intoNode();
}

std::unique_ptr<wui::Node> buildStates()
{
    using namespace wui::ui;
    auto selected = std::make_shared<std::string>("rest");
    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            Column()
            .gap(14.0f)
            .align(wui::Alignment::Stretch)
            .children(
                Text("States").size(16.0f).weight(600),
                std::make_unique<view::components::ResponsiveChoiceGroup>(
                    std::vector<view::components::ResponsiveChoiceOption>{
                        {"rest", "Rest"}, {"hovered", "Hovered"}, {"pressed", "Pressed"},
                        {"focused", "Focused"}, {"disabled", "Disabled"}},
                    [selected] { return *selected; },
                    [selected](const std::string& value) { *selected = value; },
                    "Button visual state")
            )
        )
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
                Row().children(Text("Focus width").size(12.0f), Spacer().flex(1.0f), Text(std::to_string(static_cast<int>(current.controls.focusWidth)) + " px").size(12.0f))
            )
        )
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
                buildTokens()
            )
        )
        .intoNode();
        
    bindPreview(*root, *preview, viewModel);
    return root;
}

} // namespace whatsui::gallery::view::pages
