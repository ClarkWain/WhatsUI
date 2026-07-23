#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "wui/app.h"
#include "wui/node.h"

namespace wui {
class Column;
}

namespace whatsui::gallery::view::components {

// A page shell that selects navigation from actual laid-out logical width.
// This makes resize/DPI changes deterministic without asking page view-models
// to track presentation-only breakpoint state.
class ResponsiveGalleryShell final : public wui::ContainerNode {
public:
    using NavigationFactory = std::function<std::unique_ptr<wui::Node>(bool compact)>;

    ResponsiveGalleryShell(wui::UiWindow& window,
                           std::unique_ptr<wui::Node> pageContent,
                           NavigationFactory navigationFactory);

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override;
    void layout(const wui::RectF& bounds) override;
    void paint(wui::PaintContext& context) override;
    [[nodiscard]] wui::Node* hitTest(wui::PointF point) override;

    [[nodiscard]] bool compact() const noexcept;
    static constexpr float kCompactBreakpoint{720.0f};

private:
    void updateNavigationMode(bool compact);
    void applyPageInsets();

    wui::UiWindow* window_{nullptr};
    wui::Node* pageContent_{nullptr};
    wui::Node* navigation_{nullptr};
    NavigationFactory navigationFactory_;
    bool compact_{false};
    bool modeInitialized_{false};
    wui::Column* pageColumn_{nullptr};
    std::optional<wui::InsetsF> desktopPageInsets_;
};

} // namespace whatsui::gallery::view::components
