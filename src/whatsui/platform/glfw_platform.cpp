// GLFW platform backend for WhatsUI.
// Implements PlatformHost, PlatformWindow, and RenderSurface using GLFW + WhatsCanvas OpenGL.

// Include wsc BEFORE GLFW to avoid Windows header macro pollution (NEAR/FAR).
#include <wsc/Canvas.h>
#include <wsc/Surface.h>
#include <wsc/wsc.h>

#include "wui/glfw_platform.h"
#include "wui/animation.h"
#include "wui/app.h"
#include "wui/events.h"
#include "wui/paint_context.h"
#include "wui/scheduler.h"
#include "wui/whatscanvas_text.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <stdexcept>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <GLFW/glfw3native.h>
#endif

namespace wui {

// --- GlfwRenderSurface ---

class GlfwRenderSurface : public RenderSurface {
public:
    GlfwRenderSurface(GLFWwindow* window, int fbWidth, int fbHeight)
        : window_(window)
    {
        canvas_ = wsc::Canvas::create(wsc::Canvas::Backend::OpenGL, fbWidth, fbHeight);
        canvas_->setSize(fbWidth, fbHeight);
        canvas_->initializeContext();

#if defined(_WIN32)
        wsc::NativeSurface surface;
        surface.platform = wsc::NativeSurface::Platform::Win32;
        surface.window = glfwGetWin32Window(window);
        canPresent_ = canvas_->setOutputTarget(wsc::OutputTarget::ToWindow(surface));
#endif

        fbSize_ = {static_cast<float>(fbWidth), static_cast<float>(fbHeight)};
    }

    [[nodiscard]] CanvasBackend backend() const noexcept override
    {
        return CanvasBackend::OpenGL;
    }

    [[nodiscard]] SizeF framebufferSize() const noexcept override
    {
        return fbSize_;
    }

    void beginFrame() override
    {
        canvas_->beginFrame();
        canvas_->drawColor(wsc::Color(30, 30, 30));
    }

    void endFrame() override
    {
        canvas_->endFrame();
        if (canPresent_) {
            canvas_->present();
        } else {
            glfwSwapBuffers(window_);
        }
    }

    void resize(SizeF framebufferSize) override
    {
        const int w = static_cast<int>(framebufferSize.width);
        const int h = static_cast<int>(framebufferSize.height);
        if (w > 0 && h > 0) {
            canvas_->setSize(w, h);
            canvas_->resizeOutput(w, h);
            fbSize_ = framebufferSize;
        }
    }

    [[nodiscard]] wsc::Canvas& canvas() noexcept { return *canvas_; }

private:
    GLFWwindow* window_{nullptr};
    std::unique_ptr<wsc::Canvas> canvas_;
    bool canPresent_{false};
    SizeF fbSize_{};
};

// --- Stub implementations for Clipboard, CursorService, TextInputSession ---

class GlfwClipboard : public Clipboard {
public:
    explicit GlfwClipboard(GLFWwindow* window) : window_(window) {}

    void setText(std::string_view text) override
    {
        glfwSetClipboardString(window_, std::string(text).c_str());
    }

    [[nodiscard]] std::string getText() const override
    {
        const char* text = glfwGetClipboardString(window_);
        return text ? std::string(text) : std::string{};
    }

    [[nodiscard]] bool hasText() const override
    {
        const char* text = glfwGetClipboardString(window_);
        return text != nullptr && text[0] != '\0';
    }

private:
    GLFWwindow* window_;
};

class GlfwCursorService : public CursorService {
public:
    explicit GlfwCursorService(GLFWwindow* window) : window_(window) {}

    void setCursor(CursorIcon icon) override
    {
        int shape = GLFW_ARROW_CURSOR;
        switch (icon) {
        case CursorIcon::IBeam: shape = GLFW_IBEAM_CURSOR; break;
        case CursorIcon::Hand: shape = GLFW_HAND_CURSOR; break;
        case CursorIcon::ResizeHorizontal: shape = GLFW_HRESIZE_CURSOR; break;
        case CursorIcon::ResizeVertical: shape = GLFW_VRESIZE_CURSOR; break;
        // GLFW exposes no diagonal-resize standard cursors. Arrow is the
        // portable fallback; applications needing branded diagonal cursors
        // can supply them at the platform layer in a future API extension.
        case CursorIcon::ResizeDiagonalPrimary:
        case CursorIcon::ResizeDiagonalSecondary:
        case CursorIcon::Arrow:
        default: break;
        }
        if (shape == currentShape_) {
            return;
        }
        if (currentCursor_) {
            glfwDestroyCursor(currentCursor_);
        }
        currentCursor_ = glfwCreateStandardCursor(shape);
        glfwSetCursor(window_, currentCursor_);
        currentShape_ = shape;
    }

