#include "responsive_gallery_shell.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "wui/theme.h"
#include "wui/widgets.h"

namespace whatsui::gallery::view::components {

ResponsiveGalleryShell::ResponsiveGalleryShell(wui::UiWindow& window,
                                               std::unique_ptr<wui::Node> pageContent,
                                               NavigationFactory navigationFactory)
    : window_(&window)
    , navigationFactory_(std::move(navigationFactory))
{
    if (!pageContent || !navigationFactory_) {
        throw std::invalid_argument("ResponsiveGalleryShell requires page content and navigation");
    }
    pageContent_ = pageContent.get();
    appendChild(std::move(pageContent));
}

wui::SizeF ResponsiveGalleryShell::measure(const wui::Constraints& constraints) const
{
    return constraints.clamp({constraints.maxWidth, constraints.maxHeight});
}

void ResponsiveGalleryShell::layout(const wui::RectF& bounds)
{
    wui::Node::layout(bounds);
    const bool nextCompact = bounds.width < kCompactBreakpoint;
    if (!modeInitialized_ || nextCompact != compact_) {
        updateNavigationMode(nextCompact);
    }
    applyPageInsets();

    if (compact_) {
        constexpr float kTopBarHeight = 48.0f;
        const float topBarHeight = std::min(kTopBarHeight, bounds.height);
        navigation_->layout({bounds.x, bounds.y, bounds.width, topBarHeight});
        pageContent_->layout({bounds.x, bounds.y + topBarHeight, bounds.width,
                              std::max(0.0f, bounds.height - topBarHeight)});
    } else {
        const float railWidth = std::min(232.0f, bounds.width);
        navigation_->layout({bounds.x, bounds.y, railWidth, bounds.height});
        pageContent_->layout({bounds.x + railWidth, bounds.y,
                              std::max(0.0f, bounds.width - railWidth), bounds.height});
    }
    clearLayoutDirtyRecursively();
}

void ResponsiveGalleryShell::paint(wui::PaintContext& context)
{
    context.fillRect(bounds(), wui::theme().colors.background);
    if (compact_) {
        pageContent_->paint(context);
        navigation_->paint(context);
    } else {
        navigation_->paint(context);
        pageContent_->paint(context);
    }
    clearDirty(wui::DirtyFlag::Paint);
}

wui::Node* ResponsiveGalleryShell::hitTest(wui::PointF point)
{
    if (!bounds().contains(point)) return nullptr;
    if (compact_) {
        if (auto* hit = navigation_->hitTest(point)) return hit;
        if (auto* hit = pageContent_->hitTest(point)) return hit;
    } else {
        if (auto* hit = pageContent_->hitTest(point)) return hit;
        if (auto* hit = navigation_->hitTest(point)) return hit;
    }
    return this;
}

bool ResponsiveGalleryShell::compact() const noexcept
{
    return compact_;
}

void ResponsiveGalleryShell::updateNavigationMode(bool compact)
{
    if (navigation_ != nullptr) {
        const auto& existing = children();
        for (std::size_t index = 0; index < existing.size(); ++index) {
            if (existing[index].get() == navigation_) {
                (void)removeChild(index);
                break;
            }
        }
        navigation_ = nullptr;
    }
    // Detached controls must not retain keyboard focus after a breakpoint
    // swaps the rail for the compact top bar (or vice versa).
    window_->focusManager().clear();
    compact_ = compact;
    modeInitialized_ = true;
    auto navigation = navigationFactory_(compact_);
    navigation_ = navigation.get();
    appendChild(std::move(navigation));
}

void ResponsiveGalleryShell::applyPageInsets()
{
    // Gallery pages are ScrollView > Column. Keep this responsive policy in
    // the shell so page view code remains about content rather than window
    // metrics. A 16 DIP inset preserves a useful content viewport at 267 DIP.
    if (pageColumn_ == nullptr) {
        auto* scroll = dynamic_cast<wui::ScrollViewNode*>(pageContent_);
        if (scroll == nullptr || scroll->children().empty()) return;
        pageColumn_ = dynamic_cast<wui::ColumnNode*>(scroll->children().front().get());
        if (pageColumn_ == nullptr) return;
        desktopPageInsets_ = pageColumn_->padding();
    }
    if (!desktopPageInsets_) return;
    const wui::InsetsF desired = compact_
        ? wui::InsetsF{16.0f, 16.0f, 16.0f, 32.0f}
        : *desktopPageInsets_;
    const auto current = pageColumn_->padding();
    if (current.left != desired.left || current.top != desired.top ||
        current.right != desired.right || current.bottom != desired.bottom) {
        pageColumn_->setPadding(desired);
    }
}

} // namespace whatsui::gallery::view::components
