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
#include <cmath>
#include <stdexcept>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <GLFW/glfw3native.h>
#include <imm.h>
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

// --- Platform implementations for Clipboard, CursorService, TextInputSession ---

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

// GLFW exposes committed Unicode codepoints, but deliberately does not expose
// the Win32 pre-edit (composition) messages.  IMM32 is the small, stable
// Windows bridge for the reference GLFW host: it preserves the platform-neutral
// TextInputSession API while forwarding pre-edit updates into UiWindow.
//
// This is intentionally hosted here rather than in TextInput. Widgets never
// include Win32 headers and the editing controller remains platform agnostic.
class GlfwTextInputSession : public TextInputSession {
public:
    GlfwTextInputSession(GLFWwindow* window, WindowId windowId)
        : window_(window)
        , windowId_(windowId)
    {
#if defined(_WIN32)
        nativeWindow_ = glfwGetWin32Window(window_);
        if (nativeWindow_ == nullptr) {
            return;
        }

        // GLFW uses GWLP_USERDATA itself, so a window property gives the
        // subclass procedure a private association without disturbing GLFW.
        SetPropW(nativeWindow_, propertyName(), reinterpret_cast<HANDLE>(this));
        SetLastError(ERROR_SUCCESS);
        const auto previous = SetWindowLongPtrW(
            nativeWindow_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&imeWindowProc));
        if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
            RemovePropW(nativeWindow_, propertyName());
            nativeWindow_ = nullptr;
            return;
        }
        previousWindowProc_ = reinterpret_cast<WNDPROC>(previous);
#endif
    }

    ~GlfwTextInputSession() override
    {
#if defined(_WIN32)
        if (nativeWindow_ != nullptr) {
            // Restore only while this is still our subclass. GLFW destroys the
            // HWND after the PlatformWindow members, so this is deterministic.
            if (reinterpret_cast<WNDPROC>(GetWindowLongPtrW(nativeWindow_, GWLP_WNDPROC)) == &imeWindowProc) {
                SetWindowLongPtrW(nativeWindow_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previousWindowProc_));
            }
            RemovePropW(nativeWindow_, propertyName());
        }
#endif
    }

    void setCallbacks(std::function<void(const TextInputEvent&)> onTextInput,
                      std::function<void(const CompositionInputEvent&)> onComposition)
    {
        onTextInput_ = std::move(onTextInput);
        onComposition_ = std::move(onComposition);
    }

    void activate() override
    {
        active_ = true;
#if defined(_WIN32)
        updateImeWindows();
#endif
    }

    void deactivate() override
    {
        // Mark inactive before asking IMM32 to cancel. This prevents an
        // immediate native end message from re-synchronizing the caret while
        // UiWindow is tearing the session down.
#if defined(_WIN32)
        const bool wasActive = active_;
#endif
        active_ = false;
#if defined(_WIN32)
        if (wasActive && nativeWindow_ != nullptr) {
            if (HIMC context = ImmGetContext(nativeWindow_)) {
                // Cancel, rather than commit, a pre-edit string when focus
                // leaves the widget/window. UiWindow already owns the logical
                // focus transition and will ignore any now-stale update.
                ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
                ImmReleaseContext(nativeWindow_, context);
            }
        }
#endif
        // Some IMEs do not emit WM_IME_ENDCOMPOSITION after CPS_CANCEL. Keep
        // the framework composition lifecycle balanced in that case.
        if (compositionOpen_ && onComposition_) {
            onComposition_({windowId_, {}, CompositionInputEvent::Phase::End});
        }
        compositionOpen_ = false;
    }

    void setCaretRect(const RectF& rect) override
    {
        caretRect_ = rect;
#if defined(_WIN32)
        if (active_) {
            updateImeWindows();
        }
#endif
    }

    void setSurroundingText(std::string_view text, std::size_t selectionStart, std::size_t selectionEnd) override
    {
        // IMM32 cannot publish surrounding text to modern TSF services. Keep a
        // snapshot for diagnostics and future TSF adoption; composition and
        // committed strings still route through the existing UiWindow model.
        surroundingText_.assign(text.data(), text.size());
        selectionStart_ = selectionStart;
        selectionEnd_ = selectionEnd;
    }

