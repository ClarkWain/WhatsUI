#include "wui/scheduler.h"
#include "wui/thread_check.h"

#include <utility>
#include <vector>

namespace wui {

namespace {

struct Entry {
    const void* key;
    std::function<void()> action;
};

std::vector<Entry>& queue()
{
    static std::vector<Entry> pending;
    return pending;
}

} // namespace

void scheduleStructuralUpdate(const void* key, std::function<void()> action)
{
    WUI_ASSERT_UI_THREAD();
    auto& pending = queue();
    for (auto& entry : pending) {
        if (entry.key == key) {
            entry.action = std::move(action);
            return;
        }
    }
    pending.push_back({key, std::move(action)});
}

void flushStructuralUpdates()
{
    WUI_ASSERT_UI_THREAD();
    auto& pending = queue();
    // A flushed action may schedule further work; drain with a guard against
    // pathological cycles.
    int guard = 0;
    while (!pending.empty() && guard++ < 64) {
        std::vector<Entry> batch;
        batch.swap(pending);
        for (auto& entry : batch) {
            if (entry.action) {
                entry.action();
            }
        }
    }
}

bool hasPendingStructuralUpdates() noexcept
{
    WUI_ASSERT_UI_THREAD();
    return !queue().empty();
}

} // namespace wui
