#pragma once

// Extensions for composing Fluent themes and resolving visual-state values.
//
// The existing wui::theme()/setTheme() pair remains the application default.
// ThemeScope deliberately does not mutate it: a subtree can derive a local
// theme without changing an adjacent subtree or another window.

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "wui/node.h"
#include "wui/theme.h"

namespace wui {

// Fluent dark tokens for Windows desktop content. Layout, typography, and
// control sizing intentionally keep the light-theme defaults so switching
// appearance does not make a page reflow.
[[nodiscard]] inline Theme fluentDarkTheme()
{
    Theme dark;
    dark.colors.neutralBackground1 = {{41, 41, 41, 255}, {51, 51, 51, 255},
                                      {61, 61, 61, 255}, {61, 61, 61, 255}};
    dark.colors.neutralBackground2 = {{31, 31, 31, 255}, {41, 41, 41, 255},
                                      {51, 51, 51, 255}, {51, 51, 51, 255}};
    dark.colors.neutralBackground3 = {{20, 20, 20, 255}, {31, 31, 31, 255},
                                      {41, 41, 41, 255}, {41, 41, 41, 255}};
    dark.colors.neutralCardBackground = dark.colors.neutralBackground1;
    dark.colors.brandBackground = {{71, 158, 245, 255}, {98, 171, 245, 255},
                                   {40, 134, 222, 255}, {40, 134, 222, 255}};
    dark.colors.compoundBrandForeground1 = {{71, 158, 245, 255}, {98, 171, 245, 255},
                                           {40, 134, 222, 255}, {71, 158, 245, 255}};
    dark.colors.compoundBrandStroke = dark.colors.compoundBrandForeground1;
    dark.colors.compoundBrandBackground = dark.colors.compoundBrandForeground1;
    dark.colors.neutralForeground1 = {255, 255, 255, 255};
    dark.colors.neutralForeground2 = {214, 214, 214, 255};
    dark.colors.neutralForeground3 = {173, 173, 173, 255};
    dark.colors.neutralForegroundDisabled = {92, 92, 92, 255};
    dark.colors.neutralBackgroundDisabled = {20, 20, 20, 255};
    dark.colors.neutralStroke1 = {102, 102, 102, 255};
    dark.colors.neutralStroke1Hover = {117, 117, 117, 255};
    dark.colors.neutralStroke1Pressed = {107, 107, 107, 255};
    dark.colors.neutralStroke1Selected = {112, 112, 112, 255};
    dark.colors.neutralStrokeAccessible = {173, 173, 173, 255};
    dark.colors.neutralStrokeAccessibleHover = {189, 189, 189, 255};
    dark.colors.neutralStrokeAccessiblePressed = {179, 179, 179, 255};
    dark.colors.neutralStrokeDisabled = {66, 66, 66, 255};
    dark.colors.strokeFocusOuter = {0, 0, 0, 255};
    dark.colors.strokeFocusInner = {255, 255, 255, 255};
    dark.colors.brandForeground1 = {71, 158, 245, 255};
    dark.colors.onBrand = {0, 0, 0, 255};
    dark.colors.brandShadowAmbient = {0, 0, 0, 77};
    dark.colors.brandShadowKey = {0, 0, 0, 64};
    dark.colors.scrim = {0, 0, 0, 128};

    dark.colors.background = dark.colors.neutralBackground2.rest;
    dark.colors.surface = dark.colors.neutralBackground1.rest;
    dark.colors.surfaceRaised = dark.colors.neutralCardBackground.rest;
    dark.colors.surfaceAlt = dark.colors.neutralBackground3.rest;
    dark.colors.surfaceHover = dark.colors.neutralBackground1.hover;
    dark.colors.surfacePressed = dark.colors.neutralBackground1.pressed;
    dark.colors.text = dark.colors.neutralForeground1;
    dark.colors.textMuted = dark.colors.neutralForeground3;
    dark.colors.textDisabled = dark.colors.neutralForegroundDisabled;
    dark.colors.accent = dark.colors.brandBackground.rest;
    dark.colors.accentHover = dark.colors.brandBackground.hover;
    dark.colors.accentPressed = dark.colors.brandBackground.pressed;
    dark.colors.onAccent = {0, 0, 0, 255};
    dark.colors.border = dark.colors.neutralStroke1;
    dark.colors.borderStrong = dark.colors.neutralStrokeAccessible;
    dark.colors.focus = dark.colors.strokeFocusInner;
    dark.colors.disabled = {51, 50, 48, 255};
    dark.colors.info = {96, 205, 255, 255};
    dark.colors.danger = {255, 153, 164, 255};
    dark.colors.success = {84, 189, 93, 255};
    dark.colors.warning = {252, 225, 0, 255};
    for (ElevationToken* token : {&dark.elevation.shadow2, &dark.elevation.shadow4,
                                  &dark.elevation.shadow8, &dark.elevation.shadow16,
                                  &dark.elevation.shadow28, &dark.elevation.shadow64}) {
        token->ambient.color = {0, 0, 0, 61};
        token->key.color = {0, 0, 0, 71};
    }
    return dark;
}

// A category-level override.  Replacing categories rather than individual
// fields makes inheritance predictable: a scope either owns its color (or
// spacing/radius/etc.) contract or inherits that category unchanged.
struct ThemeOverride {
    std::optional<ColorTokens> colors;
    std::optional<ElevationTokens> elevation;
    std::optional<SpacingTokens> spacing;
    std::optional<RadiusTokens> radius;
    std::optional<StrokeTokens> stroke;
    std::optional<MotionTokens> motion;
    std::optional<TypographyTokens> typography;
    std::optional<ControlTokens> controls;

