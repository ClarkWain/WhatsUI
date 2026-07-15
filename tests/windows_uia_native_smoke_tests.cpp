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
    ComPtr(ComPtr&& other) noexcept : value_(std::exchange(other.value_, nullptr)) {}
    ComPtr& operator=(ComPtr&& other) noexcept
    {
        if (this != &other) {
            reset();
            value_ = std::exchange(other.value_, nullptr);
        }
        return *this;
    }

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

ComPtr<IUIAutomationElement> findNamedControl(IUIAutomation& automation,
                                              IUIAutomationElement& root,
                                              CONTROLTYPEID type,
                                              const wchar_t* name)
{
    VARIANT expectedType{};
    expectedType.vt = VT_I4;
    expectedType.lVal = type;
    ComPtr<IUIAutomationCondition> typeCondition;
    expectSucceeded(automation.CreatePropertyCondition(UIA_ControlTypePropertyId,
                                                       expectedType,
                                                       typeCondition.put()),
                    "Unable to create the UIA control-type condition");

    VARIANT expectedName{};
    expectedName.vt = VT_BSTR;
    expectedName.bstrVal = SysAllocString(name);
    expect(expectedName.bstrVal != nullptr,
           "Unable to allocate the UIA control-name condition");
    ComPtr<IUIAutomationCondition> nameCondition;
    const HRESULT nameResult = automation.CreatePropertyCondition(
        UIA_NamePropertyId, expectedName, nameCondition.put());
    VariantClear(&expectedName);
    expectSucceeded(nameResult, "Unable to create the UIA control-name condition");

    ComPtr<IUIAutomationCondition> combined;
    expectSucceeded(automation.CreateAndCondition(typeCondition.get(), nameCondition.get(),
                                                  combined.put()),
                    "Unable to combine the UIA named-control conditions");

    ComPtr<IUIAutomationElement> element;
    expectSucceeded(root.FindFirst(TreeScope_Descendants, combined.get(), element.put()),
                    "UIA named-control lookup failed");
    return element;
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
    auto button = findNamedControl(
        automation, root, UIA_ButtonControlTypeId, L"Native action");
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

struct NativeActionState {
    int invokeCount{0};
    bool checked{false};
    std::string value;
    std::thread::id uiThread{std::this_thread::get_id()};
    std::thread::id invokeThread;
    std::thread::id toggleThread;
    std::thread::id valueThread;
};

void invokeNativeUiaActions(HWND hwnd)
{
    ScopedCom com;
    expectSucceeded(com.result(), "COM initialization failed for UIA actions");
    ComPtr<IUIAutomation> automation;
    expectSucceeded(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(automation.put())),
                    "Unable to create the UI Automation action client");
    ComPtr<IUIAutomationElement> root;
    expectSucceeded(automation->ElementFromHandle(hwnd, root.put()),
                    "ElementFromHandle failed for UIA actions");

    auto button = findNamedControl(
        *automation.get(), *root.get(), UIA_ButtonControlTypeId, L"Native action");
    auto checkbox = findNamedControl(
        *automation.get(), *root.get(), UIA_CheckBoxControlTypeId, L"Native toggle");
    auto textInput = findNamedControl(
        *automation.get(), *root.get(), UIA_EditControlTypeId, L"Native value");
    expect(static_cast<bool>(button) && static_cast<bool>(checkbox) && static_cast<bool>(textInput),
           "The native provider must project the actionable UIA fixture controls");

    ComPtr<IUIAutomationInvokePattern> invokePattern;
    expectSucceeded(button->GetCurrentPatternAs(
                        UIA_InvokePatternId, __uuidof(IUIAutomationInvokePattern),
                        reinterpret_cast<void**>(invokePattern.put())),
                    "Native action must expose the UIA Invoke pattern");
    expect(static_cast<bool>(invokePattern),
           "Native action returned no UIA Invoke pattern provider");
    expectSucceeded(invokePattern->Invoke(), "UIA Invoke failed for Native action");

    ComPtr<IUIAutomationTogglePattern> togglePattern;
    expectSucceeded(checkbox->GetCurrentPatternAs(
                        UIA_TogglePatternId, __uuidof(IUIAutomationTogglePattern),
                        reinterpret_cast<void**>(togglePattern.put())),
                    "Native toggle must expose the UIA Toggle pattern");
    expect(static_cast<bool>(togglePattern),
           "Native toggle returned no UIA Toggle pattern provider");
    expectSucceeded(togglePattern->Toggle(), "UIA Toggle failed for Native toggle");

    ComPtr<IUIAutomationValuePattern> valuePattern;
    expectSucceeded(textInput->GetCurrentPatternAs(
                        UIA_ValuePatternId, __uuidof(IUIAutomationValuePattern),
                        reinterpret_cast<void**>(valuePattern.put())),
                    "Native value must expose the UIA Value pattern");
    expect(static_cast<bool>(valuePattern),
           "Native value returned no UIA Value pattern provider");
    expectSucceeded(valuePattern->SetValue(L"Updated through UIA"),
                    "UIA SetValue failed for Native value");
    // Keep this last: UI Automation may focus an edit before setting Value.
    expectSucceeded(checkbox->SetFocus(),
                    "UIA SetFocus failed for Native toggle");

    // Keep the original COM elements/patterns alive across the UI-thread
    // action and subsequent snapshot publication. They must resolve the
    // bridge's latest immutable model rather than remaining frozen forever.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    bool observedToggle = false;
    bool observedValue = false;
    do {
        ToggleState state = ToggleState_Indeterminate;
        if (SUCCEEDED(togglePattern->get_CurrentToggleState(&state))) {
            observedToggle = state == ToggleState_On;
        }
        BSTR value = nullptr;
        if (SUCCEEDED(valuePattern->get_CurrentValue(&value))) {
            const std::wstring current = value != nullptr
                ? std::wstring(value, SysStringLen(value)) : std::wstring{};
            observedValue = current == L"Updated through UIA";
        }
        SysFreeString(value);
        if (observedToggle && observedValue) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    expect(observedToggle && observedValue,
           "Retained UIA patterns must resolve the latest published snapshot");
}

void expectNativeUiaActionState(HWND hwnd)
{
    ScopedCom com;
    expectSucceeded(com.result(), "COM initialization failed for UIA action verification");
    ComPtr<IUIAutomation> automation;
    expectSucceeded(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(automation.put())),
                    "Unable to create the UI Automation verification client");
    ComPtr<IUIAutomationElement> root;
    expectSucceeded(automation->ElementFromHandle(hwnd, root.put()),
                    "ElementFromHandle failed for UIA action verification");

    auto checkbox = findNamedControl(
        *automation.get(), *root.get(), UIA_CheckBoxControlTypeId, L"Native toggle");
    auto textInput = findNamedControl(
        *automation.get(), *root.get(), UIA_EditControlTypeId, L"Native value");
    expect(static_cast<bool>(checkbox) && static_cast<bool>(textInput),
           "Updated native controls must remain discoverable through UIA");

    ComPtr<IUIAutomationTogglePattern> togglePattern;
    expectSucceeded(checkbox->GetCurrentPatternAs(
                        UIA_TogglePatternId, __uuidof(IUIAutomationTogglePattern),
                        reinterpret_cast<void**>(togglePattern.put())),
                    "Updated Native toggle must retain the UIA Toggle pattern");
    expect(static_cast<bool>(togglePattern),
           "Updated Native toggle returned no UIA Toggle pattern provider");
    ToggleState toggleState = ToggleState_Indeterminate;
    expectSucceeded(togglePattern->get_CurrentToggleState(&toggleState),
                    "Unable to read the updated UIA toggle state");
    expect(toggleState == ToggleState_On,
           "Native toggle must publish its post-action checked state");

    ComPtr<IUIAutomationValuePattern> valuePattern;
    expectSucceeded(textInput->GetCurrentPatternAs(
                        UIA_ValuePatternId, __uuidof(IUIAutomationValuePattern),
                        reinterpret_cast<void**>(valuePattern.put())),
                    "Updated Native value must retain the UIA Value pattern");
    expect(static_cast<bool>(valuePattern),
           "Updated Native value returned no UIA Value pattern provider");
    BSTR value = nullptr;
    expectSucceeded(valuePattern->get_CurrentValue(&value),
                    "Unable to read the updated UIA text value");
    const std::wstring current = value != nullptr
        ? std::wstring(value, SysStringLen(value))
        : std::wstring{};
    SysFreeString(value);
    expect(current == L"Updated through UIA",
           "Native value must publish its post-action text");
}

