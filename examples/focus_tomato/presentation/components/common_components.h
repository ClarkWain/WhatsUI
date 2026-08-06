#pragma once

#include <functional>
#include <string>

#include "../focus_assets.h"
#include "wui/declarative/layout.h"
#include "wui/declarative/structural.h"

namespace whatsui::focus_tomato::presentation {

class FocusViewModel;

inline constexpr float kFocusWindowBarHeight = 48.0f;

struct WindowBarActions {
    std::function<void()> minimize;
    std::function<void()> toggleMaximized;
    std::function<void()> close;
};

[[nodiscard]] wui::Box buildWindowBar(
    float width,
    std::string title,
    const FocusAssets& assets,
    WindowBarActions actions,
    bool allowMaximize = true);

[[nodiscard]] wui::Box buildPill(
    std::string label, bool selected, std::function<void()> onClick = {});

[[nodiscard]] wui::Box buildIconControl(
    const wui::ImageSource& icon,
    float controlSize,
    float iconSize,
    bool primary,
    std::string accessibleLabel,
    std::function<void()> onClick);

[[nodiscard]] wui::Box buildGlyphControl(
    std::string glyph,
    float controlSize,
    float glyphSize,
    bool primary,
    std::string accessibleLabel,
    std::function<void()> onClick);

[[nodiscard]] wui::Box buildPrimaryTextButton(
    std::string label, std::function<void()> onClick);

[[nodiscard]] wui::Box buildSecondaryTextButton(
    std::string label, std::function<void()> onClick);

[[nodiscard]] wui::Box buildPageNavigationAction(
    float width,
    std::string label,
    std::string automationId,
    std::function<void()> onClick);

[[nodiscard]] wui::Box buildMetricCard(
    float width,
    std::string label,
    std::string value,
    std::string unit);

[[nodiscard]] wui::If buildOperationBanner(
    FocusViewModel& viewModel, float width);

[[nodiscard]] wui::Box buildFixedImage(
    const wui::ImageSource& source,
    float width,
    float height,
    std::string alt,
    bool circular = false,
    bool decorative = false);

} // namespace whatsui::focus_tomato::presentation