    ~GlfwCursorService() override
    {
        if (currentCursor_) {
            glfwDestroyCursor(currentCursor_);
        }
    }

private:
    GLFWwindow* window_;
    GLFWcursor* currentCursor_{nullptr};
    int currentShape_{-1};
};

class GlfwTextInputSession : public TextInputSession {
public:
    void activate() override {}
    void deactivate() override {}
    void setCaretRect(const RectF&) override {}
    void setSurroundingText(std::string_view, std::size_t, std::size_t) override {}
};

// --- GlfwPlatformWindow ---

class GlfwPlatformWindow : public PlatformWindow {
public:
    GlfwPlatformWindow(GLFWwindow* window, WindowId id)
        : window_(window)
        , id_(id)
        , clipboard_(window)
        , cursorService_(window)
    {
        glfwSetWindowUserPointer(window, this);
    }

    ~GlfwPlatformWindow() override
    {
        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
    }

    [[nodiscard]] WindowId id() const noexcept override { return id_; }

    [[nodiscard]] WindowMetrics metrics() const noexcept override
    {
        WindowMetrics m;
        int w, h, fbw, fbh;
        glfwGetWindowSize(window_, &w, &h);
        glfwGetFramebufferSize(window_, &fbw, &fbh);
        m.logicalSize = {static_cast<float>(w), static_cast<float>(h)};
        m.framebufferSize = {static_cast<float>(fbw), static_cast<float>(fbh)};
        m.scaleFactor = w > 0 ? static_cast<float>(fbw) / static_cast<float>(w) : 1.0f;
        return m;
    }

    void show() override { glfwShowWindow(window_); }
    void close() override { glfwSetWindowShouldClose(window_, GLFW_TRUE); }

    [[nodiscard]] bool isOpen() const noexcept override
    {
        return window_ != nullptr && !glfwWindowShouldClose(window_);
    }

    [[nodiscard]] bool isFocused() const noexcept override
    {
        return glfwGetWindowAttrib(window_, GLFW_FOCUSED) != 0;
    }

    void setTitle(std::string_view title) override
    {
        glfwSetWindowTitle(window_, std::string(title).c_str());
    }

    void requestRedraw() override { needsRedraw_ = true; }

    [[nodiscard]] RenderSurface& surface() override { return *surface_; }
    [[nodiscard]] Clipboard& clipboard() override { return clipboard_; }
    [[nodiscard]] CursorService& cursor() override { return cursorService_; }
    [[nodiscard]] TextInputSession& textInput() override { return textInputSession_; }

    [[nodiscard]] GLFWwindow* glfwWindow() const noexcept { return window_; }
    [[nodiscard]] bool needsRedraw() const noexcept { return needsRedraw_; }
    void clearRedraw() noexcept { needsRedraw_ = false; }

    void initSurface()
    {
        int fbw, fbh;
        glfwGetFramebufferSize(window_, &fbw, &fbh);
        surface_ = std::make_unique<GlfwRenderSurface>(window_, fbw, fbh);
    }

    [[nodiscard]] GlfwRenderSurface& glfwSurface() noexcept { return *surface_; }

    static GlfwPlatformWindow* fromGlfw(GLFWwindow* window)
    {
        return static_cast<GlfwPlatformWindow*>(glfwGetWindowUserPointer(window));
    }

    // Event callbacks (set by the host/app to dispatch into the UI tree)
    std::function<void(const PointerEvent&)> onPointerEvent;
    std::function<void(const KeyEvent&)> onKeyEvent;
    std::function<void(const TextInputEvent&)> onTextInput;
    std::function<void(bool)> onFocusChanged;

private:
    GLFWwindow* window_;
    WindowId id_;
    std::unique_ptr<GlfwRenderSurface> surface_;
    GlfwClipboard clipboard_;
    GlfwCursorService cursorService_;
    GlfwTextInputSession textInputSession_;
    bool needsRedraw_{true};
};

// --- GlfwPlatformHost ---

class GlfwPlatformHost : public PlatformHost {
public:
    GlfwPlatformHost()
    {
        if (!glfwInit()) {
            throw std::runtime_error("glfwInit() failed");
        }
    }

    ~GlfwPlatformHost() override
    {
        glfwTerminate();
    }