void pumpNativeUi(wui::UiWindow& window)
{
    glfwPollEvents();
    window.update();
    window.layout();
}

template <typename Work>
void runUiaWorkOffUiThread(wui::UiWindow& window, Work work, const char* timeoutMessage)
{
    std::packaged_task<void()> task(std::move(work));
    auto result = task.get_future();
    std::thread worker(std::move(task));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (result.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        pumpNativeUi(window);
        if (std::chrono::steady_clock::now() >= deadline) {
            std::fprintf(stderr, "FAIL: %s\n", timeoutMessage);
            std::fflush(stderr);
            std::quick_exit(1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    worker.join();
    result.get();
}

void exerciseNativeUiaActions(wui::UiWindow& window, HWND hwnd,
                              const NativeActionState& state)
{
    runUiaWorkOffUiThread(window, [hwnd] { invokeNativeUiaActions(hwnd); },
                          "UIA action dispatch timed out");
    // The last synchronous provider call can complete after the message-pump
    // iteration that published the previous action's state.
    pumpNativeUi(window);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((state.invokeCount != 1 || !state.checked || state.value != "Updated through UIA")
           && std::chrono::steady_clock::now() < deadline) {
        pumpNativeUi(window);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    expect(state.invokeCount == 1,
           "UIA Invoke must execute the Button callback exactly once on the UI thread");
    expect(state.checked,
           "UIA Toggle must update the Checkbox through its UI-thread callback");
    expect(state.value == "Updated through UIA",
           "UIA SetValue must update TextInput through its UI-thread callback");
    expect(state.invokeThread == state.uiThread
               && state.toggleThread == state.uiThread
               && state.valueThread == state.uiThread,
           "Native UIA actions must execute control callbacks on the GLFW UI thread");
    std::size_t focusedIndex = 999;
    if (window.root()) {
        for (std::size_t i = 0; i < window.root()->children().size(); ++i) {
            if (window.focusManager().focused() == window.root()->children()[i].get()) focusedIndex = i;
        }
    }
    expect(window.root() != nullptr && window.root()->children().size() > 2
               && focusedIndex == 2,
           "UIA SetFocus must update the WhatsUI focus manager");

    // A fresh client must observe the same state as the retained providers.
    runUiaWorkOffUiThread(window, [hwnd] { expectNativeUiaActionState(hwnd); },
                          "UIA post-action state query timed out");
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
    NativeActionState actionState;
    window.setRoot(wui::ui::Column()
                       .padding(24)
                       .gap(12)
                       .children(wui::ui::Text("Native accessibility boundary"),
                                 wui::ui::Button("Native action").accessibilityId("native.action").onClick([&actionState] {
                                     ++actionState.invokeCount;
                                     actionState.invokeThread = std::this_thread::get_id();
                                 }),
                                 wui::ui::Checkbox("Native toggle").accessibilityId("native.toggle").onChange(
                                     [&actionState](bool checked) {
                                         actionState.checked = checked;
                                         actionState.toggleThread = std::this_thread::get_id();
                                     }),
                                 wui::ui::TextField("Native value").accessibilityId("native.value").onChange(
                                     [&actionState](const std::string& value) {
                                         actionState.value = value;
                                         actionState.valueThread = std::this_thread::get_id();
                                     }))
                       .intoNode());
    window.update();
    window.layout();
    expect(window.root() != nullptr && window.root()->children().size() == 4,
           "The native UIA fixture must retain its action controls");
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
    exerciseNativeUiaActions(window, hwnd, actionState);
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
