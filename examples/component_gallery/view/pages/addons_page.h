#pragma once

#include <functional>
#include <memory>

#include "view_model/theme_studio_view_model.h"
#include "wui/app.h"
#include "wui/node.h"
#include "wui/theme.h"

namespace whatsui::gallery::view::pages {

// Lifetime contract: window must outlive the returned Node tree and dialogs.
// themeStudio must also outlive the tree — it stores the selected mode,
// accent, and radius preset so the Theme Studio demo reflects the active
// state after every router.refresh() rebuild.

using ApplyGalleryThemeHandler = std::function<void(wui::Theme theme, bool dark)>;

[[nodiscard]] std::unique_ptr<wui::Node> buildAddonsPage(
    wui::UiWindow& window,
    ThemeStudioViewModel& themeStudio,
    ApplyGalleryThemeHandler applyTheme);

} // namespace whatsui::gallery::view::pages
