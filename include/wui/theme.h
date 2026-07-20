#pragma once

// Fluent 2 light design tokens. These semantic aliases deliberately avoid
// tying widgets to a particular screen: applications get a coherent Windows
// default while remaining free to replace the complete Theme at startup.

#include "wui/types.h"

#include <string_view>

namespace wui {

struct ColorTokens {
    struct Interaction {
        Color rest{};
        Color hover{};
        Color pressed{};
        Color selected{};
    };

    // Fluent React alias colors. Every interactive surface owns its complete
    // state ramp; widgets must not synthesize colors by scaling an arbitrary
    // rest value.
    Interaction neutralBackground1{{255, 255, 255, 255}, {245, 245, 245, 255},
                                   {224, 224, 224, 255}, {224, 224, 224, 255}};
    Interaction neutralBackground2{{250, 250, 250, 255}, {245, 245, 245, 255},
                                   {224, 224, 224, 255}, {224, 224, 224, 255}};
    Interaction neutralBackground3{{245, 245, 245, 255}, {240, 240, 240, 255},
                                   {224, 224, 224, 255}, {224, 224, 224, 255}};
    Interaction neutralCardBackground{{255, 255, 255, 255}, {245, 245, 245, 255},
                                      {224, 224, 224, 255}, {224, 224, 224, 255}};
    Interaction brandBackground{{15, 108, 189, 255}, {17, 94, 163, 255},
                                {12, 59, 94, 255}, {15, 84, 140, 255}};
    // Compound brand aliases are used when a control combines a brand stroke
    // and foreground mark (Radio, checked selection indicators). Their
    // pressed value is intentionally brand[60], not the darker
    // colorBrandBackgroundPressed surface token.
    Interaction compoundBrandForeground1{{15, 108, 189, 255}, {17, 94, 163, 255},
                                         {15, 84, 140, 255}, {15, 108, 189, 255}};
    Interaction compoundBrandStroke{{15, 108, 189, 255}, {17, 94, 163, 255},
                                    {15, 84, 140, 255}, {15, 108, 189, 255}};
    Interaction compoundBrandBackground{{15, 108, 189, 255}, {17, 94, 163, 255},
                                        {15, 84, 140, 255}, {15, 108, 189, 255}};
    Interaction dangerBackground{{196, 49, 75, 255}, {179, 38, 30, 255},
                                 {117, 11, 28, 255}, {163, 38, 56, 255}};

    Color neutralForeground1{36, 36, 36, 255};
    Color neutralForeground2{66, 66, 66, 255};
    Color neutralForeground3{97, 97, 97, 255};
    Color neutralForegroundDisabled{179, 179, 179, 255};
    Color neutralStroke1{209, 209, 209, 255};
    Color neutralStroke1Hover{199, 199, 199, 255};
    Color neutralStroke1Pressed{179, 179, 179, 255};
    Color neutralStrokeAccessible{97, 97, 97, 255};
    Color neutralStrokeAccessibleHover{87, 87, 87, 255};
    Color neutralStrokeAccessiblePressed{66, 66, 66, 255};
    Color neutralStrokeDisabled{224, 224, 224, 255};
    // Fluent focus is a paired stroke, not an accent fill.
    Color strokeFocusOuter{255, 255, 255, 255};
    Color strokeFocusInner{0, 0, 0, 255};
    Color brandForeground1{15, 108, 189, 255};
    Color onBrand{255, 255, 255, 255};
    Color brandShadowAmbient{0, 0, 0, 77};
    Color brandShadowKey{0, 0, 0, 64};
    // Shared semantic aliases communicate state, never decoration.
    Color statusInfo{0, 120, 212, 255};
    Color statusSuccess{16, 124, 16, 255};
    Color statusWarning{157, 93, 0, 255};
    Color statusDanger{196, 49, 75, 255};
    Color scrim{0, 0, 0, 102};

