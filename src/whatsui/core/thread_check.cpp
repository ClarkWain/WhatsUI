#include "wui/thread_check.h"

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

} // namespace

void registerUiThread() noexcept
{
    uiThreadIdStorage() = std::this_thread::get_id();
    uiThreadRegisteredFlag() = true;
}

bool isOnUiThread() noexcept
{
    if (!uiThreadRegisteredFlag()) {
        return true; // No thread registered - skip assertion
    }
    return std::this_thread::get_id() == uiThreadIdStorage();
}

} // namespace wui
