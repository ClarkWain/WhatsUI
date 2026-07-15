#include "wui/windows_uia_provider.h"

#if defined(_WIN32)

#include <OleAuto.h>
#include <UIAutomationClient.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <iterator>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wui::windows {
namespace {

constexpr std::size_t kSyntheticRoot = std::numeric_limits<std::size_t>::max();

std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::optional<std::string> wideToUtf8(const wchar_t* value)
{
    if (!value) return std::nullopt;
    if (!*value) return std::string{};
    const int length = static_cast<int>(std::wcslen(value));
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length,
                                          nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return std::nullopt;
    std::string result(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length, result.data(), bytes,
                        nullptr, nullptr);
    return std::optional<std::string>{std::move(result)};
}

HRESULT setBstrVariant(const std::string& value, VARIANT* result) noexcept
{
    const std::wstring wide = utf8ToWide(value);
    result->vt = VT_BSTR;
    result->bstrVal = SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
    return result->bstrVal || wide.empty() ? S_OK : E_OUTOFMEMORY;
}

int controlType(AccessibilityRole role) noexcept
{
    switch (role) {
    case AccessibilityRole::Application: return UIA_WindowControlTypeId;
    case AccessibilityRole::Group: return UIA_GroupControlTypeId;
    case AccessibilityRole::Heading: return UIA_HeaderControlTypeId;
    case AccessibilityRole::Text: return UIA_TextControlTypeId;
    case AccessibilityRole::Button: return UIA_ButtonControlTypeId;
    case AccessibilityRole::CheckBox: return UIA_CheckBoxControlTypeId;
    case AccessibilityRole::RadioButton: return UIA_RadioButtonControlTypeId;
    case AccessibilityRole::Switch: return UIA_CheckBoxControlTypeId;
    case AccessibilityRole::Slider: return UIA_SliderControlTypeId;
    case AccessibilityRole::TextField: return UIA_EditControlTypeId;
    case AccessibilityRole::List: return UIA_ListControlTypeId;
    case AccessibilityRole::ListItem: return UIA_ListItemControlTypeId;
    case AccessibilityRole::Menu: return UIA_MenuControlTypeId;
    case AccessibilityRole::MenuItem: return UIA_MenuItemControlTypeId;
    case AccessibilityRole::Dialog: return UIA_WindowControlTypeId;
    case AccessibilityRole::ProgressBar: return UIA_ProgressBarControlTypeId;
    case AccessibilityRole::Image: return UIA_ImageControlTypeId;
    case AccessibilityRole::Separator: return UIA_SeparatorControlTypeId;
    case AccessibilityRole::Unknown: return UIA_CustomControlTypeId;
    }
    return UIA_CustomControlTypeId;
}

struct SnapshotModel {
    HWND window{};
    AccessibilitySnapshot entries;
    float scaleX{1.0f};
    float scaleY{1.0f};
    POINT clientOrigin{};
    UiaRect windowBounds{};
    std::vector<std::optional<std::size_t>> parents;
    std::vector<std::vector<std::size_t>> children;
    std::optional<std::size_t> semanticRoot;
    std::string windowTitle;
    std::unordered_map<std::string, std::size_t> automationIdCounts;

    SnapshotModel(HWND hwnd, AccessibilitySnapshot snapshot, WindowMetrics metrics)
        : window(hwnd), entries(std::move(snapshot)),
          parents(entries.size()), children(entries.size() + 1)
    {
        RECT client{};
        if (GetClientRect(window, &client)) {
            const float width = static_cast<float>(client.right - client.left);
            const float height = static_cast<float>(client.bottom - client.top);
            if (metrics.logicalSize.width > 0.0f && std::isfinite(metrics.logicalSize.width)) {
                scaleX = width / metrics.logicalSize.width;
            }
            if (metrics.logicalSize.height > 0.0f && std::isfinite(metrics.logicalSize.height)) {
                scaleY = height / metrics.logicalSize.height;
            }
        }
        const float fallbackScale = metrics.scaleFactor > 0.0f && std::isfinite(metrics.scaleFactor)
            ? metrics.scaleFactor : 1.0f;
        if (!(scaleX > 0.0f) || !std::isfinite(scaleX)) scaleX = fallbackScale;
        if (!(scaleY > 0.0f) || !std::isfinite(scaleY)) scaleY = fallbackScale;
        (void)ClientToScreen(window, &clientOrigin);
        RECT nativeWindowBounds{};
        if (GetWindowRect(window, &nativeWindowBounds)) {
            windowBounds.left = nativeWindowBounds.left;
            windowBounds.top = nativeWindowBounds.top;
            windowBounds.width = nativeWindowBounds.right - nativeWindowBounds.left;
            windowBounds.height = nativeWindowBounds.bottom - nativeWindowBounds.top;
        }
        wchar_t title[512]{};
        const int titleLength = GetWindowTextW(window, title, static_cast<int>(std::size(title)));
        if (titleLength > 0) {
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, title, titleLength, nullptr, 0,
                                                  nullptr, nullptr);
            windowTitle.resize(static_cast<std::size_t>(bytes));
            WideCharToMultiByte(CP_UTF8, 0, title, titleLength, windowTitle.data(), bytes,
                                nullptr, nullptr);
        }
        if (!entries.empty() && entries.front().path.empty() &&
            entries.front().properties.role == AccessibilityRole::Application) {
            semanticRoot = 0;
        }
        for (const auto& entry : entries) {
            if (!entry.properties.automationId.empty()) {
                ++automationIdCounts[entry.properties.automationId];
            }
        }
        for (std::size_t index = 0; index < entries.size(); ++index) {
            std::optional<std::size_t> parent;
            std::size_t longest = 0;
            for (std::size_t candidate = 0; candidate < entries.size(); ++candidate) {
                if (candidate == index || entries[candidate].path.size() >= entries[index].path.size()) {
                    continue;
                }
                const auto& prefix = entries[candidate].path;
                if (std::equal(prefix.begin(), prefix.end(), entries[index].path.begin()) &&
                    (!parent || prefix.size() > longest)) {
                    parent = candidate;
                    longest = prefix.size();
                }
            }
            parents[index] = parent;
            children[parent ? *parent : entries.size()].push_back(index);
        }
    }