    [[nodiscard]] std::unique_ptr<PlatformWindow> createWindow(std::string title, SizeF logicalSize) override
    {
        const int w = static_cast<int>(logicalSize.width);
        const int h = static_cast<int>(logicalSize.height);

        GLFWwindow* glfwWindow = glfwCreateWindow(w, h, title.c_str(), nullptr, nullptr);
        if (!glfwWindow) {
            const char* errMsg = nullptr;
            glfwGetError(&errMsg);
            throw std::runtime_error(std::string("glfwCreateWindow() failed: ") + (errMsg ? errMsg : "unknown"));
        }

        glfwMakeContextCurrent(glfwWindow);
        glfwSwapInterval(1);

        if (!glLoaded_) {
            if (!wsc::Canvas::loadOpenGL(
                    reinterpret_cast<wsc::Canvas::OpenGLProcAddress>(glfwGetProcAddress))) {
                throw std::runtime_error("Canvas::loadOpenGL() failed");
            }
            glLoaded_ = true;
        }

        auto window = std::make_unique<GlfwPlatformWindow>(glfwWindow, nextWindowId_++);
        window->initSurface();

        installCallbacks(glfwWindow);

        windows_.push_back(window.get());
        return window;
    }

    void setFrameCallback(std::function<void()> callback)
    {
        frameCallback_ = std::move(callback);
    }

    [[nodiscard]] int run() override
    {
        while (!shouldQuit_) {
            glfwPollEvents();

            // Remove closed windows
            const auto previousWindowCount = windows_.size();
            windows_.erase(
                std::remove_if(windows_.begin(), windows_.end(),
                               [](GlfwPlatformWindow* w) { return !w->isOpen(); }),
                windows_.end());
            const bool closedWindowRemoved = windows_.size() != previousWindowCount;

            if (windows_.empty()) {
                break;
            }

            const bool needsFrame = std::any_of(
                windows_.begin(), windows_.end(),
                [](const GlfwPlatformWindow* window) { return window->needsRedraw(); })
                || Ticker::instance().hasActive();
            if (frameCallback_ && (needsFrame || closedWindowRemoved)) {
                frameCallback_();
            } else {
                // Block while idle rather than polling at an uncapped rate.
                // Input, resize, and close events wake GLFW promptly.
                glfwWaitEventsTimeout(0.05);
            }
        }
        return exitCode_;
    }

    void quit(int exitCode) override
    {
        exitCode_ = exitCode;
        shouldQuit_ = true;
    }

private:
    static KeyModifiers modifiersFor(GLFWwindow* window)
    {
        KeyModifiers modifiers = 0;
        const auto pressed = [window](int key) {
            return glfwGetKey(window, key) == GLFW_PRESS;
        };
        if (pressed(GLFW_KEY_LEFT_SHIFT) || pressed(GLFW_KEY_RIGHT_SHIFT)) modifiers |= KeyModifierShift;
        if (pressed(GLFW_KEY_LEFT_CONTROL) || pressed(GLFW_KEY_RIGHT_CONTROL)) modifiers |= KeyModifierControl;
        if (pressed(GLFW_KEY_LEFT_ALT) || pressed(GLFW_KEY_RIGHT_ALT)) modifiers |= KeyModifierAlt;
        if (pressed(GLFW_KEY_LEFT_SUPER) || pressed(GLFW_KEY_RIGHT_SUPER)) modifiers |= KeyModifierSuper;
        return modifiers;
    }

    static KeyModifiers modifiersFromGlfw(int modifiers)
    {
        KeyModifiers result = 0;
        if ((modifiers & GLFW_MOD_SHIFT) != 0) result |= KeyModifierShift;
        if ((modifiers & GLFW_MOD_CONTROL) != 0) result |= KeyModifierControl;
        if ((modifiers & GLFW_MOD_ALT) != 0) result |= KeyModifierAlt;
        if ((modifiers & GLFW_MOD_SUPER) != 0) result |= KeyModifierSuper;
        return result;
    }

    void installCallbacks(GLFWwindow* window)
    {
        auto* pw = GlfwPlatformWindow::fromGlfw(window);

        glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {
            auto* pw = GlfwPlatformWindow::fromGlfw(w);
            if (!pw) return;
            PointerEvent event;
            event.windowId = pw->id();
            event.pointerType = PointerType::Mouse;
            event.action = PointerAction::Move;
            event.button = MouseButton::None;
            event.position = {static_cast<float>(x), static_cast<float>(y)};
            event.modifiers = modifiersFor(w);
            pw->requestRedraw();
            if (pw->onPointerEvent) pw->onPointerEvent(event);
        });

