#pragma once

#include <memory>
#include <string>
#include <vector>

#include "wui/frame_stats.h"
#include "wui/platform.h"
#include "wui/runtime.h"

namespace wui {

class TextInput;
class Dialog;

class UiWindow {
public:
    explicit UiWindow(std::unique_ptr<PlatformWindow> platformWindow);

    [[nodiscard]] WindowId id() const noexcept;
    [[nodiscard]] PlatformWindow& platformWindow() noexcept;
    [[nodiscard]] const PlatformWindow& platformWindow() const noexcept;

    [[nodiscard]] UiRoot& uiRoot() noexcept;
    [[nodiscard]] const UiRoot& uiRoot() const noexcept;
    [[nodiscard]] FocusManager& focusManager() noexcept;
    [[nodiscard]] const FocusManager& focusManager() const noexcept;
    [[nodiscard]] InputRouter& inputRouter() noexcept;
    [[nodiscard]] const InputRouter& inputRouter() const noexcept;
    [[nodiscard]] Navigator& navigator() noexcept;
    [[nodiscard]] const Navigator& navigator() const noexcept;
    [[nodiscard]] OverlayHost& overlayHost() noexcept;
    [[nodiscard]] const OverlayHost& overlayHost() const noexcept;

    // Modal dialogs are overlays with input isolation. Escape and an enabled
    // backdrop dismissal route through this API so the prior focus is restored.
    [[nodiscard]] OverlayId showDialog(std::unique_ptr<Dialog> dialog);
    [[nodiscard]] std::unique_ptr<Dialog> dismissDialog(OverlayId id);
    [[nodiscard]] std::unique_ptr<Dialog> dismissTopDialog();
    [[nodiscard]] bool hasDialog() const noexcept;

    void setRoot(std::unique_ptr<Node> root);
    [[nodiscard]] Node* root() const noexcept;

    // Window-level frame pipeline. update() commits deferred structural state;
    // layout() synchronizes page and overlay geometry; paint() preserves z-order.
    void update();
    void layout();
    void prepare(PaintContext& context);
    void paint(PaintContext& context);

    // Capture renderer-owned counters after the host has completed its render
    // surface frame (for WhatsCanvas, after Canvas::endFrame()). Headless and
    // non-instrumented backends leave the corresponding RenderStats counters
    // explicitly unavailable.
    void captureCompletedRendererStats(PaintContext& context);

    // The most recently completed frame. This is a diagnostics snapshot, not
    // a profiler: hosts may render it in an inspector without instrumenting
    // individual controls or introducing a backend dependency.
    [[nodiscard]] const FrameStats& frameStats() const noexcept;

    // Window-level input routing keeps overlays above the active page.
    bool dispatchPointer(const PointerEvent& event);
    bool dispatchKey(const KeyEvent& event);
    bool dispatchTextInput(const TextInputEvent& event);
    bool dispatchComposition(const CompositionInputEvent& event);

    // Hosts notify this boundary when native window activation changes. Losing
    // activation ends the platform text-input session and clears transient
    // pointer state; the logical focus is retained for restoration on return.
    void onPlatformFocusChanged(bool focused) noexcept;

private:
    void syncActiveRoot(Node* navigationRoot = nullptr) noexcept;
    void onOverlayChanged() noexcept;
    void syncTextInputSession() noexcept;
    void deactivateTextInputSession() noexcept;
    [[nodiscard]] Node* hitTest(PointF point) const;
    [[nodiscard]] Dialog* activeDialog() const noexcept;

    std::unique_ptr<PlatformWindow> platformWindow_;
    UiRoot uiRoot_;
    FocusManager focusManager_;
    InputRouter inputRouter_{&focusManager_};
    Navigator navigator_;
    OverlayHost overlayHost_;
    TextInput* activeTextInput_{nullptr};
    struct DialogEntry { OverlayId id; Node* restoreFocus; };
    std::vector<DialogEntry> dialogs_;
    FrameStats frameStats_{};
};

class UiApp {
public:
    UiApp() = default;
    explicit UiApp(std::unique_ptr<PlatformHost> host) noexcept;

    [[nodiscard]] PlatformHost* host() const noexcept;

    UiWindow& attachWindow(std::unique_ptr<PlatformWindow> platformWindow);
    UiWindow& openWindow(std::string title, SizeF logicalSize);
    [[nodiscard]] UiWindow* findWindow(WindowId id) noexcept;

    // Releases UI and platform resources for windows whose native peer has
    // closed. Hosts should call this at a safe frame boundary.
    std::size_t removeClosedWindows() noexcept;

    [[nodiscard]] const std::vector<std::unique_ptr<UiWindow>>& windows() const noexcept;

private:
    std::unique_ptr<PlatformHost> host_;
    std::vector<std::unique_ptr<UiWindow>> windows_;
};

} // namespace wui
