#include "wui/widgets.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace wui {

BoxNode& BoxNode::child(std::unique_ptr<Node> child)
{
    appendChild(std::move(child));
    return *this;
}

void BoxNode::setBackground(Color color) noexcept
{
    background_ = color;
    markDirty(DirtyFlag::Paint);
}

void BoxNode::setRadius(float radius) noexcept
{
    radius_ = radius;
    markDirty(DirtyFlag::Paint);
}

void BoxNode::setPadding(InsetsF padding) noexcept
{
    padding_ = padding;
    markDirty(DirtyFlag::Layout);
}

void BoxNode::setContentAlignment(Alignment horizontal, Alignment vertical) noexcept
{
    horizontalAlignment_ = horizontal;
    verticalAlignment_ = vertical;
    markDirty(DirtyFlag::Layout);
}

void BoxNode::setWidth(float width) noexcept
{
    width_ = std::max(0.0f, width);
    markDirty(DirtyFlag::Layout);
}

void BoxNode::clearWidth() noexcept
{
    width_.reset();
    markDirty(DirtyFlag::Layout);
}

void BoxNode::setHeight(float height) noexcept
{
    height_ = std::max(0.0f, height);
    markDirty(DirtyFlag::Layout);
}

void BoxNode::clearHeight() noexcept
{
    height_.reset();
    markDirty(DirtyFlag::Layout);
}

SizeF BoxNode::measure(const Constraints& constraints) const
{
    const Constraints innerConstraints = constraints.deflate(padding_);
    SizeF content{};
    for (const auto& child : children()) {
        const auto childSize = child->measureWithConstraints(innerConstraints);
        content.width = std::max(content.width, childSize.width);
        content.height = std::max(content.height, childSize.height);
    }
    SizeF measured{content.width + padding_.horizontal(), content.height + padding_.vertical()};
    if (width_) {
        measured.width = *width_;
    }
    if (height_) {
        measured.height = *height_;
    }
    return constraints.clamp(measured);
}

void BoxNode::layout(const RectF& bounds)
{
    Node::layout(bounds);
    const RectF contentBounds{bounds.x + padding_.left,
                              bounds.y + padding_.top,
                              std::max(0.0f, bounds.width - padding_.horizontal()),
                              std::max(0.0f, bounds.height - padding_.vertical())};
    for (const auto& child : children()) {
        SizeF childSize = child->measureWithConstraints({0.0f, contentBounds.width, 0.0f, contentBounds.height});
        float childX = contentBounds.x;
        float childY = contentBounds.y;
        switch (horizontalAlignment_) {
        case Alignment::Center: childX += (contentBounds.width - childSize.width) * 0.5f; break;
        case Alignment::End: childX += contentBounds.width - childSize.width; break;
        case Alignment::Stretch: childSize.width = contentBounds.width; break;
        case Alignment::Baseline:
        case Alignment::Start: break;
        }
        switch (verticalAlignment_) {
        case Alignment::Center: childY += (contentBounds.height - childSize.height) * 0.5f; break;
        case Alignment::End: childY += contentBounds.height - childSize.height; break;
        case Alignment::Stretch: childSize.height = contentBounds.height; break;
        case Alignment::Baseline:
        case Alignment::Start: break;
        }
        child->layout({childX, childY, childSize.width, childSize.height});
    }
    clearLayoutDirtyRecursively();
}

void BoxNode::paint(PaintContext& context)
{
    Color fill = background_;
    if (interaction_ != nullptr) {
        const auto states = interaction_->states;
        if ((states & toMask(ControlVisualState::Pressed)) != 0 &&
            interaction_->pressedBackground.has_value()) {
            fill = *interaction_->pressedBackground;
        } else if ((states & toMask(ControlVisualState::Hovered)) != 0 &&
                   interaction_->hoverBackground.has_value()) {
            fill = *interaction_->hoverBackground;
        }
    }
    if (fill.a > 0) {
        context.fillRoundRect(bounds(), radius_, fill);
    }
    ContainerNode::paint(context);
    clearDirty(DirtyFlag::Paint);
}

InteractionArea& BoxNode::ensureInteraction()
{
    if (interaction_ == nullptr) {
        interaction_ = std::make_unique<InteractionArea>();
    }
    return *interaction_;
}

