#pragma once

// ThemeStudioViewModel — carries the selection state for the Theme Studio
// demo on the Add-ons page. State survives across router.refresh() rebuilds
// because it lives on the long-lived GalleryViewModels, not inside the page
// tree. Each preset toggles its own axis (mode / accent / radius); the
// composed wui::Theme is what actually drives wui::setTheme + refresh, so
// pressing "Violet" while on Dark keeps you on Dark with a violet accent
// instead of resetting the whole theme to defaults.

#include "wui/theme.h"

namespace whatsui::gallery {

enum class ThemeStudioMode {
    Light,
    Dark,
};

enum class ThemeStudioAccent {
    Blue,
    Violet,
    Teal,
    Rose,
    Green,
    Orange,
};

enum class ThemeStudioRadius {
    Default,
    Soft,
};

class ThemeStudioViewModel {
public:
    [[nodiscard]] ThemeStudioMode mode() const noexcept { return mode_; }
    [[nodiscard]] ThemeStudioAccent accent() const noexcept { return accent_; }
    [[nodiscard]] ThemeStudioRadius radius() const noexcept { return radius_; }

    void setMode(ThemeStudioMode value) noexcept { mode_ = value; }
    void setAccent(ThemeStudioAccent value) noexcept { accent_ = value; }
    void setRadius(ThemeStudioRadius value) noexcept { radius_ = value; }

    [[nodiscard]] bool isDark() const noexcept { return mode_ == ThemeStudioMode::Dark; }

    [[nodiscard]] wui::Theme buildTheme() const;

private:
    ThemeStudioMode mode_{ThemeStudioMode::Light};
    ThemeStudioAccent accent_{ThemeStudioAccent::Blue};
    ThemeStudioRadius radius_{ThemeStudioRadius::Default};
};

} // namespace whatsui::gallery
