#include "wui/thread_check.h"

#include <mutex>

namespace wui {

namespace {

std::thread::id& uiThreadIdStorage() noexcept
{
    static std::thread::id id{};
    return id;
}

bool& uiThreadRegisteredFlag() noexcept
{
    static bool registered = false;
    return registered;
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
    if (!uiThreadRegisteredFlag()) {
        uiThreadIdStorage() = std::this_thread::get_id();
        uiThreadRegisteredFlag() = true;
    }
}

bool isOnUiThread() noexcept
{
    std::lock_guard<std::mutex> lock(uiThreadMutex());
    if (!uiThreadRegisteredFlag()) {
        return true; // No thread registered - skip assertion
    }
    return std::this_thread::get_id() == uiThreadIdStorage();
}

} // namespace wui