    [[nodiscard]] const std::vector<std::size_t>& childIndices(std::size_t index) const noexcept
    {
        if (index == kSyntheticRoot && semanticRoot) return children[*semanticRoot];
        return children[index == kSyntheticRoot ? entries.size() : index];
    }

    [[nodiscard]] UiaRect screenBounds(std::size_t index) const noexcept
    {
        UiaRect result{};
        if (index == kSyntheticRoot) {
            return windowBounds;
        }
        const auto& bounds = entries[index].properties.bounds;
        if (!bounds) {
            return result;
        }
        result.left = clientOrigin.x + static_cast<double>(bounds->x * scaleX);
        result.top = clientOrigin.y + static_cast<double>(bounds->y * scaleY);
        result.width = static_cast<double>(std::max(0.0f, bounds->width) * scaleX);
        result.height = static_cast<double>(std::max(0.0f, bounds->height) * scaleY);
        return result;
    }

    [[nodiscard]] bool hasUniqueAutomationId(const std::string& value) const noexcept
    {
        if (value.empty()) return false;
        const auto found = automationIdCounts.find(value);
        return found != automationIdCounts.end() && found->second == 1;
    }
};

struct ElementKey {
    bool root{false};
    std::string automationId;
    std::vector<std::size_t> path;
    AccessibilityRole role{AccessibilityRole::Unknown};
};

struct ProviderState {
    struct RuntimeRegistration {
        ElementKey key;
        LONG component{};
    };

    HWND window{};
    mutable std::mutex mutex;
    std::shared_ptr<const SnapshotModel> latest;
    std::shared_ptr<UiaActionQueue> actions;
    bool detached{false};
    std::vector<RuntimeRegistration> runtimeRegistrations;
    LONG nextRuntimeComponent{2};

    [[nodiscard]] std::optional<std::pair<std::shared_ptr<const SnapshotModel>, std::size_t>>
    resolve(const ElementKey& key) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (detached || !latest) return std::nullopt;
        if (key.root) return std::pair{latest, kSyntheticRoot};
        std::optional<std::size_t> match;
        for (std::size_t index = 0; index < latest->entries.size(); ++index) {
            const auto& entry = latest->entries[index];
            const bool matches = !key.automationId.empty()
                ? entry.properties.automationId == key.automationId
                : entry.path == key.path && entry.properties.role == key.role;
            if (!matches) continue;
            // Ambiguous author IDs (or a malformed duplicate path) must never
            // silently retarget a retained provider to the first match.
            if (match) return std::nullopt;
            match = index;
        }
        if (!match) return std::nullopt;
        return std::pair{latest, *match};
    }

    [[nodiscard]] LONG runtimeComponent(const ElementKey& key) noexcept
    {
        if (key.root) return 1;
        try {
            std::lock_guard<std::mutex> lock(mutex);
            const auto sameKey = [&key](const RuntimeRegistration& registration) {
                if (registration.key.root != key.root) return false;
                if (!registration.key.automationId.empty() || !key.automationId.empty()) {
                    return !registration.key.automationId.empty() &&
                           registration.key.automationId == key.automationId;
                }
                return registration.key.path == key.path &&
                       registration.key.role == key.role;
            };
            const auto found = std::find_if(
                runtimeRegistrations.begin(), runtimeRegistrations.end(), sameKey);
            if (found != runtimeRegistrations.end()) return found->component;
            if (nextRuntimeComponent == std::numeric_limits<LONG>::max()) return 0;
            const LONG component = nextRuntimeComponent++;
            runtimeRegistrations.push_back(RuntimeRegistration{key, component});
            return component;
        } catch (...) {
            return 0;
        }
    }
};

ElementKey keyFor(const SnapshotModel& model, std::size_t index)
{
    if (index == kSyntheticRoot) return {true, {}, {}, AccessibilityRole::Application};
    const auto& entry = model.entries[index];
    return {false,
            model.hasUniqueAutomationId(entry.properties.automationId)
                ? entry.properties.automationId : std::string{},
            entry.path,
            entry.properties.role};
}

