# Fluent Selection, Range, Media, and Rating Completion

Status: complete (Release verification passed). Source of truth: the official Fluent UI React v9 Storybook pages supplied for Divider, Slider, ProgressBar, Checkbox, RadioGroup, Switch, Image, Rating, and RatingDisplay.

## Acceptance rules

- Public API semantics must match Fluent concepts even when the native C++ spelling differs from React props.
- Interactive controls cover rest, hover, pressed, focused, disabled, keyboard, pointer, binding, and accessibility actions.
- Geometry must remain aligned and unclipped at 100% and 150% DPR.
- Software visual artifacts require pixel assertions and visual review; a passing executable alone is insufficient.

## A. Divider

- [x] Horizontal and vertical orientation, labelled and unlabelled content.
- [x] Default, subtle, brand, and strong appearances.
- [x] Start, center, and end content alignment plus inset line behavior.
- [x] Separator accessibility semantics and 100%/150% visual coverage.

## B. Slider and ProgressBar

- [x] Slider clamping, step, controlled binding, pointer drag, keyboard Home/End/arrows.
- [x] Slider horizontal/vertical orientation, focus, disabled and extrema geometry.
- [x] ProgressBar determinate range and indeterminate state with accessible numeric/busy semantics.
- [x] Fluent thickness, track/brand tokens and 100%/150% visual coverage.

## C. Checkbox, RadioGroup, and Switch

- [x] Checkbox unchecked, checked, mixed, disabled and full interaction state matrix.
- [x] RadioGroup owns exclusive selection, layout orientation, binding and arrow-key navigation.
- [x] Switch off/on, label placement, full interaction states and Toggle accessibility action.
- [x] Labels/baselines, focus cues, hit targets and clipping verified at 100%/150% DPR.

## D. Image

- [x] Preserve existing source interning and contain/cover/stretch behavior.
- [x] Fluent fit/shape/border/shadow concepts mapped without breaking intrinsic sizing.
- [x] Accessible name and decorative-image semantics.
- [x] Empty source/fallback, aspect-ratio variants and rounded clipping visual coverage.

## E. Rating and RatingDisplay

- [x] Rating controlled/uncontrolled value, max, step 1/0.5, clear, size and color variants.
- [x] Pointer preview/selection, keyboard navigation, focus, disabled and RadioGroup-style accessibility.
- [x] RatingDisplay rounded half-star value, optional count, compact mode, max, size and color variants.
- [x] RatingDisplay always exposes its value and never emits an unlabeled/empty semantic result.
- [x] Software visual matrix covers sizes, colors, half values, compact/count and 100%/150% DPR.

## F. Final gate

- [x] Dedicated behavior/accessibility tests pass for all nine components.
- [x] Unified Software visual artifacts reviewed at 100% and 150% with no clipping, overlap, state ambiguity or pixel drift.
- [x] Release build and all `whatsui_` CTest targets pass (43/43).
- [x] `git diff --check` passes; touched WhatsUI targets build without new warnings.

## Verification record

- Release build: `build-native-text-compare`, 2026-07-18.
- CTest: `ctest --test-dir build-native-text-compare -C Release -R '^whatsui_'` — 43/43 passed.
- Visual artifacts: checkbox, range/selection, and rating/image matrices were generated at 100% and 150% DPR and reviewed. The 150% captures confirm circular controls remain un-clipped, focus is a paired stroke, and Slider/Switch/Radio state geometry stays aligned.
- Native Windows UIA smoke covers Slider and determinate ProgressBar `RangeValue` metadata (value, min/max, step and read-only state), and verifies that indeterminate ProgressBar deliberately omits that pattern.
- Image sources now intern only immutable RGBA data; each `Image` widget owns the GPU texture for its own Canvas context, preventing cross-window texture rebinding.
