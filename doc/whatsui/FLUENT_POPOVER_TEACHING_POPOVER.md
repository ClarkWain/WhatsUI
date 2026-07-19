# Fluent Popover and TeachingPopover

## Scope

`Popover` is the compact anchored Fluent surface for contextual information or
small supplementary controls. `TeachingPopover` builds on the same placement
and dismissal contract for a guided step with an action footer.

Both surfaces use the shared `Popup` placement engine. This keeps them inside
the host bounds, flips a below placement to above when necessary, dismisses on
outside press, and accepts Escape. The visual surface uses Fluent elevation
`shadow16`, `neutralStroke1`, radius `large`, and semantic foreground/background
tokens instead of fixed component colors.

`Popover` is arrowless by default, matching Fluent's opt-in `withArrow`
behavior. Call `showArrow(true)` only when the anchor relationship needs to be
made explicit; its outlined arrow is joined under the panel surface to avoid a
high-DPI seam.

## Usage

```cpp
wui::PopoverButton help("More details");
help.bindOverlayHost(window.overlayHost())
    .popover("Project settings", "Members can update notifications at any time.");

auto onboarding = std::make_unique<wui::TeachingPopover>(
    "Create your first task", "Then assign a due date.");
onboarding->stepText("Step 1 of 3")
    .primaryAction("Next", [] { /* advance */ })
    .secondaryAction("Back", [] { /* previous */ });
onboarding->anchor({120, 80, 100, 32}).placement(wui::PopupPlacement::BelowStart);
window.overlayHost().show(std::move(onboarding));
```

For a custom trigger, construct and show a fresh `Popover` from an event
handler. Do not reuse a dismissed overlay node: `OverlayHost` owns it until
dismissal. `PopoverButton::popoverFactory` exists for this reason and creates a
new tree each time it opens.

## Interaction and accessibility contract

- `PopoverButton` exposes `ExpandCollapse`, updates `expanded`, and returns
  focus to its trigger after dismissal.
- Regular `Popover` is explicitly non-modal: opening it preserves focus on
  the trigger/page. `TeachingPopover` defaults to `TrapFocus`, so a trigger
  moves focus to the guided dialog and Tab remains in that dialog boundary.
  Use `focusPolicy(TeachingPopoverFocusPolicy::NonModal)` for passive tips
  that must not capture focus.
- `Popover` consumes an outside left press when configured to dismiss (default)
  and consumes Escape.
- `TeachingPopover` has independently invokable primary/secondary actions;
  each action invokes its callback before dismissing.
- Accessibility mapping: passive `Popover` is a labelled `Group`; a
  `TeachingPopover` is a `Dialog` so assistive technology identifies the
  guided interaction boundary. Its title is the accessible label and body is
  the description.

## Visual regression

`WhatsUIFluentPopoverVisualTests` renders normal, inverted, and teaching
surfaces at 100% and 150% DPI into `fluent_popover_100.ppm` and
`fluent_popover_150.ppm`. The test is intended for both automated pixel sanity
checks and manual visual review of arrow direction, panel elevation, type,
footer layout, and host-edge clamping.
