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
#if defined(_WIN32)
#include "wui/windows_uia_provider.h"
#endif

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

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

namespace {

float windowContentScale(GLFWwindow* window) noexcept
{
#if defined(_WIN32)
    float xScale = 1.0f;
    float yScale = 1.0f;
    glfwGetWindowContentScale(window, &xScale, &yScale);
    if (xScale > 0.0f && std::isfinite(xScale)) {
        return xScale;
    }
#else
    (void)window;
#endif
    return 1.0f;
}

PointF logicalPointerPosition(GLFWwindow* window, double x, double y) noexcept
{
    const float scale = windowContentScale(window);
    return {static_cast<float>(x) / scale, static_cast<float>(y) / scale};
}

} // namespace

#if defined(_WIN32)
// DirectWrite currently rasterizes every requested text run through a WIC/D2D
// bitmap on the latest WhatsCanvas release. That gives us better Windows font
// hinting, but its no-cache implementation is too expensive for an interactive
// Todo frame. Keep the integration available for visual evaluation without
// making it the product default until WhatsCanvas supplies a retained text-run
// cache. The portable backend remains the safe default.
[[nodiscard]] bool directWriteOptInRequested() noexcept
{
    const char* value = std::getenv("WHATSUI_TEXT_BACKEND");
    return value != nullptr && std::string_view(value) == "directwrite";
}

// ClearType is deliberately a second explicit opt-in.  It is useful for
// visual comparison on a known opaque desktop surface, but RGB subpixel
// coverage is not generally safe after alpha compositing.
[[nodiscard]] bool clearTypeOptInRequested() noexcept
{
    const char* value = std::getenv("WHATSUI_TEXT_RENDER_MODE");
    return value != nullptr && std::string_view(value) == "cleartype";
}

// GLFW only reports a framebuffer scale greater than one when the process is
// DPI aware. Without this declaration Windows bitmap-scales the entire OpenGL
// window on a 125%/150% desktop, which makes even correctly rasterized text
// blurry. Do this before glfwInit creates any HWND. A host that already chose
// an awareness context simply rejects the call; that is safe and intentional.
void enablePerMonitorV2DpiAwareness() noexcept
{
#if defined(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
    (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#else
    (void)SetProcessDPIAware();
#endif
}
#endif

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
        if (directWriteOptInRequested()) {
            // ClearType remains an explicit diagnostic opt-in.  It must never
            // become the product default until the native Todo regression has
            // verified the complete command stream at fractional DPI.
            // WhatsCanvas reports a portable fallback if DirectWrite fails.
            directWriteActive_ = canvas_->setTextBackend(
                wsc::Canvas::TextBackend::DirectWrite,
                clearTypeOptInRequested() ? wsc::Canvas::TextRenderMode::ClearType
                                           : wsc::Canvas::TextRenderMode::Grayscale);
        }

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
        // Native visual diagnostics: when explicitly requested, retain the
        // exact Canvas framebuffer before it is handed to WGL.  This keeps a
        // real Todo frame inspectable even when desktop capture is blocked by
        // a different Windows session or by an overlapping native window.
        // It has no cost and no file-system effect in ordinary runs.
        if (const char* capturePath = std::getenv("WHATSUI_DEBUG_CAPTURE_PPM");
            capturePath != nullptr && capturePath[0] != '\0') {
            (void)canvas_->savePixelsPPM(capturePath);
        }
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
            // GLFW resizes the default framebuffer but intentionally does not
            // update OpenGL's viewport. Canvas::setSize updates its logical
            // projection; without this matching viewport update the new
            // projection is rasterised into the old lower-left viewport,
            // leaving a black band above the Todo UI after a window resize.
            using GlViewportProc = void (*)(int, int, int, int);
            if (const auto glViewport = reinterpret_cast<GlViewportProc>(glfwGetProcAddress("glViewport"))) {
                glViewport(0, 0, w, h);
            }
            canvas_->setSize(w, h);
            canvas_->resizeOutput(w, h);
            fbSize_ = framebufferSize;
        }
    }

    [[nodiscard]] wsc::Canvas& canvas() noexcept { return *canvas_; }

    // The Canvas framebuffer is physical pixels while WhatsUI layout remains
    // in logical coordinates.  Let Canvas carry this transform exactly once.
    void setDevicePixelRatio(float ratio) noexcept
    {
        canvas_->setDevicePixelRatio(ratio);
    }

    [[nodiscard]] bool isUsingDirectWrite() const noexcept { return directWriteActive_; }

