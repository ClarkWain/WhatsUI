#include "wui/widgets.h"
#include "wui/theme.h"

#include <algorithm>
#include <utility>

namespace wui {

Dialog& Dialog::content(std::unique_ptr<Node> content)
{
    clearChildren();
    appendChild(std::move(content));
    return *this;
}

void Dialog::setMaxWidth(float width) noexcept
{
    maxWidth_ = std::max(0.0f, width);
    markDirty(DirtyFlag::Layout);
}

float Dialog::maxWidth() const noexcept { return maxWidth_; }

void Dialog::setBackdropDismissEnabled(bool enabled) noexcept { backdropDismissEnabled_ = enabled; }
bool Dialog::backdropDismissEnabled() const noexcept { return backdropDismissEnabled_; }
void Dialog::onDismiss(DismissHandler handler) { onDismiss_ = std::move(handler); }
void Dialog::setWindowDismissHandler(DismissHandler handler) { windowDismiss_ = std::move(handler); }

SizeF Dialog::measure(const Constraints& constraints) const
{
    return constraints.clamp({constraints.maxWidth, constraints.maxHeight});
}

void Dialog::layout(const RectF& bounds)
{
    Node::layout(bounds);
    if (!children().empty()) {
        const float horizontalMargin = std::min(24.0f, bounds.width * 0.5f);
        const float verticalMargin = std::min(24.0f, bounds.height * 0.5f);
        const float availableWidth = std::max(0.0f, bounds.width - horizontalMargin * 2.0f);
        const float availableHeight = std::max(0.0f, bounds.height - verticalMargin * 2.0f);
        const float width = std::min(availableWidth, maxWidth_);
        auto size = children().front()->measureWithConstraints({0.0f, width, 0.0f, availableHeight});
        size.width = std::min(size.width, width);
        size.height = std::min(size.height, availableHeight);
        children().front()->layout({bounds.x + (bounds.width - size.width) * 0.5f,
                                    bounds.y + (bounds.height - size.height) * 0.5f,
                                    size.width, size.height});
    }
    clearLayoutDirtyRecursively();
}

void Dialog::paint(PaintContext& context)
{
    const auto& current = theme();
    context.fillRect(bounds(), current.colors.scrim);
    if (!children().empty()) {
        const auto& panel = children().front()->bounds();
        context.fillRoundRect(panel, current.radius.lg, current.colors.border);
        context.fillRoundRect({panel.x + 1.0f, panel.y + 1.0f, std::max(0.0f, panel.width - 2.0f), std::max(0.0f, panel.height - 2.0f)},
                              std::max(0.0f, current.radius.lg - 1.0f), current.colors.surface);
    }
    ContainerNode::paint(context);
    clearDirty(DirtyFlag::Paint);
}

Node* Dialog::hitTest(PointF point)
{
    if (!bounds().contains(point)) return nullptr;
    if (!children().empty()) {
        if (auto* child = children().front()->hitTest(point)) return child;
    }
    return this;
}

bool Dialog::onPointerEvent(const PointerEvent& event)
{
    if (event.action == PointerAction::Up && event.button == MouseButton::Left &&
        backdropDismissEnabled_ && !children().empty() && !children().front()->bounds().contains(event.position)) {
        dismiss();
    }
    // The backdrop always consumes input, including when it is not dismissible.
    return true;
}

void Dialog::dismiss()
{
    // Either callback may synchronously mutate the overlay stack and destroy
    // this Dialog. Copy both first; do not touch members after invoking one.
    auto authorHandler = onDismiss_;
    auto windowHandler = windowDismiss_;
    if (authorHandler) authorHandler();
    if (windowHandler) windowHandler();
}

} // namespace wui
