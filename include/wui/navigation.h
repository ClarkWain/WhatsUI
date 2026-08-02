#pragma once

// Fluent 2 navigation and command surfaces.  The controls here deliberately
// expose real child controls (rather than drawing a monolithic strip) so
// pointer routing, focus traversal and platform accessibility retain one
// addressable item per command or destination.

#include <functional>
#include <string>
#include <vector>

#include "wui/node.h"

namespace wui {

enum class ToolbarItemAppearance { Subtle, Primary };
enum class ToolbarOrientation { Horizontal, Vertical };

class ToolbarNode;

class ToolbarItemNode : public ControlNode {
public:
    using InvokeHandler = std::function<void()>;

    explicit ToolbarItemNode(std::string label = {});
    [[nodiscard]] const std::string& label() const noexcept;
    ToolbarItemNode& label(std::string value);
    void setLabel(std::string value);
    [[nodiscard]] ToolbarItemAppearance appearance() const noexcept;
    ToolbarItemNode& appearance(ToolbarItemAppearance value) noexcept;
    void setAppearance(ToolbarItemAppearance value) noexcept;
    ToolbarItemNode& onInvoke(InvokeHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind,
                                                          std::string_view value) override;

private:
    void invoke();
    std::string label_;
    ToolbarItemAppearance appearance_{ToolbarItemAppearance::Subtle};
    InvokeHandler onInvoke_;
};

class ToolbarNode : public ContainerNode {
public:
    using OverflowHandler = std::function<void(const std::vector<std::string>&)>;
    ToolbarItemNode& addItem(std::string label, ToolbarItemAppearance appearance = ToolbarItemAppearance::Subtle);
    ToolbarNode& orientation(ToolbarOrientation value) noexcept;
    void setOrientation(ToolbarOrientation value) noexcept;
    [[nodiscard]] ToolbarOrientation orientation() const noexcept;
    // Trailing items that cannot fit remain owned by the toolbar but are not
    // painted/hit-testable. `overflowedItems()` and onOverflow() let a future
    // MenuNode/PopoverNode surface them without recomputing layout policy.
    [[nodiscard]] const std::vector<std::string>& overflowedItems() const noexcept;
    ToolbarNode& onOverflow(OverflowHandler handler);
    ToolbarNode& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    [[nodiscard]] std::size_t focusedIndex() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

protected:
    void validateChildInsertion(
        const Node& child,
        std::size_t index,
        std::size_t resultingCount) const override;

private:
    bool moveFocus(int delta);
    [[nodiscard]] RectF overflowBounds() const noexcept;
    std::string accessibleLabel_{"ToolbarNode"};
    ToolbarOrientation orientation_{ToolbarOrientation::Horizontal};
    std::size_t focusedIndex_{0};
    std::vector<std::string> overflowedItems_;
    OverflowHandler onOverflow_;
    RectF overflowBounds_{};
    bool overflowHovered_{false};
    bool overflowPressed_{false};
    bool overflowFocused_{false};
};

class TabListNode;

class TabNode : public ControlNode {
public:
    explicit TabNode(std::string value = {}, std::string label = {});
    [[nodiscard]] const std::string& value() const noexcept;
    TabNode& value(std::string value);
    void setValue(std::string value);
    [[nodiscard]] const std::string& label() const noexcept;
    TabNode& label(std::string value);
    void setLabel(std::string value);
    [[nodiscard]] bool isSelected() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind,
                                                          std::string_view value) override;

private:
    friend class TabListNode;
    void setSelectedFromList(bool selected) noexcept;
    void select();
    std::string value_;
    std::string label_;
    bool selected_{false};
};

class TabListNode : public ControlNode {
public:
    using ChangeHandler = std::function<void(const std::string&)>;

    enum class ActivationMode { Automatic, Manual };

