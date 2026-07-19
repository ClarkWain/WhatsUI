# Fluent P1 — Navigation, Overlays, and Selection

Status: complete (2026-07-18). This batch follows the Fluent UI React v9 Storybook pages
for Popover, TeachingPopover, Toolbar, TabList, Link, Breadcrumb, Combobox,
and Dropdown. `Tab`/`TabPanel` and `ListBox`/`Option` are included as the
corresponding component-family primitives.

## Acceptance gates

- [x] Popover: anchor placement, open/close, Esc/outside dismissal, focus
  restoration, elevation, collision handling, and UIA expanded state.
- [x] TeachingPopover: step content, heading/body/actions, focus policy,
  dismissal, and dialog/announcement semantics.
- [x] Toolbar: horizontal/vertical variants, grouping, overflow policy,
  roving keyboard navigation, disabled items, and toolbar semantics.
- [x] TabList, Tab, TabPanel: selected indicator, keyboard arrows/Home/End,
  manual/automatic activation, disabled tabs, panel linkage and UIA selection.
- [x] Link and Breadcrumb: visual states, keyboard activation, truncation and
  overflow handling, navigation semantics.
- [x] ListBox and Option: single/multiple selection, active option, pointer,
  keyboard/type-ahead, scrolling and listbox/option accessibility.
- [x] Combobox and Dropdown: editable vs non-editable trigger, filtering,
  menu/listbox integration, selected value display, multiselect policy,
  dismiss/focus restoration, and UIA ExpandCollapse/Selection/Value states.
- [x] Every family has behaviour tests plus Software visual captures at 100%
  and 150% DPI, personally reviewed for clipping, overlap and state clarity.
- [x] Release build, all `^whatsui_` CTest tests, and `git diff --check` pass.

## Verification record

- Built the six P1 behaviour/visual targets in `Release`.
- P1-focused CTest: 8/8 passed, including 100% and 150% Software captures.
- Full `ctest -C Release -R '^whatsui_'`: 63/63 passed.
- `git diff --check` passed. Git reports only the repository's existing CRLF
  normalization notice for `windows_uia_provider.cpp`, not a whitespace error.

## Notable implementation decisions

- Popover arrows are opt-in. Fluent's documented `withArrow` default is false;
  the default surface therefore has no detached triangle. `showArrow()` remains
  available when a contextual callout explicitly needs one.
- ListBox exposes the currently visible virtualized rows as semantic `Option`
  children with stable snapshot paths. UIA invocation is validated against the
  current row and routed to the parent ListBox's value operation, so it never
  targets a short-lived row Node.