class SnapshotProvider final : public IRawElementProviderSimple,
                               public IRawElementProviderFragment,
                               public IRawElementProviderFragmentRoot,
                               public IInvokeProvider,
                               public IToggleProvider,
                               public IValueProvider {
public:
    SnapshotProvider(std::shared_ptr<ProviderState> state, ElementKey key)
        : state_(std::move(state)), key_(std::move(key))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IRawElementProviderSimple)) {
            *object = static_cast<IRawElementProviderSimple*>(this);
        } else if (iid == __uuidof(IRawElementProviderFragment)) {
            *object = static_cast<IRawElementProviderFragment*>(this);
        } else if (iid == __uuidof(IRawElementProviderFragmentRoot) && key_.root) {
            *object = static_cast<IRawElementProviderFragmentRoot*>(this);
        } else if (iid == __uuidof(IInvokeProvider) && supportsInvoke()) {
            *object = static_cast<IInvokeProvider*>(this);
        } else if (iid == __uuidof(IToggleProvider) && supportsToggle()) {
            *object = static_cast<IToggleProvider*>(this);
        } else if (iid == __uuidof(IValueProvider) && supportsValue()) {
            *object = static_cast<IValueProvider*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* value) override
    {
        if (!value) return E_POINTER;
        *value = static_cast<ProviderOptions>(
            ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID pattern, IUnknown** provider) override
    {
        if (!provider) return E_POINTER;
        *provider = nullptr;
        if (pattern == UIA_InvokePatternId && supportsInvoke()) {
            return QueryInterface(__uuidof(IInvokeProvider), reinterpret_cast<void**>(provider));
        }
        if (pattern == UIA_TogglePatternId && supportsToggle()) {
            return QueryInterface(__uuidof(IToggleProvider), reinterpret_cast<void**>(provider));
        }
        if (pattern == UIA_ValuePatternId && supportsValue()) {
            return QueryInterface(__uuidof(IValueProvider), reinterpret_cast<void**>(provider));
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke() override
    {
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!supportsInvoke(*resolved)) return UIA_E_NOTSUPPORTED;
        const auto& entry = resolved->first->entries[resolved->second];
        if (!entry.properties.enabled) return UIA_E_ELEMENTNOTENABLED;
        AccessibilityActionRequest request;
        request.kind = AccessibilityActionKind::Invoke;
        return enqueue(*resolved, std::move(request));
    }

    HRESULT STDMETHODCALLTYPE Toggle() override
    {
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!supportsToggle(*resolved)) return UIA_E_NOTSUPPORTED;
        if (!resolved->first->entries[resolved->second].properties.enabled) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        AccessibilityActionRequest request;
        request.kind = AccessibilityActionKind::Toggle;
        return enqueue(*resolved, std::move(request));
    }

    HRESULT STDMETHODCALLTYPE get_ToggleState(ToggleState* state) override
    {
        if (!state) return E_POINTER;
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!supportsToggle(*resolved)) return UIA_E_NOTSUPPORTED;
        *state = *resolved->first->entries[resolved->second].properties.checked
            ? ToggleState_On : ToggleState_Off;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR value) override
    {
        if (!value) return E_INVALIDARG;
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!supportsValue(*resolved)) return UIA_E_NOTSUPPORTED;
        if (resolved->first->entries[resolved->second].properties.actions.valueReadOnly) {
            return UIA_E_INVALIDOPERATION;
        }
        if (!resolved->first->entries[resolved->second].properties.enabled) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        AccessibilityActionRequest request;
        request.kind = AccessibilityActionKind::SetValue;
        auto converted = wideToUtf8(value);
        if (!converted) return E_INVALIDARG;
        request.value = std::move(*converted);
        return enqueue(*resolved, std::move(request));
    }

    HRESULT STDMETHODCALLTYPE get_Value(BSTR* value) override
    {
        if (!value) return E_POINTER;
        *value = nullptr;
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!supportsValue(*resolved)) return UIA_E_NOTSUPPORTED;
        const std::wstring wide = utf8ToWide(
            *resolved->first->entries[resolved->second].properties.value);
        *value = SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
        return *value || wide.empty() ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* readOnly) override
    {
        if (!readOnly) return E_POINTER;
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!supportsValue(*resolved)) return UIA_E_NOTSUPPORTED;
        *readOnly = resolved->first->entries[resolved->second].properties.actions.valueReadOnly
            ? TRUE : FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID id, VARIANT* value) override
    {
        if (!value) return E_POINTER;
        VariantInit(value);
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        const auto& model = *resolved->first;
        const std::size_t index = resolved->second;
        const AccessibilityProperties* properties = index == kSyntheticRoot
            ? (model.semanticRoot ? &model.entries[*model.semanticRoot].properties : nullptr)
            : &model.entries[index].properties;
        switch (id) {
        case UIA_ControlTypePropertyId:
            value->vt = VT_I4;
            value->lVal = index == kSyntheticRoot ? UIA_WindowControlTypeId
                                                   : controlType(properties->role);
            return S_OK;
        case UIA_NamePropertyId:
            if (index == kSyntheticRoot && (!properties || properties->label.empty())) {
                return setBstrVariant(model.windowTitle, value);
            }
            return setBstrVariant(properties ? properties->label : std::string{}, value);
        case UIA_HelpTextPropertyId:
            return setBstrVariant(properties ? properties->description : std::string{}, value);
        case UIA_AutomationIdPropertyId:
            return setBstrVariant(automationId(model, index), value);
        case UIA_ValueValuePropertyId:
            if (properties && properties->value) return setBstrVariant(*properties->value, value);
            return S_OK;
        case UIA_IsEnabledPropertyId:
            value->vt = VT_BOOL;
            value->boolVal = (!properties || properties->enabled) ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case UIA_HasKeyboardFocusPropertyId:
            value->vt = VT_BOOL;
            value->boolVal = (properties && properties->focused) ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case UIA_IsKeyboardFocusablePropertyId:
            value->vt = VT_BOOL;
            value->boolVal = properties && properties->enabled && properties->actions.focus
                ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case UIA_IsControlElementPropertyId:
        case UIA_IsContentElementPropertyId:
            value->vt = VT_BOOL;
            value->boolVal = VARIANT_TRUE;
            return S_OK;
        case UIA_ToggleToggleStatePropertyId:
            if (properties && properties->checked) {
                value->vt = VT_I4;
                value->lVal = *properties->checked ? ToggleState_On : ToggleState_Off;
            }
            return S_OK;
        case UIA_SelectionItemIsSelectedPropertyId:
            if (properties && properties->checked) {
                value->vt = VT_BOOL;
                value->boolVal = *properties->checked ? VARIANT_TRUE : VARIANT_FALSE;
            }
            return S_OK;
        case UIA_FrameworkIdPropertyId:
            return setBstrVariant("WhatsUI", value);
        case UIA_ClassNamePropertyId:
            return setBstrVariant(index == kSyntheticRoot ? "WhatsUI.Window" : "WhatsUI.Node", value);
        default:
            return S_OK;
        }
    }

    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** provider) override
    {
        if (!provider) return E_POINTER;
        *provider = nullptr;
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        return resolved->second == kSyntheticRoot
            ? UiaHostProviderFromHwnd(resolved->first->window, provider) : S_OK;
    }

    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,
                                       IRawElementProviderFragment** result) override
    {
        if (!result) return E_POINTER;
        *result = nullptr;
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        const auto& model = *resolved->first;
        const std::size_t index = resolved->second;
        std::optional<std::size_t> target;
        if (direction == NavigateDirection_Parent) {
            if (index != kSyntheticRoot) {
                const std::size_t parent = model.parents[index].value_or(kSyntheticRoot);
                target = model.semanticRoot && parent == *model.semanticRoot
                    ? kSyntheticRoot : parent;
            }
        } else if (direction == NavigateDirection_FirstChild ||
                   direction == NavigateDirection_LastChild) {
            const auto& children = model.childIndices(index);
            if (!children.empty()) target = direction == NavigateDirection_FirstChild
                ? children.front() : children.back();
        } else if (index != kSyntheticRoot) {
            std::size_t parent = model.parents[index].value_or(kSyntheticRoot);
            if (model.semanticRoot && parent == *model.semanticRoot) parent = kSyntheticRoot;
            const auto& siblings = model.childIndices(parent);
            const auto position = std::find(siblings.begin(), siblings.end(), index);
            if (direction == NavigateDirection_NextSibling && position != siblings.end() &&
                std::next(position) != siblings.end()) target = *std::next(position);
            if (direction == NavigateDirection_PreviousSibling && position != siblings.begin() &&
                position != siblings.end()) target = *std::prev(position);
        }
        if (target) return make(model, *target, result);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** runtimeId) override
    {
        if (!runtimeId) return E_POINTER;
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        *runtimeId = SafeArrayCreateVector(VT_I4, 0, 3);
        if (!*runtimeId) return E_OUTOFMEMORY;
        LONG* values = nullptr;
        HRESULT hr = SafeArrayAccessData(*runtimeId, reinterpret_cast<void**>(&values));
        if (FAILED(hr)) { SafeArrayDestroy(*runtimeId); *runtimeId = nullptr; return hr; }
        values[0] = UiaAppendRuntimeId;
        values[1] = static_cast<LONG>(reinterpret_cast<std::uintptr_t>(resolved->first->window) & 0x7fffffffU);
        values[2] = state_->runtimeComponent(key_);
        if (values[2] == 0) {
            SafeArrayUnaccessData(*runtimeId);
            SafeArrayDestroy(*runtimeId);
            *runtimeId = nullptr;
            return E_OUTOFMEMORY;
        }
        SafeArrayUnaccessData(*runtimeId);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* rectangle) override
    {
        if (!rectangle) return E_POINTER;
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        *rectangle = resolved->first->screenBounds(resolved->second);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** roots) override
    {
        if (!roots) return E_POINTER;
        *roots = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetFocus() override
    {
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        if (resolved->second == kSyntheticRoot) return UIA_E_NOTSUPPORTED;
        const auto& properties = resolved->first->entries[resolved->second].properties;
        if (!properties.actions.focus) return UIA_E_NOTSUPPORTED;
        if (!properties.enabled) return UIA_E_ELEMENTNOTENABLED;
        AccessibilityActionRequest request;
        request.kind = AccessibilityActionKind::SetFocus;
        return enqueue(*resolved, std::move(request));
    }

    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** root) override
    {
        if (!root) return E_POINTER;
        return makeRoot(root);
    }

    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x, double y,
        IRawElementProviderFragment** provider) override
    {
        if (!provider) return E_POINTER;
        *provider = nullptr;
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        const auto& model = *resolved->first;
        std::size_t selected = kSyntheticRoot;
        std::size_t bestDepth = 0;
        for (std::size_t index = 0; index < model.entries.size(); ++index) {
            const UiaRect bounds = model.screenBounds(index);
            if (bounds.width > 0.0 && bounds.height > 0.0 && x >= bounds.left && y >= bounds.top &&
                x <= bounds.left + bounds.width && y <= bounds.top + bounds.height &&
                (selected == kSyntheticRoot || model.entries[index].depth >= bestDepth)) {
                selected = index;
                bestDepth = model.entries[index].depth;
            }
        }
        return make(model, selected, provider);
    }

    HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** provider) override
    {
        if (!provider) return E_POINTER;
        *provider = nullptr;
        const auto resolved = resolve();
        if (!resolved) return UIA_E_ELEMENTNOTAVAILABLE;
        const auto& model = *resolved->first;
        const auto focused = std::find_if(model.entries.begin(), model.entries.end(),
            [](const AccessibilitySnapshotEntry& entry) { return entry.properties.focused; });
        if (focused == model.entries.end()) return S_OK;
        return make(model, static_cast<std::size_t>(std::distance(model.entries.begin(), focused)), provider);
    }