    TabNode& addTab(std::string value, std::string label, bool enabled = true);
    [[nodiscard]] const std::string& value() const noexcept;
    TabListNode& value(std::string value);
    void setValue(std::string value);
    TabListNode& onChange(ChangeHandler handler);
    [[nodiscard]] ActivationMode activationMode() const noexcept;
    TabListNode& activationMode(ActivationMode value) noexcept;
    void setActivationMode(ActivationMode value) noexcept;
    [[nodiscard]] std::size_t focusedIndex() const noexcept;
    TabListNode& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    bool onKeyEvent(const KeyEvent& event) override;

protected:
    void validateChildInsertion(
        const Node& child,
        std::size_t index,
        std::size_t resultingCount) const override;

private:
    friend class TabNode;
    void selectTab(TabNode& tab, bool notify = true);
    bool moveSelection(int delta);
    bool moveFocus(int delta);
    bool selectFocused();
    std::string value_;
    std::string accessibleLabel_{"Tabs"};
    ChangeHandler onChange_;
    ActivationMode activationMode_{ActivationMode::Automatic};
    std::size_t focusedIndex_{0};
};

// TabPanelNode is intentionally a normal container. Application code selects
// which panel is visible (or binds visibility structurally) while this class
// gives that panel a stable tab value and a labelled accessibility boundary.
class TabPanelNode : public ContainerNode {
public:
    explicit TabPanelNode(std::string value = {});
    [[nodiscard]] const std::string& value() const noexcept;
    TabPanelNode& value(std::string value);
    void setValue(std::string value);
    TabPanelNode& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    // Links this panel's value to a TabListNode. Only the matching active panel
    // participates in measure/layout/paint/hit-testing; applications can
    // therefore keep all panels retained without stale hidden content.
    TabPanelNode& tabList(TabListNode& value) noexcept;
    void setTabList(TabListNode* value) noexcept;
    [[nodiscard]] const TabListNode* tabList() const noexcept;
    [[nodiscard]] bool isActive() const noexcept;
    TabPanelNode& active(bool value) noexcept;
    void setActive(bool value) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;

private:
    std::string value_;
    std::string accessibleLabel_;
    TabListNode* tabList_{nullptr};
    bool active_{true};
};

class LinkNode : public ControlNode {
public:
    using InvokeHandler = std::function<void()>;
    explicit LinkNode(std::string label = {});
    [[nodiscard]] const std::string& label() const noexcept;
    LinkNode& label(std::string value);
    void setLabel(std::string value);
    [[nodiscard]] const std::string& href() const noexcept;
    LinkNode& href(std::string value);
    void setHref(std::string value);
    LinkNode& onInvoke(InvokeHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind,
                                                          std::string_view value) override;

private:
    void invoke();
    std::string label_;
    std::string href_;
    InvokeHandler onInvoke_;
};

class BreadcrumbNode;

class BreadcrumbItemNode : public ControlNode {
public:
    using InvokeHandler = std::function<void()>;
    explicit BreadcrumbItemNode(std::string label = {}, bool current = false);
    [[nodiscard]] const std::string& label() const noexcept;
    BreadcrumbItemNode& label(std::string value);
    void setLabel(std::string value);
    [[nodiscard]] bool isCurrent() const noexcept;
    BreadcrumbItemNode& current(bool value = true) noexcept;
    void setCurrent(bool value) noexcept;
    BreadcrumbItemNode& onInvoke(InvokeHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind,
                                                          std::string_view value) override;

private:
    void invoke();
    std::string label_;
    bool current_{false};
    InvokeHandler onInvoke_;
};

// BreadcrumbNode uses a deterministic responsive collapse policy: retain first
// and final items, then elide middle destinations as width requires. A future
// menu/PopoverNode can use hiddenItems() to surface the collapsed destinations.
class BreadcrumbNode : public ContainerNode {
public:
    BreadcrumbItemNode& addItem(std::string label, bool current = false);
    BreadcrumbNode& maxVisible(std::size_t value) noexcept;
    void setMaxVisible(std::size_t value) noexcept;
    [[nodiscard]] std::size_t maxVisible() const noexcept;
    BreadcrumbNode& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    [[nodiscard]] std::vector<std::string> hiddenItems() const;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;

protected:
    void validateChildInsertion(
        const Node& child,
        std::size_t index,
        std::size_t resultingCount) const override;

private:
    [[nodiscard]] std::vector<std::size_t> visibleIndices() const;
    std::size_t maxVisible_{4};
    std::string accessibleLabel_{"BreadcrumbNode"};
};

} // namespace wui
