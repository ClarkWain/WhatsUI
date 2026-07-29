#include "view_model/theme_studio_view_model.h"

#include "wui/theme_extensions.h"

namespace whatsui::gallery {
namespace {

// Each accent overlay swaps the compound brand tokens and the semantic
// accent colors. Rest / hover / pressed / selected quads are hand-tuned to
// stay legible against the Fluent light and dark surfaces, so the same
// preset works in both modes without extra branching.
struct AccentPalette {
    wui::Color rest;
    wui::Color hover;
    wui::Color pressed;
    wui::Color selected;
};

wui::ColorTokens::Interaction toInteraction(const AccentPalette& palette) noexcept
{
    return {palette.rest, palette.hover, palette.pressed, palette.selected};
}

AccentPalette paletteFor(ThemeStudioAccent accent) noexcept
{
    switch (accent) {
    case ThemeStudioAccent::Blue:
        // Matches the Fluent default so the "Blue" option acts as an
        // explicit revert rather than a silent no-op.
        return {{15, 108, 189, 255}, {17, 94, 163, 255},
                {12, 59, 94, 255}, {23, 121, 194, 255}};
    case ThemeStudioAccent::Violet:
        return {{113, 96, 232, 255}, {98, 82, 212, 255},
                {73, 57, 170, 255}, {87, 71, 194, 255}};
    case ThemeStudioAccent::Teal:
        return {{0, 156, 168, 255}, {0, 138, 149, 255},
                {0, 105, 114, 255}, {0, 121, 131, 255}};
    case ThemeStudioAccent::Rose:
        return {{219, 87, 133, 255}, {197, 72, 117, 255},
                {162, 55, 95, 255}, {182, 64, 108, 255}};
    case ThemeStudioAccent::Green:
        return {{16, 137, 62, 255}, {13, 118, 53, 255},
                {10, 92, 42, 255}, {12, 106, 48, 255}};
    case ThemeStudioAccent::Orange:
        return {{202, 80, 16, 255}, {181, 71, 14, 255},
                {143, 55, 10, 255}, {162, 62, 12, 255}};
    }
    return {{15, 108, 189, 255}, {17, 94, 163, 255},
            {12, 59, 94, 255}, {23, 121, 194, 255}};
}

void applyAccent(wui::Theme& theme, ThemeStudioAccent accent)
{
    const auto palette = paletteFor(accent);
    const auto interaction = toInteraction(palette);
    theme.colors.brandBackground = interaction;
    theme.colors.compoundBrandForeground1 = interaction;
    theme.colors.compoundBrandStroke = interaction;
    theme.colors.compoundBrandBackground = interaction;
    theme.colors.brandForeground1 = palette.rest;
    theme.colors.accent = palette.rest;
    theme.colors.accentHover = palette.hover;
    theme.colors.accentPressed = palette.pressed;
}

void applySoftRadius(wui::Theme& theme)
{
    theme.radius.small = 4.0f;
    theme.radius.medium = 8.0f;
    theme.radius.large = 10.0f;
    theme.radius.xLarge = 12.0f;
    theme.radius.xxLarge = 16.0f;
    theme.radius.sm = 8.0f;
    theme.radius.md = 10.0f;
    theme.radius.lg = 12.0f;
}

} // namespace

wui::Theme ThemeStudioViewModel::buildTheme() const
{
    wui::Theme theme = mode_ == ThemeStudioMode::Dark
        ? wui::fluentDarkTheme()
        : wui::Theme{};
    applyAccent(theme, accent_);
    if (radius_ == ThemeStudioRadius::Soft) applySoftRadius(theme);
    return theme;
}

} // namespace whatsui::gallery
