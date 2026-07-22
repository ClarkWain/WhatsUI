#pragma once

#include <functional>
#include <memory>

#include "wui/app.h"
#include "wui/node.h"
#include "wui/theme.h"

namespace whatsui::gallery::view::pages {

// Lifetime contract: window must outlive the returned Node tree and dialogs.

using ApplyGalleryThemeHandler = std::function<void(wui::Theme theme, bool dark)>;

[[nodiscard]] std::unique_ptr<wui::Node> buildAddonsPage(
    wui::UiWindow& window,
    ApplyGalleryThemeHandler applyTheme);

} // namespace whatsui::gallery::view::pages
