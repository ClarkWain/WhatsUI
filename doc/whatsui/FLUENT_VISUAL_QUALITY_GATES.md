# Fluent visual quality gates

`WhatsUIFluentVisualAcceptanceTests` is the compact, deterministic visual
acceptance gate for the shared Fluent primitives. It renders the Software
backend at 100%, 125%, 150%, and 200% DPI, then evaluates physical pixels,
logical geometry, and semantic colors. It complements the larger component
matrix rather than replacing it: the matrix remains the broad review artifact;
this test makes the most failure-prone visual rules executable.

The Radio/Checkbox failure that led to the device-space curve tolerance and
radial symmetry gate is documented in
[Fluent indicator geometry at fractional DPI](FLUENT_INDICATOR_GEOMETRY.md).

## Release quality bar

The following rules are normative for every new or changed component. “One
pixel” always means one final framebuffer pixel after DPR conversion, not one
logical DIP.

### Typography and alignment

- A single-line label's rendered ink centre must be within one physical pixel
  of its control or declared line-box centre.
- Text, icon, checkbox/radio indicator, and spinner placed on the same row must
  share a centre or typographic baseline within one physical pixel.
- Button labels use the active Fluent button style and semibold weight; body,
  caption, subtitle, and title text come from the theme type ramp rather than
  ad-hoc sizes.
- Text must not touch or cross its content box. Truncation must use the
  component's declared wrapping, ellipsis, or scrolling behavior.
- Editable controls must show a correctly sized, clipped, blinking caret; the
  caret and selection must use the same content inset as the text.

### Geometry, stroke, and clipping

- Nominal circles must differ by at most one physical pixel in raster width and
  height. Their four corner samples must remain outside the fill.
- Rounded corners must be continuous at every tested DPR: no flat edge,
  missing quadrant, or parent-clip bite is allowed.
- Border/focus/indicator stroke thickness may deviate by at most one physical
  pixel from its token after rasterization, and opposite edges must remain
  symmetric.
- Hover or pressed growth must remain inside the declared paint/hit bounds.
  Endpoint controls (for example Slider at minimum/maximum) must keep a clean
  background gutter and may not be clipped flat.
- Repeated controls use token spacing. Equivalent left/right padding and
  repeated row gaps may differ by at most one physical pixel.

### State completeness

- Every interactive component is reviewed in all states it supports: rest,
  hover, pressed, focused, disabled, selected/checked, mixed, and
  invalid/error.
- Adjacent states must differ through their documented theme tokens. Focus
  rings must remain visible and un-clipped; pressed and hover layers may not
  obscure icons or produce stacked, contradictory outlines.
- Selection glyphs are centred and use one coherent state composition. A
  current Fluent mixed Checkbox uses a transparent face with compound-brand
  border and mark; it must not regress to either a checked blue face or a
  nested filled square with contradictory colors.

### Color, elevation, and content

- Normal text must meet WCAG AA 4.5:1 contrast; large text and non-text
  indicators must meet 3:1 against adjacent surfaces.
- Surface, stroke, foreground, shadow, and elevation values come from theme
  tokens. A visual state must not introduce an untracked literal color.
- Empty, long, localized, and high-count content must be checked. A component
  cannot pass using only a short English happy-path label.

### DPI and evidence

- Deterministic visual gates run at 100%, 125%, 150%, and 200%. Native Windows
  text additionally proves that Canvas/PaintContext DPR ownership is applied
  exactly once. GLFW content-scale discovery, callbacks, and cross-monitor
  movement remain native release checks.
- The generated artifact is saved before assertions, and the component state
  matrix is visually inspected at 150% after a relevant rendering change.
- A whole-image hash is supporting evidence, never the only gate. Every fixed
  visual defect receives a local geometry or semantic-pixel regression test
  with a diagnostic failure message.
- No acceptance tolerance may be widened merely to make a failing screenshot
  pass. Any exception must identify the font/backend-specific measurement and
  retain a separate exact logical-geometry assertion.

## Commands

```powershell
ctest --test-dir build -C Release --output-on-failure `
  -R '^whatsui_fluent_visual_acceptance(_(125|150|200)dpi)?$'
```

The four tests write `fluent_visual_acceptance*.ppm` beside the CTest binary.
They are review artifacts and are not source-controlled. The image is always
written before assertions run so a CI failure remains inspectable.

For compact selection indicators, also run the real Canvas-DPR ownership path:

```powershell
ctest --test-dir build -C Release --output-on-failure `
  -R '^whatsui_fluent_indicator_geometry_(100|150|200)dpi$'
```

