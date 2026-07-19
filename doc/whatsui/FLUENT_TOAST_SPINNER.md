# Fluent Toast / Toaster / Spinner

WhatsUI exposes Fluent 2 transient feedback through `wui::Toast`, `wui::Toaster`,
and `wui::Spinner`.

## Toast and Toaster

`Toast` provides an `Info`, `Success`, `Warning`, or `Error` intent, title/body,
optional action, visible dismissal, Escape dismissal, elevation, and a 5-second
default timeout. Pointer hover and press pause the timeout; an explicit zero
timeout leaves the notification open until it is dismissed. `Toaster` binds to
an `OverlayHost`, positions notifications at one of the four screen corners,
and displays queued items FIFO with exactly one visible notification.

```cpp
wui::Toaster notifications(window.overlays(), wui::ToastPosition::BottomEnd);
auto saved = std::make_unique<wui::Toast>("Task completed", "The task was moved to Completed.");
saved->setIntent(wui::ToastIntent::Success);
saved->setAction("Undo", undoLastTask);
notifications.show(std::move(saved));
```

Toast is represented as an `Alert` in the accessibility snapshot. It is a
polite live region (`live=true`) with the title as its name and body as its
description; it does not steal keyboard focus. Windows UI Automation projects
the live setting to `AutomationLiveSetting_Polite`.

## Spinner

`Spinner` is an indeterminate busy indicator with the Fluent size ramp
`ExtraTiny` through `Huge`, and label placement `Before`, `After`, `Above`, or
`Below`. `motionEnabled(false)` freezes the indicator for a reduced-motion
preference while retaining the same busy accessibility semantics.

Spinner is exposed as a busy, indeterminate `ProgressBar` with its label as
the accessible operation name.

## Validation

`WhatsUIFluentFeedbackTests` verifies timeout pause/action/queue/accessibility
semantics. `WhatsUIFluentFeedbackVisualTests` produces 100% and 150% DPI PPM
review captures and checks elevation/margins.
