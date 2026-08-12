#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "wui/accessibility.h"
#include "wui/types.h"

namespace wui {

struct WindowMetrics {
    SizeF logicalSize{};
    SizeF framebufferSize{};
    float scaleFactor{1.0f};
};

enum class WindowFrameStyle {
    System,
    Custom,
    Borderless,
};

enum class WindowBackdrop {
    Opaque,
    Transparent,
    Blur,
    SystemMaterial,
};

enum class WindowState {
    Hidden,
    Normal,
    Minimized,
    Maximized,
};

enum class WindowFrameRegionKind {
    Client,
    Caption,
    MinimizeButton,
    MaximizeButton,
    CloseButton,
};

struct WindowFrameRegion {
    RectF bounds{};
    WindowFrameRegionKind kind{WindowFrameRegionKind::Client};
};

[[nodiscard]] inline std::optional<WindowFrameRegionKind>
hitTestWindowFrameRegions(
    const std::vector<WindowFrameRegion>& regions,
    PointF point) noexcept
{
    const auto contains = [point](const RectF& bounds) {
        return point.x >= bounds.x && point.y >= bounds.y
            && point.x < bounds.x + bounds.width
            && point.y < bounds.y + bounds.height;
    };
    for (const auto& region : regions) {
        if (region.kind == WindowFrameRegionKind::Client
            && contains(region.bounds)) {
            return region.kind;
        }
    }
    for (const auto& region : regions) {
        if (contains(region.bounds)) return region.kind;
    }
    return std::nullopt;
}

struct WindowOptions {
    std::string title;
    SizeF initialSize{800.0f, 600.0f};
    // A zero-vector minimum/maximum means "unconstrained on that axis";
    // to lock a dimension, set both minimum and maximum to the same value.
    SizeF minimumSize{};
    SizeF maximumSize{};
    WindowFrameStyle frameStyle{WindowFrameStyle::System};
    WindowBackdrop backdrop{WindowBackdrop::Opaque};
    bool resizable{true};
    bool transparentFramebuffer{false};
    bool alwaysOnTop{false};
    bool visibleOnCreate{true};
};

// Capability sets grow only additively: new fields default to false so
// clients using aggregate-init or field-by-field checks keep compiling.
struct WindowCapabilities {
    bool customFrame{false};
    bool transparentFramebuffer{false};
    bool systemMove{false};
    bool systemResize{false};
    bool alwaysOnTop{false};
    bool backdropBlur{false};
};

enum class WindowCloseDecision {
    Close,
    Hide,
    Cancel,
};

using WindowCloseRequestHandler = std::function<WindowCloseDecision()>;

enum class DesktopOperationResult {
    Succeeded,
    Unsupported,
    PermissionDenied,
    Failed,
};

// pre-1.0 preview: tray menus expose only Action/Check/Separator. Submenu
// and Radio semantics are deferred to a future tray-menu-v2 revision so
// backends do not need to emulate them today.
enum class TrayMenuItemKind {
    Action,
    Check,
    Separator,
};

struct TrayMenuItem {
    std::string id;
    std::string label;
    TrayMenuItemKind kind{TrayMenuItemKind::Action};
    bool enabled{true};
    bool checked{false};
};

// Packed RGBA icon buffer: one row is exactly pixelWidth*4 bytes with no
// stride padding, channel order is R,G,B,A, alpha is straight (not
// premultiplied), and colours are in sRGB. Backends convert to BGRA or
// premultiplied variants when the platform requires it.
struct DesktopIcon {
    std::vector<unsigned char> rgbaPixels;
    int pixelWidth{0};
    int pixelHeight{0};

    [[nodiscard]] bool empty() const noexcept
    {
        return rgbaPixels.empty();
    }
    [[nodiscard]] bool valid() const noexcept
    {
        return pixelWidth > 0 && pixelHeight > 0
            && rgbaPixels.size()
                == static_cast<std::size_t>(pixelWidth)
                    * static_cast<std::size_t>(pixelHeight) * 4u;
    }
};

struct TrayIconOptions {
    std::string tooltip;
    DesktopIcon icon;
    std::vector<TrayMenuItem> menu;
    std::string defaultActionId;
};

enum class NotificationUrgency {
    Low,
    Normal,
    Critical,
};

struct DesktopNotification {
    std::string id;
    std::string title;
    std::string body;
    std::string activationPayload;
    NotificationUrgency urgency{NotificationUrgency::Normal};
};

// Capability sets grow only additively: new fields default to false so
// clients using aggregate-init or field-by-field checks keep compiling.
struct DesktopCapabilities {
    bool tray{false};
    bool notifications{false};
    bool notificationActions{false};
};

enum class DesktopEventKind {
    TrayAction,
    NotificationActivated,
};

struct DesktopEvent {
    DesktopEventKind kind{DesktopEventKind::TrayAction};
    std::string id;
    std::string payload;
};

using DesktopEventHandler = std::function<void(const DesktopEvent&)>;
using DesktopEventDispatcher =
    std::function<void(std::function<void()>)>;

class DesktopServices {
public:
    virtual ~DesktopServices() = default;

