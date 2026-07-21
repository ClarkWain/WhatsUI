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
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <memory>
#include <mutex>
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

std::wstring currentItemStatus(IUIAutomationElement& element)
{
    BSTR value = nullptr;
    expectSucceeded(element.get_CurrentItemStatus(&value),
                    "UIA element item-status query failed");
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

void expectNativeFormAndBusyProperties(IUIAutomation& automation,
                                       IUIAutomationElement& root)
{
    const struct RequiredControl {
        CONTROLTYPEID type;
        const wchar_t* name;
    } requiredControls[]{
        {UIA_CheckBoxControlTypeId, L"Native toggle"},
        {UIA_GroupControlTypeId, L"Native choices"},
        {UIA_CheckBoxControlTypeId, L"Native required switch"},
    };
    for (const auto& expected : requiredControls) {
        auto element = findNamedControl(
            automation, root, expected.type, expected.name);
        expect(static_cast<bool>(element),
               "Required WhatsUI control must be present in the native UIA tree");
        BOOL required = FALSE;
        expectSucceeded(element->get_CurrentIsRequiredForForm(&required),
                        "UIA IsRequiredForForm property query failed");
        expect(required != FALSE,
               "Required Checkbox/RadioGroup/Switch must expose UIA_IsRequiredForForm=true");
    }

    auto progress = findNamedControl(
        automation, root, UIA_ProgressBarControlTypeId, L"Native busy progress");
    expect(static_cast<bool>(progress),
           "Indeterminate ProgressBar must be present in the native UIA tree");
    expect(currentItemStatus(*progress.get()) == L"Busy",
           "Indeterminate ProgressBar must expose UIA_ItemStatus='Busy'");
    ComPtr<IUIAutomationRangeValuePattern> range;
    (void)progress->GetCurrentPatternAs(
        UIA_RangeValuePatternId, __uuidof(IUIAutomationRangeValuePattern),
        reinterpret_cast<void**>(range.put()));
    // UI Automation clients may report either an unavailable-pattern HRESULT
    // or S_OK with a null pattern pointer for an absent optional pattern.
    // The returned COM provider is the observable capability.
    expect(!range,
           "Indeterminate ProgressBar must not expose the UIA RangeValue pattern");
}

void expectRangeValueProperties(IUIAutomation& automation,
                                IUIAutomationElement& root)
{
    auto slider = findNamedControl(
        automation, root, UIA_SliderControlTypeId, L"Native range slider");
    auto progress = findNamedControl(
        automation, root, UIA_ProgressBarControlTypeId, L"Native determinate progress");
    expect(static_cast<bool>(slider) && static_cast<bool>(progress),
           "The native UIA fixture must project the Slider and determinate ProgressBar");

    const auto verify = [](IUIAutomationElement& element, double expectedMinimum,
                           double expectedMaximum, double expectedValue,
                           double expectedSmallChange, bool readOnly,
                           const char* description) {
        ComPtr<IUIAutomationRangeValuePattern> range;
        expectSucceeded(element.GetCurrentPatternAs(
                            UIA_RangeValuePatternId,
                            __uuidof(IUIAutomationRangeValuePattern),
                            reinterpret_cast<void**>(range.put())),
                        description);
        expect(static_cast<bool>(range), "UIA RangeValue returned no pattern provider");
        double value = 0.0;
        double minimum = 0.0;
        double maximum = 0.0;
        double smallChange = 0.0;
        BOOL actualReadOnly = FALSE;
        expectSucceeded(range->get_CurrentValue(&value), "Unable to read UIA RangeValue.Value");
        expectSucceeded(range->get_CurrentMinimum(&minimum), "Unable to read UIA RangeValue.Minimum");
        expectSucceeded(range->get_CurrentMaximum(&maximum), "Unable to read UIA RangeValue.Maximum");
        expectSucceeded(range->get_CurrentSmallChange(&smallChange), "Unable to read UIA RangeValue.SmallChange");
        expectSucceeded(range->get_CurrentIsReadOnly(&actualReadOnly), "Unable to read UIA RangeValue.IsReadOnly");
        expect(std::fabs(value - expectedValue) < 0.001
                   && std::fabs(minimum - expectedMinimum) < 0.001
                   && std::fabs(maximum - expectedMaximum) < 0.001
                   && std::fabs(smallChange - expectedSmallChange) < 0.001,
               "Native RangeValue properties must preserve the WhatsUI numeric range");
        expect((actualReadOnly != FALSE) == readOnly,
               "Native RangeValue IsReadOnly must preserve control semantics");
    };

    verify(*slider.get(), 10.0, 90.0, 40.0, 5.0, false,
           "Native Slider must expose UIA RangeValue");
    verify(*progress.get(), 0.0, 100.0, 60.0, 0.0, true,
           "Native determinate ProgressBar must expose UIA RangeValue");
}

void expectSelectionPatterns(IUIAutomation& automation, IUIAutomationElement& root)
{
    auto group = findNamedControl(
        automation, root, UIA_GroupControlTypeId, L"Native choices");
    auto first = findNamedControl(
        automation, root, UIA_RadioButtonControlTypeId, L"First choice");
    auto second = findNamedControl(
        automation, root, UIA_RadioButtonControlTypeId, L"Second choice");
    expect(static_cast<bool>(group) && static_cast<bool>(first) && static_cast<bool>(second),
           "Native UIA fixture must expose the RadioGroup and its RadioButton children");

    ComPtr<IUIAutomationSelectionPattern> selection;
    expectSucceeded(group->GetCurrentPatternAs(
                        UIA_SelectionPatternId, __uuidof(IUIAutomationSelectionPattern),
                        reinterpret_cast<void**>(selection.put())),
                    "RadioGroup must expose the UIA Selection pattern");
    expect(static_cast<bool>(selection),
           "RadioGroup returned no UIA Selection pattern provider");
    BOOL multiple = TRUE;
    BOOL required = FALSE;
    expectSucceeded(selection->get_CurrentCanSelectMultiple(&multiple),
                    "Unable to read UIA Selection.CanSelectMultiple");
    expectSucceeded(selection->get_CurrentIsSelectionRequired(&required),
                    "Unable to read UIA Selection.IsSelectionRequired");
    expect(multiple == FALSE && required != FALSE,
           "RadioGroup must retain its exclusive, required selection policy");

    ComPtr<IUIAutomationSelectionItemPattern> firstItem;
    expectSucceeded(first->GetCurrentPatternAs(
                        UIA_SelectionItemPatternId,
                        __uuidof(IUIAutomationSelectionItemPattern),
                        reinterpret_cast<void**>(firstItem.put())),
                    "RadioButton must expose the UIA SelectionItem pattern");
    expect(static_cast<bool>(firstItem),
           "RadioButton returned no UIA SelectionItem pattern provider");
    ComPtr<IUIAutomationTogglePattern> radioToggle;
    (void)first->GetCurrentPatternAs(
        UIA_TogglePatternId, __uuidof(IUIAutomationTogglePattern),
        reinterpret_cast<void**>(radioToggle.put()));
    expect(!radioToggle,
           "RadioButton must use SelectionItem rather than incorrectly exposing UIA Toggle");
    BOOL firstSelected = FALSE;
    expectSucceeded(firstItem->get_CurrentIsSelected(&firstSelected),
                    "Unable to read UIA SelectionItem.IsSelected");
    expect(firstSelected != FALSE,
           "The RadioGroup's initial selected option must be reflected by SelectionItem");

    ComPtr<IUIAutomationElement> container;
    expectSucceeded(firstItem->get_CurrentSelectionContainer(container.put()),
                    "Unable to read UIA SelectionItem.SelectionContainer");
    expect(static_cast<bool>(container) && currentName(*container.get()) == L"Native choices",
           "RadioButton SelectionItem must resolve its owning RadioGroup");
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
    expectNativeFormAndBusyProperties(*automation.get(), *root.get());
    expectRangeValueProperties(*automation.get(), *root.get());
    expectSelectionPatterns(*automation.get(), *root.get());
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

struct NativeEventState {
    std::mutex mutex;
    std::condition_variable changed;
    int togglePropertyChanges{0};
    int valuePropertyChanges{0};
    int enabledPropertyChanges{0};
    int namePropertyChanges{0};
    int boundsPropertyChanges{0};
    int valueFocusChanges{0};
    int structureChanges{0};
    RECT latestBounds{};
    bool releaseSubscriber{false};

    bool complete() const noexcept
    {
        return togglePropertyChanges > 0 && valuePropertyChanges > 0
            && enabledPropertyChanges > 0 && namePropertyChanges > 0
            && boundsPropertyChanges > 0
            && valueFocusChanges > 0 && structureChanges > 0;
    }

    int totalEvents() const noexcept
    {
        return togglePropertyChanges + valuePropertyChanges
            + enabledPropertyChanges + namePropertyChanges
            + boundsPropertyChanges + valueFocusChanges + structureChanges;
    }
};

template <typename Interface>
class UiaEventHandlerBase : public Interface {
public:
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = --references_;
        if (remaining == 0) delete this;
        return remaining;
    }

protected:
    explicit UiaEventHandlerBase(NativeEventState& state) : state_(state) {}
    virtual ~UiaEventHandlerBase() = default;

    NativeEventState& state_;

private:
    std::atomic<ULONG> references_{1};
};

class NativePropertyChangedHandler final
    : public UiaEventHandlerBase<IUIAutomationPropertyChangedEventHandler> {
public:
    explicit NativePropertyChangedHandler(NativeEventState& state)
        : UiaEventHandlerBase(state) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) override
    {
        if (result == nullptr) return E_POINTER;
        *result = nullptr;
        if (iid == __uuidof(IUnknown)
            || iid == __uuidof(IUIAutomationPropertyChangedEventHandler)) {
            *result = static_cast<IUIAutomationPropertyChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(
        IUIAutomationElement* sender, PROPERTYID propertyId, VARIANT newValue) override
    {
        int* counter = nullptr;
        {
            std::lock_guard lock(state_.mutex);
            if (propertyId == UIA_ToggleToggleStatePropertyId
                && newValue.vt == VT_I4 && newValue.lVal == ToggleState_Off) {
                counter = &state_.togglePropertyChanges;
            } else if (propertyId == UIA_ValueValuePropertyId
                       && newValue.vt == VT_BSTR && newValue.bstrVal != nullptr
                       && std::wstring_view{newValue.bstrVal,
                                           SysStringLen(newValue.bstrVal)}
                           == L"Event value") {
                counter = &state_.valuePropertyChanges;
            } else if (propertyId == UIA_IsEnabledPropertyId
                       && newValue.vt == VT_BOOL
                       && newValue.boolVal == VARIANT_FALSE) {
                counter = &state_.enabledPropertyChanges;
            } else if (propertyId == UIA_NamePropertyId
                       && newValue.vt == VT_BSTR && newValue.bstrVal != nullptr
                       && std::wstring_view{newValue.bstrVal,
                                           SysStringLen(newValue.bstrVal)}
                           == L"Native toggle renamed") {
                counter = &state_.namePropertyChanges;
            }
            if (counter != nullptr) ++*counter;
        }
        if (counter != nullptr) state_.changed.notify_all();
        return S_OK;
    }
};

class RootBoundsChangedHandler final
    : public UiaEventHandlerBase<IUIAutomationPropertyChangedEventHandler> {
public:
    explicit RootBoundsChangedHandler(NativeEventState& state)
        : UiaEventHandlerBase(state) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) override
    {
        if (result == nullptr) return E_POINTER;
        *result = nullptr;
        if (iid == __uuidof(IUnknown)
            || iid == __uuidof(IUIAutomationPropertyChangedEventHandler)) {
            *result = static_cast<IUIAutomationPropertyChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(
        IUIAutomationElement*, PROPERTYID propertyId, VARIANT newValue) override
    {
        if (propertyId != UIA_BoundingRectanglePropertyId
            || newValue.vt != (VT_ARRAY | VT_R8) || newValue.parray == nullptr) {
            return S_OK;
        }
        double* parts = nullptr;
        if (FAILED(SafeArrayAccessData(
                newValue.parray, reinterpret_cast<void**>(&parts)))) {
            return S_OK;
        }
        const RECT bounds{
            static_cast<LONG>(std::lround(parts[0])),
            static_cast<LONG>(std::lround(parts[1])),
            static_cast<LONG>(std::lround(parts[0] + parts[2])),
            static_cast<LONG>(std::lround(parts[1] + parts[3]))};
        SafeArrayUnaccessData(newValue.parray);
        {
            std::lock_guard lock(state_.mutex);
            state_.latestBounds = bounds;
            ++state_.boundsPropertyChanges;
        }
        state_.changed.notify_all();
        return S_OK;
    }
};

class FocusChangedHandler final
    : public UiaEventHandlerBase<IUIAutomationFocusChangedEventHandler> {
public:
    explicit FocusChangedHandler(NativeEventState& state)
        : UiaEventHandlerBase(state) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) override
    {
        if (result == nullptr) return E_POINTER;
        *result = nullptr;
        if (iid == __uuidof(IUnknown)
            || iid == __uuidof(IUIAutomationFocusChangedEventHandler)) {
            *result = static_cast<IUIAutomationFocusChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(IUIAutomationElement*) override
    {
        {
            std::lock_guard lock(state_.mutex);
            ++state_.valueFocusChanges;
        }
        state_.changed.notify_all();
        return S_OK;
    }
};

class StructureChangedHandler final
    : public UiaEventHandlerBase<IUIAutomationStructureChangedEventHandler> {
public:
    explicit StructureChangedHandler(NativeEventState& state)
        : UiaEventHandlerBase(state) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) override
    {
        if (result == nullptr) return E_POINTER;
        *result = nullptr;
        if (iid == __uuidof(IUnknown)
            || iid == __uuidof(IUIAutomationStructureChangedEventHandler)) {
            *result = static_cast<IUIAutomationStructureChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE HandleStructureChangedEvent(
        IUIAutomationElement*, StructureChangeType changeType, SAFEARRAY*) override
    {
        if (changeType != StructureChangeType_ChildAdded
            && changeType != StructureChangeType_ChildrenInvalidated) {
            return S_OK;
        }
        {
            std::lock_guard lock(state_.mutex);
            ++state_.structureChanges;
        }
        state_.changed.notify_all();
        return S_OK;
    }
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

void invokeNativeUiaSelection(HWND hwnd)
{
    ScopedCom com;
    expectSucceeded(com.result(), "COM initialization failed for UIA selection");
    ComPtr<IUIAutomation> automation;
    expectSucceeded(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(automation.put())),
                    "Unable to create the UI Automation selection client");
    ComPtr<IUIAutomationElement> root;
    expectSucceeded(automation->ElementFromHandle(hwnd, root.put()),
                    "ElementFromHandle failed for UIA selection");
    auto second = findNamedControl(
        *automation.get(), *root.get(), UIA_RadioButtonControlTypeId, L"Second choice");
    expect(static_cast<bool>(second),
           "Native UIA fixture must expose the second RadioButton for SelectionItem action");
    ComPtr<IUIAutomationSelectionItemPattern> selectionItem;
    expectSucceeded(second->GetCurrentPatternAs(
                        UIA_SelectionItemPatternId,
                        __uuidof(IUIAutomationSelectionItemPattern),
                        reinterpret_cast<void**>(selectionItem.put())),
                    "Second RadioButton must expose the UIA SelectionItem pattern");
    expect(static_cast<bool>(selectionItem),
           "Second RadioButton returned no UIA SelectionItem pattern provider");
    expectSucceeded(selectionItem->Select(),
                    "UIA SelectionItem.Select failed for RadioButton");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    bool selected = false;
    do {
        BOOL state = FALSE;
        if (SUCCEEDED(selectionItem->get_CurrentIsSelected(&state))) selected = state != FALSE;
        if (selected) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    expect(selected,
           "Retained UIA SelectionItem must observe its post-Select RadioButton state");
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

std::vector<int> currentRuntimeId(IUIAutomationElement& element)
{
    VARIANT property{};
    VariantInit(&property);
    expectSucceeded(element.GetCurrentPropertyValue(UIA_RuntimeIdPropertyId, &property),
                    "Unable to read the retained UIA RuntimeId");
    expect(property.vt == (VT_ARRAY | VT_I4),
           "UIA RuntimeId must be an integer SAFEARRAY");
    SAFEARRAY* runtimeId = property.parray;
    expect(runtimeId != nullptr, "UIA element returned no RuntimeId");
    LONG lower = 0;
    LONG upper = -1;
    expectSucceeded(SafeArrayGetLBound(runtimeId, 1, &lower),
                    "Unable to read the RuntimeId lower bound");
    expectSucceeded(SafeArrayGetUBound(runtimeId, 1, &upper),
                    "Unable to read the RuntimeId upper bound");
    std::vector<int> values;
    for (LONG index = lower; index <= upper; ++index) {
        int value = 0;
        expectSucceeded(SafeArrayGetElement(runtimeId, &index, &value),
                        "Unable to read a RuntimeId component");
        values.push_back(value);
    }
    VariantClear(&property);
    return values;
}

void subscribeAndWaitForNativeUiaEvents(HWND hwnd, NativeEventState& state,
                                        std::promise<void>& subscribed)
{
    ScopedCom com;
    try {
        expectSucceeded(com.result(), "COM initialization failed for UIA events");
        ComPtr<IUIAutomation> automation;
        expectSucceeded(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(automation.put())),
                        "Unable to create the UI Automation event client");
        ComPtr<IUIAutomationElement> root;
        expectSucceeded(automation->ElementFromHandle(hwnd, root.put()),
                        "ElementFromHandle failed for UIA event subscriptions");
        auto checkbox = findNamedControl(
            *automation.get(), *root.get(), UIA_CheckBoxControlTypeId, L"Native toggle");
        expect(static_cast<bool>(checkbox),
               "The UIA event fixture must expose Native toggle");
        const auto retainedRuntimeId = currentRuntimeId(*checkbox.get());

        auto* propertyHandler = new NativePropertyChangedHandler(state);
        auto* boundsHandler = new RootBoundsChangedHandler(state);
        auto* focusHandler = new FocusChangedHandler(state);
        auto* structureHandler = new StructureChangedHandler(state);
        bool propertyRegistered = false;
        bool boundsRegistered = false;
        bool focusRegistered = false;
        bool structureRegistered = false;
        auto cleanup = [&] {
            if (structureRegistered) {
                automation->RemoveStructureChangedEventHandler(
                    root.get(), structureHandler);
            }
            if (focusRegistered) {
                automation->RemoveFocusChangedEventHandler(focusHandler);
            }
            if (boundsRegistered) {
                automation->RemovePropertyChangedEventHandler(
                    root.get(), boundsHandler);
            }
            if (propertyRegistered) {
                automation->RemovePropertyChangedEventHandler(
                    root.get(), propertyHandler);
            }
            structureHandler->Release();
            focusHandler->Release();
            boundsHandler->Release();
            propertyHandler->Release();
        };

        try {
            PROPERTYID properties[]{UIA_ToggleToggleStatePropertyId,
                                    UIA_ValueValuePropertyId,
                                    UIA_IsEnabledPropertyId,
                                    UIA_NamePropertyId};
            expectSucceeded(automation->AddPropertyChangedEventHandlerNativeArray(
                                root.get(), TreeScope_Subtree, nullptr,
                                propertyHandler, properties,
                                static_cast<int>(std::size(properties))),
                            "Unable to subscribe to native UIA property events");
            propertyRegistered = true;
            PROPERTYID boundsProperty[]{UIA_BoundingRectanglePropertyId};
            expectSucceeded(automation->AddPropertyChangedEventHandlerNativeArray(
                                root.get(), TreeScope_Element, nullptr,
                                boundsHandler, boundsProperty, 1),
                            "Unable to subscribe to the root UIA bounds event");
            boundsRegistered = true;
            expectSucceeded(automation->AddFocusChangedEventHandler(nullptr, focusHandler),
                            "Unable to subscribe to the UIA focus event");
            focusRegistered = true;
            expectSucceeded(automation->AddStructureChangedEventHandler(
                                root.get(), TreeScope_Subtree, nullptr, structureHandler),
                            "Unable to subscribe to the UIA structure event");
            structureRegistered = true;
            subscribed.set_value();

            std::unique_lock lock(state.mutex);
            const bool completed = state.changed.wait_for(
                lock, std::chrono::seconds(5), [&state] { return state.complete(); });
            const bool released = completed && state.changed.wait_for(
                lock, std::chrono::seconds(2),
                [&state] { return state.releaseSubscriber; });
            lock.unlock();
            const auto publishedRuntimeId = currentRuntimeId(*checkbox.get());
            cleanup();
            expect(completed,
                   "Timed out waiting for native UIA property/focus/structure events");
            expect(released,
                   "Timed out waiting to finish the native UIA event subscription");
            expect(retainedRuntimeId == publishedRuntimeId,
                   "A retained UIA element RuntimeId must remain stable across publishes");
        } catch (...) {
            cleanup();
            throw;
        }
    } catch (...) {
        try {
            subscribed.set_exception(std::current_exception());
        } catch (...) {
            // The subscription-ready promise was already fulfilled; the
            // packaged task transports this failure to the UI thread.
        }
        throw;
    }
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
    // SelectionItem::Select is deliberately exercised after the SetFocus
    // assertion because UI Automation clients are permitted to move native
    // focus while querying/activating a RadioButton.
    runUiaWorkOffUiThread(window, [hwnd] { invokeNativeUiaSelection(hwnd); },
                          "UIA selection dispatch timed out");
}

void exerciseNativeUiaEvents(wui::UiWindow& window, HWND hwnd)
{
    NativeEventState state;
    std::promise<void> subscribed;
    auto subscription = subscribed.get_future();
    std::packaged_task<void()> task([hwnd, &state, &subscribed] {
        subscribeAndWaitForNativeUiaEvents(hwnd, state, subscribed);
    });
    auto result = task.get_future();
    std::thread worker(std::move(task));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (subscription.wait_for(std::chrono::milliseconds(0))
           != std::future_status::ready) {
        pumpNativeUi(window);
        if (std::chrono::steady_clock::now() >= deadline) {
            std::fputs("FAIL: UIA event subscription timed out\n", stderr);
            std::fflush(stderr);
            std::quick_exit(1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    subscription.get();

    expect(window.root() != nullptr && window.root()->children().size() == 9,
           "The native UIA event fixture must retain its controls");
    auto* checkbox = dynamic_cast<wui::Checkbox*>(window.root()->children()[2].get());
    expect(checkbox != nullptr, "The UIA property event fixture must be a Checkbox");
    auto* textInput = dynamic_cast<wui::TextInput*>(window.root()->children()[3].get());
    expect(textInput != nullptr, "The UIA value event fixture must be a TextInput");
    checkbox->setChecked(false);
    checkbox->setEnabled(false);
    checkbox->setLabel("Native toggle renamed");
    textInput->text("Event value");
    window.focusManager().setFocused(window.root()->children()[3].get());
    window.root()->appendChild(
        wui::ui::Text("Native event child")
            .accessibilityId("native.event.child")
            .intoNode());
    window.update();
    window.layout();

    RECT previousWindowBounds{};
    expect(GetWindowRect(hwnd, &previousWindowBounds) != FALSE,
           "GetWindowRect failed before the UIA bounds event fixture moved");
    expect(SetWindowPos(hwnd, nullptr,
                        previousWindowBounds.left + 24,
                        previousWindowBounds.top + 18,
                        0, 0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
           "SetWindowPos failed for the UIA bounds event fixture");
    pumpNativeUi(window);

    bool complete = false;
    while (!complete) {
        {
            std::lock_guard lock(state.mutex);
            complete = state.complete();
        }
        if (complete) break;
        pumpNativeUi(window);
        if (result.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            worker.join();
            result.get();
            throw std::runtime_error("UIA event subscriber exited before all events arrived");
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            std::lock_guard lock(state.mutex);
            std::fprintf(stderr,
                         "FAIL: native UIA event delivery timed out "
                         "(toggle=%d value=%d enabled=%d name=%d bounds=%d focus=%d structure=%d)\n",
                         state.togglePropertyChanges, state.valuePropertyChanges,
                         state.enabledPropertyChanges, state.namePropertyChanges,
                         state.boundsPropertyChanges, state.valueFocusChanges,
                         state.structureChanges);
            std::fflush(stderr);
            std::quick_exit(1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    RECT expectedWindowBounds{};
    expect(GetWindowRect(hwnd, &expectedWindowBounds) != FALSE,
           "GetWindowRect failed after the UIA bounds event fixture moved");
    // complete() becomes true as soon as the first event of every required
    // category arrives. Drain the remainder of that same publication batch
    // without publishing another snapshot before taking the no-op baseline.
    const auto drainDeadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(100);
    while (std::chrono::steady_clock::now() < drainDeadline) {
        glfwPollEvents();
        window.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    int eventCountBeforeDuplicate = 0;
    bool boundsMatch = false;
    {
        std::lock_guard lock(state.mutex);
        eventCountBeforeDuplicate = state.totalEvents();
        const double expectedCenterX =
            (static_cast<double>(expectedWindowBounds.left) + expectedWindowBounds.right) / 2.0;
        const double expectedCenterY =
            (static_cast<double>(expectedWindowBounds.top) + expectedWindowBounds.bottom) / 2.0;
        const double actualCenterX =
            (static_cast<double>(state.latestBounds.left) + state.latestBounds.right) / 2.0;
        const double actualCenterY =
            (static_cast<double>(state.latestBounds.top) + state.latestBounds.bottom) / 2.0;
        boundsMatch = std::fabs(actualCenterX - expectedCenterX) <= 8.0
            && std::fabs(actualCenterY - expectedCenterY) <= 8.0;
    }

    // Publishing an identical immutable model must be a true no-op for UIA
    // clients; otherwise every frame would generate an accessibility storm.
    const auto quietDeadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < quietDeadline) {
        window.layout();
        pumpNativeUi(window);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    bool duplicateFree = false;
    {
        std::lock_guard lock(state.mutex);
        duplicateFree = state.totalEvents() == eventCountBeforeDuplicate;
        state.releaseSubscriber = true;
    }
    state.changed.notify_all();

    while (result.wait_for(std::chrono::milliseconds(0))
           != std::future_status::ready) {
        pumpNativeUi(window);
        if (std::chrono::steady_clock::now() >= deadline) {
            std::fputs("FAIL: native UIA event cleanup timed out\n", stderr);
            std::fflush(stderr);
            std::quick_exit(1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    worker.join();
    result.get();
    std::lock_guard lock(state.mutex);
    expect(boundsMatch,
           "The UIA BoundingRectangle event must publish the moved HWND bounds");
    expect(duplicateFree,
           "Republishing an identical snapshot must not raise duplicate UIA events");
    expect(state.complete(),
           "Native UIA event delivery must cover property, focus, and structure changes");
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
    auto& window = app.openWindow("WhatsUI native UIA smoke", {560.0f, 600.0f});
    NativeActionState actionState;
    window.setRoot(wui::ui::Column()
                       .padding(24)
                       .gap(12)
                       .children(wui::ui::Text("Native accessibility boundary"),
                                 wui::ui::Button("Native action").accessibilityId("native.action").onClick([&actionState] {
                                     ++actionState.invokeCount;
                                     actionState.invokeThread = std::this_thread::get_id();
                                 }),
                                 wui::ui::Checkbox("Native toggle").required().accessibilityId("native.toggle").onChange(
                                     [&actionState](bool checked) {
                                         actionState.checked = checked;
                                         actionState.toggleThread = std::this_thread::get_id();
                                     }),
                                 wui::ui::TextField("Native value").accessibilityId("native.value").onChange(
                                     [&actionState](const std::string& value) {
                                         actionState.value = value;
                                         actionState.valueThread = std::this_thread::get_id();
                                     }),
                                 wui::ui::RadioGroup()
                                     .accessibleLabel("Native choices")
                                     .required()
                                     .value("first")
                                     .option("first", "First choice")
                                     .option("second", "Second choice"),
                                 wui::ui::Switch("Native required switch").required(),
                                 wui::ui::ProgressBar()
                                     .accessibleLabel("Native busy progress"),
                                 wui::ui::Slider(10.0f, 90.0f, 40.0f)
                                     .step(5.0f)
                                     .accessibleLabel("Native range slider"),
                                 wui::ui::ProgressBar(0.0f, 100.0f, 60.0f)
                                     .accessibleLabel("Native determinate progress"))
                       .intoNode());
    window.update();
    window.layout();
    expect(window.root() != nullptr && window.root()->children().size() == 9,
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
    exerciseNativeUiaEvents(window, hwnd);
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
