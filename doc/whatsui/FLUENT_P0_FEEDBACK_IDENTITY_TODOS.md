# Fluent P0 — Form Feedback and Identity

Status: complete (2026-07-18). This batch follows the official Fluent UI React v9
Storybook pages supplied for Field, MessageBar, Toast, Spinner, Badge,
CounterBadge, PresenceBadge, Avatar, and AvatarGroup.

## Source references

- <https://storybooks.fluentui.dev/react/?path=/docs/components-field--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-messagebar--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-toast--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-spinner--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-badge-badge--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-badge-counter-badge--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-badge-presencebadge--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-avatar--docs>
- <https://storybooks.fluentui.dev/react/?path=/docs/components-avatargroup--docs>

## Acceptance gates

- [x] Field: label/control/hint/validation composition, required association,
  vertical and horizontal layouts, disabled state, keyboard and UIA semantics.
- [x] MessageBar: info/success/warning/error intents, multiline content,
  actions/dismissal, focus and non-modal announcement semantics.
- [x] Toast/Toaster: intent, title/body/action/dismiss, FIFO queue, position,
  pause/timeout and live announcement semantics.
- [x] Spinner: all Fluent sizes, label positions, reduced motion, busy
  semantics and no unnecessary animation work while detached.
- [x] Badge: appearance/color/size/shape state matrix and concise text
  accessibility.
- [x] CounterBadge: zero policy, overflow formatting, numeric accessibility.
- [x] PresenceBadge: all presence states, anchored avatar geometry, named
  status semantics and 100%/150% clipping coverage. Figma's discrete Avatar
  mapping is 6/10/12/16/20/28 DIP; the instance frame is edge-aligned and its
  own 1-DIP white ring provides the optical separation.
- [x] Avatar/AvatarGroup: image/initials/person-icon fallback, color/shape/
  size variants, activity ring, stacked/spread groups, overflow indicator and
  image/group semantics. The activity ring is painted outside its fixed Avatar
  footprint (2 DIP through 48, 3 DIP from 56) so it never changes layout.
- [x] Unified 100% and 150% Software visual capture personally reviewed.
- [x] Release build, full `whatsui_` CTest suite, and `git diff --check` pass.

## Verification record

- Focused P0 suite: 12/12 passed, covering behaviour and Software captures at
  100% and 150% DPI for Field/MessageBar, Toast/Spinner, Badge/Presence, and
  Avatar/AvatarGroup.
- Release regression: all 55 tests matching `^whatsui_` passed; no test
  failure was recorded in CTest's final log.
- Visual review: the 150% captures were opened and reviewed for clipping,
  state color contrast, roundness, spacing, text placement, elevation, avatar
  overlap, and overflow rendering.
- Avatar alignment review (2026-07-21): compared directly with Figma's
  `Avatar/Avatar` component variants. Added the neutral Person fallback,
  Figma's activity-ring guard/stroke geometry, exact PresenceBadge size table
  and edge anchoring; the regenerated 150% capture was visually reviewed.