    [[nodiscard]] virtual DesktopCapabilities capabilities() const noexcept
    {
        return {};
    }
    virtual DesktopOperationResult setTrayIcon(const TrayIconOptions&)
    {
        return DesktopOperationResult::Unsupported;
    }
    virtual void removeTrayIcon() {}
    virtual DesktopOperationResult showNotification(
        const DesktopNotification&)
    {
        return DesktopOperationResult::Unsupported;
    }
    virtual void setEventHandler(DesktopEventHandler handler)
    {
        std::lock_guard<std::mutex> lock(eventChannel_->mutex);
        eventChannel_->handler = std::move(handler);
    }
    virtual void setEventDispatcher(DesktopEventDispatcher dispatcher)
    {
        std::lock_guard<std::mutex> lock(eventChannel_->mutex);
        eventChannel_->dispatcher = std::move(dispatcher);
    }

protected:
    void publishEvent(const DesktopEvent& event) const
    {
        const auto channel = eventChannel_;
        DesktopEventDispatcher dispatcher;
        {
            std::lock_guard<std::mutex> lock(channel->mutex);
            dispatcher = channel->dispatcher;
        }
        auto deliver = [channel, event] {
            DesktopEventHandler handler;
            {
                std::lock_guard<std::mutex> lock(channel->mutex);
                handler = channel->handler;
            }
            if (handler) handler(event);
        };
        if (dispatcher) dispatcher(std::move(deliver));
        else deliver();
    }

private:
    struct EventChannel {
        std::mutex mutex;
        DesktopEventHandler handler;
        DesktopEventDispatcher dispatcher;
    };
    std::shared_ptr<EventChannel> eventChannel_ =
        std::make_shared<EventChannel>();
};

// Projects a logical TextFieldNode caret to the coordinate space required by
// native client-area APIs such as IMM32.  The returned point is deliberately
// rounded because Win32 candidate/composition windows accept integer client
// pixels.  Keep this independent of a particular window backend so the exact
// fractional-DPI boundary can be exercised in headless tests.
[[nodiscard]] inline PointF projectLogicalCaretToClientPixels(
    const RectF& caret,
    SizeF logicalWindowSize,
    SizeF clientPixelSize) noexcept
{
    const float scaleX = logicalWindowSize.width > 0.0f
        ? clientPixelSize.width / logicalWindowSize.width
        : 1.0f;
    const float scaleY = logicalWindowSize.height > 0.0f
        ? clientPixelSize.height / logicalWindowSize.height
        : 1.0f;
    return {std::round(caret.x * scaleX),
            std::round((caret.y + caret.height) * scaleY)};
}

class RenderSurface {
public:
    virtual ~RenderSurface() = default;

    [[nodiscard]] virtual CanvasBackend backend() const noexcept = 0;
    [[nodiscard]] virtual SizeF framebufferSize() const noexcept = 0;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void resize(SizeF framebufferSize) = 0;
};

class Clipboard {
public:
    virtual ~Clipboard() = default;

    virtual void setText(std::string_view text) = 0;
    [[nodiscard]] virtual std::string getText() const = 0;
    [[nodiscard]] virtual bool hasText() const = 0;
};

enum class CursorIcon {
    Arrow,
    IBeam,
    Hand,
    ResizeHorizontal,
    ResizeVertical,
    ResizeDiagonalPrimary,
    ResizeDiagonalSecondary,
};

class CursorService {
public:
    virtual ~CursorService() = default;

    virtual void setCursor(CursorIcon icon) = 0;
};

class TextInputSession {
public:
    virtual ~TextInputSession() = default;

    virtual void activate() = 0;
    virtual void deactivate() = 0;
    virtual void setCaretRect(const RectF& rect) = 0;
    virtual void setSurroundingText(std::string_view text, std::size_t selectionStart, std::size_t selectionEnd) = 0;
};

class PlatformWindow {
public:
    virtual ~PlatformWindow() = default;

