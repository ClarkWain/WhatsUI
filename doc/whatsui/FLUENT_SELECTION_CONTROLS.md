# Fluent P1 — ListBox, Combobox and Dropdown

This implementation follows the Fluent selection-control model: an option has
a stable programmatic `value`, a display `text`, optional secondary text and
an enabled state.  The same `Option` model backs the standalone ListBox and
the popup listboxes opened by Combobox and Dropdown.

## Behaviour contract

- `ListBox` has one active option for roving keyboard focus. Arrow keys,
  Home/End, Enter and Space skip disabled options. Printable keys provide
  700ms native-style type-ahead (repeating a letter cycles matches). Single
  selection commits one option; multiple selection toggles with Space.
- The list paints only the currently visible row window. Wheel input updates
  its logical scroll offset and keyboard/type-ahead selection automatically
  scrolls the active row into view. `accessibilityOptions()` provides a
  materialized semantic record per Option, with bounds only for viewport rows,
  for native virtual-child providers.
- `Combobox` retains `TextInput`, therefore native text input, IME and the
  caret are never reimplemented. Typing filters options by primary or
  secondary text. Down/Up opens its list, Escape collapses it.
- `Dropdown` is non-editable. Pointer, Enter, Space and Down open the popup;
  Up moves to a prior enabled option while closed. Both Dropdown and Combobox
  support Fluent multiselect: their popup ListBox remains open while choices
  are toggled; Dropdown presents the first choice plus a `+N` summary and
  Combobox keeps the editable filter clear rather than pretending only one
  choice is selected.
- Popups use `OverlayHost` and `Popup`, including Fluent elevation and
  outside-press/Escape dismissal. Closing restores focus to the owning field.
- Selection fills use named Fluent interaction aliases
  (`neutralBackground1/2`), not synthesized RGB values. Popup elevation is
  supplied by the shared `Popup` surface (`shadow16`).

## Accessibility contract

The common accessibility bridge projects `Combobox`/`Dropdown` as `ComboBox`,
the standalone list as `ListBox`, and individual visible values as `Option`.
Combobox and Dropdown expose expand/collapse and selected value; ListBox
accepts `SetValue` using the stable option value. Disabled options cannot be
selected by pointer, keyboard, or accessibility action.

## Verification

`fluent_selection_controls_tests.cpp` exercises disabled-item skipping,
pointer/keyboard selection, type-ahead, scroll-windowing, virtual Option
semantics, single/multiple selection, overlay open/collapse, focus restoration,
and `SetValue`. `fluent_selection_controls_visual_tests.cpp` produces
deterministic 100% and 150% PPM captures for visual review of windowed rows,
disabled/selected state, multi-select Dropdown and editable Combobox.