void BoxNode::setOnClick(std::function<void()> handler)
{
    ensureInteraction().onClick = std::move(handler);
}

void BoxNode::setOnPointerDown(std::function<bool(const PointerEvent&)> handler)
{
    ensureInteraction().onPointerDown = std::move(handler);
}

void BoxNode::setOnPointerMove(std::function<bool(const PointerEvent&)> handler)
{
    ensureInteraction().onPointerMove = std::move(handler);
}

void BoxNode::setOnPointerUp(std::function<bool(const PointerEvent&)> handler)
{
    ensureInteraction().onPointerUp = std::move(handler);
}

void BoxNode::setOnHoverChange(std::function<void(bool)> handler)
{
    ensureInteraction().onHoverChange = std::move(handler);
}

void BoxNode::setOnFocusChange(std::function<void(bool)> handler)
{
    ensureInteraction().onFocusChange = std::move(handler);
}

void BoxNode::setOnKey(std::function<bool(const KeyEvent&)> handler)
{
    ensureInteraction().onKey = std::move(handler);
}

void BoxNode::setHoverBackground(Color color) noexcept
{
    ensureInteraction().hoverBackground = color;
    markDirty(DirtyFlag::Paint);
}

void BoxNode::setPressedBackground(Color color) noexcept
{
    ensureInteraction().pressedBackground = color;
    markDirty(DirtyFlag::Paint);
}

void BoxNode::setAccessibleRole(AccessibilityRole role) noexcept
{
    ensureInteraction().accessibleRole = role;
}

void BoxNode::setAccessibleLabel(std::string label)
{
    ensureInteraction().accessibleLabel = std::move(label);
}

namespace {
bool setInteractionState(InteractionArea& area, ControlVisualState flag,
                         bool value) noexcept
{
    const ControlVisualStates mask = toMask(flag);
    const bool current = (area.states & mask) != 0;
    if (current == value) return false;
    if (value) area.states |= mask;
    else area.states &= ~mask;
    return true;
}
} // namespace

EventResult BoxNode::onPointerEvent(const PointerEvent& event,
                                      EventContext& context)
{
    // BoxNode is a router-aware node: without an interaction it defers to
    // the framework contract (Node forwards Target/Bubble to the legacy bool
    // overload). With an interaction, a click callback may synchronously
    // rebuild the tree — so once the callback has been dispatched we must ask
    // the router to stop bubbling before it visits ancestors that have just
    // been freed. Fluent ButtonNode gets away with a plain bool because layout
    // ancestors ignore the same event; a BoxNode-with-InteractionArea is
    // observed by many more nodes and would otherwise hit the latent UAF.
    if (interaction_ == nullptr) {
        return Node::onPointerEvent(event, context);
    }
    if (context.phase() == EventPhase::Capture) return EventResult::Ignored;

    auto& area = *interaction_;
    switch (event.action) {
    case PointerAction::Enter:
        if (setInteractionState(area, ControlVisualState::Hovered, true)) {
            markDirty(DirtyFlag::Paint);
            if (area.onHoverChange) area.onHoverChange(true);
        }
        return EventResult::Handled;
    case PointerAction::Leave:
        if (setInteractionState(area, ControlVisualState::Hovered, false)) {
            markDirty(DirtyFlag::Paint);
            if (area.onHoverChange) area.onHoverChange(false);
        }
        setInteractionState(area, ControlVisualState::Pressed, false);
        return EventResult::Handled;
    case PointerAction::Move:
        if (area.onPointerMove && area.onPointerMove(event)) {
            return EventResult::StopPropagation;
        }
        if (setInteractionState(area, ControlVisualState::Hovered,
                                bounds().contains(event.position))) {
            markDirty(DirtyFlag::Paint);
            if (area.onHoverChange) {
                area.onHoverChange((area.states &
                                    toMask(ControlVisualState::Hovered)) != 0);
            }
        }
        return EventResult::Ignored;
    case PointerAction::Down:
        if (event.button == MouseButton::Left) {
            if (setInteractionState(area, ControlVisualState::Pressed, true)) {
                markDirty(DirtyFlag::Paint);
            }
            if (area.onPointerDown && area.onPointerDown(event)) {
                return EventResult::StopPropagation;
            }
            return EventResult::Handled;
        }
        if (area.onPointerDown && area.onPointerDown(event)) {
            return EventResult::StopPropagation;
        }
        return EventResult::Ignored;
    case PointerAction::Up:
        if (event.button == MouseButton::Left) {
            const bool wasPressed =
                (area.states & toMask(ControlVisualState::Pressed)) != 0;
            setInteractionState(area, ControlVisualState::Pressed, false);
            markDirty(DirtyFlag::Paint);
            const bool insideBounds = bounds().contains(event.position);
            auto onClick = area.onClick;
            auto rawUp = area.onPointerUp;
            if (wasPressed && insideBounds && onClick) {
                // Copy the callback out first: it may rebuild the tree and
                // free `this` and `area`. StopPropagation halts the router
                // before it walks any now-freed ancestor.
                onClick();
                return EventResult::StopPropagation;
            }
            if (rawUp && rawUp(event)) return EventResult::StopPropagation;
            return EventResult::Handled;
        }
        if (area.onPointerUp && area.onPointerUp(event)) {
            return EventResult::StopPropagation;
        }
        return EventResult::Ignored;
    case PointerAction::Cancel:
        setInteractionState(area, ControlVisualState::Pressed, false);
        markDirty(DirtyFlag::Paint);
        return EventResult::Handled;
    case PointerAction::Scroll:
        break;
    }
    return EventResult::Ignored;
}

