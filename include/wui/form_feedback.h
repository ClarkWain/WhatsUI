#pragma once

#include <functional>
#include <string>
#include <vector>
#include "wui/node.h"

namespace wui {
enum class FieldOrientation { Vertical, Horizontal };
enum class FieldValidationState { None, Warning, Error, Success };
class FieldNode : public ContainerNode {
public:
    explicit FieldNode(std::string label = {});
    FieldNode& label(std::string value); void setLabel(std::string value); [[nodiscard]] const std::string& label() const noexcept;
    FieldNode& hint(std::string value); void setHint(std::string value); [[nodiscard]] const std::string& hint() const noexcept;
    FieldNode& validationMessage(std::string value); void setValidationMessage(std::string value); [[nodiscard]] const std::string& validationMessage() const noexcept;
    FieldNode& validationState(FieldValidationState value) noexcept; void setValidationState(FieldValidationState value) noexcept; [[nodiscard]] FieldValidationState validationState() const noexcept;
    FieldNode& required(bool value = true) noexcept; void setRequired(bool value) noexcept; [[nodiscard]] bool isRequired() const noexcept;
    FieldNode& orientation(FieldOrientation value) noexcept; void setOrientation(FieldOrientation value) noexcept; [[nodiscard]] FieldOrientation orientation() const noexcept;
    FieldNode& enabled(bool value) noexcept; void setEnabled(bool value) noexcept; [[nodiscard]] bool isEnabled() const noexcept;
    FieldNode& control(std::unique_ptr<Node> value); void setControl(std::unique_ptr<Node> value); [[nodiscard]] Node* control() const noexcept;
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override; void paint(PaintContext& context) override;
private:
    void syncControlSemantics(); [[nodiscard]] float labelWidth() const noexcept;
    std::string label_, hint_, validationMessage_; FieldValidationState validationState_{FieldValidationState::None};
    FieldOrientation orientation_{FieldOrientation::Vertical}; bool required_{false}; bool enabled_{true};
};

enum class MessageBarIntent { Info, Success, Warning, Error };
struct MessageBarAction { std::string label; std::function<void()> onInvoke; };
class MessageBarNode : public ControlNode {
public:
    using DismissHandler = std::function<void()>;
    explicit MessageBarNode(std::string body = {});
    MessageBarNode& title(std::string value); void setTitle(std::string value); [[nodiscard]] const std::string& title() const noexcept;
    MessageBarNode& body(std::string value); void setBody(std::string value); [[nodiscard]] const std::string& body() const noexcept;
    MessageBarNode& intent(MessageBarIntent value) noexcept; void setIntent(MessageBarIntent value) noexcept; [[nodiscard]] MessageBarIntent intent() const noexcept;
    MessageBarNode& multiline(bool value = true) noexcept; void setMultiline(bool value) noexcept; [[nodiscard]] bool isMultiline() const noexcept;
    MessageBarNode& addAction(MessageBarAction action); MessageBarNode& clearActions(); [[nodiscard]] const std::vector<MessageBarAction>& actions() const noexcept;
    MessageBarNode& dismissible(bool value = true) noexcept; void setDismissible(bool value) noexcept; [[nodiscard]] bool isDismissible() const noexcept;
    MessageBarNode& onDismiss(DismissHandler handler);
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override; void layout(const RectF& bounds) override; void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override; bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind, std::string_view value) override;
private:
    [[nodiscard]] RectF dismissBounds() const noexcept; [[nodiscard]] RectF actionBounds(std::size_t index) const noexcept; void dismiss();
    std::string title_, body_; MessageBarIntent intent_{MessageBarIntent::Info}; bool multiline_{false}; bool dismissible_{false};
    std::vector<MessageBarAction> actions_; DismissHandler onDismiss_;
};
} // namespace wui