        glfwSetCursorEnterCallback(window, [](GLFWwindow* w, int entered) {
            auto* pw = GlfwPlatformWindow::fromGlfw(w);
            if (!pw) return;
            double x, y;
            glfwGetCursorPos(w, &x, &y);
            PointerEvent event;
            event.windowId = pw->id();
            event.pointerType = PointerType::Mouse;
            event.action = entered == GLFW_TRUE ? PointerAction::Enter : PointerAction::Leave;
            event.position = {static_cast<float>(x), static_cast<float>(y)};
            event.modifiers = modifiersFor(w);
            pw->requestRedraw();
            if (pw->onPointerEvent) pw->onPointerEvent(event);
        });

        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
            auto* pw = GlfwPlatformWindow::fromGlfw(w);
            if (!pw) return;
            double x, y;
            glfwGetCursorPos(w, &x, &y);
            PointerEvent event;
            event.windowId = pw->id();
            event.pointerType = PointerType::Mouse;
            event.action = (action == GLFW_PRESS) ? PointerAction::Down : PointerAction::Up;
            switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT: event.button = MouseButton::Left; break;
            case GLFW_MOUSE_BUTTON_RIGHT: event.button = MouseButton::Right; break;
            case GLFW_MOUSE_BUTTON_MIDDLE: event.button = MouseButton::Middle; break;
            default: event.button = MouseButton::None; break;
            }
            event.position = {static_cast<float>(x), static_cast<float>(y)};
            event.modifiers = modifiersFromGlfw(mods);
            pw->requestRedraw();
            if (pw->onPointerEvent) pw->onPointerEvent(event);
        });

        glfwSetScrollCallback(window, [](GLFWwindow* w, double xoffset, double yoffset) {
            auto* pw = GlfwPlatformWindow::fromGlfw(w);
            if (!pw) return;
            double x, y;
            glfwGetCursorPos(w, &x, &y);
            PointerEvent event;
            event.windowId = pw->id();
            event.pointerType = PointerType::Mouse;
            event.action = PointerAction::Scroll;
            event.position = {static_cast<float>(x), static_cast<float>(y)};
            event.modifiers = modifiersFor(w);
            // GLFW reports abstract wheel "steps". Convert to the framework's
            // logical-pixel convention so all backends expose the same API.
            event.scrollDelta = {static_cast<float>(xoffset * 40.0), static_cast<float>(yoffset * 40.0)};
            pw->requestRedraw();
            if (pw->onPointerEvent) pw->onPointerEvent(event);
        });

        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int /*scancode*/, int action, int mods) {
            auto* pw = GlfwPlatformWindow::fromGlfw(w);
            if (!pw) return;
            if (action == GLFW_REPEAT || action == GLFW_PRESS || action == GLFW_RELEASE) {
                KeyEvent event;
                event.windowId = pw->id();
                event.action = (action == GLFW_RELEASE) ? KeyAction::Up : KeyAction::Down;
                event.keyCode = key;
                event.modifiers = modifiersFromGlfw(mods);
                event.isRepeat = (action == GLFW_REPEAT);
                pw->requestRedraw();
                if (pw->onKeyEvent) pw->onKeyEvent(event);
            }
        });

        glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int codepoint) {
            auto* pw = GlfwPlatformWindow::fromGlfw(w);
            if (!pw) return;
            char buf[5] = {};
            if (codepoint < 0x80) {
                buf[0] = static_cast<char>(codepoint);
            } else if (codepoint < 0x800) {
                buf[0] = static_cast<char>(0xC0 | (codepoint >> 6));
                buf[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
            } else if (codepoint < 0x10000) {
                buf[0] = static_cast<char>(0xE0 | (codepoint >> 12));
                buf[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                buf[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
            } else {
                buf[0] = static_cast<char>(0xF0 | (codepoint >> 18));
                buf[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                buf[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                buf[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
            }
            TextInputEvent event;
            event.windowId = pw->id();
            event.text = buf;
            pw->requestRedraw();
            if (pw->onTextInput) pw->onTextInput(event);
        });

        glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) {
            auto* pw = GlfwPlatformWindow::fromGlfw(w);
            if (!pw) return;
            pw->requestRedraw();
            if (pw->onFocusChanged) pw->onFocusChanged(focused == GLFW_TRUE);
        });

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
            auto* pw = GlfwPlatformWindow::fromGlfw(w);
            if (!pw) return;
            if (width > 0 && height > 0) {
                glfwMakeContextCurrent(w);
                pw->glfwSurface().resize({static_cast<float>(width), static_cast<float>(height)});
            }
            pw->requestRedraw();
        });

        (void)pw;
    }

    std::vector<GlfwPlatformWindow*> windows_;
    WindowId nextWindowId_{1};
    bool glLoaded_{false};
    bool shouldQuit_{false};
    int exitCode_{0};
    std::function<void()> frameCallback_;
};

