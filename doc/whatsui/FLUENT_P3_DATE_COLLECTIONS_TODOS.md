# Fluent P3 — Date Inputs and Collections

Status: complete. This batch follows the supplied Fluent UI React v9
Calendar, DatePicker, TimePicker, DataGrid, Tree, and Table references.

## Delivery order

1. **Foundation:** locale-neutral date/time value types, parsing/formatting
   boundary, calendar-grid navigation, and shared collection selection/focus
   primitives. No OS locale strings are parsed in core widgets.
2. **Calendar / DatePicker / TimePicker:** single/range selection, disabled
   values, keyboard navigation, popup/focus restoration, validation and
   Dialog/combobox accessibility.
3. **Table / DataGrid:** columns, header/body/cell semantics, sorting,
   selectable rows, scalable layout/windowing, keyboard grid navigation and
   deterministic data contracts.
4. **Tree:** hierarchy, expand/collapse, indentation, selection, keyboard
   navigation, virtualized rendering and treeitem accessibility.

## Acceptance gates

- [x] Date/time value handling has deterministic tests for invalid input,
  boundaries, locale-independent storage, and daylight-saving-free UI values.
- [x] Calendar and pickers implement pointer, keyboard, disabled-state,
  focus-restoration, validation and 100%/150% Software capture coverage.
  DatePicker and TimePicker also expose UIA Expand/Collapse/SetValue; the
  TimePicker popup scrolls its active time into view.
- [x] Table and DataGrid distinguish static table semantics from interactive
  grid semantics; header association, sort state, selection and windowed data
  are testable without an application backend.
- [x] Tree has stable item identity, keyboard arrows/Home/End, expand/collapse,
  disabled state and semantic level/selection data.
- [x] All public APIs are exported through umbrella and builder headers;
  Windows UI Automation roles/patterns are backed by snapshot tests.
- [x] Behaviour, 100%/150% visual, full CTest and whitespace gates pass.

## Completion evidence

- `WhatsUIFluentDateTimeTests`, `WhatsUIFluentTableTests`, and
  `WhatsUIFluentTreeTests` cover behavior, virtual semantic materialization,
  keyboard operation, validation, selection, disabled state, and UIA action
  routing.
- Their Software visual captures run at both 100% and 150% DPI. The final
  150% review checked date-range continuity, time-popup active-row visibility,
  table trailing-column fit, and tree indentation/collapsed branches.
- Release verification: `ctest --test-dir build-native-text-compare -C Release
  --output-on-failure -R '^whatsui_'` — 81/81 passed.