private:
#if defined(_WIN32)
    static constexpr const wchar_t* propertyName() noexcept
    {
        return L"WhatsUI.GlfwTextInputSession";
    }

    static std::string utf8FromWide(const std::wstring& text)
    {
        if (text.empty()) {
            return {};
        }
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(std::max(0, size)), '\0');
        if (size > 0) {
            WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        }
        return result;
    }

    static std::string readImeString(HIMC context, DWORD kind)
    {
        const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
        if (bytes <= 0) {
            return {};
        }
        std::wstring value(static_cast<std::size_t>(bytes) / sizeof(wchar_t), L'\0');
        const LONG copied = ImmGetCompositionStringW(context, kind, value.data(), bytes);
        if (copied <= 0) {
            return {};
        }
        value.resize(static_cast<std::size_t>(copied) / sizeof(wchar_t));
        return utf8FromWide(value);
    }

    POINT nativeCaretPoint() const noexcept
    {
        RECT client{};
        GetClientRect(nativeWindow_, &client);
        int logicalWidth = 0;
        int logicalHeight = 0;
        glfwGetWindowSize(window_, &logicalWidth, &logicalHeight);
        const float scaleX = logicalWidth > 0
            ? static_cast<float>(client.right - client.left) / static_cast<float>(logicalWidth)
            : 1.0f;
        const float scaleY = logicalHeight > 0
            ? static_cast<float>(client.bottom - client.top) / static_cast<float>(logicalHeight)
            : 1.0f;
        return {static_cast<LONG>(std::lround(caretRect_.x * scaleX)),
                static_cast<LONG>(std::lround((caretRect_.y + caretRect_.height) * scaleY))};
    }

    void updateImeWindows() noexcept
    {
        if (nativeWindow_ == nullptr) {
            return;
        }
        HIMC context = ImmGetContext(nativeWindow_);
        if (context == nullptr) {
            return;
        }
        const POINT point = nativeCaretPoint();
        COMPOSITIONFORM composition{};
        composition.dwStyle = CFS_POINT;
        composition.ptCurrentPos = point;
        ImmSetCompositionWindow(context, &composition);

        CANDIDATEFORM candidate{};
        candidate.dwIndex = 0;
        candidate.dwStyle = CFS_CANDIDATEPOS;
        candidate.ptCurrentPos = point;
        ImmSetCandidateWindow(context, &candidate);
        ImmReleaseContext(nativeWindow_, context);
    }

    LRESULT handleImeMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
    {
        if (!active_) {
            return CallWindowProcW(previousWindowProc_, hwnd, message, wParam, lParam);
        }

        switch (message) {
        case WM_IME_STARTCOMPOSITION:
            updateImeWindows();
            compositionOpen_ = true;
            if (onComposition_) {
                onComposition_({windowId_, {}, CompositionInputEvent::Phase::Start});
            }
            return 0;
        case WM_IME_COMPOSITION: {
            HIMC context = ImmGetContext(hwnd);
            if (context == nullptr) {
                return 0;
            }
            if ((lParam & GCS_COMPSTR) != 0 && onComposition_) {
                onComposition_({windowId_, readImeString(context, GCS_COMPSTR), CompositionInputEvent::Phase::Update});
            }
            if ((lParam & GCS_RESULTSTR) != 0) {
                const std::string committed = readImeString(context, GCS_RESULTSTR);
                if (!committed.empty() && onTextInput_) {
                    onTextInput_({windowId_, committed});
                }
                if (compositionOpen_ && onComposition_) {
                    onComposition_({windowId_, {}, CompositionInputEvent::Phase::End});
                }
                compositionOpen_ = false;
            }
            ImmReleaseContext(hwnd, context);
            return 0;
        }
        case WM_IME_ENDCOMPOSITION:
            if (compositionOpen_ && onComposition_) {
                onComposition_({windowId_, {}, CompositionInputEvent::Phase::End});
            }
            compositionOpen_ = false;
            return 0;
        case WM_IME_NOTIFY:
            if (wParam == IMN_OPENCANDIDATE || wParam == IMN_CHANGECANDIDATE) {
                updateImeWindows();
            }
            break;
        default:
            break;
        }
        return CallWindowProcW(previousWindowProc_, hwnd, message, wParam, lParam);
    }

    static LRESULT CALLBACK imeWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
    {
        auto* session = reinterpret_cast<GlfwTextInputSession*>(GetPropW(hwnd, propertyName()));
        if (session != nullptr && session->previousWindowProc_ != nullptr) {
            return session->handleImeMessage(hwnd, message, wParam, lParam);
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
#endif

    GLFWwindow* window_{nullptr};
    WindowId windowId_{0};
    RectF caretRect_{};
    std::string surroundingText_;
    std::size_t selectionStart_{0};
    std::size_t selectionEnd_{0};
    bool active_{false};
    bool compositionOpen_{false};
    std::function<void(const TextInputEvent&)> onTextInput_;
    std::function<void(const CompositionInputEvent&)> onComposition_;
#if defined(_WIN32)
    HWND nativeWindow_{nullptr};
    WNDPROC previousWindowProc_{nullptr};
#endif
};

// --- GlfwPlatformWindow ---

class GlfwPlatformWindow : public PlatformWindow {
public:
    GlfwPlatformWindow(GLFWwindow* window, WindowId id)
        : window_(window)
        , id_(id)
        , clipboard_(window)
        , cursorService_(window)
        , textInputSession_(window, id)
    {
        glfwSetWindowUserPointer(window, this);
        textInputSession_.setCallbacks(
            [this](const TextInputEvent& event) {
                requestRedraw();
                if (onTextInput) onTextInput(event);
            },
            [this](const CompositionInputEvent& event) {
                requestRedraw();
                if (onCompositionInput) onCompositionInput(event);
            });
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
        textMeasurer_ = std::make_unique<WhatsCanvasTextMeasurer>(surface_->canvas());
        textMeasurer_->installWindowsFallbackPolicy();
    }

    [[nodiscard]] GlfwRenderSurface& glfwSurface() noexcept { return *surface_; }
    [[nodiscard]] WhatsCanvasTextMeasurer& textMeasurer() noexcept { return *textMeasurer_; }

    static GlfwPlatformWindow* fromGlfw(GLFWwindow* window)
    {
        return static_cast<GlfwPlatformWindow*>(glfwGetWindowUserPointer(window));
    }

    // Event callbacks (set by the host/app to dispatch into the UI tree)
    std::function<void(const PointerEvent&)> onPointerEvent;
    std::function<void(const KeyEvent&)> onKeyEvent;
    std::function<void(const TextInputEvent&)> onTextInput;
    std::function<void(const CompositionInputEvent&)> onCompositionInput;
    std::function<void(bool)> onFocusChanged;

private:
    GLFWwindow* window_;
    WindowId id_;
    std::unique_ptr<GlfwRenderSurface> surface_;
    GlfwClipboard clipboard_;
    GlfwCursorService cursorService_;
    GlfwTextInputSession textInputSession_;
    std::unique_ptr<WhatsCanvasTextMeasurer> textMeasurer_;
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
    glfwWin->onCompositionInput = [&uiWindow](const CompositionInputEvent& event) {
        uiWindow.dispatchComposition(event);
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
            glfwWin->textMeasurer().setScaleFactor(m.scaleFactor);
            setTextMeasurer(&glfwWin->textMeasurer());
            win.layout();

            // Paint
            PaintContext ctx(glfwSurface.canvas(), m.scaleFactor);
            win.prepare(ctx);
            platformWin.surface().beginFrame();

            win.paint(ctx);

            platformWin.surface().endFrame();
            // WhatsCanvas only finalizes command execution/draw-call counts
            // at endFrame(). Sample after that boundary so FrameStats never
            // presents a pre-flush command count as completed GPU work.
            win.captureCompletedRendererStats(ctx);
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
