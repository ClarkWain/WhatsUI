#pragma once

// Fluent transient feedback controls. ToastNode is intentionally a leaf overlay:
// Toaster owns its lifetime/queue through OverlayHost, while ToastNode owns the
// visual surface, keyboard/pointer dismissal and pausable timeout contract.

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "wui/node.h"

namespace wui {

class OverlayHost;

enum class ToastIntent { Info, Success, Warning, Error };
enum class ToastPosition { TopStart, TopEnd, BottomStart, BottomEnd };

class ToastNode : public ControlNode {
public:
    using Handler = std::function<void()>;

    explicit ToastNode(std::string title = {}, std::string body = {});
    ~ToastNode() override;

    ToastNode& title(std::string value); void setTitle(std::string value);
    [[nodiscard]] const std::string& title() const noexcept;
    ToastNode& body(std::string value); void setBody(std::string value);
    [[nodiscard]] const std::string& body() const noexcept;
    ToastNode& intent(ToastIntent value) noexcept; void setIntent(ToastIntent value) noexcept;
    [[nodiscard]] ToastIntent intent() const noexcept;
    ToastNode& position(ToastPosition value) noexcept; void setPosition(ToastPosition value) noexcept;
    [[nodiscard]] ToastPosition position() const noexcept;
    ToastNode& action(std::string label, Handler handler); void setAction(std::string label, Handler handler);
    [[nodiscard]] const std::string& actionLabel() const noexcept;
    ToastNode& onDismiss(Handler handler);
    ToastNode& timeout(std::chrono::milliseconds value) noexcept; void setTimeout(std::chrono::milliseconds value) noexcept;
    [[nodiscard]] std::chrono::milliseconds timeout() const noexcept;
    [[nodiscard]] bool isPaused() const noexcept;
    void setPaused(bool value) noexcept;
    // Deterministic hook for host tests and non-frame-driven embedders.
    void advanceTimeout(std::chrono::milliseconds elapsed);
    void dismiss();

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& hostBounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind, std::string_view value) override;

protected:
    void onAttach() noexcept override;
    void onDetach() noexcept override;

private:
    friend class Toaster;
    void setHostDismiss(Handler handler);
    void startTimeoutTicker() noexcept;
    void stopTimeoutTicker() noexcept;
    [[nodiscard]] RectF actionBounds() const noexcept;
    [[nodiscard]] RectF dismissBounds() const noexcept;

    std::string title_;
    std::string body_;
    std::string actionLabel_;
    ToastIntent intent_{ToastIntent::Info};
    ToastPosition position_{ToastPosition::BottomEnd};
    Handler onAction_;
    Handler onDismiss_;
    Handler hostDismiss_;
    std::chrono::milliseconds timeout_{5000};
    std::chrono::milliseconds elapsed_{0};
    bool paused_{false};
    bool dismissed_{false};
    std::optional<std::size_t> tickerId_;
    std::chrono::steady_clock::time_point lastTick_{};
};

// A single visible toast follows Fluent's non-modal notification behavior.
// Further notifications stay FIFO queued until their predecessor dismisses.
class Toaster {
public:
    explicit Toaster(OverlayHost& host, ToastPosition position = ToastPosition::BottomEnd) noexcept;
    ~Toaster();
    Toaster(const Toaster&) = delete;
    Toaster& operator=(const Toaster&) = delete;

    void show(std::unique_ptr<ToastNode> toast);
    void dismiss();
    void clear();
    [[nodiscard]] bool hasActiveToast() const noexcept;
    [[nodiscard]] std::size_t queuedCount() const noexcept;
    [[nodiscard]] ToastNode* activeToast() const noexcept;
    void setPosition(ToastPosition value) noexcept;
    [[nodiscard]] ToastPosition position() const noexcept;

private:
    void showNext();
    void dismissActiveSafely();

    OverlayHost* host_{nullptr};
    ToastPosition position_{ToastPosition::BottomEnd};
    std::vector<std::unique_ptr<ToastNode>> queue_;
    std::optional<std::size_t> activeId_;
    ToastNode* active_{nullptr};
    std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
};

enum class SpinnerSize { ExtraTiny, Tiny, ExtraSmall, Small, Medium, Large, ExtraLarge, Huge };
enum class SpinnerLabelPosition { After, Before, Above, Below };

class SpinnerNode : public Node {
public:
    explicit SpinnerNode(std::string label = {});
    ~SpinnerNode() override;
    SpinnerNode& label(std::string value); void setLabel(std::string value);
    [[nodiscard]] const std::string& label() const noexcept;
    SpinnerNode& size(SpinnerSize value) noexcept; void setSize(SpinnerSize value) noexcept;
    [[nodiscard]] SpinnerSize size() const noexcept;
    SpinnerNode& labelPosition(SpinnerLabelPosition value) noexcept; void setLabelPosition(SpinnerLabelPosition value) noexcept;
    [[nodiscard]] SpinnerLabelPosition labelPosition() const noexcept;
    SpinnerNode& motionEnabled(bool value) noexcept; void setMotionEnabled(bool value) noexcept;
    [[nodiscard]] bool isMotionEnabled() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;

protected:
    void onAttach() noexcept override;
    void onDetach() noexcept override;

private:
    void startTicker() noexcept;
    void stopTicker() noexcept;
    [[nodiscard]] float indicatorSize() const noexcept;
    std::string label_;
    SpinnerSize size_{SpinnerSize::Medium};
    SpinnerLabelPosition labelPosition_{SpinnerLabelPosition::After};
    bool motionEnabled_{true};
    float phase_{0.0f};
    std::optional<std::size_t> tickerId_;
};

} // namespace wui
