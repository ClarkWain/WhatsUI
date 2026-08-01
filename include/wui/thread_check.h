#pragma once

// UI-thread assertion utility (WHATSUI_ARCHITECTURE safety).
//
// State<T> and other UI-only types should only be accessed from the UI thread.
// This header provides a lightweight mechanism to register the UI thread and
// validate at runtime that operations occur on a registered UI thread.

#include <cassert>
#include <thread>

namespace wui {

// Call once from the UI thread (typically at app startup) to register it.
void registerUiThread() noexcept;

// Returns true if the current thread is the registered UI thread, or if no
// UI thread has been registered yet (permissive mode for tests/simple apps).
[[nodiscard]] bool isOnUiThread() noexcept;
void requireUiThread();

// Kept as a source-compatible internal spelling, but now validates in every
// build configuration. Context-owned objects use UiContext directly.
#define WUI_ASSERT_UI_THREAD() wui::requireUiThread()

} // namespace wui
