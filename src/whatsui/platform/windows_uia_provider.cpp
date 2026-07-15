#include "wui/windows_uia_provider.h"

#if defined(_WIN32)

#include <OleAuto.h>
#include <UIAutomationClient.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <iterator>
#include <string>
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
    std::vector<std::optional<std::size_t>> parents;
    std::vector<std::vector<std::size_t>> children;
    std::optional<std::size_t> semanticRoot;
    std::string windowTitle;

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
            RECT rect{};
            if (GetWindowRect(window, &rect)) {
                result.left = rect.left;
                result.top = rect.top;
                result.width = rect.right - rect.left;
                result.height = rect.bottom - rect.top;
            }
            return result;
        }
        const auto& bounds = entries[index].properties.bounds;
        if (!bounds) {
            return result;
        }
        POINT origin{};
        ClientToScreen(window, &origin);
        result.left = origin.x + static_cast<double>(bounds->x * scaleX);
        result.top = origin.y + static_cast<double>(bounds->y * scaleY);
        result.width = static_cast<double>(std::max(0.0f, bounds->width) * scaleX);
        result.height = static_cast<double>(std::max(0.0f, bounds->height) * scaleY);
        return result;
    }
};

class SnapshotProvider final : public IRawElementProviderSimple,
                               public IRawElementProviderFragment,
                               public IRawElementProviderFragmentRoot {
public:
    SnapshotProvider(std::shared_ptr<const SnapshotModel> model, std::size_t index)
        : model_(std::move(model)), index_(index)
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
        } else if (iid == __uuidof(IRawElementProviderFragmentRoot) && index_ == kSyntheticRoot) {
            *object = static_cast<IRawElementProviderFragmentRoot*>(this);
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

    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID, IUnknown** provider) override
    {
        if (!provider) return E_POINTER;
        *provider = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID id, VARIANT* value) override
    {
        if (!value) return E_POINTER;
        VariantInit(value);
        const AccessibilityProperties* properties = index_ == kSyntheticRoot
            ? (model_->semanticRoot ? &model_->entries[*model_->semanticRoot].properties : nullptr)
            : &model_->entries[index_].properties;
        switch (id) {
        case UIA_ControlTypePropertyId:
            value->vt = VT_I4;
            value->lVal = index_ == kSyntheticRoot ? UIA_WindowControlTypeId
                                                   : controlType(properties->role);
            return S_OK;
        case UIA_NamePropertyId:
            if (index_ == kSyntheticRoot && (!properties || properties->label.empty())) {
                return setBstrVariant(model_->windowTitle, value);
            }
            return setBstrVariant(properties ? properties->label : std::string{}, value);
        case UIA_HelpTextPropertyId:
            return setBstrVariant(properties ? properties->description : std::string{}, value);
        case UIA_AutomationIdPropertyId:
            return setBstrVariant(automationId(), value);
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
            value->boolVal = properties && isFocusable(properties->role) ? VARIANT_TRUE : VARIANT_FALSE;
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
            return setBstrVariant(index_ == kSyntheticRoot ? "WhatsUI.Window" : "WhatsUI.Node", value);
        default:
            return S_OK;
        }
    }

    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** provider) override
    {
        if (!provider) return E_POINTER;
        *provider = nullptr;
        return index_ == kSyntheticRoot ? UiaHostProviderFromHwnd(model_->window, provider) : S_OK;
    }

    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,
                                       IRawElementProviderFragment** result) override
    {
        if (!result) return E_POINTER;
        *result = nullptr;
        std::optional<std::size_t> target;
        if (direction == NavigateDirection_Parent) {
            if (index_ != kSyntheticRoot) {
                const std::size_t parent = model_->parents[index_].value_or(kSyntheticRoot);
                target = model_->semanticRoot && parent == *model_->semanticRoot
                    ? kSyntheticRoot : parent;
            }
        } else if (direction == NavigateDirection_FirstChild ||
                   direction == NavigateDirection_LastChild) {
            const auto& children = model_->childIndices(index_);
            if (!children.empty()) target = direction == NavigateDirection_FirstChild
                ? children.front() : children.back();
        } else if (index_ != kSyntheticRoot) {
            std::size_t parent = model_->parents[index_].value_or(kSyntheticRoot);
            if (model_->semanticRoot && parent == *model_->semanticRoot) parent = kSyntheticRoot;
            const auto& siblings = model_->childIndices(parent);
            const auto position = std::find(siblings.begin(), siblings.end(), index_);
            if (direction == NavigateDirection_NextSibling && position != siblings.end() &&
                std::next(position) != siblings.end()) target = *std::next(position);
            if (direction == NavigateDirection_PreviousSibling && position != siblings.begin() &&
                position != siblings.end()) target = *std::prev(position);
        }
        if (target) return make(*target, result);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** runtimeId) override
    {
        if (!runtimeId) return E_POINTER;
        *runtimeId = SafeArrayCreateVector(VT_I4, 0, 3);
        if (!*runtimeId) return E_OUTOFMEMORY;
        LONG* values = nullptr;
        HRESULT hr = SafeArrayAccessData(*runtimeId, reinterpret_cast<void**>(&values));
        if (FAILED(hr)) { SafeArrayDestroy(*runtimeId); *runtimeId = nullptr; return hr; }
        values[0] = UiaAppendRuntimeId;
        values[1] = static_cast<LONG>(reinterpret_cast<std::uintptr_t>(model_->window) & 0x7fffffffU);
        values[2] = runtimeComponent();
        SafeArrayUnaccessData(*runtimeId);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* rectangle) override
    {
        if (!rectangle) return E_POINTER;
        *rectangle = model_->screenBounds(index_);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** roots) override
    {
        if (!roots) return E_POINTER;
        *roots = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetFocus() override { return UIA_E_NOTSUPPORTED; }

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
        std::size_t selected = kSyntheticRoot;
        std::size_t bestDepth = 0;
        for (std::size_t index = 0; index < model_->entries.size(); ++index) {
            const UiaRect bounds = model_->screenBounds(index);
            if (bounds.width > 0.0 && bounds.height > 0.0 && x >= bounds.left && y >= bounds.top &&
                x <= bounds.left + bounds.width && y <= bounds.top + bounds.height &&
                (selected == kSyntheticRoot || model_->entries[index].depth >= bestDepth)) {
                selected = index;
                bestDepth = model_->entries[index].depth;
            }
        }
        return make(selected, provider);
    }

    HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** provider) override
    {
        if (!provider) return E_POINTER;
        *provider = nullptr;
        const auto focused = std::find_if(model_->entries.begin(), model_->entries.end(),
            [](const AccessibilitySnapshotEntry& entry) { return entry.properties.focused; });
        if (focused == model_->entries.end()) return S_OK;
        return make(static_cast<std::size_t>(std::distance(model_->entries.begin(), focused)), provider);
    }

