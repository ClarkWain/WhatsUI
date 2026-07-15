#if !defined(_WIN32)
#error "The native UI Automation smoke test is Windows-only"
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <objbase.h>
#include <UIAutomationClient.h>
#include <oleauto.h>

// Legacy Win32 headers expose generic drawing/window macros (notably
// WINDING). WhatsCanvas uses the same spellings for scoped path enums, so
// remove them before including WhatsUI's renderer-facing headers.
#ifdef CLOSE
#undef CLOSE
#endif
#ifdef WINDING
#undef WINDING
#endif

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "wui/glfw_platform.h"
#include "wui/ui.h"

namespace {

constexpr wchar_t kWindowTitle[] = L"WhatsUI native UIA smoke";

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* get() const noexcept { return value_; }
    T** put() noexcept
    {
        reset();
        return &value_;
    }
    T* operator->() const noexcept { return value_; }
    explicit operator bool() const noexcept { return value_ != nullptr; }

    void reset() noexcept
    {
        if (value_ != nullptr) {
            value_->Release();
            value_ = nullptr;
        }
    }

private:
    T* value_{nullptr};
};

class ScopedCom {
public:
    ScopedCom() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ~ScopedCom()
    {
        if (SUCCEEDED(result_)) CoUninitialize();
    }

    HRESULT result() const noexcept { return result_; }

private:
    HRESULT result_;
};

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void expectSucceeded(HRESULT result, const char* message)
{
    if (FAILED(result)) {
        char detail[192]{};
        std::snprintf(detail, sizeof(detail), "%s (HRESULT 0x%08lX)",
                      message, static_cast<unsigned long>(result));
        throw std::runtime_error(detail);
    }
}

std::wstring currentName(IUIAutomationElement& element)
{
    BSTR value = nullptr;
    expectSucceeded(element.get_CurrentName(&value), "UIA element name query failed");
    const std::wstring result = value != nullptr
        ? std::wstring(value, SysStringLen(value))
        : std::wstring{};
    SysFreeString(value);
    return result;
}

std::wstring currentFrameworkId(IUIAutomationElement& element)
{
    BSTR value = nullptr;
    expectSucceeded(element.get_CurrentFrameworkId(&value),
                    "UIA element framework-id query failed");
    const std::wstring result = value != nullptr
        ? std::wstring(value, SysStringLen(value))
        : std::wstring{};
    SysFreeString(value);
    return result;
}

bool hasInteractiveDesktop()
{
    // OpenInputDesktop can be denied to an otherwise interactive process by
    // desktop ACLs (and made this gate skip after it had already opened real
    // GLFW windows). Let GLFW creation be the authoritative availability
    // check; screen metrics only reject service/headless sessions early.
    return GetSystemMetrics(SM_CXSCREEN) > 0 && GetSystemMetrics(SM_CYSCREEN) > 0;
}

bool focusNativeWindow(HWND hwnd)
{
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        glfwPollEvents();
        if (GetForegroundWindow() == hwnd && GetFocus() == hwnd) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (std::chrono::steady_clock::now() < deadline);

    return false;
}

void expectBoundsMatchWindow(IUIAutomationElement& element, HWND hwnd)
{
    RECT actual{};
    expectSucceeded(element.get_CurrentBoundingRectangle(&actual),
                    "UIA root bounding rectangle query failed");

    RECT expected{};
    expect(GetWindowRect(hwnd, &expected) != FALSE, "GetWindowRect failed for the GLFW HWND");
    expect(actual.right > actual.left && actual.bottom > actual.top,
           "UIA root must expose non-empty screen bounds");

    // UIA and GetWindowRect may disagree by a small invisible-resize border on
    // some Windows versions.  Their centres and visible extents must still
    // describe the same top-level HWND.
    const double expectedCenterX = (static_cast<double>(expected.left) + expected.right) / 2.0;
    const double expectedCenterY = (static_cast<double>(expected.top) + expected.bottom) / 2.0;
    const double actualCenterX = (static_cast<double>(actual.left) + actual.right) / 2.0;
    const double actualCenterY = (static_cast<double>(actual.top) + actual.bottom) / 2.0;
    expect(std::fabs(actualCenterX - expectedCenterX) <= 8.0
               && std::fabs(actualCenterY - expectedCenterY) <= 8.0,
           "UIA root bounds must identify the GLFW top-level window");
}

