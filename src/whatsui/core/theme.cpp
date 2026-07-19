#include "wui/theme.h"
#include "wui/theme_access.h"

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

std::string_view activeTextFamily() noexcept
{
    return g_theme.typography.familyBase;
}

std::string_view activeTextFallbackFamily() noexcept
{
    return g_theme.typography.familyBaseFallback;
}

std::string_view activeMonospaceFamily() noexcept
{
    return g_theme.typography.familyMonospace;
}

} // namespace wui