    // Compatibility aliases. New widgets must use the named Fluent aliases
    // above; these remain until the public Theme API completes its migration.
    Color background{250, 250, 250, 255};
    Color surface{255, 255, 255, 255};
    Color surfaceRaised{255, 255, 255, 255};
    Color surfaceAlt{245, 245, 245, 255};
    Color surfaceHover{245, 245, 245, 255};
    Color surfacePressed{224, 224, 224, 255};
    Color text{36, 36, 36, 255};
    Color textMuted{97, 97, 97, 255};
    Color textDisabled{179, 179, 179, 255};
    Color accent{15, 108, 189, 255};
    Color accentHover{17, 94, 163, 255};
    Color accentPressed{12, 59, 94, 255};
    Color onAccent{255, 255, 255, 255};
    Color border{209, 209, 209, 255};
    Color borderStrong{97, 97, 97, 255};
    Color focus{0, 0, 0, 255};
    Color disabled{245, 245, 245, 255};
    Color info{0, 120, 212, 255};
    Color success{16, 124, 16, 255};
    Color warning{157, 93, 0, 255};
    Color danger{196, 49, 75, 255};
};

// A Fluent shadow contains a diffuse ambient layer and a directional key
// layer. Values are logical pixels and are scaled by PaintContext.
struct ShadowLayerToken {
    float blur{0.0f};
    float offsetX{0.0f};
    float offsetY{0.0f};
    float spread{0.0f};
    Color color{0, 0, 0, 0};
};

struct ElevationToken {
    ShadowLayerToken ambient{};
    ShadowLayerToken key{};
};

struct ElevationTokens {
    // Official Fluent ramp: ambient is centred; key defines a consistent
    // downward light source. Windows keeps a surface stroke in addition.
    ElevationToken shadow2{{2.0f, 0.0f, 0.0f, 0.0f, {0, 0, 0, 31}},
                           {2.0f, 0.0f, 1.0f, 0.0f, {0, 0, 0, 36}}};
    ElevationToken shadow4{{2.0f, 0.0f, 0.0f, 0.0f, {0, 0, 0, 31}},
                           {4.0f, 0.0f, 2.0f, 0.0f, {0, 0, 0, 36}}};
    ElevationToken shadow8{{2.0f, 0.0f, 0.0f, 0.0f, {0, 0, 0, 31}},
                           {8.0f, 0.0f, 4.0f, 0.0f, {0, 0, 0, 36}}};
    ElevationToken shadow16{{2.0f, 0.0f, 0.0f, 0.0f, {0, 0, 0, 31}},
                            {16.0f, 0.0f, 8.0f, 0.0f, {0, 0, 0, 36}}};
    ElevationToken shadow28{{8.0f, 0.0f, 0.0f, 0.0f, {0, 0, 0, 31}},
                            {28.0f, 0.0f, 14.0f, 0.0f, {0, 0, 0, 36}}};
    ElevationToken shadow64{{8.0f, 0.0f, 0.0f, 0.0f, {0, 0, 0, 31}},
                            {64.0f, 0.0f, 32.0f, 0.0f, {0, 0, 0, 36}}};
    bool useWindowsStroke{true};
};

struct SpacingTokens {
    struct Scale {
        float none{0.0f};
        float xxs{2.0f};
        float xs{4.0f};
        float sNudge{6.0f};
        float s{8.0f};
        float mNudge{10.0f};
        float m{12.0f};
        float l{16.0f};
        float xl{20.0f};
        float xxl{24.0f};
        float xxxl{32.0f};
    };
    Scale horizontal{};
    Scale vertical{};
    // Deprecated short aliases retained for source compatibility.
    float xs{4.0f};
    float sm{8.0f};
    float md{12.0f};
    float lg{16.0f};
    float xl{24.0f};
    float xxl{32.0f};
};

struct RadiusTokens {
    float none{0.0f};
    float small{2.0f};
    float medium{4.0f};
    float large{6.0f};
    float xLarge{8.0f};
    float xxLarge{12.0f};
    float xxxLarge{16.0f};
    float xxxxLarge{24.0f};
    float xxxxxLarge{32.0f};
    float xxxxxxLarge{40.0f};
    float circular{10000.0f};
    // Deprecated short aliases retained for source compatibility.
    float sm{4.0f};
    float md{6.0f};
    float lg{8.0f};
    float pill{999.0f};
};

struct StrokeTokens {
    float thin{1.0f};
    float thick{2.0f};
    float thicker{3.0f};
    float thickest{4.0f};
};

struct MotionTokens {
    // Fluent global duration aliases, expressed in seconds for Animation.
    float durationUltraFast{0.05f};
    float durationFaster{0.10f};
    float durationFast{0.15f};
    float durationNormal{0.20f};
    float durationGentle{0.25f};
    float durationSlow{0.30f};
};

// Windows 11 exposes Segoe UI Variable through DirectWrite.  Keep the
// long-established Segoe UI face alongside it as an explicit fallback token:
// applications that install a custom renderer can preserve the same contract
// on Windows versions where the variable family is unavailable.
inline constexpr std::string_view kFluentWindowsFontFamily{"Segoe UI Variable"};
inline constexpr std::string_view kFluentWindowsFontFallback{"Segoe UI"};

struct TextStyleToken {
    std::string_view family{kFluentWindowsFontFamily};
    float size{14.0f};
    int weight{400};
    float lineHeight{20.0f};
    // Kept separate from `family`: native backends select exactly one family
    // name, while font fallback engines need the ordered fallback explicitly.
    std::string_view fallbackFamily{kFluentWindowsFontFallback};
};

struct TypographyTokens {
    std::string_view familyBase{kFluentWindowsFontFamily};
    std::string_view familyBaseFallback{kFluentWindowsFontFallback};
    std::string_view familyMonospace{"Consolas"};
    std::string_view familyNumeric{"Bahnschrift"};
    float fontSizeBase100{10.0f};
    float fontSizeBase200{12.0f};
    float fontSizeBase300{14.0f};
    float fontSizeBase400{16.0f};
    float fontSizeBase500{20.0f};
    float fontSizeBase600{24.0f};
    float fontSizeHero700{28.0f};
    float fontSizeHero800{32.0f};
    float fontSizeHero900{40.0f};
    float fontSizeHero1000{68.0f};
    float lineHeightBase100{14.0f};
    float lineHeightBase200{16.0f};
    float lineHeightBase300{20.0f};
    float lineHeightBase400{22.0f};
    float lineHeightBase500{28.0f};
    float lineHeightBase600{32.0f};
    float lineHeightHero700{36.0f};
    float lineHeightHero800{40.0f};
    float lineHeightHero900{52.0f};
    float lineHeightHero1000{92.0f};
    int weightRegular{400};
    int weightMedium{500};
    int weightSemibold{600};
    int weightBold{700};
    // Web-compatible named aliases retained for existing applications. They
    // all inherit the Windows default family above; the canonical Windows
    // ramp below is the source of truth for new Windows-first components.
    TextStyleToken caption2{{kFluentWindowsFontFamily}, 10.0f, 400, 14.0f};
    TextStyleToken caption2Strong{{kFluentWindowsFontFamily}, 10.0f, 600, 14.0f};
    TextStyleToken caption1{{kFluentWindowsFontFamily}, 12.0f, 400, 16.0f};
    TextStyleToken caption1Strong{{kFluentWindowsFontFamily}, 12.0f, 600, 16.0f};
    TextStyleToken caption1Stronger{{kFluentWindowsFontFamily}, 12.0f, 700, 16.0f};
    TextStyleToken body1{{kFluentWindowsFontFamily}, 14.0f, 400, 20.0f};
    TextStyleToken body1Strong{{kFluentWindowsFontFamily}, 14.0f, 600, 20.0f};
    TextStyleToken body1Stronger{{kFluentWindowsFontFamily}, 14.0f, 700, 20.0f};
    TextStyleToken body2{{kFluentWindowsFontFamily}, 16.0f, 400, 22.0f};
    TextStyleToken subtitle2{{kFluentWindowsFontFamily}, 16.0f, 600, 22.0f};
    TextStyleToken subtitle2Stronger{{kFluentWindowsFontFamily}, 16.0f, 700, 22.0f};
    TextStyleToken subtitle1{{kFluentWindowsFontFamily}, 20.0f, 600, 28.0f};
    TextStyleToken title3{{kFluentWindowsFontFamily}, 24.0f, 600, 32.0f};
    TextStyleToken title2{{kFluentWindowsFontFamily}, 28.0f, 600, 36.0f};
    TextStyleToken title1{{kFluentWindowsFontFamily}, 32.0f, 600, 40.0f};
    TextStyleToken largeTitle{{kFluentWindowsFontFamily}, 40.0f, 600, 52.0f};
    TextStyleToken display{{kFluentWindowsFontFamily}, 68.0f, 600, 92.0f};

