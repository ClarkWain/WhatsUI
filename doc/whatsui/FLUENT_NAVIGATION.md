# Fluent P1 — Navigation and command surfaces

This P1 slice implements the Fluent 2 navigation primitives used to move
between peer views and expose compact command rows:

- `Toolbar` / `ToolbarItem`: horizontal or vertical command strips with subtle
  and primary variants, pointer press feedback, Enter/Space invocation and
  roving arrow/Home/End keyboard focus. When constrained, trailing commands
  are deterministically retained as `overflowedItems()` and rendered behind a
  compact overflow affordance; `onOverflow()` hands that list to an app-owned
  Menu/Popover without discarding item identity.
- `TabList` / `Tab` / `TabPanel`: horizontal Fluent tabs with a selected brand
  indicator, disabled-tab skipping, automatic or manual activation, keyboard
  selection, stable string values and linked panel visibility. `TabPanel`
  linked with `.tabList(tabs)` measures, paints and hit-tests only when the
  matching tab value is active, while keeping inactive content retained.
- `Link`: an explicit Invoke callback and optional `href` metadata. WhatsUI
  never opens URLs implicitly; the application owns navigation policy.
- `Breadcrumb` / `BreadcrumbItem`: linked destinations plus a current page.
  The width-independent fallback keeps first/final context and elides the
  middle; `hiddenItems()` exposes elided labels for a future Popover menu.

## Fluent token policy

All controls use `Theme` semantic ramps and typography:

- neutral hover / pressed backgrounds for subtle toolbar commands;
- `brandBackground` for primary commands and `brandForeground1` for selected
  tabs, links and their underlines;
- `body1` / `body1Strong` typography and the shared focus stroke tokens;
- standard 4/8/12/16 logical spacing and `controls.height` rather than fixed
  physical pixels. The visual test captures both 100% and 150% DPR.

## Accessibility contract

The platform-neutral snapshot exposes semantic `Toolbar`, `TabList`, `Tab`,
`TabPanel` and `Link` roles. Tabs are invoke/toggle-capable and expose
`checked` as the selected state; toolbar items and links expose Invoke.
Breadcrumb is a named grouping containing independently invokable destination
items; the current item is text-only.

## Validation

`WhatsUIFluentNavigationTests` covers action routing, horizontal/vertical
toolbar overflow, automatic/manual TabList policy, disabled tabs, linked panel
visibility, collapse semantics, stable panel identity and the semantic snapshot.
`WhatsUIFluentNavigationVisualTests` renders a complete command/tab/link/
breadcrumb matrix through the software canvas at 100% and 150% scale.
