#pragma once

// Fluent 2 AccordionNode.  AccordionItemNode deliberately owns both its disclosure
// header and retained body subtree: this preserves child identity while the
// body is collapsed, without leaving hidden content in layout or hit testing.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "wui/node.h"

namespace wui {

enum class AccordionExpandMode { Single, Multiple };

class AccordionNode;

class AccordionItemNode : public ControlNode {
public:
    using ChangeHandler = std::function<void(bool)>;

    explicit AccordionItemNode(std::string header = {}, std::string body = {});

    [[nodiscard]] const std::string& header() const noexcept;
    AccordionItemNode& header(std::string value);
    void setHeader(std::string value);
    [[nodiscard]] const std::string& body() const noexcept;
    AccordionItemNode& body(std::string value);
    void setBody(std::string value);

    [[nodiscard]] bool isExpanded() const noexcept;
    AccordionItemNode& expanded(bool value = true);
    void setExpanded(bool value);
    AccordionItemNode& onExpandedChange(ChangeHandler handler);

    // Replaces the optional retained body subtree. The subtree is laid out,
    // painted and hit-tested only while expanded; its identity is retained
    // across collapse/expand cycles.
    AccordionItemNode& content(std::unique_ptr<Node> value);
    void setContent(std::unique_ptr<Node> value);
    [[nodiscard]] Node* content() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind,
                                                          std::string_view value) override;

private:
    friend class AccordionNode;
    void setExpandedFromOwner(bool value, bool notify);
    [[nodiscard]] RectF headerBounds() const noexcept;
    [[nodiscard]] float textBodyHeight(float availableWidth) const;

    std::string header_;
    std::string body_;
    bool expanded_{false};
    ChangeHandler onExpandedChange_;
};

class AccordionNode : public ContainerNode {
public:
    using ChangeHandler = std::function<void(std::size_t, bool)>;

    AccordionItemNode& addItem(std::string header, std::string body = {});
    AccordionNode& expandMode(AccordionExpandMode value) noexcept;
    void setExpandMode(AccordionExpandMode value) noexcept;
    [[nodiscard]] AccordionExpandMode expandMode() const noexcept;
    AccordionNode& onExpandedChange(ChangeHandler handler);

    // The group label is used by the semantic tree instead of exposing a
    // nameless generic container to screen readers.
    AccordionNode& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    [[nodiscard]] std::size_t focusedIndex() const noexcept;
    [[nodiscard]] std::vector<std::size_t> expandedIndices() const;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    bool onKeyEvent(const KeyEvent& event) override;

protected:
    void validateChildInsertion(
        const Node& child,
        std::size_t index,
        std::size_t resultingCount) const override;

private:
    friend class AccordionItemNode;
    bool setItemExpanded(AccordionItemNode& item, bool value, bool notify = true);
    bool toggleItem(AccordionItemNode& item);
    bool moveFocus(int delta);
    bool focusBoundary(bool final);
    [[nodiscard]] std::size_t indexOf(const AccordionItemNode& item) const noexcept;
    void focusItem(std::size_t index);

    AccordionExpandMode expandMode_{AccordionExpandMode::Single};
    std::string accessibleLabel_{"AccordionNode"};
    ChangeHandler onExpandedChange_;
    std::size_t focusedIndex_{0};
};

} // namespace wui