    // Fluent 2 Windows type ramp. These exact eight roles intentionally do
    // not mirror the more granular Web ramp above: Windows specifies one
    // caption, body, body-large, subtitle and title scale. Keep this nested
    // group additive so the historical API remains source compatible.
    struct WindowsRamp {
        TextStyleToken caption{{kFluentWindowsFontFamily}, 12.0f, 400, 16.0f};
        TextStyleToken body{{kFluentWindowsFontFamily}, 14.0f, 400, 20.0f};
        TextStyleToken bodyStrong{{kFluentWindowsFontFamily}, 14.0f, 600, 20.0f};
        TextStyleToken bodyLarge{{kFluentWindowsFontFamily}, 18.0f, 400, 24.0f};
        TextStyleToken subtitle{{kFluentWindowsFontFamily}, 20.0f, 600, 28.0f};
        TextStyleToken title{{kFluentWindowsFontFamily}, 28.0f, 600, 36.0f};
        TextStyleToken largeTitle{{kFluentWindowsFontFamily}, 40.0f, 600, 52.0f};
        TextStyleToken display{{kFluentWindowsFontFamily}, 68.0f, 600, 92.0f};
    } windows{};

    // Deprecated convenience aliases; new widgets use named styles.
    // Windows desktop defaults use even logical sizes so common fractional
    // scales (notably 150%) land on whole physical-pixel font heights.
    float caption{14.0f};
    float body{16.0f};
    float bodyLarge{18.0f};
    float subtitle{22.0f};
    float title{32.0f};
    float bodyLineHeight{24.0f};
};

struct ControlTokens {
    // Fluent medium controls are 32 logical pixels. Component-specific small
    // and large variants expose 24 and 40 without redefining the theme.
    float height{32.0f};
    float compactHeight{24.0f};
    float horizontalPadding{12.0f};
    float focusInset{2.0f};
    float focusWidth{2.0f};
    float checkboxSize{16.0f};
};

struct Theme {
    ColorTokens colors{};
    ElevationTokens elevation{};
    SpacingTokens spacing{};
    RadiusTokens radius{};
    StrokeTokens stroke{};
    MotionTokens motion{};
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

// Fluent brand shadows compensate for the luminance of a colored surface so
// the perceived elevation remains consistent.  The official equations use
// 0.2126R + 0.7152G + 0.0722B and return an opacity percentage.
[[nodiscard]] inline Color fluentBrandShadow(Color surface, bool keyLayer) noexcept
{
    const float luminance = 0.2126f * surface.r + 0.7152f * surface.g + 0.0722f * surface.b;
    const float opacityPercent = keyLayer ? 42.0f - 0.116f * luminance
                                          : 34.0f - 0.09f * luminance;
    const float alpha = std::clamp(opacityPercent, 0.0f, 100.0f) * 2.55f;
    return Color{0, 0, 0, static_cast<std::uint8_t>(alpha + 0.5f)};
}

} // namespace wui
