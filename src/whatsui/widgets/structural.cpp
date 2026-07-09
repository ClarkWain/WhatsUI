#include "wui/structural.h"

namespace wui {

void IfNode::setFactory(Factory factory)
{
    factory_ = std::move(factory);
    reconcile();
}

void IfNode::setVisible(bool visible)
{
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;
    reconcile();
}

void IfNode::reconcile()
{
    if (visible_) {
        if (children().empty() && factory_) {
            if (auto child = factory_()) {
                appendChild(std::move(child));
            }
        }
    } else {
        while (!children().empty()) {
            removeChild(children().size() - 1);
        }
    }
}

SizeF IfNode::measure(const Constraints& constraints) const
{
    const auto& childNodes = children();
    if (childNodes.empty()) {
        return constraints.clamp({0.0f, 0.0f});
    }
    return constraints.clamp(childNodes.front()->measure(constraints));
}

void IfNode::layout(const RectF& bounds)
{
    Node::layout(bounds);
    const auto& childNodes = children();
    if (!childNodes.empty()) {
        childNodes.front()->layout(bounds);
    }
}

} // namespace wui