private:
    GLFWwindow* window_{nullptr};
    std::unique_ptr<wsc::Canvas> canvas_;
    bool canPresent_{false};
    bool directWriteActive_{false};
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
        shutdown();
    }

    void shutdown() noexcept
    {
        if (currentCursor_) {
            glfwDestroyCursor(currentCursor_);
            currentCursor_ = nullptr;
        }
        currentShape_ = -1;
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
        accessibilityBridge_ = std::make_unique<windows::UiaSnapshotBridge>(nativeWindow_);

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
        shutdown();
    }

    // The Win32 subclass belongs to the GLFW HWND. It must be removed before
    // glfwDestroyWindow() invalidates that HWND; reverse member destruction is
    // too late because it follows the owning window's destructor body.
    void shutdown() noexcept
    {
#if defined(_WIN32)
        accessibilityBridge_.reset();
        if (nativeWindow_ != nullptr) {
            // Restore only while this is still our subclass. GLFW destroys the
            // HWND after the PlatformWindow members, so this is deterministic.
            if (reinterpret_cast<WNDPROC>(GetWindowLongPtrW(nativeWindow_, GWLP_WNDPROC)) == &imeWindowProc) {
                SetWindowLongPtrW(nativeWindow_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previousWindowProc_));
            }
            RemovePropW(nativeWindow_, propertyName());
            nativeWindow_ = nullptr;
            previousWindowProc_ = nullptr;
        }
#endif
        active_ = false;
        compositionOpen_ = false;
        onTextInput_ = {};
        onComposition_ = {};
    }

    void setCallbacks(std::function<void(const TextInputEvent&)> onTextInput,
                      std::function<void(const CompositionInputEvent&)> onComposition)
    {
        onTextInput_ = std::move(onTextInput);
        onComposition_ = std::move(onComposition);
    }

#if defined(_WIN32)
    void publishAccessibilitySnapshot(AccessibilitySnapshot snapshot, WindowMetrics metrics)
    {
        if (accessibilityBridge_) {
            accessibilityBridge_->publish(std::move(snapshot), metrics);
        }
    }

    void setAccessibilityActionHandler(AccessibilityActionHandler handler)
    {
        if (accessibilityBridge_) {
            accessibilityBridge_->setActionCallback(std::move(handler));
        }
    }
#endif

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
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(window_, &windowWidth, &windowHeight);
        const float contentScale = windowContentScale(window_);
        const PointF point = projectLogicalCaretToClientPixels(
            caretRect_,
            {static_cast<float>(windowWidth) / contentScale,
             static_cast<float>(windowHeight) / contentScale},
            {static_cast<float>(client.right - client.left), static_cast<float>(client.bottom - client.top)});
        return {static_cast<LONG>(point.x), static_cast<LONG>(point.y)};
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
        if (accessibilityBridge_ && accessibilityBridge_->handleActionMessage(message)) {
            return 0;
        }
        if (message == WM_GETOBJECT && static_cast<LONG>(lParam) == UiaRootObjectId) {
            if (accessibilityBridge_) {
                if (const auto result = accessibilityBridge_->handleWmGetObject(wParam, lParam)) {
                    return *result;
                }
            }
        }
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
    std::unique_ptr<windows::UiaSnapshotBridge> accessibilityBridge_;
#endif
};

// --- GlfwPlatformWindow ---

class GlfwPlatformWindow : public PlatformWindow {
public:
    GlfwPlatformWindow(GLFWwindow* window, WindowId id, std::string title)
        : window_(window)
        , id_(id)
        , title_(std::move(title))
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
            // These resources access the active GL context / Win32 HWND while
            // being destroyed. Tear them down before GLFW destroys either.
            glfwMakeContextCurrent(window_);
            textInputSession_.shutdown();
            textMeasurer_.reset();
            surface_.reset();
            cursorService_.shutdown();
            glfwSetWindowUserPointer(window_, nullptr);
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
        const float contentScale = windowContentScale(window_);
#if defined(_WIN32)
        // Win32 GLFW window/framebuffer sizes are both physical pixels.  The
        // monitor content scale is the logical-DIP conversion; fb/window is
        // always 1 and silently disabled 125%/150% text rasterization.
        m.logicalSize = {static_cast<float>(w) / contentScale,
                         static_cast<float>(h) / contentScale};
        m.scaleFactor = contentScale;
#else
        m.logicalSize = {static_cast<float>(w), static_cast<float>(h)};
        m.scaleFactor = w > 0 ? static_cast<float>(fbw) / static_cast<float>(w) : 1.0f;
#endif
        m.framebufferSize = {static_cast<float>(fbw), static_cast<float>(fbh)};
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
        title_.assign(title.data(), title.size());
        glfwSetWindowTitle(window_, title_.c_str());
    }

    [[nodiscard]] std::string title() const override { return title_; }

    void requestRedraw() override { needsRedraw_ = true; }

    void publishAccessibilitySnapshot(AccessibilitySnapshot snapshot) override
    {
#if defined(_WIN32)
        textInputSession_.publishAccessibilitySnapshot(std::move(snapshot), metrics());
#else
        (void)snapshot;
#endif
    }

    void setAccessibilityActionHandler(AccessibilityActionHandler handler) override
    {
#if defined(_WIN32)
        textInputSession_.setAccessibilityActionHandler(std::move(handler));
#else
        (void)handler;
#endif
    }

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
    std::string title_;
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
#if defined(_WIN32)
        enablePerMonitorV2DpiAwareness();
