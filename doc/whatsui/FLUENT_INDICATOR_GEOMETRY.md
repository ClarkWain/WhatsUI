# Fluent indicator geometry at fractional DPI

## Problem record

On a 4K Windows display at 150% scale, the unchecked Radio and Checkbox
indicators exposed two related visual defects:

- the Radio outline had long flat chords and visible direction changes, so it
  read as a polygon instead of a circle;
- the Checkbox corners and opposite edges did not carry visually even ink,
  making the small frame look coarse and slightly asymmetric.

The defects are visible in these monitor photographs:

![Unchecked Radio before the fix: flat chords and a polygonal outline](../images/fluent_radio_polygon_before.jpg)

![Unchecked Checkbox before the fix: coarse corners and uneven edge coverage](../images/fluent_checkbox_stroke_before.jpg)

The colored fringe and screen-door pattern in these photographs come partly
from photographing the LCD subpixel grid. They are not the acceptance signal.
The long Radio chords, discontinuous curvature, unequal edge mass, and
clipping are geometry signals and remain visible independently of the camera
moire.

## Root cause

WhatsCanvas generated Circle, Oval, and rounded-rectangle paths in local
coordinates, then applied the Canvas root transform. The old curve subdivision
heuristics only inspected the local radius:

- an ellipse used `circumference * 0.25`, with a 16-segment minimum;
- each rounded-rectangle quadrant used `ceil(radius * 0.35)`, with a
  four-segment minimum.

The native GLFW host configures 150% scale through
`Canvas::setDevicePixelRatio(1.5)`. Curve tessellation therefore saw an
8-10 DIP radius before the root transform, while the framebuffer saw a
12-15-pixel radius afterwards. A 16-segment circle at a 15-pixel physical
radius has a chord sagitta of approximately 0.288 pixel:

```text
error = radius * (1 - cos(pi / segments))
      = 15 * (1 - cos(pi / 16))
      ~= 0.288 px
```

That error is large enough to expose flat edges on a compact, high-contrast
indicator. Antialiasing can soften a chord edge, but it cannot turn insufficient
geometry into a circle.

The 150% Checkbox also uses a legitimate 1 DIP Fluent stroke, which maps to
1.5 physical pixels. Fractional coverage is expected and must not be "fixed"
by silently turning every border into a one-physical-pixel hairline. The
requirement is symmetric coverage and continuous corners, not an integer
number of fully opaque pixels.

## Solution

### Device-space curve tolerance

WhatsCanvas now includes the current Canvas transform when choosing curve
segments. The required segment count bounds every chord to a maximum
0.1-physical-pixel sagitta:

```text
physicalRadius = logicalRadius * effectiveDeviceScale
maximumStep =
    2 * acos(1 - maximumSagittaPixels / physicalRadius)
segments = ceil(abs(sweepRadians) / maximumStep)
```

The result is combined with the previous circumference heuristic and existing
segment caps. Small UI curves gain only the detail they need; large curves and
the tessellation-cache contract remain bounded.

This matters specifically for the native path: tests must call
`Canvas::setDevicePixelRatio(scale)` and construct
`PaintContext(canvas, scale, true)`. Merely multiplying every coordinate by
1.5 before calling Canvas does not reproduce the original failure.

### Semantic circle primitives

`PaintContext` now exposes `fillCircle`, `strokeCircle`, and
`fillStrokeCircle`. Radio uses these primitives for:

- the outer selected/unselected surface;
- the inset outline;
- the white selected dot;
- both focus-ring strokes.

All four layers share one centre and radius model. Radio no longer relies on a
square rounded rectangle with an oversized "circular" corner token.

Checkbox remains a rounded rectangle, as required by Fluent, but benefits from
the device-aware rounded-corner tessellation. Its 16 DIP layout box, hit box,
and 1 DIP stroke token are unchanged.

## Result

The fixed 150% native-DPR probe is shown at normal size and with nearest-neighbor
pixel enlargement:

![Radio and Checkbox after the fix at 150%](../images/fluent_indicator_geometry_after_150dpi.png)

![Radio and Checkbox after the fix at 150%, enlarged without smoothing](../images/fluent_indicator_geometry_after_150dpi_pixelzoom.png)

At normal size, the Radio reads as a circle and the Checkbox has balanced
edges. In the pixel enlargement, ordinary raster stair steps remain visible;
that is unavoidable for a 24-pixel circle. The acceptance criteria are smooth
radial progression, symmetry, bounded chord error, and no long geometric
facets—not a mathematically continuous edge on a discrete pixel grid.

## Automated acceptance

`WhatsUIFluentIndicatorGeometryTests` renders the real Canvas-DPR ownership
path at 100%, 150%, and 200%. It writes the PPM artifact before assertions and
checks:

- Radio ink exists on 24 radial directions;
- the Radio ink bounds remain centred and circular;
- radial spread is at most 1.25 physical pixels;
- opposite, cardinal, and diagonal samples remain symmetric;
- Checkbox width and height agree within one physical pixel;
- all four Checkbox edges carry comparable ink;
- horizontal and vertical mirror mismatch stay below the documented bound;
- the immediate exterior remains the page token, proving no overflow or clip.

Run the focused gate with:

```powershell
cmake --build build-native-text-compare --config Release `
  --target WhatsUIFluentIndicatorGeometryTests --parallel 4

ctest --test-dir build-native-text-compare -C Release --output-on-failure `
  -R '^whatsui_fluent_indicator_geometry_(100|150|200)dpi$'
```

The broader regression set remains:

```powershell
ctest --test-dir build-native-text-compare -C Release --output-on-failure `
  -R '^whatsui_fluent_(component_visual_matrix|component_visual_matrix_150dpi|range_controls_visual|range_controls_visual_150dpi|visual_acceptance|visual_acceptance_125dpi|visual_acceptance_150dpi|visual_acceptance_200dpi)$'
```

## Files and ownership

- WhatsCanvas owns device-aware Circle/Oval/rounded-rectangle subdivision in
  `src/canvas/Canvas.cpp`.
- WhatsUI owns backend-neutral circle drawing in
  `include/wui/paint_context.h`.
- Radio composition remains in
  `src/whatsui/widgets/basic_controls.cpp`.
- The dedicated regression is
  `tests/fluent_indicator_geometry_tests.cpp`.

Do not solve future indicator defects by increasing a global fixed segment
count, disabling antialiasing, or snapping every logical stroke to one physical
pixel. Measure the final device-space geometry, preserve the design token, and
add a local pixel assertion that reproduces the failing DPR path.
