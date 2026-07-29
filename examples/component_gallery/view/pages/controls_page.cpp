#include "controls_page.h"

#include <utility>
#include <vector>

#include "view/components/page_header.h"
#include "view/components/preview_surface.h"
#include "view/components/responsive_action_grid.h"
#include "wui/theme.h"
#include "wui/ui.h"

using namespace wui::ui;

namespace whatsui::gallery::view::pages {
namespace {

std::unique_ptr<wui::Node> buildButtonSection(const std::function<void()>& onOpenDetail)
{
    return view::components::buildPreviewSurface(
        {"Button", "Appearance and intent", "Interactive", 224.0f, true},
        Column()
        .gap(14.0f)
        .align(wui::Alignment::Center)
        .children(
            std::make_unique<view::components::ResponsiveActionGrid>(
                std::vector<view::components::ResponsiveActionGrid::ItemFactory>{
                    [] { return Button("Primary").appearance(wui::ButtonAppearance::Primary).intoNode(); },
                    [] { return Button("Secondary").appearance(wui::ButtonAppearance::Secondary).intoNode(); },
                    [] { return Button("Subtle").appearance(wui::ButtonAppearance::Subtle).intoNode(); },
                    [] { return Button("Danger").appearance(wui::ButtonAppearance::Danger).intoNode(); },
                }, 520.0f, 0.0f, 200.0f),

            Button("Open Button detail")
            .appearance(wui::ButtonAppearance::Transparent)
            .icon(wui::IconName::ChevronRight)
            .iconPosition(wui::ButtonIconPosition::After)
            .onClick(onOpenDetail)
        )
        .intoNode());
}

std::unique_ptr<wui::Node> buildCheckboxSection()
{
    return view::components::buildPreviewSurface(
        {"Checkbox", "Binary and mixed selection", "Interactive", 216.0f, true},
        Column()
        .gap(12.0f)
        .align(wui::Alignment::Stretch)
        .children(
            Checkbox("Unchecked", false),
            Checkbox("Checked option", true),
            Checkbox("Mixed selection").mixed(),
            Checkbox("Disabled option", false).enabled(false)
        )
        .intoNode());
}

std::unique_ptr<wui::Node> buildToggleSection()
{
    return view::components::buildPreviewSurface(
        {"Toggle", "Switches and toggle buttons", "Interactive", 176.0f, true},
        Column()
        .gap(14.0f)
        .align(wui::Alignment::Start)
        .children(
            Switch("Enable notifications", true),
            Switch("Use compact layout", false),
            Row()
            .gap(8.0f)
            .children(
                ToggleButton("Pinned", true).icon(wui::IconName::Important),
                ToggleButton("Favorite", false).icon(wui::IconName::Star)
            )
        )
        .intoNode());
}

std::unique_ptr<wui::Node> buildSliderSection()
{
    auto volumeText = std::make_unique<wui::Text>("Volume · 64");
    volumeText->setFontSize(12.0f);
    volumeText->setColor(wui::theme().colors.textMuted);
    auto* volumeLabel = volumeText.get();
    auto volume = std::make_unique<wui::Slider>(0.0f, 100.0f, 64.0f);
    volume->setStep(1.0f);
    volume->setAccessibleLabel("Volume");
    volume->onChange([volumeLabel](float value) {
        volumeLabel->setValue("Volume · " + std::to_string(static_cast<int>(value)));
    });

    auto zoomText = std::make_unique<wui::Text>("Zoom · 125%");
    zoomText->setFontSize(12.0f);
    zoomText->setColor(wui::theme().colors.textMuted);
    auto* zoomLabel = zoomText.get();
    auto zoom = std::make_unique<wui::Slider>(50.0f, 200.0f, 125.0f);
    zoom->setStep(25.0f);
    zoom->setAccessibleLabel("Zoom");
    zoom->onChange([zoomLabel](float value) {
        zoomLabel->setValue("Zoom · " + std::to_string(static_cast<int>(value)) + "%");
    });

    return view::components::buildPreviewSurface(
        {"Slider", "Continuous and stepped values", "Interactive", 176.0f, true},
        Column()
        .gap(16.0f)
        .align(wui::Alignment::Stretch)
        .children(
            std::move(volumeText),
            std::move(volume),
            std::move(zoomText),
            std::move(zoom)
        )
        .intoNode());
}

} // namespace

std::unique_ptr<wui::Node> buildControlsPage(std::function<void()> onOpenButtonDetail)
{
    return ScrollView()
        .children(
            Column()
            .gap(20.0f)
            .padding({32.0f, 32.0f, 40.0f, 32.0f})
            .align(wui::Alignment::Stretch)
            .children(
                view::components::buildPageHeader({"COMPONENTS", "Controls", "Core interactive controls rendered with their real WhatsUI implementations.", {}}),
                buildButtonSection(onOpenButtonDetail),
                buildCheckboxSection(),
                buildToggleSection(),
                buildSliderSection()
            )
        )
        .intoNode();
}

} // namespace whatsui::gallery::view::pages
