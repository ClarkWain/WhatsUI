#pragma once

// Fluent-inspired light design tokens. These semantic roles deliberately avoid
// tying widgets to a particular screen: applications get a coherent Windows
// default while remaining free to replace the complete Theme at startup.

#include "wui/types.h"

namespace wui {

struct ColorTokens {
    // Neutral surfaces: canvas, elevated card/dialog, and subtle input fill.
    Color background{250, 249, 248, 255};
    Color surface{255, 255, 255, 255};
    Color surfaceAlt{245, 244, 242, 255};
    Color surfaceHover{245, 244, 242, 255};
    Color surfacePressed{237, 235, 233, 255};
    // Typography and strokes use the same high-contrast neutral family.
    Color text{32, 31, 30, 255};
    Color textMuted{96, 94, 92, 255};
    Color textDisabled{161, 159, 157, 255};
    Color accent{15, 108, 189, 255};
    Color accentHover{17, 94, 163, 255};
    Color accentPressed{12, 82, 146, 255};
    Color onAccent{255, 255, 255, 255};
    Color border{216, 214, 212, 255};
    Color borderStrong{138, 136, 134, 255};
    Color focus{0, 120, 212, 255};
    Color disabled{243, 242, 241, 255};
    Color danger{196, 49, 75, 255};
    Color success{16, 124, 16, 255};
    Color scrim{0, 0, 0, 82};
};

struct SpacingTokens {
    float xs{4.0f};
    float sm{8.0f};
    float md{12.0f};
    float lg{16.0f};
    float xl{24.0f};
    float xxl{32.0f};
};

struct RadiusTokens {
    float sm{4.0f};
    float md{6.0f};
    float lg{8.0f};
    float pill{999.0f};
};

struct TypographyTokens {
    float caption{12.0f};
    float body{14.0f};
    float bodyLarge{16.0f};
    float subtitle{20.0f};
    float title{28.0f};
    float bodyLineHeight{20.0f};
};

struct ControlTokens {
    float height{32.0f};
    float compactHeight{24.0f};
    float horizontalPadding{12.0f};
    float focusInset{2.0f};
    float focusWidth{2.0f};
    float checkboxSize{18.0f};
};

struct Theme {
    ColorTokens colors{};
    SpacingTokens spacing{};
    RadiusTokens radius{};
    TypographyTokens typography{};
    ControlTokens controls{};
};

// Process-wide theme. Widgets read this during paint.
void setTheme(const Theme& theme);
[[nodiscard]] const Theme& theme() noexcept;

// Scale a color's RGB by `factor` (for hover/pressed control-state tints).
[[nodiscard]] inline Color scaleColor(Color color, float factor) noexcept
{
    auto clamp = [](float v) -> std::uint8_t {
        if (v < 0.0f) {
            v = 0.0f;
        }
        if (v > 255.0f) {
            v = 255.0f;
        }
        return static_cast<std::uint8_t>(v);
    };
    return Color{clamp(color.r * factor), clamp(color.g * factor), clamp(color.b * factor), color.a};
}

} // namespace wui
