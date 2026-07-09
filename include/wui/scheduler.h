#pragma once

// Deferred structural-update scheduler (WHATSUI_ARCHITECTURE §8 帧管线 / §11.7
// 更新规则). State changes mark work to do; structural rebuilds (If mount/unmount,
// ForEach regeneration) are queued and flushed at a safe point (frame boundary),
// never synchronously inside an event handler. This lets a handler mutate State
// without destroying the node it is currently running in.

#include <functional>

namespace wui {

// Queue a coalesced structural update. Repeated schedules for the same `key`
// (typically a node pointer) collapse into the latest action.
void scheduleStructuralUpdate(const void* key, std::function<void()> action);

// Run all queued structural updates. Call once per frame, before layout.
void flushStructuralUpdates();

// Whether any structural updates are pending.
[[nodiscard]] bool hasPendingStructuralUpdates() noexcept;

} // namespace wui
