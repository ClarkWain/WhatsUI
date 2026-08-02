#include "wui/thread_check.h"

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace wui {

namespace {

std::vector<std::thread::id>& uiThreadIds() noexcept
{
    static std::vector<std::thread::id> ids;
    return ids;
}

std::mutex& uiThreadMutex() noexcept
{
    static std::mutex mutex;
    return mutex;
}

} // namespace

void registerUiThread() noexcept
{
    std::lock_guard<std::mutex> lock(uiThreadMutex());
    const auto current = std::this_thread::get_id();
    if (std::find(uiThreadIds().begin(), uiThreadIds().end(), current)
        == uiThreadIds().end()) {
        uiThreadIds().push_back(current);
    }
}

bool isOnUiThread() noexcept
{
    std::lock_guard<std::mutex> lock(uiThreadMutex());
    if (uiThreadIds().empty()) {
        return true; // No thread registered - skip assertion
    }
    const auto current = std::this_thread::get_id();
    return std::find(uiThreadIds().begin(), uiThreadIds().end(), current)
        != uiThreadIds().end();
}

void requireUiThread()
{
    if (!isOnUiThread()) {
        throw std::logic_error(
            "This operation must run on a registered UI thread");
    }
}

} // namespace wui
