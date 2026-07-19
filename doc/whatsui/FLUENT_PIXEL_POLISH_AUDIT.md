# Fluent pixel-polish audit

This checklist is the visual acceptance gate for WhatsUI controls. Review
captures are rendered by the Software backend at 150% DPI unless a test
explicitly owns a different scale. A component is complete only when geometry,
type, state styling and fractional-DPI rendering have all passed.

## Shared rendering rules

- [x] Paint bordered rounded surfaces with one outer fill plus an inset stroke.
  Do not stack two same-sized rounded fills; their independently antialiased
  edges create a dark or fuzzy seam at fractional DPI.
- [x] Paint chevrons and compact check marks as open polylines with round caps
  and joins. Do not close a three-point chevron into a triangle or assemble it
  from one-pixel rectangles.
- [x] Center text from measured glyph metrics and the token's baseline. Do not
  approximate text width from character count or hard-coded glyph widths.
- [x] Centre the ascent/descent box, not `lineHeight / 2`. The latter assumes
  zero descent and historically pushed 14-DIP control labels down by 3–4 DIP.
- [x] Pass the typography token's family, size and weight together.
- [x] Use paired focus strokes outside the component instead of repainting the
  component interior with nested focus-colored fills.
- [x] Keep circle/ring geometry in logical units and let the renderer apply DPR
  exactly once.

## Component review

| Surface | Geometry | Typography | States | 150% capture |
|---|---:|---:|---:|---:|
| Button / CompoundButton / ToggleButton | pass | pass | pass | pass |
| Checkbox / Radio / Switch | pass | pass | pass | pass |
| TextInput / SearchBox / TextArea | pass | pass | pass | pass |
| Slider / ProgressBar / Divider | pass | pass | pass | pass |
| Card | pass | pass | pass | pass |
| Toast / Spinner | pass | pass | pass | pass |
| ListBox / Combobox / Dropdown | pass | pass | pass | pass |
| MenuButton / SplitButton / Menu / Tooltip | pass | pass | pass | pass |
| Accordion / Tree | pass | pass | pass | pass |
| Calendar / DatePicker / TimePicker | pass | pass | pass | pass |
| Toolbar / TabList / Link / Breadcrumb | pass | pass | pass | pass |
| Field / MessageBar | pass | pass | pass | pass |
| Badge / Avatar / Persona | pass | pass | pass | pass |
| Table / DataGrid | pass | pass | pass | pass |
| Image / Rating | pass | pass | pass | pass |
| Popover / TeachingPopover / Dialog / Drawer | pass | pass | pass | pass |

## Automated visual gates

- `whatsui_fluent_component_visual_matrix_150dpi`
- `whatsui_fluent_visual_acceptance`
- `whatsui_fluent_visual_acceptance_125dpi`
- `whatsui_fluent_visual_acceptance_150dpi`
- `whatsui_fluent_visual_acceptance_200dpi`
- `whatsui_fluent_feedback_visual_150dpi`
- `whatsui_fluent_selection_controls_visual_150dpi`
- `whatsui_fluent_range_controls_visual_150dpi`
- `whatsui_fluent_accordion_visual_150dpi`
- `whatsui_fluent_date_time_visual_150dpi`
- `whatsui_fluent_tree_visual_150dpi`
- `whatsui_fluent_navigation_visual_150dpi`
- `whatsui_todo_visual_review`

The Spinner visual test additionally asserts that the center of its 32-DIP
indicator remains background-colored at 150% DPI. This catches filled,
off-center and dot-based regressions without manual review. Todo review captures
cover compact, regular and wide layouts and verify that structural rebuilds do
not leak clip or paint state.

## Review artifacts

Generated files live under `build-native-text-compare/tests/`. The main audit
images are:

- `fluent_component_visual_matrix_150dpi.ppm`
- `fluent_feedback_review_150dpi.ppm`
- `fluent_selection_controls_150dpi.ppm`
- `fluent_range_controls_review_150dpi.ppm`
- `fluent_accordion_review_150dpi.ppm`
- `fluent_date_time_review_150dpi.ppm`
- `fluent_tree_review_150dpi.ppm`
- `todo_visual_review/{narrow,regular,wide}/`

Most generated screenshots remain local review artifacts. A small, named set
of incident evidence is source-controlled under `doc/images/` and linked from
the relevant postmortem.

The strict acceptance suite uses a maximum displacement of one final physical
pixel. The incident, metric formula, before/after captures, and measured
100/125/150/200% results are recorded in
[Fluent control text alignment postmortem](FLUENT_TEXT_BASELINE_POSTMORTEM.md).
