#pragma once

// Theme tokens (ADR-004 §12). A small, strongly-typed visual language: colors,
// spacing, and radius. Widgets read the process-wide theme; there is no CSS
// cascade or selector engine. Set once at startup (or per subtree later).

#include "wui/types.h"

namespace wui {

struct ColorTokens {
    Color surface{248, 249, 251, 255};
    Color surfaceAlt{238, 240, 244, 255};
    Color text{28, 30, 34, 255};
    Color textMuted{110, 116, 128, 255};
    Color accent{34, 114, 229, 255};
    Color onAccent{255, 255, 255, 255};
    Color border{210, 214, 222, 255};
    Color danger{221, 61, 61, 255};
    Color success{40, 168, 110, 255};
};

struct SpacingTokens {
    float xs{4.0f};
    float sm{8.0f};
    float md{12.0f};
    float lg{16.0f};
    float xl{24.0f};
};

struct RadiusTokens {
    float sm{4.0f};
    float md{8.0f};
    float lg{16.0f};
};

struct Theme {
    ColorTokens colors{};
    SpacingTokens spacing{};
    RadiusTokens radius{};
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
