# Fluent Default Theme

WhatsUI ships with a light, Fluent-inspired Windows default theme. It is a
semantic design system, not a copy of Microsoft assets: product screens should
use token roles instead of hard-coded RGB values and magic control dimensions.

`Theme` groups the roles that applications need:

- `colors`: background/surface layers, typography, outline, accent states,
  disabled content, focus and dialog scrim.
- `spacing`: a compact 4-point rhythm (`xs` through `xxl`).
- `radius`: 4px controls, 6px fields/buttons, 8px cards and dialogs.
- `typography`: caption, body, subtitle and title scales.
- `controls`: common 32px control height, horizontal padding, focus stroke and
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