bool BoxNode::onPointerEvent(const PointerEvent& event)
{
    // Legacy bool overload retained for test hosts and any code that calls
    // BoxNode::onPointerEvent(event) directly. The interactive router uses
    // the EventContext overload above, which is where the propagation control
    // that avoids the tree-rebuild UAF lives. Behavior here matches the
    // EventContext path for anything a bool caller can express.
    if (interaction_ == nullptr) {
        return ContainerNode::onPointerEvent(event);
    }
    auto& area = *interaction_;
    bool handled = false;

    switch (event.action) {
    case PointerAction::Enter:
        if (setInteractionState(area, ControlVisualState::Hovered, true)) {
            markDirty(DirtyFlag::Paint);
            if (area.onHoverChange) area.onHoverChange(true);
        }
        handled = true;
        break;
    case PointerAction::Leave:
        if (setInteractionState(area, ControlVisualState::Hovered, false)) {
            markDirty(DirtyFlag::Paint);
            if (area.onHoverChange) area.onHoverChange(false);
        }
        setInteractionState(area, ControlVisualState::Pressed, false);
        handled = true;
        break;
    case PointerAction::Move:
        if (area.onPointerMove && area.onPointerMove(event)) handled = true;
        if (setInteractionState(area, ControlVisualState::Hovered,
                                bounds().contains(event.position))) {
            markDirty(DirtyFlag::Paint);
            if (area.onHoverChange) {
                area.onHoverChange((area.states &
                                    toMask(ControlVisualState::Hovered)) != 0);
            }
        }
        break;
    case PointerAction::Down:
        if (event.button == MouseButton::Left) {
            if (setInteractionState(area, ControlVisualState::Pressed, true)) {
                markDirty(DirtyFlag::Paint);
            }
            handled = true;
        }
        if (area.onPointerDown && area.onPointerDown(event)) handled = true;
        break;
    case PointerAction::Up:
        if (event.button == MouseButton::Left) {
            const bool wasPressed =
                (area.states & toMask(ControlVisualState::Pressed)) != 0;
            setInteractionState(area, ControlVisualState::Pressed, false);
            markDirty(DirtyFlag::Paint);
            const bool insideBounds = bounds().contains(event.position);
            auto onClick = area.onClick;
            auto rawUp = area.onPointerUp;
            if (wasPressed && insideBounds && onClick) {
                onClick();
                return true;
            }
            if (rawUp && rawUp(event)) return true;
            return true;
        }
        if (area.onPointerUp && area.onPointerUp(event)) handled = true;
        break;
    case PointerAction::Cancel:
        setInteractionState(area, ControlVisualState::Pressed, false);
        markDirty(DirtyFlag::Paint);
        handled = true;
        break;
    case PointerAction::Scroll:
        break;
    }

    if (handled) return true;
    return ContainerNode::onPointerEvent(event);
}

