#pragma once

#include "wui/theme.h"
#include "wui/types.h"

namespace whatsui::focus_tomato::presentation::style {

inline constexpr wui::Color canvas{248, 243, 235, 255};
inline constexpr wui::Color surface{255, 252, 247, 255};
inline constexpr wui::Color border{238, 230, 218, 255};
inline constexpr wui::Color textPrimary{45, 41, 38, 255};
inline constexpr wui::Color textSecondary{110, 104, 99, 255};
inline constexpr wui::Color textMuted{164, 157, 150, 255};
inline constexpr wui::Color accent{241, 91, 78, 255};
inline constexpr wui::Color actionPrimary{184, 58, 50, 255};
inline constexpr wui::Color actionPrimaryHover{163, 48, 41, 255};
inline constexpr wui::Color actionPrimaryPressed{140, 41, 31, 255};
inline constexpr wui::Color successSurface{234, 242, 223, 255};
inline constexpr wui::Color success{110, 163, 72, 255};
inline constexpr wui::Color transparent{0, 0, 0, 0};

[[nodiscard]] inline wui::Theme focusTheme()
{
    wui::Theme value;
    const wui::Color surfaceHover{250, 246, 240, 255};
    const wui::Color surfacePressed{238, 230, 218, 255};
    const wui::Color surfaceSelected{255, 238, 232, 255};
    const wui::Color surfaceAlt{252, 248, 242, 255};
    const wui::Color canvasPressed{232, 224, 213, 255};
    const wui::Color dangerRest{194, 52, 52, 255};
    const wui::Color dangerHover{168, 43, 39, 255};
    const wui::Color dangerPressed{137, 34, 29, 255};

    value.colors.neutralBackground1 = {
        surface, surfaceHover, surfacePressed, surfaceSelected};
    value.colors.neutralBackground2 = {
        canvas, surfaceAlt, surfacePressed, surfacePressed};
    value.colors.neutralBackground3 = {
        surfaceAlt, canvas, canvasPressed, surfacePressed};
    value.colors.neutralCardBackground = {
        surface, surfaceHover, surfacePressed, surfaceSelected};

    value.colors.brandBackground = {
        actionPrimary,
        actionPrimaryHover,
        actionPrimaryPressed,
        wui::Color{168, 52, 44, 255}};
    value.colors.compoundBrandForeground1 = {
        accent,
        actionPrimary,
        actionPrimaryPressed,
        accent};
    value.colors.compoundBrandStroke =
        value.colors.compoundBrandForeground1;
    value.colors.compoundBrandBackground =
        value.colors.compoundBrandForeground1;
    value.colors.dangerBackground = {
        dangerRest,
        dangerHover,
        dangerPressed,
        wui::Color{171, 43, 43, 255}};

    value.colors.neutralForeground1 = textPrimary;
    value.colors.neutralForeground2 = textSecondary;
    value.colors.neutralForeground3 = textMuted;
    value.colors.neutralForegroundDisabled = wui::Color{185, 178, 170, 255};
    value.colors.neutralBackgroundDisabled = wui::Color{244, 239, 232, 255};
    value.colors.neutralStroke1 = border;
    value.colors.neutralStroke1Hover = wui::Color{220, 209, 196, 255};
    value.colors.neutralStroke1Pressed = wui::Color{193, 180, 165, 255};
    value.colors.neutralStroke1Selected = wui::Color{205, 191, 175, 255};
    value.colors.neutralStrokeAccessible = textSecondary;
    value.colors.neutralStrokeAccessibleHover = textPrimary;
    value.colors.neutralStrokeAccessiblePressed = actionPrimaryPressed;
    value.colors.neutralStrokeDisabled = wui::Color{228, 221, 212, 255};
    value.colors.brandForeground1 = actionPrimary;
    value.colors.onBrand = surface;
    value.colors.statusInfo = actionPrimary;
    value.colors.statusSuccess = success;
    value.colors.statusDanger = dangerRest;

    value.colors.background = canvas;
    value.colors.surface = surface;
    value.colors.surfaceRaised = surface;
    value.colors.surfaceAlt = surfaceAlt;
    value.colors.surfaceHover = surfaceHover;
    value.colors.surfacePressed = surfacePressed;
    value.colors.text = textPrimary;
    value.colors.textMuted = textSecondary;
    value.colors.textDisabled = value.colors.neutralForegroundDisabled;
    value.colors.accent = actionPrimary;
    value.colors.accentHover = actionPrimaryHover;
    value.colors.accentPressed = actionPrimaryPressed;
    value.colors.onAccent = surface;
    value.colors.border = border;
    value.colors.borderStrong = textSecondary;
    value.colors.disabled = value.colors.neutralBackgroundDisabled;
    value.colors.info = actionPrimary;
    value.colors.success = success;
    value.colors.danger = dangerRest;
    return value;
}

inline wui::TextStyleToken text(float size, int weight = 400,
                                float lineHeight = 0.0f)
{
    auto token = wui::theme().typography.windows.body;
    token.size = size;
    token.weight = weight;
    token.lineHeight = lineHeight > 0.0f ? lineHeight : size * 1.45f;
    return token;
}

} // namespace whatsui::focus_tomato::presentation::style