// --- Factory function ---

std::unique_ptr<PlatformHost> createGlfwPlatformHost()
{
    return std::make_unique<GlfwPlatformHost>();
}

// --- Convenience one-liner ---

int runGlfwApp(std::string title, SizeF size, GlfwRootFactory rootFactory)
{
    auto hostPtr = std::make_unique<GlfwPlatformHost>();
    GlfwPlatformHost* host = hostPtr.get();

    UiApp app(std::move(hostPtr));
    auto& uiWindow = app.openWindow(std::move(title), size);
    uiWindow.setRoot(rootFactory(uiWindow));

    // Get the concrete platform window to wire event callbacks
    auto& pw = uiWindow.platformWindow();
    // We know it's GlfwPlatformWindow since we created it via GlfwPlatformHost
    GlfwPlatformWindow* glfwWin = static_cast<GlfwPlatformWindow*>(&pw);

    // Wire event callbacks to dispatch into the UI tree
    glfwWin->onPointerEvent = [&uiWindow](const PointerEvent& event) {
        uiWindow.dispatchPointer(event);
    };

    glfwWin->onKeyEvent = [&uiWindow](const KeyEvent& event) {
        uiWindow.dispatchKey(event);
    };

    glfwWin->onTextInput = [&uiWindow](const TextInputEvent& event) {
        uiWindow.dispatchTextInput(event);
    };
    glfwWin->onFocusChanged = [&uiWindow](bool focused) {
        uiWindow.onPlatformFocusChanged(focused);
    };

    // Set frame callback: flush structural updates, tick animations, layout, paint
    host->setFrameCallback([&app, lastFrame = std::chrono::steady_clock::now()]() mutable {
        const auto now = std::chrono::steady_clock::now();
        const float deltaSeconds = std::clamp(
            std::chrono::duration<float>(now - lastFrame).count(), 0.0f, 0.1f);
        lastFrame = now;

        const bool hadActiveAnimations = Ticker::instance().hasActive();
        // The ticker is application-scoped: advance it exactly once per host
        // frame, regardless of how many windows are currently open.
        if (hadActiveAnimations) {
            Ticker::instance().tick(deltaSeconds);
        }

        // The host has already dropped its non-owning native-window pointers
        // for closed peers. Release the corresponding UI windows before any
        // event callback can observe stale application state.
        app.removeClosedWindows();

        for (const auto& winPtr : app.windows()) {
            auto& win = *winPtr;
            auto& platformWin = win.platformWindow();

            if (!platformWin.isOpen()) continue;

            // Ensure GL context is current before rendering
            auto* glfwWin = static_cast<GlfwPlatformWindow*>(&platformWin);
            if (!glfwWin->needsRedraw() && !hadActiveAnimations) continue;
            // Consume this request before rendering. Invalidations raised by
            // update/layout/paint intentionally schedule one more frame.
            glfwWin->clearRedraw();
            glfwMakeContextCurrent(glfwWin->glfwWindow());

            // Commit deferred state changes at the window frame boundary.
            win.update();

            // Measure, layout and paint against the same font backend and DPR.
            auto m = platformWin.metrics();
            auto& glfwSurface = static_cast<GlfwRenderSurface&>(platformWin.surface());
            WhatsCanvasTextMeasurer textMeasurer(glfwSurface.canvas(), m.scaleFactor);
            setTextMeasurer(&textMeasurer);
            win.layout();

            // Paint
            PaintContext ctx(glfwSurface.canvas(), m.scaleFactor);
            win.prepare(ctx);
            platformWin.surface().beginFrame();

            win.paint(ctx);

            platformWin.surface().endFrame();
            setTextMeasurer(nullptr);
        }
    });

    return host->run();
}

int runGlfwApp(std::string title, SizeF size, std::unique_ptr<Node> root)
{
    // std::function requires a copyable callable while a UI tree is
    // move-only. Share the one-shot holder so the factory itself remains
    // copyable and still transfers ownership exactly once.
    auto rootHolder = std::make_shared<std::unique_ptr<Node>>(std::move(root));
    return runGlfwApp(std::move(title), size,
                      [rootHolder](UiWindow&) { return std::move(*rootHolder); });
}

} // namespace wui
