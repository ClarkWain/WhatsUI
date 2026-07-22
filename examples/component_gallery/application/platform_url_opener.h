#pragma once

#include <string_view>

namespace whatsui::gallery {

// Opens an external HTTPS resource through the operating-system shell.
// Platform policy belongs at the application boundary; Views only emit URLs.
[[nodiscard]] bool openExternalUrl(std::string_view url) noexcept;

} // namespace whatsui::gallery
