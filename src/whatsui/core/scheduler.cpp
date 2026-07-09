#include "wui/scheduler.h"

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
    return !queue().empty();
}

} // namespace wui