    [[nodiscard]] virtual WindowId id() const noexcept = 0;
    [[nodiscard]] virtual WindowMetrics metrics() const noexcept = 0;

    virtual void show() = 0;
    virtual void hide() {}
    virtual void close() = 0;
    virtual void requestClose() { close(); }
    [[nodiscard]] virtual bool isOpen() const noexcept = 0;
    [[nodiscard]] virtual bool isFocused() const noexcept = 0;
    [[nodiscard]] virtual bool isVisible() const noexcept { return isOpen(); }

    virtual void focus() {}
    virtual void minimize() {}
    virtual void maximize() {}
    virtual void restore() {}
    // Backends overriding maximize()/restore() must also override state() so
    // that this default toggle can distinguish Maximized from Normal. The
    // Hidden/Minimized fallback intentionally maps to maximize() rather than
    // a no-op to match the historical Win32 taskbar behaviour.
    virtual void toggleMaximized()
    {
        switch (state()) {
        case WindowState::Maximized:
            restore();
            break;
        case WindowState::Hidden:
            show();
            maximize();
            break;
        case WindowState::Minimized:
        case WindowState::Normal:
        default:
            maximize();
            break;
        }
    }
    [[nodiscard]] virtual WindowState state() const noexcept
    {
        return isVisible() ? WindowState::Normal : WindowState::Hidden;
    }
    [[nodiscard]] virtual WindowCapabilities capabilities() const noexcept
    {
        return {};
    }
    virtual void setAlwaysOnTop(bool) {}
    virtual void setOpacity(float) {}
    virtual void setFrameRegions(std::vector<WindowFrameRegion>) {}
    virtual void setCloseRequestHandler(WindowCloseRequestHandler) {}

    virtual void setTitle(std::string_view title) = 0;
    // Backends that retain a native title can expose it as the accessible
    // application name. Older/headless hosts may keep the empty default.
    [[nodiscard]] virtual std::string title() const { return {}; }
    virtual void requestRedraw() = 0;

    // Native accessibility adapters consume an immutable semantic snapshot
    // instead of reaching back into the mutable UI tree from an OS callback
    // thread. Backends without a native bridge intentionally ignore it.
    virtual void publishAccessibilitySnapshot(AccessibilitySnapshot snapshot)
    {
        (void)snapshot;
    }
    virtual void setAccessibilityActionHandler(AccessibilityActionHandler handler)
    {
        (void)handler;
    }

    [[nodiscard]] virtual RenderSurface& surface() = 0;
    [[nodiscard]] virtual Clipboard& clipboard() = 0;
    [[nodiscard]] virtual CursorService& cursor() = 0;
    [[nodiscard]] virtual TextInputSession& textInput() = 0;
};

class PlatformHost {
public:
    virtual ~PlatformHost() = default;

    [[nodiscard]] virtual std::unique_ptr<PlatformWindow> createWindow(std::string title, SizeF logicalSize) = 0;
    // Pre-1.0 preview: the default WindowOptions overload only forwards
    // title/initialSize + the runtime-settable alwaysOnTop/visibleOnCreate
    // fields. frameStyle, backdrop, transparentFramebuffer, resizable and
    // min/max size cannot be expressed after the fact and are silently
    // dropped when a backend has not overridden this method. Backends that
    // want to honour those options must override this overload directly.
    [[nodiscard]] virtual std::unique_ptr<PlatformWindow> createWindow(
        const WindowOptions& options)
    {
        auto window = createWindow(options.title, options.initialSize);
        if (window) {
            if (options.alwaysOnTop) window->setAlwaysOnTop(true);
            if (!options.visibleOnCreate) window->hide();
        }
        return window;
    }
    // Each PlatformHost owns its own no-op DesktopServices fallback so that
    // multiple hosts do not share a single process-wide event channel.
    // Backends with real tray/notification support override this to return
    // their own service instance.
    [[nodiscard]] virtual DesktopServices& desktopServices() noexcept
    {
        return defaultDesktopServices_;
    }
    [[nodiscard]] virtual int run() = 0;
    virtual void quit(int exitCode = 0) = 0;
    // Thread-safe wake-up hook used when a worker posts UI work. Backends
    // without a blocking event loop can keep the default no-op.
    virtual void wake() {}

private:
    DesktopServices defaultDesktopServices_;
};

} // namespace wui
