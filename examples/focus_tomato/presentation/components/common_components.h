#pragma once

#include <functional>
#include <memory>
#include <string>

#include "../focus_assets.h"
#include "wui/node.h"

namespace whatsui::focus_tomato::presentation {

[[nodiscard]] std::unique_ptr<wui::Node> buildWindowBar(
    float width, std::string title, const FocusAssets& assets);

[[nodiscard]] std::unique_ptr<wui::Node> buildPill(
    std::string label, bool selected, std::function<void()> onClick = {});

[[nodiscard]] std::unique_ptr<wui::Node> buildIconControl(
    const wui::ImageSource& icon,
    float controlSize,
    float iconSize,
    bool primary,
    std::string accessibleLabel,
    std::function<void()> onClick);

[[nodiscard]] std::unique_ptr<wui::Node> buildGlyphControl(
    std::string glyph,
    float controlSize,
    float glyphSize,
    bool primary,
    std::string accessibleLabel,
    std::function<void()> onClick);

[[nodiscard]] std::unique_ptr<wui::Node> buildPrimaryTextButton(
    std::string label, std::function<void()> onClick);

[[nodiscard]] std::unique_ptr<wui::Node> buildSecondaryTextButton(
    std::string label, std::function<void()> onClick);

[[nodiscard]] std::unique_ptr<wui::Node> buildMetricCard(
    float width,
    std::string label,
    std::string value,
    std::string unit);

[[nodiscard]] std::unique_ptr<wui::Node> buildFixedImage(
    const wui::ImageSource& source,
    float width,
    float height,
    std::string alt,
    bool circular = false,
    bool decorative = false);

} // namespace whatsui::focus_tomato::presentation