bool BoxNode::onKeyEvent(const KeyEvent& event)
{
    if (interaction_ == nullptr) {
        return Node::onKeyEvent(event);
    }
    auto& area = *interaction_;
    if (area.onKey && area.onKey(event)) return true;
    if (event.action == KeyAction::Down && area.onClick) {
        // Space (32) and Enter (13) activate the interaction area, mirroring
        // the Fluent ButtonNode keyboard contract so a11y stays uniform.
        if (event.keyCode == 13 || event.keyCode == 32) {
            // Copy the handler out before invoking it: an onClick that swaps
            // the route will destroy `this` and free `area`, so we cannot
            // touch either after the callback has run.
            auto onClick = area.onClick;
            onClick();
            return true;
        }
    }
    return Node::onKeyEvent(event);
}

AccessibilityActionCapabilities BoxNode::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    if (interaction_ != nullptr && interaction_->onClick) {
        actions.invoke = true;
    }
    return actions;
}

AccessibilityActionStatus BoxNode::performAccessibilityAction(
    AccessibilityActionKind kind, std::string_view value)
{
    (void)value;
    if (interaction_ == nullptr) return AccessibilityActionStatus::NotSupported;
    if (kind != AccessibilityActionKind::Invoke) {
        return AccessibilityActionStatus::NotSupported;
    }
    if (!interaction_->onClick) return AccessibilityActionStatus::NotSupported;
    // Copy the handler before running it — a11y Invoke can (and, for the nav
    // rail, does) tear down the tree that owns this BoxNode.
    auto onClick = interaction_->onClick;
    onClick();
    return AccessibilityActionStatus::Succeeded;
}

RowNode& RowNode::child(std::unique_ptr<Node> child)
{
    appendChild(std::move(child));
    return *this;
}

RowNode& RowNode::gap(float gap) noexcept
{
    setGap(gap);
    return *this;
}

void RowNode::setGap(float gap) noexcept
{
    gap_ = gap;
    markDirty(DirtyFlag::Layout);
}

float RowNode::gap() const noexcept
{
    return gap_;
}

RowNode& RowNode::padding(InsetsF padding) noexcept
{
    setPadding(padding);
    return *this;
}

void RowNode::setPadding(InsetsF padding) noexcept
{
    padding_ = padding;
    markDirty(DirtyFlag::Layout);
}

InsetsF RowNode::padding() const noexcept
{
    return padding_;
}

RowNode& RowNode::align(Alignment align) noexcept
{
    setAlign(align);
    return *this;
}

void RowNode::setAlign(Alignment align) noexcept
{
    align_ = align;
    markDirty(DirtyFlag::Layout);
}

Alignment RowNode::align() const noexcept
{
    return align_;
}

SizeF RowNode::measure(const Constraints& constraints) const
{
    const Constraints inner = constraints.deflate(padding_);
    float width = 0.0f;
    float height = 0.0f;
    float maxBaseline = 0.0f;
    float maxBelowBaseline = 0.0f;
    bool hasActiveChild = false;

    const auto& childNodes = children();
    for (std::size_t index = 0; index < childNodes.size(); ++index) {
        const float leadingGap = hasActiveChild ? gap_ : 0.0f;
        const float remainingWidth = std::max(0.0f, inner.maxWidth - width - leadingGap);
        const auto childSize = childNodes[index]->measureWithConstraints({0.0f, remainingWidth, 0.0f, inner.maxHeight});
        const bool active = childNodes[index]->flex() > 0.0f || childSize.width > 0.0f || childSize.height > 0.0f;
        if (active) {
            width += leadingGap + childSize.width;
            hasActiveChild = true;
        }
        height = std::max(height, childSize.height);
        if (align_ == Alignment::Baseline) {
            const float baseline = childNodes[index]->baselineOffset();
            if (baseline >= 0.0f) {
                maxBaseline = std::max(maxBaseline, baseline);
                maxBelowBaseline = std::max(maxBelowBaseline, childSize.height - baseline);
            } else {
                maxBelowBaseline = std::max(maxBelowBaseline, childSize.height);
            }
        }
    }

    if (align_ == Alignment::Baseline) {
        height = std::max(height, maxBaseline + maxBelowBaseline);
    }

    return constraints.clamp({width + padding_.horizontal(), height + padding_.vertical()});
}