void expectWhatsUiChildProvider(IUIAutomation& automation, IUIAutomationElement& root,
                                HWND hwnd, const RECT& expectedBounds)
{
    VARIANT expectedType{};
    expectedType.vt = VT_I4;
    expectedType.lVal = UIA_ButtonControlTypeId;

    ComPtr<IUIAutomationCondition> condition;
    expectSucceeded(automation.CreatePropertyCondition(UIA_ControlTypePropertyId,
                                                       expectedType,
                                                       condition.put()),
                    "Unable to create the UIA Button condition");

    VARIANT expectedName{};
    expectedName.vt = VT_BSTR;
    expectedName.bstrVal = SysAllocString(L"Native action");
    expect(expectedName.bstrVal != nullptr,
           "Unable to allocate the UIA action-name condition");
    ComPtr<IUIAutomationCondition> nameCondition;
    const HRESULT nameResult = automation.CreatePropertyCondition(
        UIA_NamePropertyId, expectedName, nameCondition.put());
    VariantClear(&expectedName);
    expectSucceeded(nameResult, "Unable to create the UIA action-name condition");

    ComPtr<IUIAutomationCondition> actionCondition;
    expectSucceeded(automation.CreateAndCondition(condition.get(), nameCondition.get(),
                                                  actionCondition.put()),
                    "Unable to combine the UIA action conditions");

    ComPtr<IUIAutomationElement> button;
    expectSucceeded(root.FindFirst(TreeScope_Descendants, actionCondition.get(), button.put()),
                    "UIA descendant lookup failed");
    expect(static_cast<bool>(button),
           "The native provider must project the WhatsUI Native action Button");

    CONTROLTYPEID controlType = 0;
    expectSucceeded(button->get_CurrentControlType(&controlType),
                    "UIA child control type query failed");
    expect(controlType == UIA_ButtonControlTypeId,
           "The projected WhatsUI action must expose the Button control type");
    expect(currentName(*button.get()) == L"Native action",
           "The projected WhatsUI action must retain its accessible name");
    expect(currentFrameworkId(*button.get()) == L"WhatsUI",
           "The projected WhatsUI action must identify the WhatsUI framework");

    RECT bounds{};
    expectSucceeded(button->get_CurrentBoundingRectangle(&bounds),
                    "UIA child bounding rectangle query failed");
    expect(bounds.right > bounds.left && bounds.bottom > bounds.top,
           "The projected WhatsUI action must expose non-empty bounds");

    RECT client{};
    expect(GetClientRect(hwnd, &client) != FALSE,
           "GetClientRect failed while validating the UIA child bounds");
    POINT clientOrigin{client.left, client.top};
    expect(ClientToScreen(hwnd, &clientOrigin) != FALSE,
           "ClientToScreen failed while validating the UIA child bounds");
    const LONG clientRight = clientOrigin.x + (client.right - client.left);
    const LONG clientBottom = clientOrigin.y + (client.bottom - client.top);
    expect(bounds.left >= clientOrigin.x - 1 && bounds.top >= clientOrigin.y - 1
               && bounds.right <= clientRight + 1 && bounds.bottom <= clientBottom + 1,
           "The projected WhatsUI action bounds must remain inside the HWND client area");
    expect(std::abs(bounds.left - expectedBounds.left) <= 2
               && std::abs(bounds.top - expectedBounds.top) <= 2
               && std::abs(bounds.right - expectedBounds.right) <= 2
               && std::abs(bounds.bottom - expectedBounds.bottom) <= 2,
           "The UIA child bounds must use the actual native-client/logical DPI ratio");
}

void queryNativeUia(HWND hwnd, bool validateFocus, RECT expectedButtonBounds)
{
    ScopedCom com;
    expectSucceeded(com.result(), "COM initialization failed for UI Automation");

    ComPtr<IUIAutomation> automation;
    expectSucceeded(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(automation.put())),
                    "Unable to create the Windows UI Automation client");

    ComPtr<IUIAutomationElement> root;
    expectSucceeded(automation->ElementFromHandle(hwnd, root.put()),
                    "IUIAutomation::ElementFromHandle failed for the GLFW HWND");
    expect(static_cast<bool>(root), "ElementFromHandle returned no UIA root");
    expect(currentName(*root.get()) == kWindowTitle,
           "The UIA root name must match the GLFW window title");

    CONTROLTYPEID rootType = 0;
    expectSucceeded(root->get_CurrentControlType(&rootType),
                    "UIA root control type query failed");
    expect(rootType == UIA_WindowControlTypeId,
           "The native GLFW root must expose the Window control type");
    expectBoundsMatchWindow(*root.get(), hwnd);

    if (validateFocus) {
        ComPtr<IUIAutomationElement> focused;
        expectSucceeded(automation->GetFocusedElement(focused.put()),
                        "UI Automation focused-element query failed");
        expect(static_cast<bool>(focused), "UI Automation returned no focused element");
        CONTROLTYPEID focusedType = 0;
        expectSucceeded(focused->get_CurrentControlType(&focusedType),
                        "Focused element control type query failed");
        expect(focusedType == UIA_ButtonControlTypeId,
               "The logically focused WhatsUI action must expose the Button control type");
        expect(currentName(*focused.get()) == L"Native action",
               "UI Automation focus must resolve to the named WhatsUI action");
        BOOL focusedState = FALSE;
        expectSucceeded(focused->get_CurrentHasKeyboardFocus(&focusedState),
                        "Focused element state query failed");
        expect(focusedState != FALSE,
               "The logically focused WhatsUI action must report UIA keyboard focus");
    }

    expectWhatsUiChildProvider(
        *automation.get(), *root.get(), hwnd, expectedButtonBounds);
}

