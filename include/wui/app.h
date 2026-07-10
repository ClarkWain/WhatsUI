#pragma once

#include <memory>
#include <string>
#include <vector>

#include "wui/platform.h"
#include "wui/runtime.h"

namespace wui {

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

    void setRoot(std::unique_ptr<Node> root);
    [[nodiscard]] Node* root() const noexcept;

    // Window-level frame pipeline. update() commits deferred structural state;
    // layout() synchronizes page and overlay geometry; paint() preserves z-order.
    void update();
    void layout();
    void prepare(PaintContext& context);
    void paint(PaintContext& context);

    // Window-level input routing keeps overlays above the active page.
    bool dispatchPointer(const PointerEvent& event);
    bool dispatchKey(const KeyEvent& event);
    bool dispatchTextInput(const TextInputEvent& event);
    bool dispatchComposition(const CompositionInputEvent& event);

private:
    void syncActiveRoot(Node* navigationRoot = nullptr) noexcept;
    [[nodiscard]] Node* hitTest(PointF point) const;

    std::unique_ptr<PlatformWindow> platformWindow_;
    UiRoot uiRoot_;
    FocusManager focusManager_;
    InputRouter inputRouter_{&focusManager_};
    Navigator navigator_;
    OverlayHost overlayHost_;
};

class UiApp {
public:
    UiApp() = default;
    explicit UiApp(std::unique_ptr<PlatformHost> host) noexcept;

    [[nodiscard]] PlatformHost* host() const noexcept;

    UiWindow& attachWindow(std::unique_ptr<PlatformWindow> platformWindow);
    UiWindow& openWindow(std::string title, SizeF logicalSize);
    [[nodiscard]] UiWindow* findWindow(WindowId id) noexcept;

    [[nodiscard]] const std::vector<std::unique_ptr<UiWindow>>& windows() const noexcept;

private:
    std::unique_ptr<PlatformHost> host_;
    std::vector<std::unique_ptr<UiWindow>> windows_;
};

} // namespace wui
