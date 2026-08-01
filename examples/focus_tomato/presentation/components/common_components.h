#pragma once

#include <functional>
#include <string>

#include "../focus_assets.h"
#include "wui/declarative/layout.h"

namespace whatsui::focus_tomato::presentation {

[[nodiscard]] wui::Box buildWindowBar(
    float width, std::string title, const FocusAssets& assets);

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

[[nodiscard]] wui::Box buildMetricCard(
    float width,
    std::string label,
    std::string value,
    std::string unit);

[[nodiscard]] wui::Box buildFixedImage(
    const wui::ImageSource& source,
    float width,
    float height,
    std::string alt,
    bool circular = false,
    bool decorative = false);

} // namespace whatsui::focus_tomato::presentation
