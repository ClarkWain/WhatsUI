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

class Toolbar;

class ToolbarItem : public ControlNode {
public:
    using InvokeHandler = std::function<void()>;

    explicit ToolbarItem(std::string label = {});
    [[nodiscard]] const std::string& label() const noexcept;
    ToolbarItem& label(std::string value);
    void setLabel(std::string value);
    [[nodiscard]] ToolbarItemAppearance appearance() const noexcept;
    ToolbarItem& appearance(ToolbarItemAppearance value) noexcept;
    void setAppearance(ToolbarItemAppearance value) noexcept;
    ToolbarItem& onInvoke(InvokeHandler handler);

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

class Toolbar : public ContainerNode {
public:
    using OverflowHandler = std::function<void(const std::vector<std::string>&)>;
    ToolbarItem& addItem(std::string label, ToolbarItemAppearance appearance = ToolbarItemAppearance::Subtle);
    Toolbar& orientation(ToolbarOrientation value) noexcept;
    void setOrientation(ToolbarOrientation value) noexcept;
    [[nodiscard]] ToolbarOrientation orientation() const noexcept;
    // Trailing items that cannot fit remain owned by the toolbar but are not
    // painted/hit-testable. `overflowedItems()` and onOverflow() let a future
    // Menu/Popover surface them without recomputing layout policy.
    [[nodiscard]] const std::vector<std::string>& overflowedItems() const noexcept;
    Toolbar& onOverflow(OverflowHandler handler);
    Toolbar& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    [[nodiscard]] std::size_t focusedIndex() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    bool moveFocus(int delta);
    [[nodiscard]] RectF overflowBounds() const noexcept;
    std::string accessibleLabel_{"Toolbar"};
    ToolbarOrientation orientation_{ToolbarOrientation::Horizontal};
    std::size_t focusedIndex_{0};
    std::vector<std::string> overflowedItems_;
    OverflowHandler onOverflow_;
    RectF overflowBounds_{};
};

class TabList;

class Tab : public ControlNode {
public:
    explicit Tab(std::string value = {}, std::string label = {});
    [[nodiscard]] const std::string& value() const noexcept;
    Tab& value(std::string value);
    void setValue(std::string value);
    [[nodiscard]] const std::string& label() const noexcept;
    Tab& label(std::string value);
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
    friend class TabList;
    void setSelectedFromList(bool selected) noexcept;
    void select();
    std::string value_;
    std::string label_;
    bool selected_{false};
};

class TabList : public ControlNode {
public:
    using ChangeHandler = std::function<void(const std::string&)>;

    enum class ActivationMode { Automatic, Manual };

    Tab& addTab(std::string value, std::string label, bool enabled = true);
    [[nodiscard]] const std::string& value() const noexcept;
    TabList& value(std::string value);
    void setValue(std::string value);
    TabList& onChange(ChangeHandler handler);
    [[nodiscard]] ActivationMode activationMode() const noexcept;
    TabList& activationMode(ActivationMode value) noexcept;
    void setActivationMode(ActivationMode value) noexcept;
    [[nodiscard]] std::size_t focusedIndex() const noexcept;
    TabList& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    friend class Tab;
    void selectTab(Tab& tab, bool notify = true);
    bool moveSelection(int delta);
    bool moveFocus(int delta);
    bool selectFocused();
    std::string value_;
    std::string accessibleLabel_{"Tabs"};
    ChangeHandler onChange_;
    ActivationMode activationMode_{ActivationMode::Automatic};
    std::size_t focusedIndex_{0};
};

// TabPanel is intentionally a normal container. Application code selects
// which panel is visible (or binds visibility structurally) while this class
// gives that panel a stable tab value and a labelled accessibility boundary.
class TabPanel : public ContainerNode {
public:
    explicit TabPanel(std::string value = {});
    [[nodiscard]] const std::string& value() const noexcept;
    TabPanel& value(std::string value);
    void setValue(std::string value);
    TabPanel& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    // Links this panel's value to a TabList. Only the matching active panel
    // participates in measure/layout/paint/hit-testing; applications can
    // therefore keep all panels retained without stale hidden content.
    TabPanel& tabList(TabList& value) noexcept;
    void setTabList(TabList* value) noexcept;
    [[nodiscard]] const TabList* tabList() const noexcept;
    [[nodiscard]] bool isActive() const noexcept;
    TabPanel& active(bool value) noexcept;
    void setActive(bool value) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;

private:
    std::string value_;
    std::string accessibleLabel_;
    TabList* tabList_{nullptr};
    bool active_{true};
};

class Link : public ControlNode {
public:
    using InvokeHandler = std::function<void()>;
    explicit Link(std::string label = {});
    [[nodiscard]] const std::string& label() const noexcept;
    Link& label(std::string value);
    void setLabel(std::string value);
    [[nodiscard]] const std::string& href() const noexcept;
    Link& href(std::string value);
    void setHref(std::string value);
    Link& onInvoke(InvokeHandler handler);

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

class Breadcrumb;

class BreadcrumbItem : public ControlNode {
public:
    using InvokeHandler = std::function<void()>;
    explicit BreadcrumbItem(std::string label = {}, bool current = false);
    [[nodiscard]] const std::string& label() const noexcept;
    BreadcrumbItem& label(std::string value);
    void setLabel(std::string value);
    [[nodiscard]] bool isCurrent() const noexcept;
    BreadcrumbItem& current(bool value = true) noexcept;
    void setCurrent(bool value) noexcept;
    BreadcrumbItem& onInvoke(InvokeHandler handler);

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

// Breadcrumb uses a deterministic responsive collapse policy: retain first
// and final items, then elide middle destinations as width requires. A future
// menu/Popover can use hiddenItems() to surface the collapsed destinations.
class Breadcrumb : public ContainerNode {
public:
    BreadcrumbItem& addItem(std::string label, bool current = false);
    Breadcrumb& maxVisible(std::size_t value) noexcept;
    void setMaxVisible(std::size_t value) noexcept;
    [[nodiscard]] std::size_t maxVisible() const noexcept;
    Breadcrumb& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    [[nodiscard]] std::vector<std::string> hiddenItems() const;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;

private:
    [[nodiscard]] std::vector<std::size_t> visibleIndices() const;
    std::size_t maxVisible_{4};
    std::string accessibleLabel_{"Breadcrumb"};
};

} // namespace wui
