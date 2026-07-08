#pragma once

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