#endif
        if (!glfwInit()) {
            throw std::runtime_error("glfwInit() failed");
        }
#if defined(_WIN32) && defined(GLFW_SCALE_TO_MONITOR)
        // Treat the requested window size as logical DIPs. GLFW expands the
        // Win32 client area to physical pixels for the current monitor.
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif
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

        auto window = std::make_unique<GlfwPlatformWindow>(glfwWindow, nextWindowId_++, title);
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
            const bool closedWindowRemoved = discardClosedWindows();

            if (windows_.empty()) {
                // Release the owning UiWindow while GLFW is still alive. The
                // frame callback removes the matching UI owner after this
                // host has discarded its non-owning raw pointer; deferring
                // it to UiApp teardown can destroy GL/IME resources after
                // the host begins shutdown.
                if (frameCallback_ && closedWindowRemoved) {
                    frameCallback_();
                }
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

    // Raw platform-window pointers must be discarded before UiApp destroys
    // their owners. Frame callbacks may close a window themselves (for
    // example an animation or a dialog action), so they need the same safe
    // boundary that the next event-loop iteration normally provides.
    bool discardClosedWindows()
    {
        const auto previousWindowCount = windows_.size();
        windows_.erase(
            std::remove_if(windows_.begin(), windows_.end(),
                           [](GlfwPlatformWindow* w) { return !w->isOpen(); }),
            windows_.end());
        return windows_.size() != previousWindowCount;
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
            event.position = logicalPointerPosition(w, x, y);
            event.modifiers = modifiersFor(w);
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
            event.position = logicalPointerPosition(w, x, y);
            event.modifiers = modifiersFor(w);
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
            event.position = logicalPointerPosition(w, x, y);
            event.modifiers = modifiersFromGlfw(mods);
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
            event.position = logicalPointerPosition(w, x, y);
            event.modifiers = modifiersFor(w);
            // GLFW reports abstract wheel "steps". Convert to the framework's
            // logical-pixel convention so all backends expose the same API.
            event.scrollDelta = {static_cast<float>(xoffset * 40.0), static_cast<float>(yoffset * 40.0)};
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

        glfwSetWindowPosCallback(window, [](GLFWwindow* w, int, int) {
            auto* pw = GlfwPlatformWindow::fromGlfw(w);
            if (!pw) return;
            // Native accessibility bounds are screen-relative. A pure move
            // does not resize or dirty layout, but it must republish the
            // immutable snapshot with the new client origin.
            pw->requestRedraw();
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

#if defined(_WIN32)
        glfwSetWindowContentScaleCallback(window, [](GLFWwindow* w, float, float) {
            auto* pw = GlfwPlatformWindow::fromGlfw(w);
            if (!pw) return;
            // The framebuffer callback owns physical surface resize. This
            // callback invalidates logical layout/text even when the driver
            // reports the same framebuffer dimensions during monitor moves.
            pw->requestRedraw();
        });
#endif

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
    host->setFrameCallback([&app, host, lastFrame = std::chrono::steady_clock::now()]() mutable {
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

        // An animation can close a native peer during this callback. Purge
        // the host's non-owning pointer first, then release the UI owner.
        // This keeps both the current and the next event-loop iteration free
        // of stale native-window pointers.
        host->discardClosedWindows();
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
            // WhatsCanvas now owns DPR in its root transform.  Text layout
            // metrics therefore stay in logical coordinates while drawText
            // is rasterized at the physical DPR by Canvas.
            glfwSurface.setDevicePixelRatio(m.scaleFactor);
            glfwWin->textMeasurer().setScaleFactor(1.0f);
            setTextMeasurer(&glfwWin->textMeasurer());
            win.layout();

            // Paint
            PaintContext ctx(glfwSurface.canvas(), m.scaleFactor, true);
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