void RowNode::layout(const RectF& bounds)
{
    Node::layout(bounds);

    const auto& childNodes = children();
    const float innerWidth = std::max(0.0f, bounds.width - padding_.horizontal());
    const float innerHeight = std::max(0.0f, bounds.height - padding_.vertical());
    const Constraints loose{0.0f, innerWidth, 0.0f, innerHeight};

    // Pass 1: measure fixed children; accumulate width and total flex weight.
    std::vector<SizeF> sizes(childNodes.size());
    std::vector<bool> active(childNodes.size(), false);
    float fixedWidth = 0.0f;
    float totalFlex = 0.0f;
    std::size_t activeCount = 0;
    for (std::size_t i = 0; i < childNodes.size(); ++i) {
        if (childNodes[i]->flex() > 0.0f) {
            totalFlex += childNodes[i]->flex();
            active[i] = true;
        } else {
            sizes[i] = childNodes[i]->measureWithConstraints(loose);
            active[i] = sizes[i].width > 0.0f || sizes[i].height > 0.0f;
            if (active[i]) {
                fixedWidth += sizes[i].width;
            }
        }
        if (active[i]) {
            ++activeCount;
        }
    }
    if (activeCount > 1) {
        fixedWidth += gap_ * static_cast<float>(activeCount - 1);
    }
    const float remaining = std::max(0.0f, innerWidth - fixedWidth);

    float rowBaseline = 0.0f;
    if (align_ == Alignment::Baseline) {
        for (std::size_t i = 0; i < childNodes.size(); ++i) {
            if (childNodes[i]->flex() <= 0.0f) {
                rowBaseline = std::max(rowBaseline, childNodes[i]->baselineOffset());
            }
        }
    }

    // Pass 2: size flex children from the remainder, then place with cross align.
    float cursorX = bounds.x + padding_.left;
    bool placedActiveChild = false;
    for (std::size_t i = 0; i < childNodes.size(); ++i) {
        Node* child = childNodes[i].get();
        SizeF childSize = sizes[i];
        if (child->flex() > 0.0f) {
            const float allocated = totalFlex > 0.0f ? remaining * (child->flex() / totalFlex) : 0.0f;
            childSize = child->measureWithConstraints(Constraints{0.0f, allocated, 0.0f, innerHeight});
            childSize.width = allocated;
        }
        if (active[i] && placedActiveChild) {
            cursorX += gap_;
        }
        float childY = bounds.y + padding_.top;
        switch (align_) {
        case Alignment::Baseline: {
            const float baseline = child->baselineOffset();
            if (baseline >= 0.0f) {
                childY += rowBaseline - baseline;
            } else {
                childY += std::max(0.0f, rowBaseline - childSize.height);
            }
            break;
        }
        case Alignment::Center:
            childY += (innerHeight - childSize.height) * 0.5f;
            break;
        case Alignment::End:
            childY += innerHeight - childSize.height;
            break;
        case Alignment::Stretch:
            childSize.height = innerHeight;
            break;
        case Alignment::Start:
        default:
            break;
        }
        child->layout({cursorX, childY, childSize.width, childSize.height});
        if (active[i]) {
            cursorX += childSize.width;
            placedActiveChild = true;
        }
    }
    clearLayoutDirtyRecursively();
}

ColumnNode& ColumnNode::child(std::unique_ptr<Node> child)
{
    appendChild(std::move(child));
    return *this;
}

ColumnNode& ColumnNode::gap(float gap) noexcept
{
    setGap(gap);
    return *this;
}

void ColumnNode::setGap(float gap) noexcept
{
    gap_ = gap;
    markDirty(DirtyFlag::Layout);
}

float ColumnNode::gap() const noexcept
{
    return gap_;
}

ColumnNode& ColumnNode::padding(InsetsF padding) noexcept
{
    setPadding(padding);
    return *this;
}

void ColumnNode::setPadding(InsetsF padding) noexcept
{
    padding_ = padding;
    markDirty(DirtyFlag::Layout);
}