private:
    using Resolution = std::pair<std::shared_ptr<const SnapshotModel>, std::size_t>;

    [[nodiscard]] std::optional<Resolution> resolve() const
    {
        return state_->resolve(key_);
    }

    [[nodiscard]] static bool supportsInvoke(const Resolution& resolved) noexcept
    {
        if (resolved.second == kSyntheticRoot) return false;
        return resolved.first->entries[resolved.second].properties.actions.invoke;
    }

    [[nodiscard]] bool supportsInvoke() const noexcept
    {
        const auto resolved = resolve();
        return resolved && supportsInvoke(*resolved);
    }

    [[nodiscard]] static bool supportsToggle(const Resolution& resolved) noexcept
    {
        if (resolved.second == kSyntheticRoot) return false;
        const auto& properties = resolved.first->entries[resolved.second].properties;
        return properties.checked && properties.actions.toggle;
    }

    [[nodiscard]] bool supportsToggle() const noexcept
    {
        const auto resolved = resolve();
        return resolved && supportsToggle(*resolved);
    }

    [[nodiscard]] static bool supportsValue(const Resolution& resolved) noexcept
    {
        if (resolved.second == kSyntheticRoot) return false;
        const auto& properties = resolved.first->entries[resolved.second].properties;
        return properties.value.has_value() &&
               (properties.actions.setValue || properties.actions.valueReadOnly);
    }

    [[nodiscard]] bool supportsValue() const noexcept
    {
        const auto resolved = resolve();
        return resolved && supportsValue(*resolved);
    }

    HRESULT enqueue(const Resolution& resolved, AccessibilityActionRequest request) noexcept
    {
        const auto& entry = resolved.first->entries[resolved.second];
        request.path = entry.path;
        request.expectedRole = entry.properties.role;
        request.automationId = entry.properties.automationId;
        request.expectedLabel = entry.properties.label;
        return state_->actions->submit(
            state_->window, UiaSnapshotBridge::actionMessageId(), std::move(request));
    }

    static std::string automationId(const SnapshotModel& model, std::size_t index)
    {
        if (index == kSyntheticRoot) return "window";
        if (model.hasUniqueAutomationId(
                model.entries[index].properties.automationId)) {
            return model.entries[index].properties.automationId;
        }
        std::string result{"node"};
        for (const std::size_t part : model.entries[index].path) {
            result.push_back('.');
            result += std::to_string(part);
        }
        return result;
    }

    HRESULT make(const SnapshotModel& model, std::size_t index,
                 IRawElementProviderFragment** result) const noexcept
    {
        auto* provider = new (std::nothrow) SnapshotProvider(state_, keyFor(model, index));
        if (!provider) return E_OUTOFMEMORY;
        *result = static_cast<IRawElementProviderFragment*>(provider);
        return S_OK;
    }

    HRESULT makeRoot(IRawElementProviderFragmentRoot** result) const noexcept
    {
        auto* provider = new (std::nothrow) SnapshotProvider(
            state_, ElementKey{true, {}, {}, AccessibilityRole::Application});
        if (!provider) return E_OUTOFMEMORY;
        *result = static_cast<IRawElementProviderFragmentRoot*>(provider);
        return S_OK;
    }

    std::atomic<ULONG> references_{1};
    std::shared_ptr<ProviderState> state_;
    ElementKey key_;
};