void runUiaClientOffUiThread(HWND hwnd, bool validateFocus, RECT expectedButtonBounds)
{
    // Microsoft recommends keeping UIA client calls off the thread that owns
    // the provider HWND.  Pumping the GLFW/Win32 queue here both follows that
    // contract and makes a provider-side cross-thread deadlock a deterministic
    // timeout instead of hanging the complete CTest shard.
    std::packaged_task<void()> task([hwnd, validateFocus, expectedButtonBounds] {
        queryNativeUia(hwnd, validateFocus, expectedButtonBounds);
    });
    auto result = task.get_future();
    std::thread worker(std::move(task));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (result.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        glfwPollEvents();
        if (std::chrono::steady_clock::now() >= deadline) {
            std::fputs("FAIL: UI Automation client/provider exchange timed out\n", stderr);
            std::fflush(stderr);
            std::quick_exit(1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    worker.join();
    result.get();
}

void testNativeUiaRoot()
{
    auto host = wui::createGlfwPlatformHost();
    wui::UiApp app(std::move(host));
    auto& window = app.openWindow("WhatsUI native UIA smoke", {560.0f, 320.0f});
    window.setRoot(wui::ui::Column()
                       .padding(24)
                       .gap(12)
                       .children(wui::ui::Text("Native accessibility boundary"),
                                 wui::ui::Button("Native action"))
                       .intoNode());
    window.update();
    window.layout();
    expect(window.root() != nullptr && window.root()->children().size() == 2,
           "The native UIA fixture must retain its action node");
    window.focusManager().setFocused(window.root()->children()[1].get());
    // Focus is semantic state, not a layout invalidation. Republish the
    // immutable native snapshot after changing it so GetFocusedElement sees
    // the same state as the in-process accessibility projection.
    window.layout();
    window.platformWindow().show();

    GLFWwindow* glfwWindow = glfwGetCurrentContext();
    expect(glfwWindow != nullptr, "The GLFW UIA smoke test requires a current native window");
    HWND hwnd = glfwGetWin32Window(glfwWindow);
    expect(hwnd != nullptr && IsWindow(hwnd), "GLFW did not expose a valid Win32 HWND");
    const bool nativeFocusAcquired = focusNativeWindow(hwnd);
    // The GLFW focus callback changes whether retained framework focus is
    // exposed as native keyboard focus. Publish that state before UIA reads.
    window.layout();

    const auto snapshot = window.accessibilitySnapshot();
    const auto button = std::find_if(snapshot.begin(), snapshot.end(),
        [](const wui::AccessibilitySnapshotEntry& entry) {
            return entry.properties.role == wui::AccessibilityRole::Button
                && entry.properties.label == "Native action";
        });
    expect(button != snapshot.end() && button->properties.bounds.has_value(),
           "The native UIA fixture must expose logical Button bounds");
    RECT client{};
    expect(GetClientRect(hwnd, &client) != FALSE,
           "GetClientRect failed while preparing the DPI expectation");
    POINT origin{};
    expect(ClientToScreen(hwnd, &origin) != FALSE,
           "ClientToScreen failed while preparing the DPI expectation");
    const auto metrics = window.platformWindow().metrics();
    expect(metrics.logicalSize.width > 0.0f && metrics.logicalSize.height > 0.0f,
           "The native UIA fixture must expose a non-empty logical client size");
    const float scaleX = static_cast<float>(client.right - client.left)
        / metrics.logicalSize.width;
    const float scaleY = static_cast<float>(client.bottom - client.top)
        / metrics.logicalSize.height;
    const auto& logical = *button->properties.bounds;
    RECT expectedButtonBounds{
        static_cast<LONG>(std::lround(origin.x + logical.x * scaleX)),
        static_cast<LONG>(std::lround(origin.y + logical.y * scaleY)),
        static_cast<LONG>(std::lround(origin.x + (logical.x + logical.width) * scaleX)),
        static_cast<LONG>(std::lround(origin.y + (logical.y + logical.height) * scaleY))};

    runUiaClientOffUiThread(
        hwnd, nativeFocusAcquired, expectedButtonBounds);
}

} // namespace

int main()
{
    if (!hasInteractiveDesktop()) {
        std::fputs("SKIP: Windows interactive desktop unavailable\n", stderr);
        return 77;
    }

    try {
        testNativeUiaRoot();
        return 0;
    } catch (const std::exception& error) {
        const std::string_view message{error.what()};
        if (message.find("glfwInit() failed") != std::string_view::npos
            || message.find("glfwCreateWindow() failed") != std::string_view::npos) {
            std::fprintf(stderr, "SKIP: native GLFW/Windows desktop unavailable: %s\n", error.what());
            return 77;
        }
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
