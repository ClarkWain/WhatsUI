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

class UiaSnapshotBridge {
public:
    explicit UiaSnapshotBridge(HWND window);
    ~UiaSnapshotBridge();

    UiaSnapshotBridge(const UiaSnapshotBridge&) = delete;
    UiaSnapshotBridge& operator=(const UiaSnapshotBridge&) = delete;

    void publish(AccessibilitySnapshot snapshot, WindowMetrics metrics);
    [[nodiscard]] std::optional<LRESULT> handleWmGetObject(WPARAM wParam,
                                                           LPARAM lParam) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wui::windows

#endif
