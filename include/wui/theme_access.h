#pragma once

#include <string_view>

namespace wui {

// Lightweight accessors for render headers that must remain safe after
// windows.h. Keeping Theme's token structures out of those headers avoids
// collisions with legacy Windows macros such as `small`.
[[nodiscard]] std::string_view activeTextFamily() noexcept;
[[nodiscard]] std::string_view activeTextFallbackFamily() noexcept;
[[nodiscard]] std::string_view activeMonospaceFamily() noexcept;

} // namespace wui
