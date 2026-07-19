# Fluent Default Theme

WhatsUI ships with a light Fluent 2 Windows default theme. It is a semantic
design system, not a copy of Microsoft assets: product screens should use token
roles instead of hard-coded RGB values and magic control dimensions.

`Theme` groups the roles that applications need:

- `colors`: Fluent alias state ramps (`rest`, `hover`, `pressed`, `selected`)
  for neutral, brand and danger surfaces; semantic foreground/stroke/focus,
  status and scrim tokens. A widget must select a state token, never darken an
  arbitrary color itself.
- `elevation`: the official two-layer ambient/key `shadow2` through
  `shadow64` scale. Dialogs and popups draw both layers and keep a Windows
  surface stroke.
- `spacing`: horizontal and vertical Fluent scale from `none` through `xxxl`
  (0, 2, 4, 6, 8, 10, 12, 16, 20, 24, 32).
- `radius`: `none`, `small` through `6xlarge`, and `circular`; radio, switch,
  slider and progress indicators use `circular` rather than an oversized
  conventional corner radius.
- `stroke`: `thin`, `thick`, `thicker`, `thickest` (1–4 logical pixels).
- `typography`: exact Fluent base/hero size and line-height scales plus named
  styles such as `body1`, `body1Strong`, `subtitle1`, `title1` and `display`.
  `ui::Text(...).style(theme().typography.body1)` applies its family, size,
  weight and line-height together.
- `controls`: common 36px control height, horizontal padding, focus stroke and
  checkbox dimensions.

The default widget language is intentionally quiet: white elevated surfaces,
warm neutral canvas, blue primary action, visible keyboard focus, and thin
neutral borders. `Button::Ghost` is the secondary outlined action; `Primary`
is the blue call to action; `Danger` retains the same geometry but uses the
destructive semantic color. Text inputs and dialogs use the shared surface,
outline and radius tokens.

Install a custom complete theme at application startup with `setTheme()`. Do
not mutate individual global tokens during event dispatch; widgets read the
theme at paint time and should invalidate after an application changes it.

`ThemeScope` can instead replace a complete category (`colors`, `elevation`,
`spacing`, `radius`, `stroke`, `typography`, or `controls`) for one subtree.
This is the supported path for a future Material or application-defined design
system: existing compatibility aliases remain only while older widget code is
migrated; new code must use the named Fluent roles.

## Sources

- [Fluent UI React theme colors](https://storybooks.fluentui.dev/react/?path=/docs/theme-colors--docs)
- [Border radii](https://storybooks.fluentui.dev/react/?path=/docs/theme-border-radii--docs), [spacing](https://storybooks.fluentui.dev/react/?path=/docs/theme-spacing--docs), and [stroke widths](https://storybooks.fluentui.dev/react/?path=/docs/theme-stroke-widths--docs)
- [Fonts](https://storybooks.fluentui.dev/react/?path=/docs/theme-fonts--docs) and [typography](https://storybooks.fluentui.dev/react/?path=/docs/theme-typography--docs)
- [Shadows](https://storybooks.fluentui.dev/react/?path=/docs/theme-shadows--docs)
