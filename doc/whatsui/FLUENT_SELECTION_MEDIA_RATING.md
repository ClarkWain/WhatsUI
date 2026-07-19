# Fluent selection, range, media, and rating controls

This note records the Fluent UI React v9 behavior used by WhatsUI for the
Divider, Slider, ProgressBar, Checkbox, RadioGroup, Switch, Image, Rating, and
RatingDisplay completion batch. It is an implementation contract, not a copy
of the React API: C++ names may differ, but observable behavior and semantics
must remain equivalent.

## Official references

- <https://storybooks.fluentui.dev/react/?path=/docs/components-divider--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-slider--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-progressbar--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-checkbox--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-radiogroup--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-switch--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-image--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-rating--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-ratingdisplay--docs>

## Component contracts

### Divider

- Supports horizontal and vertical separators, optional content, start/center/end
  content alignment, inset lines, and default/subtle/brand/strong appearances.
- Content must interrupt the line cleanly without overlap. An unlabelled divider
  remains a separator in the accessibility tree.

### Slider

- Supports controlled and uncontrolled values, clamping, step snapping,
  horizontal and vertical orientation, and small/medium sizing.
- Pointer drag and keyboard arrows, Home, and End must use the same normalization
  path. The vertical maximum is at the top.
- The accessibility value is writable only while the control is enabled.

### ProgressBar

- A supplied value produces determinate progress; indeterminate mode omits a
  numeric value and exposes a busy operation instead.
- Rounded/square shapes, medium/large thickness, and
  brand/error/warning/success colors use theme tokens.
- Reduced-motion mode must yield a stable, non-animated indicator.

### Checkbox

- Supports unchecked, checked, and mixed states; square/circular shapes;
  medium/large sizes; labels before/after; required and disabled states.
- A wrapped label aligns the indicator with the first text line. The complete
  label and indicator form one hit target.
- Mixed state maps to the native indeterminate Toggle state.

### RadioGroup

- The group, rather than individual Radio controls, owns exclusive selection,
  controlled binding, and arrow-key movement.
- Vertical, horizontal, and horizontal-stacked layouts are supported. Disabled
  options are skipped by keyboard movement.
- Use two to five concise choices. If selection is optional, represent it as an
  explicit “None” choice rather than silently clearing a required group.

### Switch

- A switch changes state immediately; use Checkbox for choices that are only
  committed on form submission.
- Supports small/medium sizes and before/above/after label placement. Required,
  disabled, focus, hover, and pressed states stay visually distinct.
- The complete label and indicator form one hit target and expose Toggle
  semantics.

### Image

- Preserves source interning and intrinsic sizing while supporting
  default/none/center/fill/contain/cover fit, square/circular/rounded shapes,
  borders, shadows, and block layout.
- Informative images require concise contextual alternative text. Decorative
  images are omitted from the accessibility tree.
- Shape clipping applies to the image itself and never clips the border or
  elevation outside the image bounds.

### Rating

- Interactive Rating supports controlled/uncontrolled values, configurable
  maximum, whole/half steps, clear-to-zero behavior, four sizes, and
  neutral/brand/marigold colors.
- Pointer preview is transient; committing by pointer, keyboard, or accessibility
  uses the same clamp/snap/change-notification path.
- Each possible value needs a meaningful accessible label; the aggregate control
  also exposes its current value.

### RatingDisplay

- RatingDisplay is passive aggregate output, never an input replacement.
- It rounds visual fill to the nearest half item, optionally shows a localized
  count, supports compact one-item mode, and shares Rating sizes/colors/icons.
- A visible RatingDisplay always has a value and a non-empty generated accessible
  name. No-value content is represented by omitting the component from the view.

## Verification contract

- Dedicated tests cover value/state transitions, bindings, keyboard and pointer
  input, disabled behavior, and accessibility actions.
- A unified Software-backend matrix is rendered at DPR 1.0 and 1.5. Review checks
  circular geometry, baseline alignment, focus boundaries, half fills, image
  clipping, vertical extrema, and disabled-state contrast.
- Release build, all `whatsui_` CTest targets, and `git diff --check` are the final
  gate. The tracked checklist is
  [FLUENT_SELECTION_MEDIA_RATING_TODOS.md](FLUENT_SELECTION_MEDIA_RATING_TODOS.md).
