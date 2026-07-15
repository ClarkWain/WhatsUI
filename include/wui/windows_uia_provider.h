#pragma once

// Windows UI Automation projection for an immutable accessibility snapshot.
//
// This header deliberately exposes no Windows types on other platforms. The
// GLFW host owns WM_GETOBJECT routing; this module only implements the COM
// provider tree and never retains WhatsUI Nodes.

#include "wui/accessibility.h"
#include "wui/platform.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Unknwn.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>

#include <memory>
#include <optional>

namespace wui::windows {

// Thread-safe bounded command queue shared by every provider created for a
// native window. COM pattern calls only enqueue. dispatchPending() invokes the
// callback synchronously on its caller, which is expected to be the UI thread.
class UiaActionQueue {
public:
    UiaActionQueue();
    ~UiaActionQueue();

    UiaActionQueue(const UiaActionQueue&) = delete;
    UiaActionQueue& operator=(const UiaActionQueue&) = delete;

    void setCallback(AccessibilityActionHandler callback);
    [[nodiscard]] std::size_t dispatchPending();
    [[nodiscard]] HRESULT submit(HWND window, UINT message,
                                 AccessibilityActionRequest request) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Creates a UIA fragment root whose complete semantic state is frozen in
// snapshot. Bounds are interpreted as logical client coordinates and are
// converted to physical screen pixels using the actual client/logical ratio.
//
// The returned object has one caller-owned COM reference. Its lifetime is
// independent of snapshot and of the visual Node tree.
[[nodiscard]] HRESULT createUiaSnapshotProvider(
    HWND window,
    AccessibilitySnapshot snapshot,
    WindowMetrics metrics,
    IRawElementProviderFragmentRoot** provider) noexcept;

[[nodiscard]] HRESULT createUiaSnapshotProvider(
    HWND window,
    AccessibilitySnapshot snapshot,
    WindowMetrics metrics,
    std::shared_ptr<UiaActionQueue> actions,
    IRawElementProviderFragmentRoot** provider) noexcept;

class UiaSnapshotBridge {
public:
    explicit UiaSnapshotBridge(HWND window);
    ~UiaSnapshotBridge();

    UiaSnapshotBridge(const UiaSnapshotBridge&) = delete;
    UiaSnapshotBridge& operator=(const UiaSnapshotBridge&) = delete;

    void publish(AccessibilitySnapshot snapshot, WindowMetrics metrics);
    void setActionCallback(AccessibilityActionHandler callback);
    [[nodiscard]] std::size_t dispatchPendingActions();
    [[nodiscard]] static UINT actionMessageId() noexcept;
    [[nodiscard]] bool handleActionMessage(UINT message);
    [[nodiscard]] std::optional<LRESULT> handleWmGetObject(WPARAM wParam,
                                                           LPARAM lParam) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wui::windows

#endif
