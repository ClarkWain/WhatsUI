#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <string_view>

#include "wui/types.h"

namespace wui {

struct WindowMetrics {
    SizeF logicalSize{};
    SizeF framebufferSize{};
    float scaleFactor{1.0f};
};

// Projects a logical TextInput caret to the coordinate space required by
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
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const noexcept = 0;
    [[nodiscard]] virtual bool isFocused() const noexcept = 0;

    virtual void setTitle(std::string_view title) = 0;
    // Backends that retain a native title can expose it as the accessible
    // application name. Older/headless hosts may keep the empty default.
    [[nodiscard]] virtual std::string title() const { return {}; }
    virtual void requestRedraw() = 0;

    [[nodiscard]] virtual RenderSurface& surface() = 0;
    [[nodiscard]] virtual Clipboard& clipboard() = 0;
    [[nodiscard]] virtual CursorService& cursor() = 0;
    [[nodiscard]] virtual TextInputSession& textInput() = 0;
};

class PlatformHost {
public:
    virtual ~PlatformHost() = default;

    [[nodiscard]] virtual std::unique_ptr<PlatformWindow> createWindow(std::string title, SizeF logicalSize) = 0;
    [[nodiscard]] virtual int run() = 0;
    virtual void quit(int exitCode = 0) = 0;
};

} // namespace wui
