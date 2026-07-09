#pragma once

// UI-thread assertion utility (WHATSUI_ARCHITECTURE safety).
//
// State<T> and other UI-only types should only be accessed from the UI thread.
// This header provides a lightweight mechanism to register the UI thread and
// assert at runtime that operations occur on it. The assertion is active in
// debug builds (NDEBUG not defined) and is a no-op in release builds.

#include <cassert>
#include <thread>

namespace wui {

// Call once from the UI thread (typically at app startup) to register it.
void registerUiThread() noexcept;

// Returns true if the current thread is the registered UI thread, or if no
// UI thread has been registered yet (permissive mode for tests/simple apps).
[[nodiscard]] bool isOnUiThread() noexcept;

// Assert macro for UI-thread-only operations.
// Active only in debug builds. In release builds, compiles to nothing.
#ifdef NDEBUG
#define WUI_ASSERT_UI_THREAD() ((void)0)
#else
#define WUI_ASSERT_UI_THREAD() \
    assert(wui::isOnUiThread() && "This operation must be called from the UI thread")
#endif

} // namespace wui