std::optional<std::size_t> matchingIndex(const SnapshotModel& source,
                                         std::size_t sourceIndex,
                                         const SnapshotModel& target)
{
    const ElementKey key = keyFor(source, sourceIndex);
    std::optional<std::size_t> match;
    for (std::size_t index = 0; index < target.entries.size(); ++index) {
        const auto& candidate = target.entries[index];
        const bool matches = !key.automationId.empty()
            ? target.hasUniqueAutomationId(candidate.properties.automationId) &&
              candidate.properties.automationId == key.automationId
            : candidate.path == key.path && candidate.properties.role == key.role;
        if (!matches) continue;
        if (match) return std::nullopt;
        match = index;
    }
    return match;
}

bool sameElement(const SnapshotModel& left, std::size_t leftIndex,
                 const SnapshotModel& right, std::size_t rightIndex)
{
    const auto match = matchingIndex(left, leftIndex, right);
    return match && *match == rightIndex;
}

IRawElementProviderSimple* eventProvider(const std::shared_ptr<ProviderState>& state,
                                         const SnapshotModel& model,
                                         std::size_t index) noexcept
{
    try {
        auto* provider = new (std::nothrow) SnapshotProvider(state, keyFor(model, index));
        return provider ? static_cast<IRawElementProviderSimple*>(provider) : nullptr;
    } catch (...) {
        return nullptr;
    }
}

void raiseStringProperty(IRawElementProviderSimple* provider, PROPERTYID property,
                         const std::string& oldValue, const std::string& newValue) noexcept
{
    VARIANT before;
    VARIANT after;
    VariantInit(&before);
    VariantInit(&after);
    try {
        if (SUCCEEDED(setBstrVariant(oldValue, &before)) &&
            SUCCEEDED(setBstrVariant(newValue, &after))) {
            (void)UiaRaiseAutomationPropertyChangedEvent(
                provider, property, before, after);
        }
    } catch (...) {
        // Accessibility event delivery is best-effort and must never take down
        // the UI thread under allocation pressure.
    }
    VariantClear(&before);
    VariantClear(&after);
}

void raiseBoolProperty(IRawElementProviderSimple* provider, PROPERTYID property,
                       bool oldValue, bool newValue) noexcept
{
    VARIANT before;
    VARIANT after;
    VariantInit(&before);
    VariantInit(&after);
    before.vt = VT_BOOL;
    before.boolVal = oldValue ? VARIANT_TRUE : VARIANT_FALSE;
    after.vt = VT_BOOL;
    after.boolVal = newValue ? VARIANT_TRUE : VARIANT_FALSE;
    (void)UiaRaiseAutomationPropertyChangedEvent(provider, property, before, after);
}

void raiseIntProperty(IRawElementProviderSimple* provider, PROPERTYID property,
                      LONG oldValue, LONG newValue) noexcept
{
    VARIANT before;
    VARIANT after;
    VariantInit(&before);
    VariantInit(&after);
    before.vt = VT_I4;
    before.lVal = oldValue;
    after.vt = VT_I4;
    after.lVal = newValue;
    (void)UiaRaiseAutomationPropertyChangedEvent(provider, property, before, after);
}

bool meaningfullyDifferent(const UiaRect& left, const UiaRect& right) noexcept
{
    constexpr double threshold = 0.25;
    return std::abs(left.left - right.left) > threshold ||
           std::abs(left.top - right.top) > threshold ||
           std::abs(left.width - right.width) > threshold ||
           std::abs(left.height - right.height) > threshold;
}

void raiseRectProperty(IRawElementProviderSimple* provider,
                       const UiaRect& oldValue, const UiaRect& newValue) noexcept
{
    VARIANT before;
    VARIANT after;
    VariantInit(&before);
    VariantInit(&after);
    before.vt = VT_ARRAY | VT_R8;
    after.vt = VT_ARRAY | VT_R8;
    before.parray = SafeArrayCreateVector(VT_R8, 0, 4);
    after.parray = SafeArrayCreateVector(VT_R8, 0, 4);
    if (before.parray && after.parray) {
        double* oldParts = nullptr;
        double* newParts = nullptr;
        if (SUCCEEDED(SafeArrayAccessData(
                before.parray, reinterpret_cast<void**>(&oldParts))) &&
            SUCCEEDED(SafeArrayAccessData(
                after.parray, reinterpret_cast<void**>(&newParts)))) {
            oldParts[0] = oldValue.left;
            oldParts[1] = oldValue.top;
            oldParts[2] = oldValue.width;
            oldParts[3] = oldValue.height;
            newParts[0] = newValue.left;
            newParts[1] = newValue.top;
            newParts[2] = newValue.width;
            newParts[3] = newValue.height;
            SafeArrayUnaccessData(before.parray);
            SafeArrayUnaccessData(after.parray);
            (void)UiaRaiseAutomationPropertyChangedEvent(
                provider, UIA_BoundingRectanglePropertyId, before, after);
        } else {
            if (oldParts) SafeArrayUnaccessData(before.parray);
            if (newParts) SafeArrayUnaccessData(after.parray);
        }
    }
    VariantClear(&before);
    VariantClear(&after);
}

bool supportsTogglePattern(const AccessibilityProperties& properties) noexcept
{
    return properties.checked.has_value() && properties.actions.toggle;
}

bool supportsValuePattern(const AccessibilityProperties& properties) noexcept
{
    return properties.value.has_value() &&
           (properties.actions.setValue || properties.actions.valueReadOnly);
}

bool sameChildren(const SnapshotModel& previous, std::size_t previousParent,
                  const SnapshotModel& current, std::size_t currentParent) noexcept
{
    const auto& oldChildren = previous.childIndices(previousParent);
    const auto& newChildren = current.childIndices(currentParent);
    if (oldChildren.size() != newChildren.size()) return false;
    for (std::size_t position = 0; position < oldChildren.size(); ++position) {
        if (!sameElement(previous, oldChildren[position],
                         current, newChildren[position])) {
            return false;
        }
    }
    return true;
}

