#pragma once

// Fluent 2 Tree.  TreeItem owns its nested item objects so application keys
// remain stable when a branch is expanded, collapsed, or reordered.

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "wui/node.h"

namespace wui {

class Tree;

class TreeItem : public ControlNode {
public:
    explicit TreeItem(std::string id = {}, std::string label = {});

    [[nodiscard]] const std::string& id() const noexcept;
    TreeItem& id(std::string value);
    void setId(std::string value);
    [[nodiscard]] const std::string& label() const noexcept;
    TreeItem& label(std::string value);
    void setLabel(std::string value);

    TreeItem& addItem(std::string id, std::string label);
    [[nodiscard]] bool hasChildren() const noexcept;
    [[nodiscard]] bool isExpanded() const noexcept;
    TreeItem& expanded(bool value = true);
    void setExpanded(bool value);
    [[nodiscard]] bool isSelected() const noexcept;
    // One-based structural level for screen-reader semantics and diagnostics.
    // It is derived from the retained parent chain, so it remains stable while
    // ancestors are collapsed or rows are windowed out of the viewport.
    [[nodiscard]] std::size_t level() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind,
                                                          std::string_view value) override;

private:
    friend class Tree;
    void setSelectedFromOwner(bool value) noexcept;
    [[nodiscard]] Tree* ownerTree() const noexcept;
    [[nodiscard]] std::size_t depth() const noexcept;
    [[nodiscard]] RectF disclosureBounds() const noexcept;

    std::string id_;
    std::string label_;
    bool expanded_{true};
    bool selected_{false};
};

class Tree : public ContainerNode {
public:
    using SelectionHandler = std::function<void(TreeItem&)>;
    using ExpandHandler = std::function<void(TreeItem&, bool)>;

    TreeItem& addItem(std::string id, std::string label);
    Tree& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    Tree& rowHeight(float value) noexcept;
    void setRowHeight(float value) noexcept;
    [[nodiscard]] float rowHeight() const noexcept;
    Tree& maxVisibleItems(std::size_t value) noexcept;
    void setMaxVisibleItems(std::size_t value) noexcept;
    [[nodiscard]] std::size_t maxVisibleItems() const noexcept;
    [[nodiscard]] float scrollOffset() const noexcept;
    void setScrollOffset(float value) noexcept;
    [[nodiscard]] float maximumScrollOffset() const noexcept;

    [[nodiscard]] TreeItem* selectedItem() const noexcept;
    [[nodiscard]] const std::string& selectedId() const noexcept;
    bool select(std::string_view id);
    Tree& onSelectionChanged(SelectionHandler handler);
    Tree& onExpandedChange(ExpandHandler handler);

    [[nodiscard]] std::vector<TreeItem*> visibleItems() const;
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    friend class TreeItem;
    void appendVisible(TreeItem& item, std::vector<TreeItem*>& items) const;
    [[nodiscard]] TreeItem* findItem(std::string_view id) const noexcept;
    [[nodiscard]] TreeItem* nextEnabled(TreeItem* from, int delta) const noexcept;
    void focus(TreeItem* item) noexcept;
    bool setExpanded(TreeItem& item, bool value);
    bool selectItem(TreeItem& item, bool requestFocus = true);
    void scrollIntoView(TreeItem& item) noexcept;

    std::string accessibleLabel_{"Tree"};
    std::string selectedId_;
    float rowHeight_{32.0f};
    std::size_t maxVisibleItems_{10};
    float scrollOffset_{0.0f};
    TreeItem* focused_{nullptr};
    SelectionHandler onSelectionChanged_;
    ExpandHandler onExpandedChange_;
};

} // namespace wui
