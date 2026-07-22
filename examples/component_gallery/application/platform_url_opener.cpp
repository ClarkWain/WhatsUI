#include "application/platform_url_opener.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

#include <cstdint>
#include <string>

namespace whatsui::gallery {

bool openExternalUrl(std::string_view url) noexcept
{
    if (url.rfind("https://", 0) != 0) return false;
#if defined(_WIN32)
    const std::string terminated(url);
    const auto result = ShellExecuteA(nullptr, "open", terminated.c_str(), nullptr,
                                      nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<std::intptr_t>(result) > 32;
#else
    (void)url;
    return false;
#endif
}

} // namespace whatsui::gallery