bool sameChildSet(const SnapshotModel& previous, std::size_t previousParent,
                  const SnapshotModel& current, std::size_t currentParent)
{
    const auto& oldChildren = previous.childIndices(previousParent);
    const auto& newChildren = current.childIndices(currentParent);
    if (oldChildren.size() != newChildren.size()) return false;
    for (const std::size_t oldChild : oldChildren) {
        const auto matched = matchingIndex(previous, oldChild, current);
        if (!matched || std::find(newChildren.begin(), newChildren.end(), *matched) ==
                            newChildren.end()) {
            return false;
        }
    }
    return true;
}

StructureChangeType childStructureChange(
    const SnapshotModel& previous, std::size_t previousParent,
    const SnapshotModel& current, std::size_t currentParent)
{
    return sameChildSet(previous, previousParent, current, currentParent)
        ? StructureChangeType_ChildrenReordered
        : StructureChangeType_ChildrenInvalidated;
}

const std::string& rootName(const SnapshotModel& model) noexcept
{
    if (model.semanticRoot &&
        !model.entries[*model.semanticRoot].properties.label.empty()) {
        return model.entries[*model.semanticRoot].properties.label;
    }
    return model.windowTitle;
}

void raiseSnapshotEvents(const std::shared_ptr<ProviderState>& state,
                         const SnapshotModel& previous,
                         const SnapshotModel& current) noexcept
{
    if (!UiaClientsAreListening()) return;

    // Structure precedes property and focus notifications so clients refresh
    // the fragment tree before interpreting state changes on retained nodes.
    if (!sameChildren(previous, kSyntheticRoot, current, kSyntheticRoot)) {
        if (auto* provider = eventProvider(state, current, kSyntheticRoot)) {
            (void)UiaRaiseStructureChangedEvent(
                provider,
                childStructureChange(
                    previous, kSyntheticRoot, current, kSyntheticRoot),
                nullptr, 0);
            provider->Release();
        }
    }
    for (std::size_t oldParent = 0; oldParent < previous.entries.size(); ++oldParent) {
        if (previous.semanticRoot && oldParent == *previous.semanticRoot) continue;
        const auto currentParent = matchingIndex(previous, oldParent, current);
        if (!currentParent ||
            (current.semanticRoot && *currentParent == *current.semanticRoot) ||
            sameChildren(previous, oldParent, current, *currentParent)) {
            continue;
        }
        if (auto* provider = eventProvider(state, current, *currentParent)) {
            (void)UiaRaiseStructureChangedEvent(
                provider,
                childStructureChange(previous, oldParent, current, *currentParent),
                nullptr, 0);
            provider->Release();
        }
    }

    // The synthetic fragment root represents the semantic application root.
    // Its provider identity remains stable for the native window lifetime.
    const bool rootNameChanged = rootName(previous) != rootName(current);
    const bool rootBoundsChanged = meaningfullyDifferent(
        previous.windowBounds, current.windowBounds);
    if (rootNameChanged || rootBoundsChanged) {
        if (auto* provider = eventProvider(state, current, kSyntheticRoot)) {
            if (rootNameChanged) {
                raiseStringProperty(provider, UIA_NamePropertyId,
                                    rootName(previous), rootName(current));
            }
            if (rootBoundsChanged) {
                raiseRectProperty(provider, previous.windowBounds, current.windowBounds);
            }
            provider->Release();
        }
    }
    const bool oldRootEnabled = !previous.semanticRoot ||
        previous.entries[*previous.semanticRoot].properties.enabled;
    const bool newRootEnabled = !current.semanticRoot ||
        current.entries[*current.semanticRoot].properties.enabled;
    if (oldRootEnabled != newRootEnabled) {
        if (auto* provider = eventProvider(state, current, kSyntheticRoot)) {
            raiseBoolProperty(provider, UIA_IsEnabledPropertyId,
                              oldRootEnabled, newRootEnabled);
            provider->Release();
        }
    }

    for (std::size_t oldIndex = 0; oldIndex < previous.entries.size(); ++oldIndex) {
        if (previous.semanticRoot && oldIndex == *previous.semanticRoot) continue;
        const auto currentIndex = matchingIndex(previous, oldIndex, current);
        if (!currentIndex ||
            (current.semanticRoot && *currentIndex == *current.semanticRoot)) {
            continue;
        }
        const auto& oldProperties = previous.entries[oldIndex].properties;
        const auto& newProperties = current.entries[*currentIndex].properties;
        const bool nameChanged = oldProperties.label != newProperties.label;
        const bool enabledChanged = oldProperties.enabled != newProperties.enabled;
        const bool toggleChanged = supportsTogglePattern(oldProperties) &&
            supportsTogglePattern(newProperties) &&
            oldProperties.checked != newProperties.checked;
        const bool valueChanged = supportsValuePattern(oldProperties) &&
            supportsValuePattern(newProperties) &&
            oldProperties.value != newProperties.value;
        const UiaRect oldBounds = previous.screenBounds(oldIndex);
        const UiaRect newBounds = current.screenBounds(*currentIndex);
        const bool boundsChanged = meaningfullyDifferent(oldBounds, newBounds);
        if (!nameChanged && !enabledChanged && !toggleChanged &&
            !valueChanged && !boundsChanged) continue;

        auto* provider = eventProvider(state, current, *currentIndex);
        if (!provider) continue;
        if (nameChanged) {
            raiseStringProperty(provider, UIA_NamePropertyId,
                                oldProperties.label, newProperties.label);
        }
        if (enabledChanged) {
            raiseBoolProperty(provider, UIA_IsEnabledPropertyId,
                              oldProperties.enabled, newProperties.enabled);
        }
        if (toggleChanged) {
            raiseIntProperty(provider, UIA_ToggleToggleStatePropertyId,
                oldProperties.checked.value_or(false) ? ToggleState_On : ToggleState_Off,
                newProperties.checked.value_or(false) ? ToggleState_On : ToggleState_Off);
        }
        if (valueChanged) {
            raiseStringProperty(provider, UIA_ValueValuePropertyId,
                *oldProperties.value, *newProperties.value);
        }
        if (boundsChanged) {
            raiseRectProperty(provider, oldBounds, newBounds);
        }
        provider->Release();
    }

    // Focus properties are delivered after all other property changes. The
    // focus automation event itself is last, so its target already reports the
    // new HasKeyboardFocus value when queried by a client callback.
    const auto oldFocus = std::find_if(previous.entries.begin(), previous.entries.end(),
        [](const AccessibilitySnapshotEntry& entry) { return entry.properties.focused; });
    const auto newFocus = std::find_if(current.entries.begin(), current.entries.end(),
        [](const AccessibilitySnapshotEntry& entry) { return entry.properties.focused; });
    const std::optional<std::size_t> oldFocusIndex = oldFocus == previous.entries.end()
        ? std::nullopt
        : std::optional<std::size_t>{static_cast<std::size_t>(
            std::distance(previous.entries.begin(), oldFocus))};
    const std::optional<std::size_t> newFocusIndex = newFocus == current.entries.end()
        ? std::nullopt
        : std::optional<std::size_t>{static_cast<std::size_t>(
            std::distance(current.entries.begin(), newFocus))};
    const bool focusChanged = oldFocusIndex.has_value() != newFocusIndex.has_value() ||
        (oldFocusIndex && newFocusIndex &&
         !sameElement(previous, *oldFocusIndex, current, *newFocusIndex));
    if (focusChanged && oldFocusIndex) {
        const auto retainedOldFocus = matchingIndex(previous, *oldFocusIndex, current);
        if (retainedOldFocus) {
            if (auto* provider = eventProvider(state, current, *retainedOldFocus)) {
                raiseBoolProperty(provider, UIA_HasKeyboardFocusPropertyId, true, false);
                provider->Release();
            }
        }
    }
    if (focusChanged && newFocusIndex) {
        if (auto* provider = eventProvider(state, current, *newFocusIndex)) {
            raiseBoolProperty(provider, UIA_HasKeyboardFocusPropertyId, false, true);
            (void)UiaRaiseAutomationEvent(provider, UIA_AutomationFocusChangedEventId);
            provider->Release();
        }
    }
}

} // namespace

