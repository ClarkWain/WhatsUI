# Fluent Image and Rating contract

This implementation follows the Fluent UI React v9 Storybook contracts for
[Image](https://storybooks.fluentui.dev/react/?path=/docs/components-image--docs),
[Rating](https://storybooks.fluentui.dev/react/?path=/docs/components-rating--docs), and
[RatingDisplay](https://storybooks.fluentui.dev/react/?path=/docs/components-ratingdisplay--docs).

## Image

- `ImageFit` supports Fluent's default, none, center, contain, and cover
  behavior; `Fill` remains as the explicit stretching option.
- `ImageShape` supports square, circular, and rounded clipping.
- `bordered`, `shadow`, and `block` use theme stroke, radius, elevation, and
  sizing tokens rather than application-specific colors or dimensions.
- An immutable `ImageSource` can be configured as a fallback. It becomes the
  effective source whenever the primary source is empty.
- Informative images expose their concise `alt` text with the Image role.
  Decorative images are omitted from the accessibility snapshot.

## Rating

- Maximum defaults to five and is constrained to at least two items.
- Step is either one or one half. Value, pointer preview, keyboard navigation,
  state binding, and accessibility SetValue all share the same snapping path.
- Item sizes match Fluent's 12, 16, 20, and 28 logical-pixel scale.
- Neutral, brand, and marigold color variants and star, circle, and square
  shapes are supported. Disabled, focused, read-only, and pointer states are
  deterministic.
- `itemLabel(value)` supplies the spoken label for a value, matching Fluent's
  item-label callback contract.

The web implementation uses a radiogroup containing native radio inputs for
each whole or half value. WhatsUI intentionally does not manufacture semantic
children that have no corresponding actionable `Node`. Its equivalent native
contract is one focusable `RadioGroup` with the current `"value out of max"`
ValuePattern, SetValue, arrow/Home/End keyboard input, and `itemLabel` as the
current value description. This keeps every exposed accessibility action real.

## RatingDisplay

`RatingDisplay` is passive. It renders half-filled aggregate results, always
shows the numeric value, optionally formats a count with thousands separators,
and supports the compact one-symbol form. A rendered instance always has a
value (default zero) and therefore always produces a non-empty accessible name.
Callers with no rating result should omit the component.

## Verification

- `WhatsUIRatingImageTests` covers value normalization, pointer and keyboard
  changes, binding, read-only/disabled behavior, SetValue, item labels, image
  fallback, and accessibility snapshots.
- `WhatsUIFluentRatingImageVisualTests` generates the complete visual matrix at
  both 100% and 150% scale. It also guards against clip/batch corruption and
  border color leaking into transparent image letterbox space.
