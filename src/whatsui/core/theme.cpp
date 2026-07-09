#include "wui/theme.h"

namespace wui {

namespace {
Theme g_theme{};
}

void setTheme(const Theme& theme)
{
    g_theme = theme;
}

const Theme& theme() noexcept
{
    return g_theme;
}

} // namespace wui