struct UiaActionQueue::Impl {
    static constexpr std::size_t kMaximumPending = 1024;
    struct Pending {
        enum class Phase { Queued, Started, Completed, Cancelled };
        explicit Pending(AccessibilityActionRequest action) : request(std::move(action)) {}
        AccessibilityActionRequest request;
        std::atomic<AccessibilityActionStatus> status{AccessibilityActionStatus::Failed};
        std::atomic<Phase> phase{Phase::Queued};
    };
    std::mutex mutex;
    std::deque<std::shared_ptr<Pending>> pending;
    AccessibilityActionHandler callback;
    bool attached{false};
};

UiaActionQueue::UiaActionQueue() : impl_(std::make_unique<Impl>()) {}
UiaActionQueue::~UiaActionQueue() = default;

void UiaActionQueue::setCallback(AccessibilityActionHandler callback)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->callback = std::move(callback);
    impl_->attached = static_cast<bool>(impl_->callback);
    if (!impl_->attached) {
        for (const auto& pending : impl_->pending) {
            pending->status = AccessibilityActionStatus::ElementNotAvailable;
            pending->phase = Impl::Pending::Phase::Completed;
        }
        impl_->pending.clear();
    }
}

namespace {
HRESULT actionStatusResult(AccessibilityActionStatus status) noexcept
{
    switch (status) {
    case AccessibilityActionStatus::Succeeded: return S_OK;
    case AccessibilityActionStatus::ElementNotAvailable:
    case AccessibilityActionStatus::WindowClosed: return UIA_E_ELEMENTNOTAVAILABLE;
    case AccessibilityActionStatus::ElementNotEnabled: return UIA_E_ELEMENTNOTENABLED;
    case AccessibilityActionStatus::NotSupported: return UIA_E_NOTSUPPORTED;
    case AccessibilityActionStatus::InvalidValue: return E_INVALIDARG;
    case AccessibilityActionStatus::TimedOut: return UIA_E_TIMEOUT;
    case AccessibilityActionStatus::Failed: return E_FAIL;
    }
    return E_FAIL;
}
} // namespace

HRESULT UiaActionQueue::submit(HWND window, UINT message,
                               AccessibilityActionRequest request) noexcept
{
    std::shared_ptr<Impl::Pending> pending;
    try {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->attached) return UIA_E_ELEMENTNOTAVAILABLE;
        if (impl_->pending.size() >= Impl::kMaximumPending) return E_OUTOFMEMORY;
        pending = std::make_shared<Impl::Pending>(std::move(request));
        impl_->pending.push_back(pending);
    } catch (...) {
        return E_OUTOFMEMORY;
    }

    DWORD_PTR ignored = 0;
    SetLastError(ERROR_SUCCESS);
    if (!SendMessageTimeoutW(window, message, 0, 0,
                             SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_NOTIMEOUTIFNOTHUNG,
                             5000, &ignored)) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        auto expected = Impl::Pending::Phase::Queued;
        (void)pending->phase.compare_exchange_strong(
            expected, Impl::Pending::Phase::Cancelled);
        const auto queued = std::find(impl_->pending.begin(), impl_->pending.end(), pending);
        if (queued != impl_->pending.end()) impl_->pending.erase(queued);
        return UIA_E_TIMEOUT;
    }
    if (pending->phase.load() != Impl::Pending::Phase::Completed) return E_FAIL;
    return actionStatusResult(pending->status.load());
}

std::size_t UiaActionQueue::dispatchPending()
{
    std::deque<std::shared_ptr<Impl::Pending>> pending;
    AccessibilityActionHandler callback;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        pending.swap(impl_->pending);
        callback = impl_->callback;
    }
    if (!callback) return 0;
    for (const auto& action : pending) {
        auto expected = Impl::Pending::Phase::Queued;
        if (!action->phase.compare_exchange_strong(
                expected, Impl::Pending::Phase::Started)) {
            continue;
        }
        try {
            action->status = callback(action->request);
        } catch (...) {
            action->status = AccessibilityActionStatus::Failed;
        }
        action->phase = Impl::Pending::Phase::Completed;
    }
    return pending.size();
}

HRESULT createUiaSnapshotProvider(HWND window, AccessibilitySnapshot snapshot,
                                  WindowMetrics metrics,
                                  IRawElementProviderFragmentRoot** provider) noexcept
{
    try {
        return createUiaSnapshotProvider(window, std::move(snapshot), metrics,
                                         std::make_shared<UiaActionQueue>(), provider);
    } catch (...) {
        if (provider) *provider = nullptr;
        return E_OUTOFMEMORY;
    }
}

