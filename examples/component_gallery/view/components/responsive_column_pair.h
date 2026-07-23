#pragma once

#include "wui/node.h"

namespace whatsui::gallery::view::components {

// A content-level two-column layout that becomes a single column below the
// Gallery breakpoint. It is intentionally not a change to the framework Row:
// application pages opt in where their cards must remain readable on resize.
class ResponsiveColumnPair final : public wui::ContainerNode {
public:
    ResponsiveColumnPair& gap(float value) noexcept;
    void setGap(float value) noexcept;
    ResponsiveColumnPair& align(wui::Alignment value) noexcept;
    void setAlign(wui::Alignment value) noexcept;

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override;
    void layout(const wui::RectF& bounds) override;

private:
    [[nodiscard]] bool compact(float width) const noexcept;

    float gap_{12.0f};
    wui::Alignment alignment_{wui::Alignment::Stretch};
};

} // namespace whatsui::gallery::view::components
