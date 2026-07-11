#include <stdexcept>
#include <string>

#include "wui/theme.h"
#include "wui/theme_extensions.h"

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
    expect(equal(dark.colors.background, {28, 27, 26, 255}),
           "Fluent dark must expose the documented Windows canvas token");
    expect(equal(dark.colors.accent, {71, 158, 245, 255}),
           "Fluent dark must expose its accessible blue accent");
    expect(equal(dark.colors.onAccent, {0, 0, 0, 255}),
           "Fluent dark accent foreground must remain legible");
    expect(dark.typography.body == wui::Theme{}.typography.body &&
               dark.controls.height == wui::Theme{}.controls.height,
           "Appearance changes must preserve Fluent layout and control metrics");
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

    expect(equal(appTheme.colors.accent, {15, 108, 189, 255}),
           "A local theme scope must not mutate its inherited theme");
    expect(equal(darkPane.resolvedTheme().colors.background, {28, 27, 26, 255}),
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

} // namespace

int main()
{
    testFluentDarkTokens();
    testScopedOverrideIsolation();
    testStatePropertyResolution();
    return 0;
}