HRESULT createUiaSnapshotProvider(HWND window, AccessibilitySnapshot snapshot,
                                  WindowMetrics metrics,
                                  std::shared_ptr<UiaActionQueue> actions,
                                  IRawElementProviderFragmentRoot** provider) noexcept
{
    if (!provider) return E_POINTER;
    *provider = nullptr;
    if (!IsWindow(window)) return E_INVALIDARG;
    try {
        if (!actions) actions = std::make_shared<UiaActionQueue>();
        auto state = std::make_shared<ProviderState>();
        state->window = window;
        state->actions = std::move(actions);
        state->latest = std::make_shared<const SnapshotModel>(
            window, std::move(snapshot), metrics);
        auto* created = new (std::nothrow) SnapshotProvider(
            std::move(state), ElementKey{true, {}, {}, AccessibilityRole::Application});
        if (!created) return E_OUTOFMEMORY;
        *provider = static_cast<IRawElementProviderFragmentRoot*>(created);
        return S_OK;
    } catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    } catch (...) {
        return E_FAIL;
    }
}

struct UiaSnapshotBridge::Impl {
    explicit Impl(HWND nativeWindow)
        : window(nativeWindow), state(std::make_shared<ProviderState>())
    {
        state->window = window;
        state->actions = std::make_shared<UiaActionQueue>();
        root = new SnapshotProvider(
            state, ElementKey{true, {}, {}, AccessibilityRole::Application});
    }
    HWND window{};
    std::shared_ptr<ProviderState> state;
    IRawElementProviderFragmentRoot* root{};
    std::shared_ptr<const SnapshotModel> pendingEventBaseline;
    std::shared_ptr<const SnapshotModel> pendingEventLatest;
    bool eventFlushPosted{false};
};

UiaSnapshotBridge::UiaSnapshotBridge(HWND window)
    : impl_(std::make_unique<Impl>(window))
{
}

UiaSnapshotBridge::~UiaSnapshotBridge()
{
    impl_->state->actions->setCallback({});
    impl_->pendingEventBaseline.reset();
    impl_->pendingEventLatest.reset();
    impl_->eventFlushPosted = false;
    {
        std::lock_guard<std::mutex> lock(impl_->state->mutex);
        impl_->state->detached = true;
        impl_->state->latest.reset();
    }
    // Disconnecting while servicing a synchronous cross-thread SendMessage
    // can re-enter UIA/User32 in a teardown-sensitive context. Outside that
    // context it proactively invalidates UIA's provider cache before the root
    // releases its bridge-owned reference.
    if (impl_->root && InSendMessageEx(nullptr) == ISMEX_NOSEND) {
        IRawElementProviderSimple* simple = nullptr;
        if (SUCCEEDED(impl_->root->QueryInterface(IID_PPV_ARGS(&simple)))) {
            (void)UiaDisconnectProvider(simple);
            simple->Release();
        }
    }
    if (impl_->root) impl_->root->Release();
}

void UiaSnapshotBridge::publish(AccessibilitySnapshot snapshot, WindowMetrics metrics)
{
    auto latest = std::make_shared<const SnapshotModel>(
        impl_->window, std::move(snapshot), metrics);
    std::shared_ptr<const SnapshotModel> previous;
    {
        std::lock_guard<std::mutex> lock(impl_->state->mutex);
        if (impl_->state->detached) return;
        previous = std::exchange(impl_->state->latest, latest);
    }
    // publish() is called by the UI-thread layout/update path. State is made
    // visible first so event consumers querying retained providers observe the
    // new values. UIA event delivery is deferred to a posted message: raising
    // synchronously can re-enter an MTA client that is still completing its
    // Invoke/Toggle/Value/SetFocus COM call and deadlock the UI thread.
    if (!previous) return;
    if (!impl_->pendingEventBaseline) {
        impl_->pendingEventBaseline = std::move(previous);
    }
    impl_->pendingEventLatest = latest;
    if (!impl_->eventFlushPosted) {
        impl_->eventFlushPosted = PostMessageW(
            impl_->window, eventFlushMessageId(), 0, 0) != FALSE;
    }
}

void UiaSnapshotBridge::setActionCallback(AccessibilityActionHandler callback)
{
    impl_->state->actions->setCallback(std::move(callback));
}

std::size_t UiaSnapshotBridge::dispatchPendingActions()
{
    return impl_->state->actions->dispatchPending();
}

UINT UiaSnapshotBridge::actionMessageId() noexcept
{
    static const UINT message = RegisterWindowMessageW(L"WhatsUI.UIAutomation.Action");
    return message;
}

UINT UiaSnapshotBridge::eventFlushMessageId() noexcept
{
    static const UINT message = RegisterWindowMessageW(
        L"WhatsUI.UIAutomation.EventFlush");
    return message;
}

bool UiaSnapshotBridge::handleActionMessage(UINT message)
{
    if (message == actionMessageId()) {
        (void)dispatchPendingActions();
        return true;
    }
    if (message != eventFlushMessageId()) return false;

    impl_->eventFlushPosted = false;
    auto baseline = std::exchange(impl_->pendingEventBaseline, {});
    auto latest = std::exchange(impl_->pendingEventLatest, {});
    if (baseline && latest) {
        // This registered message is handled by the native UI thread, after
        // the action SendMessage stack has unwound. Consecutive publishes are
        // coalesced from the first baseline to the newest immutable model.
        raiseSnapshotEvents(impl_->state, *baseline, *latest);
    }
    return true;
}

std::optional<LRESULT> UiaSnapshotBridge::handleWmGetObject(WPARAM wParam,
                                                             LPARAM lParam) noexcept
{
    if (static_cast<LONG>(lParam) != UiaRootObjectId) return std::nullopt;

    IRawElementProviderSimple* simple = nullptr;
    const HRESULT queryResult = impl_->root->QueryInterface(IID_PPV_ARGS(&simple));
    if (FAILED(queryResult)) {
        return static_cast<LRESULT>(0);
    }
    const LRESULT result = UiaReturnRawElementProvider(impl_->window, wParam, lParam, simple);
    simple->Release();
    return result;
}

} // namespace wui::windows

#endif
