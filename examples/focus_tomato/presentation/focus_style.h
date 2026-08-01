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