These tests write `fluent_indicator_geometry_*.ppm` and validate Radio radial
symmetry plus Checkbox edge/corner symmetry and clip containment.

## Executable rules

| Rule | Assertion | Threshold / reason |
|---|---|---|
| Button text vertical alignment | White text ink in the opaque primary Button stays around the 32-DIP centre | `<= 1 physical pixel` at every tested scale. |
| Label and Input text vertical alignment | Dark label and placeholder ink stay around their logical line centres | `<= 1 physical pixel`; no multi-DIP optical-bias exemption. |
| Input left padding | Placeholder first ink pixel starts after the 14-DIP token plus the default face's measured 1-DIP `P` side-bearing | `<= 1 physical pixel`; the public caret geometry is checked separately so a font bearing cannot hide an inset regression. |
| Input right padding | Public `TextInput::caretRect()` for a long line ends inside the right content inset | Exact logical value: `right - horizontalPadding - 1 DIP`; this catches clipped long input and asymmetric padding without relying on glyph shape. |
| Circular geometry | Circular Button corners remain page-colored while its centre is filled; unselected Radio has page-colored corner, stroke axis and a transparent centre showing its parent background | Exact semantic token samples at logical geometry points. This detects square fills, clipping, accidental Radio fills and lost ring strokes. |
| Compact indicator geometry | Unselected Radio has ink on 24 radial directions with bounded spread and mirror symmetry; Checkbox keeps equal bounds, comparable four-side ink and a clean exterior gutter | Runs through `Canvas::setDevicePixelRatio()` at 100/150/200%; catches curve tessellation that ignores the native DPR root transform. |
| Checkbox checkmark optical centre | The visible Checkmark12 ink centre is compared with the complete 16-DIP indicator's physical-pixel centre | `<= 0.75 physical pixel` at 100/125/150/200%; catches baseline/em-box centring that leaves the actual mark one DIP high. |
| Input / Textarea field layers | Rest and hover frames sample their neutral outer and accessible bottom stroke separately; focus-in samples brand at the centre while retaining neutral stroke near the ends | Runs at 100/125/150/200%; proves the 200 ms centre-out focus animation and requires integer physical-pixel stroke thickness, preventing a uniform grey, soft 1.5-pixel edge, or instantly full-width field border. |
| Editable caret geometry | Input and Textarea caret ink is measured independently from the focus line | Exactly `1 physical pixel` wide and one 20-DIP Body 1 line high; Input is vertically centred and medium Textarea begins after its 6-DIP top padding. |
| Stateful surfaces | Compound Button, Checkbox, Radio, and determinate Progress states sample their dedicated Fluent tokens | Checkbox verifies unchecked, checked, mixed, pointer-focus, keyboard-focus and disabled states, including compound-brand surfaces/marks, at 100/125/150/200%. Radio verifies its separate outer stroke, 10 DIP dot and transparent annular gap at the same scales. |
| Fractional DPI | The identical rules run at `1.0`, `1.25`, `1.5`, and `2.0` scale | Coordinates are converted with the renderer's `lround(logical * scale)` convention, exposing double-scale, half-pixel drift, and scale-specific clipping. |

The centering threshold is expressed in output pixels, not DIPs:
`logical tolerance = 1 / scale`. Therefore it tightens from 1 DIP at 100% to
0.5 DIP at 200%. A control cannot pass at high DPI merely because a visually
large DIP tolerance was chosen.

## Scope and interpretation

These gates intentionally use relative token checks rather than a whole-image
golden hash. Whole-image snapshots are useful review artifacts but are too
fragile across font rasterizers and GPU/software implementations. A failure in
this suite has a local diagnostic meaning: a baseline moved, a token/state was
lost, a content inset regressed, or a circular/stroke shape became clipped.

Native DirectWrite quality and one-DPR ownership are separately gated by
`whatsui_windows_native_text_dpi_tests`; this suite validates the portable
Software component path used by the deterministic visual matrix. Reviewers
must still inspect the generated matrix for gestalt issues (visual hierarchy,
crowding, and composition) that no local pixel assertion can prove.

`whatsui_fluent_component_visual_matrix*` remains a broad 100/150% review gate
and uses a deliberately wider 1.5-DIP ink band across varied strings. It is not
the strict one-pixel authority; `whatsui_fluent_visual_acceptance*` owns that
threshold with controlled probe text.