InsetsF ColumnNode::padding() const noexcept
{
    return padding_;
}

ColumnNode& ColumnNode::align(Alignment align) noexcept
{
    setAlign(align);
    return *this;
}

void ColumnNode::setAlign(Alignment align) noexcept
{
    align_ = align;
    markDirty(DirtyFlag::Layout);
}

Alignment ColumnNode::align() const noexcept
{
    return align_;
}

SizeF ColumnNode::measure(const Constraints& constraints) const
{
    const Constraints inner = constraints.deflate(padding_);
    float width = 0.0f;
    float height = 0.0f;
    bool hasActiveChild = false;

    const auto& childNodes = children();
    for (std::size_t index = 0; index < childNodes.size(); ++index) {
        const float leadingGap = hasActiveChild ? gap_ : 0.0f;
        const float remainingHeight = std::max(0.0f, inner.maxHeight - height - leadingGap);
        const auto childSize = childNodes[index]->measureWithConstraints({0.0f, inner.maxWidth, 0.0f, remainingHeight});
        width = std::max(width, childSize.width);
        const bool active = childNodes[index]->flex() > 0.0f || childSize.width > 0.0f || childSize.height > 0.0f;
        if (active) {
            height += leadingGap + childSize.height;
            hasActiveChild = true;
        }
    }

    return constraints.clamp({width + padding_.horizontal(), height + padding_.vertical()});
}

void ColumnNode::layout(const RectF& bounds)
{
    Node::layout(bounds);

    const auto& childNodes = children();
    const float innerWidth = std::max(0.0f, bounds.width - padding_.horizontal());
    const float innerHeight = std::max(0.0f, bounds.height - padding_.vertical());
    const Constraints loose{0.0f, innerWidth, 0.0f, innerHeight};

    // Pass 1: measure fixed children; accumulate height and total flex weight.
    std::vector<SizeF> sizes(childNodes.size());
    std::vector<bool> active(childNodes.size(), false);
    float fixedHeight = 0.0f;
    float totalFlex = 0.0f;
    std::size_t activeCount = 0;
    for (std::size_t i = 0; i < childNodes.size(); ++i) {
        if (childNodes[i]->flex() > 0.0f) {
            totalFlex += childNodes[i]->flex();
            active[i] = true;
        } else {
            sizes[i] = childNodes[i]->measureWithConstraints(loose);
            active[i] = sizes[i].width > 0.0f || sizes[i].height > 0.0f;
            if (active[i]) {
                fixedHeight += sizes[i].height;
            }
        }
        if (active[i]) {
            ++activeCount;
        }
    }
    if (activeCount > 1) {
        fixedHeight += gap_ * static_cast<float>(activeCount - 1);
    }
    const float remaining = std::max(0.0f, innerHeight - fixedHeight);

    // Pass 2: size flex children from the remainder, then place with cross align.
    float cursorY = bounds.y + padding_.top;
    bool placedActiveChild = false;
    for (std::size_t i = 0; i < childNodes.size(); ++i) {
        Node* child = childNodes[i].get();
        SizeF childSize = sizes[i];
        if (child->flex() > 0.0f) {
            const float allocated = totalFlex > 0.0f ? remaining * (child->flex() / totalFlex) : 0.0f;
            childSize = child->measureWithConstraints(Constraints{0.0f, innerWidth, 0.0f, allocated});
            childSize.height = allocated;
        }
        if (active[i] && placedActiveChild) {
            cursorY += gap_;
        }
        float childX = bounds.x + padding_.left;
        switch (align_) {
        case Alignment::Baseline:
            break;
        case Alignment::Center:
            childX += (innerWidth - childSize.width) * 0.5f;
            break;
        case Alignment::End:
            childX += innerWidth - childSize.width;
            break;
        case Alignment::Stretch:
            childSize.width = innerWidth;
            break;
        case Alignment::Start:
        default:
            break;
        }
        child->layout({childX, cursorY, childSize.width, childSize.height});
        if (active[i]) {
            cursorY += childSize.height;
            placedActiveChild = true;
        }
    }
    clearLayoutDirtyRecursively();
}

} // namespace wui