private:
    static bool isFocusable(AccessibilityRole role) noexcept
    {
        switch (role) {
        case AccessibilityRole::Button:
        case AccessibilityRole::CheckBox:
        case AccessibilityRole::RadioButton:
        case AccessibilityRole::Switch:
        case AccessibilityRole::Slider:
        case AccessibilityRole::TextField:
        case AccessibilityRole::MenuItem: return true;
        default: return false;
        }
    }

    LONG runtimeComponent() const noexcept
    {
        if (index_ == kSyntheticRoot) return 1;
        std::uint32_t hash = 2166136261U;
        for (const std::size_t part : model_->entries[index_].path) {
            hash ^= static_cast<std::uint32_t>(part + 1U);
            hash *= 16777619U;
        }
        hash ^= static_cast<std::uint32_t>(model_->entries[index_].properties.role) + 1U;
        return static_cast<LONG>((hash & 0x7fffffffU) + 2U);
    }

    std::string automationId() const
    {
        if (index_ == kSyntheticRoot) return "window";
        std::string result{"node"};
        for (const std::size_t part : model_->entries[index_].path) {
            result.push_back('.');
            result += std::to_string(part);
        }
        return result;
    }

    HRESULT make(std::size_t index, IRawElementProviderFragment** result) const noexcept
    {
        auto* provider = new (std::nothrow) SnapshotProvider(model_, index);
        if (!provider) return E_OUTOFMEMORY;
        *result = static_cast<IRawElementProviderFragment*>(provider);
        return S_OK;
    }

    HRESULT makeRoot(IRawElementProviderFragmentRoot** result) const noexcept
    {
        auto* provider = new (std::nothrow) SnapshotProvider(model_, kSyntheticRoot);
        if (!provider) return E_OUTOFMEMORY;
        *result = static_cast<IRawElementProviderFragmentRoot*>(provider);
        return S_OK;
    }

    std::atomic<ULONG> references_{1};
    std::shared_ptr<const SnapshotModel> model_;
    std::size_t index_{kSyntheticRoot};
};

} // namespace

HRESULT createUiaSnapshotProvider(HWND window, AccessibilitySnapshot snapshot,
                                  WindowMetrics metrics,
                                  IRawElementProviderFragmentRoot** provider) noexcept
{
    if (!provider) return E_POINTER;
    *provider = nullptr;
    if (!IsWindow(window)) return E_INVALIDARG;
    try {
        auto model = std::make_shared<const SnapshotModel>(
            window, std::move(snapshot), metrics);
        auto* created = new (std::nothrow) SnapshotProvider(std::move(model), kSyntheticRoot);
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
    explicit Impl(HWND nativeWindow) noexcept : window(nativeWindow) {}
    HWND window{};
    std::mutex mutex;
    AccessibilitySnapshot snapshot;
    WindowMetrics metrics{};
};

UiaSnapshotBridge::UiaSnapshotBridge(HWND window)
    : impl_(std::make_unique<Impl>(window))
{
}

UiaSnapshotBridge::~UiaSnapshotBridge() = default;

void UiaSnapshotBridge::publish(AccessibilitySnapshot snapshot, WindowMetrics metrics)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->snapshot = std::move(snapshot);
    impl_->metrics = metrics;
}

std::optional<LRESULT> UiaSnapshotBridge::handleWmGetObject(WPARAM wParam,
                                                             LPARAM lParam) noexcept
{
    if (static_cast<LONG>(lParam) != UiaRootObjectId) return std::nullopt;

    AccessibilitySnapshot snapshot;
    WindowMetrics metrics;
    try {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        snapshot = impl_->snapshot;
        metrics = impl_->metrics;
    } catch (...) {
        return static_cast<LRESULT>(0);
    }
    IRawElementProviderFragmentRoot* root = nullptr;
    const HRESULT hr = createUiaSnapshotProvider(
        impl_->window, std::move(snapshot), metrics, &root);
    if (FAILED(hr)) return static_cast<LRESULT>(0);
    IRawElementProviderSimple* simple = nullptr;
    const HRESULT queryResult = root->QueryInterface(IID_PPV_ARGS(&simple));
    if (FAILED(queryResult)) {
        root->Release();
        return static_cast<LRESULT>(0);
    }
    const LRESULT result = UiaReturnRawElementProvider(impl_->window, wParam, lParam, simple);
    simple->Release();
    root->Release();
    return result;
}

} // namespace wui::windows

#endif
