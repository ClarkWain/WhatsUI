#pragma once

// Fluent 2 TreeNode.  TreeItemNode owns its nested item objects so application keys
// remain stable when a branch is expanded, collapsed, or reordered.

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "wui/node.h"

namespace wui {

class TreeNode;

class TreeItemNode : public ControlNode {
public:
    explicit TreeItemNode(std::string id = {}, std::string label = {});

    [[nodiscard]] const std::string& id() const noexcept;
    TreeItemNode& id(std::string value);
    void setId(std::string value);
    [[nodiscard]] const std::string& label() const noexcept;
    TreeItemNode& label(std::string value);
    void setLabel(std::string value);

    TreeItemNode& addItem(std::string id, std::string label);
    [[nodiscard]] bool hasChildren() const noexcept;
    [[nodiscard]] bool isExpanded() const noexcept;
    TreeItemNode& expanded(bool value = true);
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
    [[nodiscard]] std::unique_ptr<Node> removeChild(std::size_t index);
    void clearChildren();
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind,
                                                          std::string_view value) override;

private:
    friend class TreeNode;
    void setSelectedFromOwner(bool value) noexcept;
    [[nodiscard]] TreeNode* ownerTree() const noexcept;
    [[nodiscard]] std::size_t depth() const noexcept;
    [[nodiscard]] RectF disclosureBounds() const noexcept;

    std::string id_;
    std::string label_;
    bool expanded_{true};
    bool selected_{false};
};

class TreeNode : public ContainerNode {
public:
    struct Range {
        std::size_t first{0};
        std::size_t last{0};

        [[nodiscard]] std::size_t size() const noexcept { return last - first; }
        [[nodiscard]] bool empty() const noexcept { return first == last; }
    };

    using SelectionHandler = std::function<void(TreeItemNode&)>;
    using ExpandHandler = std::function<void(TreeItemNode&, bool)>;

    TreeNode();
    ~TreeNode() override;

    TreeItemNode& addItem(std::string id, std::string label);
    TreeNode& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    TreeNode& rowHeight(float value) noexcept;
    void setRowHeight(float value) noexcept;
    [[nodiscard]] float rowHeight() const noexcept;
    TreeNode& maxVisibleItems(std::size_t value) noexcept;
    void setMaxVisibleItems(std::size_t value) noexcept;
    [[nodiscard]] std::size_t maxVisibleItems() const noexcept;
    [[nodiscard]] float scrollOffset() const noexcept;
    void setScrollOffset(float value) noexcept;
    [[nodiscard]] float maximumScrollOffset() const noexcept;
    [[nodiscard]] Range visibleRange() const noexcept;

    [[nodiscard]] TreeItemNode* selectedItem() const noexcept;
    [[nodiscard]] const std::string& selectedId() const noexcept;
    bool select(std::string_view id);
    TreeNode& onSelectionChanged(SelectionHandler handler);
    TreeNode& onExpandedChange(ExpandHandler handler);
    [[nodiscard]] std::unique_ptr<Node> removeChild(std::size_t index);
    void clearChildren();

    [[nodiscard]] std::vector<TreeItemNode*> visibleItems() const;
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    struct State;

    friend class TreeItemNode;
    void appendVisible(TreeItemNode& item, std::vector<TreeItemNode*>& items) const;
    [[nodiscard]] const std::vector<TreeItemNode*>& visibleItemsCache() const;
    void invalidateVisibleItems() noexcept;
    [[nodiscard]] TreeItemNode* findItem(std::string_view id) const noexcept;
    [[nodiscard]] TreeItemNode* nextEnabled(TreeItemNode* from, int delta) const noexcept;
    void focus(TreeItemNode* item) noexcept;
    bool setExpanded(TreeItemNode& item, bool value);
    bool selectItem(TreeItemNode& item, bool requestFocus = true);
    void scrollIntoView(TreeItemNode& item) noexcept;
    void syncViewport(std::size_t visibleCount) noexcept;

    std::string accessibleLabel_{"TreeNode"};
    std::string selectedId_;
    float rowHeight_{32.0f};
    std::size_t maxVisibleItems_{10};
    std::unique_ptr<State> state_;
    SelectionHandler onSelectionChanged_;
    ExpandHandler onExpandedChange_;
};

} // namespace wui
