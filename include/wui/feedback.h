#pragma once

// Fluent transient feedback controls. Toast is intentionally a leaf overlay:
// Toaster owns its lifetime/queue through OverlayHost, while Toast owns the
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

class Toast : public ControlNode {
public:
    using Handler = std::function<void()>;

    explicit Toast(std::string title = {}, std::string body = {});
    ~Toast() override;

    Toast& title(std::string value); void setTitle(std::string value);
    [[nodiscard]] const std::string& title() const noexcept;
    Toast& body(std::string value); void setBody(std::string value);
    [[nodiscard]] const std::string& body() const noexcept;
    Toast& intent(ToastIntent value) noexcept; void setIntent(ToastIntent value) noexcept;
    [[nodiscard]] ToastIntent intent() const noexcept;
    Toast& position(ToastPosition value) noexcept; void setPosition(ToastPosition value) noexcept;
    [[nodiscard]] ToastPosition position() const noexcept;
    Toast& action(std::string label, Handler handler); void setAction(std::string label, Handler handler);
    [[nodiscard]] const std::string& actionLabel() const noexcept;
    Toast& onDismiss(Handler handler);
    Toast& timeout(std::chrono::milliseconds value) noexcept; void setTimeout(std::chrono::milliseconds value) noexcept;
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

    void show(std::unique_ptr<Toast> toast);
    void dismiss();
    void clear();
    [[nodiscard]] bool hasActiveToast() const noexcept;
    [[nodiscard]] std::size_t queuedCount() const noexcept;
    [[nodiscard]] Toast* activeToast() const noexcept;
    void setPosition(ToastPosition value) noexcept;
    [[nodiscard]] ToastPosition position() const noexcept;

private:
    void showNext();
    void dismissActiveSafely();

    OverlayHost* host_{nullptr};
    ToastPosition position_{ToastPosition::BottomEnd};
    std::vector<std::unique_ptr<Toast>> queue_;
    std::optional<std::size_t> activeId_;
    Toast* active_{nullptr};
    std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
};

enum class SpinnerSize { ExtraTiny, Tiny, ExtraSmall, Small, Medium, Large, ExtraLarge, Huge };
enum class SpinnerLabelPosition { After, Before, Above, Below };

class Spinner : public Node {
public:
    explicit Spinner(std::string label = {});
    ~Spinner() override;
    Spinner& label(std::string value); void setLabel(std::string value);
    [[nodiscard]] const std::string& label() const noexcept;
    Spinner& size(SpinnerSize value) noexcept; void setSize(SpinnerSize value) noexcept;
    [[nodiscard]] SpinnerSize size() const noexcept;
    Spinner& labelPosition(SpinnerLabelPosition value) noexcept; void setLabelPosition(SpinnerLabelPosition value) noexcept;
    [[nodiscard]] SpinnerLabelPosition labelPosition() const noexcept;
    Spinner& motionEnabled(bool value) noexcept; void setMotionEnabled(bool value) noexcept;
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
