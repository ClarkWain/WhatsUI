#include <stdexcept>
#include <string>

#include "wui/theme.h"
#include "wui/theme_extensions.h"
#include "wui/widgets.h"

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool equal(wui::Color left, wui::Color right)
{
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

void testFluentDarkTokens()
{
    const wui::Theme dark = wui::fluentDarkTheme();
    expect(equal(dark.colors.background, {31, 31, 31, 255}),
           "Fluent dark must expose neutralBackground2");
    expect(equal(dark.colors.surfaceRaised, {41, 41, 41, 255}),
           "Fluent dark raised surfaces must use neutralBackground1");
    expect(equal(dark.colors.accent, {71, 158, 245, 255}),
           "Fluent dark must expose its accessible blue accent");
    expect(equal(dark.colors.onAccent, {0, 0, 0, 255}),
           "Fluent dark accent foreground must remain legible");
    expect(dark.elevation.shadow16.ambient.color.a == 61 &&
               dark.elevation.shadow16.key.color.a == 71,
           "Fluent dark elevation must retain ambient and key layers");
    expect(dark.typography.body == wui::Theme{}.typography.body &&
               dark.controls.height == wui::Theme{}.controls.height,
           "Appearance changes must preserve Fluent layout and control metrics");
}

void testFluentColorAndElevationTokens()
{
    const wui::Theme light;
    expect(equal(light.colors.surfaceRaised, {255, 255, 255, 255}) &&
               equal(light.colors.border, {209, 209, 209, 255}),
           "Fluent aliases must expose a raised neutral surface and Windows stroke");
    expect(light.elevation.useWindowsStroke && light.elevation.shadow16.ambient.blur == 2.0f &&
               light.elevation.shadow16.key.blur == 16.0f &&
               light.elevation.shadow16.key.offsetY == 8.0f &&
               light.elevation.shadow16.ambient.color.a == 31 &&
               light.elevation.shadow16.key.color.a == 36,
           "Fluent elevation must preserve its official ambient and key ramp");
    expect(equal(light.colors.brandBackground.rest, {15, 108, 189, 255}) &&
               equal(light.colors.brandBackground.hover, {17, 94, 163, 255}) &&
               equal(light.colors.brandBackground.pressed, {12, 59, 94, 255}) &&
               equal(light.colors.brandBackground.selected, {15, 84, 140, 255}),
           "Fluent brand background states must use the documented alias tokens");

    expect(equal(wui::fluentBrandShadow({255, 255, 255, 255}, true), {0, 0, 0, 32}) &&
               equal(wui::fluentBrandShadow({15, 108, 189, 255}, false), {0, 0, 0, 65}),
           "Brand shadow opacity must be derived from the Fluent luminosity equations");
    expect(light.spacing.horizontal.sNudge == 6.0f && light.spacing.vertical.xl == 20.0f &&
               light.radius.small == 2.0f && light.radius.xxLarge == 12.0f &&
               light.radius.circular == 10000.0f && light.stroke.thickest == 4.0f,
           "Fluent spacing, radii and stroke scales must expose every documented token");
    expect(light.typography.body1.size == 14.0f && light.typography.body1.lineHeight == 20.0f &&
               light.typography.title1.size == 32.0f && light.typography.title1.weight == 600 &&
               light.typography.familyBase == "Segoe UI",
           "Fluent typography must expose named styles instead of only loose sizes");
}

void testScopedOverrideIsolation()
{
    const auto processAccentBefore = wui::theme().colors.accent;
    wui::Theme appTheme;
    appTheme.colors.accent = {15, 108, 189, 255};
    appTheme.typography.title = 31.0f;

    wui::ThemeOverride darkColors;
    darkColors.colors = wui::fluentDarkTheme().colors;
    const wui::ThemeScope darkPane(appTheme, darkColors);

    wui::ThemeOverride compactControls;
    auto compact = darkPane.resolvedTheme().controls;
    compact.height = 28.0f;
    compactControls.controls = compact;
    const wui::ThemeScope compactDialog(darkPane, compactControls);

    wui::ThemeOverride popupElevation;
    auto elevated = compactDialog.resolvedTheme().elevation;
    elevated.shadow16.key.blur = 20.0f;
    popupElevation.elevation = elevated;
    const wui::ThemeScope popup(compactDialog, popupElevation);

    expect(equal(appTheme.colors.accent, {15, 108, 189, 255}),
           "A local theme scope must not mutate its inherited theme");
    expect(equal(darkPane.resolvedTheme().colors.background, {31, 31, 31, 255}),
           "A child scope must resolve its local colors");
    expect(darkPane.resolvedTheme().typography.title == 31.0f,
           "An omitted category must inherit from the parent scope");
    expect(darkPane.resolvedTheme().controls.height == appTheme.controls.height,
           "A sibling scope must not receive a nested override");
    expect(compactDialog.resolvedTheme().controls.height == 28.0f &&
               equal(compactDialog.resolvedTheme().colors.accent, darkPane.resolvedTheme().colors.accent),
           "Nested scopes must override only their own categories");
    expect(equal(wui::theme().colors.accent, processAccentBefore),
           "ThemeScope must not change the process-wide default theme");
    expect(popup.resolvedTheme().elevation.shadow16.key.blur == 20.0f &&
               compactDialog.resolvedTheme().elevation.shadow16.key.blur == 16.0f,
           "Elevation overrides must remain local to their theme scope");
}

void testStatePropertyResolution()
{
    wui::StateProperty<std::string> fill{"rest"};
    fill.set(wui::ControlVisualState::Focused, "focus")
        .set(wui::ControlVisualState::Hovered, "hover")
        .set(wui::ControlVisualState::Pressed, "press")
        .set(wui::ControlVisualState::Disabled, "disabled")
        .set(wui::visualStateMask(wui::ControlVisualState::Hovered,
                                  wui::ControlVisualState::Focused),
             "hover-focus");

    expect(fill.resolve(0) == "rest", "StateProperty must fall back when no rule matches");
    expect(fill.resolve(wui::ControlVisualState::Focused) == "focus",
           "A single focused rule must resolve");
    expect(fill.resolve(wui::visualStateMask(wui::ControlVisualState::Hovered,
                                             wui::ControlVisualState::Focused)) == "hover-focus",
           "The most specific matching rule must resolve before a single-state rule");
    expect(fill.resolve(wui::visualStateMask(wui::ControlVisualState::Pressed,
                                             wui::ControlVisualState::Hovered)) == "press",
           "Pressed must beat hovered when no compound rule exists");
    expect(fill.resolve(wui::visualStateMask(wui::ControlVisualState::Disabled,
                                             wui::ControlVisualState::Pressed)) == "disabled",
           "Disabled must beat pressed when no compound rule exists");

    fill.set(wui::ControlVisualState::Hovered, "hover-updated");
    expect(fill.resolve(wui::ControlVisualState::Hovered) == "hover-updated",
           "Replacing a rule must update its resolved value without adding ambiguity");
}

void testTextConsumesNamedTypographyStyle()
{
    const wui::Theme fluent;
    wui::Text text("Token text");
    text.setTextStyle(fluent.typography.subtitle1);
    expect(text.fontFamily() == "Segoe UI" && text.fontSize() == 20.0f &&
               text.fontWeight() == 600 && text.lineHeight() == 28.0f,
           "Text must consume a named Fluent style as one family/size/weight/line-height contract");
}

} // namespace

int main()
{
    testFluentDarkTokens();
    testFluentColorAndElevationTokens();
    testScopedOverrideIsolation();
    testStatePropertyResolution();
    testTextConsumesNamedTypographyStyle();
    return 0;
}
