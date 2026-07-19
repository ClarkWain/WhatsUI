# Fluent Accordion

`wui::Accordion` and `wui::AccordionItem` implement Fluent's disclosure
pattern for compact, related settings. An accordion is not a navigation
replacement: keep headers short, and use the expanded body for immediately
related explanatory content or retained controls.

## API and policy

```cpp
wui::Accordion settings;
settings.accessibleLabel("Settings sections")
        .setExpandMode(wui::AccordionExpandMode::Single);
settings.addItem("Account", "Manage your sign-in preferences.").setExpanded(true);
settings.addItem("Appearance", "Choose a theme.");
```

The default policy is `Single`, matching an accordion's compact disclosure
intent. `Multiple` permits independent sections to remain open. Switching
back to `Single` deterministically retains the first expanded section and
collapses the rest.

An item can retain a real subtree via `setContent()`. Collapsing removes that
subtree from measurement, layout, paint and hit testing, but does not destroy
it; state and focus identity are therefore stable when it is expanded again.

## Input and accessibility

- `Enter` and `Space` toggle the focused item.
- `Up` / `Down` move roving focus, skipping disabled sections.
- `Home` / `End` move to the first / last enabled section.
- Items expose deterministic `Invoke`, `Expand`, and `Collapse` actions.
- The semantic projection is an `Accordion` group plus a Button for
  each item, with `expanded` accurately reflecting its disclosure state.

The chevron is a compact stroked indicator, rather than a detached filled
triangle; it is paired with the header's hover, pressed, disabled and focus
states. The implementation intentionally does not time-drive height changes:
the state transition is deterministic for accessibility, screenshot testing,
reduced-motion environments and native hosts. Hosts may animate the retained
body bounds above this component without changing its logical expansion state.

## Verification

- `fluent_accordion_tests.cpp`: policy, keyboard, disabled-state, accessibility
  actions and retained-body identity.
- `fluent_accordion_visual_tests.cpp`: Software renderer captures at 100% and
  150% scale, covering expanded, hover, disabled and multiple-open surfaces.