    [[nodiscard]] Theme apply(const Theme& inherited) const
    {
        Theme resolved = inherited;
        if (colors) resolved.colors = *colors;
        if (elevation) resolved.elevation = *elevation;
        if (spacing) resolved.spacing = *spacing;
        if (radius) resolved.radius = *radius;
        if (stroke) resolved.stroke = *stroke;
        if (motion) resolved.motion = *motion;
        if (typography) resolved.typography = *typography;
        if (controls) resolved.controls = *controls;
        return resolved;
    }
};

// An immutable, lexical theme scope.  Construct nested scopes from a parent's
// resolved theme; the parent and the process-wide default are never modified.
class ThemeScope {
public:
    explicit ThemeScope(const Theme& inherited, ThemeOverride override = {})
        : resolved_(override.apply(inherited))
    {
    }

    explicit ThemeScope(const ThemeScope& parent, ThemeOverride override = {})
        : ThemeScope(parent.resolvedTheme(), std::move(override))
    {
    }

    [[nodiscard]] const Theme& resolvedTheme() const noexcept
    {
        return resolved_;
    }

private:
    Theme resolved_;
};

// Builds a state mask without exposing bit-twiddling to individual controls.
// It is also useful for rules such as {Hovered, Focused}.
[[nodiscard]] constexpr ControlVisualStates visualStateMask(ControlVisualState state) noexcept
{
    return toMask(state);
}

template <typename... States>
[[nodiscard]] constexpr ControlVisualStates visualStateMask(ControlVisualState first, States... rest) noexcept
{
    static_assert((std::is_same_v<States, ControlVisualState> && ...),
                  "visualStateMask only accepts ControlVisualState values");
    return (toMask(first) | ... | toMask(rest));
}

// A reusable property resolver for visual states.  More-specific rules win;
// for equally specific matches the Fluent priority is Disabled > Pressed >
// Hovered > Focused, then the most recently assigned rule wins.  This gives
// every control one deterministic rule set instead of bespoke if-chains.
template <typename T>
class StateProperty {
public:
    explicit StateProperty(T value)
        : fallback_(std::move(value))
    {
    }

    StateProperty(const StateProperty&) = default;
    StateProperty(StateProperty&&) noexcept = default;
    StateProperty& operator=(const StateProperty&) = default;
    StateProperty& operator=(StateProperty&&) noexcept = default;

    StateProperty& set(ControlVisualState state, T value)
    {
        return set(toMask(state), std::move(value));
    }

    StateProperty& set(ControlVisualStates requiredStates, T value)
    {
        for (auto& rule : rules_) {
            if (rule.requiredStates == requiredStates) {
                rule.value = std::move(value);
                rule.order = nextOrder_++;
                return *this;
            }
        }
        rules_.push_back({requiredStates, std::move(value), nextOrder_++});
        return *this;
    }

    [[nodiscard]] const T& resolve(ControlVisualStates states) const noexcept
    {
        const Rule* chosen = nullptr;
        for (const auto& rule : rules_) {
            if (!matches(states, rule.requiredStates)) {
                continue;
            }
            if (!chosen || isBetter(rule, *chosen)) {
                chosen = &rule;
            }
        }
        return chosen ? chosen->value : fallback_;
    }

    [[nodiscard]] const T& resolve(ControlVisualState state) const noexcept
    {
        return resolve(toMask(state));
    }

    [[nodiscard]] const T& fallback() const noexcept
    {
        return fallback_;
    }

private:
    struct Rule {
        ControlVisualStates requiredStates;
        T value;
        std::uint64_t order;
    };

    [[nodiscard]] static constexpr bool matches(ControlVisualStates actual,
                                                 ControlVisualStates required) noexcept
    {
        return (actual & required) == required;
    }

    [[nodiscard]] static constexpr int bitCount(ControlVisualStates states) noexcept
    {
        int count = 0;
        while (states != 0) {
            count += static_cast<int>(states & 1u);
            states >>= 1u;
        }
        return count;
    }

    [[nodiscard]] static constexpr int fluentPriority(ControlVisualStates states) noexcept
    {
        if ((states & toMask(ControlVisualState::Disabled)) != 0) return 4;
        if ((states & toMask(ControlVisualState::Pressed)) != 0) return 3;
        if ((states & toMask(ControlVisualState::Hovered)) != 0) return 2;
        if ((states & toMask(ControlVisualState::Focused)) != 0) return 1;
        return 0;
    }

    [[nodiscard]] static constexpr bool isBetter(const Rule& candidate, const Rule& current) noexcept
    {
        const int candidateSpecificity = bitCount(candidate.requiredStates);
        const int currentSpecificity = bitCount(current.requiredStates);
        if (candidateSpecificity != currentSpecificity) {
            return candidateSpecificity > currentSpecificity;
        }
        const int candidatePriority = fluentPriority(candidate.requiredStates);
        const int currentPriority = fluentPriority(current.requiredStates);
        if (candidatePriority != currentPriority) {
            return candidatePriority > currentPriority;
        }
        return candidate.order > current.order;
    }

    T fallback_;
    std::vector<Rule> rules_;
    std::uint64_t nextOrder_{0};
};

} // namespace wui
