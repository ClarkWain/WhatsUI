# UIA Text and TextRange pattern — design proposal

> Status: **design draft**. Not implemented. This document lets future work
> proceed against a shared plan; it is a scoping and contract document, not a
> stability promise. Anyone starting the implementation should update this
> file first if they diverge from the plan below.

## Motivation

Windows UI Automation exposes editable and read-only text to screen readers
(Narrator, JAWS, NVDA) through two COM interfaces:

- `IUIAutomationTextProvider` / `ITextProvider` — attached to a control that
  contains text (TextInput, TextArea, read-only labels used as live regions).
- `IUIAutomationTextRangeProvider` / `ITextRangeProvider` — a bounded run of
  text inside that provider that the client can inspect, move, or select.

WhatsUI's Windows UIA adapter currently ships Invoke, Toggle, Value,
ExpandCollapse, RangeValue, Selection, and SelectionItem, plus native
focus/property/structure events. **The `Text` pattern is not implemented.**
Without it, Narrator can only announce the whole `Value` string returned by
`IValueProvider`; it cannot read a caret line, extend a selection with
`Shift+Right`, report a word under the reading cursor, or expose format
attributes such as bold or a spellcheck marker. That is the last
accessibility-critical UIA pattern for the 1.0 candidate.

## Goals

1. Deliver a `TextPattern` that lets Narrator read a `TextInput` / `TextArea`
   by character, word, line, paragraph, and document, following the caret
   and framework selection.
2. Expose `SupportedTextSelection::Single`, `GetSelection`, `GetActiveComposition`,
   `RangeFromPoint`, and `DocumentRange` for a typical single-selection
   editor.
3. Keep the `TextEditingController` on the WhatsUI UI thread as the single
   source of truth for text, selection, and composition. UIA calls resolve
   against a retained immutable snapshot exactly the way the current
   provider does for other patterns.
4. Emit `UIA_Text_TextChangedEventId` and `UIA_Text_TextSelectionChangedEventId`
   from the same `raiseSnapshotEvents` diff loop that already dispatches
   property, focus, structure, and selection events.
5. Keep the entire pattern optional and role-gated: only controls that opt
   into "text pattern" via their `AccessibilityProperties.actions` and role
   expose it, so existing consumers do not accidentally receive text ranges.

## Non-goals

- Multi-selection ranges. WhatsUI's editor exposes exactly one selection
  range plus one composition range; `SupportedTextSelection` starts at
  `Single`.
- Rich-text attributes beyond a documented white-list. Only attributes that
  WhatsUI can genuinely produce (`IsReadOnly`, `IsHidden`, `AnimationStyle`,
  `CultureAttribute`, `FontName`, `FontSize`) go on the initial white-list;
  everything else returns `UiaGetReservedNotSupportedValue()`.
- Table / hyperlink / annotation text ranges (`ITextRangeProvider::GetChildren`,
  `TextPattern2`, `TextPattern_HyperlinkPatternId`). These are optional in
  UIA and out of scope for 1.0.
- Full TSF integration for `GetActiveComposition` beyond exposing what the
  existing IMM32 controller already knows. Anything requiring TSF migration
  is deferred.
- Editing through UIA (`ITextRangeProvider::AddToSelection`, direct text
  insertion). UIA clients that want to type must continue to use the Value
  pattern or synthesize keyboard input.

## Shared model contract

The Text pattern MUST NOT introduce a second source of truth. The provider
reads exclusively from an immutable per-frame snapshot, exactly like the
existing pattern implementations. Two adjustments to the snapshot pipeline
are required:

1. `AccessibilitySnapshotEntry` gains an optional `TextModel` sub-record
   carrying:
   - `text`: the UTF-8 byte string.
   - `selection`: `{ start, end }` in UTF-8 byte offsets, empty when the
     control has no caret.
   - `composition`: `{ start, end }` in UTF-8 byte offsets, empty when no
     IME session is active.
   - `lineBreaks`: a monotonically increasing list of UTF-8 byte offsets that
     mark hard line breaks (the character *after* the break). Populated by
     TextInput/TextArea's layout using the same wrapping the paint pass
     produced; empty for single-line inputs.
   - `documentBounds`: the client-space rectangle of the whole editor.
   - `caretBounds`: the client-space rectangle of the primary caret if the
     control has focus.
   The record is `std::optional<TextModel>`; controls without a text pattern
   leave it null.
2. `AccessibilityActionCapabilities` gains one flag: `text`. Controls that
   opt in are required to populate `TextModel` on every publish.

Because `TextModel` participates in the snapshot payload, extending
`snapshotsSemanticallyIdentical` to include it is mandatory before the
pattern can be safely enabled: without that extension, `publish()` would
short-circuit legitimate text changes.

The provider **does not** call back into the UI thread synchronously to
resolve a text range. All queries return computed answers derived from the
retained snapshot. Coordinate conversions reuse the existing `screenBounds`
helper.

## Text-unit mapping

UIA defines the `TextUnit` enumeration; WhatsUI must map every value to a
deterministic offset transformation over `TextModel::text`:

| TextUnit | Definition | WhatsUI mapping |
| --- | --- | --- |
| `Character` | One grapheme cluster | UTF-8 grapheme walk over `text`; falls back to code point when a grapheme table is unavailable. |
| `Format` | Run with the same formatting | Whole document. WhatsUI editors are single-format for 1.0. |
| `Word` | User-perceived word | Reuse `TextEditingController::previousWordBoundary` / `nextWordBoundary`, which already power `Ctrl+Left/Right`. |
| `Line` | Wrapped visual line | Use `TextModel::lineBreaks`. Single-line inputs report one line. |
| `Paragraph` | Author-defined paragraph | Hard line break in the source string (`\n`). For a `TextInput`, one paragraph covers the whole content. |
| `Page` | Scrollable page | Whole document. Paginated content is out of scope. |
| `Document` | Entire editable content | The `DocumentRange`. |

Every unit must be idempotent: `ExpandToEnclosingUnit(unit)` followed by
`Move(unit, 0)` must return the same offsets. Implementations MUST NOT rely
on any hidden UI-thread state to make this hold.

## Interface surface

### `IUIAutomationTextProvider` methods

| Method | Behavior |
| --- | --- |
| `GetSelection` | Returns a `SAFEARRAY` with one `ITextRangeProvider` when the snapshot has a caret; empty array otherwise. |
| `GetVisibleRanges` | Returns one range covering `documentBounds ∩ viewport`. The viewport is derived from the retained control bounds plus any enclosing scroll container's current offset (already tracked by TextArea). |
| `RangeFromChild` | Returns the caret range; children are not supported for 1.0. |
| `RangeFromPoint` | Convert screen point to logical coordinates via `screenBounds`, then use TextInput's existing `caretAt` mapping to find the byte offset. |
| `DocumentRange` | `{ 0, text.size() }`. |
| `SupportedTextSelection` | `SupportedTextSelection_Single`. |

`ITextProvider2`'s `GetCaretRange` and `RangeFromAnnotation` are optional and
deferred; the initial provider implements only `ITextProvider`.

### `IUIAutomationTextRangeProvider` methods

The range is a `{ start, end }` byte pair plus a stable back-reference to
the snapshot's provider ID, exactly like the existing SnapshotProvider
addressing. Range immutability follows the pattern rules: `Clone` returns a
new object; `Select` requests the UI thread to apply via
`TextEditingController::setSelection`.

| Method | Behavior |
| --- | --- |
| `Compare` | Byte-offset equality on the same document. |
| `CompareEndpoints` | Compare `Start` vs `End` byte offsets. |
| `ExpandToEnclosingUnit` | Grow both endpoints to unit boundaries per the mapping table. |
| `FindAttribute` | Deferred; return `UiaGetReservedNotSupportedValue()`. |
| `FindText` | UTF-8 substring search, honoring `ignoreCase` when the search string is ASCII (documented limitation). |
| `GetAttributeValue` | White-list from Non-goals: `IsReadOnly`, `IsHidden` (false), `FontName`, `FontSize`, `CultureAttribute`; everything else `NotSupported`. |
| `GetBoundingRectangles` | For each visual line inside the range, compute a client-space rectangle using the paint layout; convert to screen using `screenBounds`. |
| `GetEnclosingElement` | The owning `SnapshotProvider`. |
| `GetText` | UTF-8 slice, honoring `maxLength`. |
| `Move` / `MoveEndpointByUnit` | Advance by unit using the mapping table. |
| `MoveEndpointByRange` | Copy the target endpoint. |
| `Select` | Post an `AccessibilityActionRequest` of kind `SetSelection` (new kind, mirrors the existing `SetValue` pattern) with `{ start, end }` as the payload. |
| `AddToSelection` / `RemoveFromSelection` | Deferred; return `UIA_E_INVALIDOPERATION`. |
| `ScrollIntoView` | Post a `ScrollIntoView` action; TextArea already has scroll offset control. |
| `GetChildren` | Empty. |

Every method that touches `SnapshotProvider`'s pointer must go through the
existing `resolve()` helper so a stale range returns `UIA_E_ELEMENTNOTAVAILABLE`
instead of crashing.

## Action plumbing

Two new `AccessibilityActionKind` values are needed:

- `SetSelection`, carrying a `{ start, end }` byte range in the request's
  string payload (or via a new optional field — the exact shape is a small
  RFC).
- `ScrollTextRange`, carrying an alignment enum.

The UI-thread handler (already dispatched by `UiaActionQueue`) resolves the
action against the current controller and applies the change, then relies on
the next publish to raise the text change events.

## Events

Two new UIA events must be raised by `raiseSnapshotEvents`:

- `UIA_Text_TextChangedEventId` — when the previous and current TextModel
  differ in `text`, `lineBreaks`, or `composition`.
- `UIA_Text_TextSelectionChangedEventId` — when the selection endpoints
  differ, or when focus moves between text controls.

Both integrate into the same short-circuit / diff pipeline that already
protects UIA clients from steady-state event storms (see the existing
`snapshotsSemanticallyIdentical` fast path).

## Testing strategy

The Text pattern is the largest UIA surface WhatsUI will ship; it needs its
own dedicated smoke test binary rather than an extension to
`whatsui_windows_uia_native_smoke_tests`.

| Test | Coverage |
| --- | --- |
| Unit: text unit walker | `Character`, `Word`, `Line`, `Paragraph`, `Document` boundary tables against a fixed corpus (ASCII, CJK, emoji, mixed grapheme). No Win32. |
| Unit: FindText | ASCII and non-ASCII, `ignoreCase` fallback, empty haystack, empty needle. |
| Unit: attribute white-list | Every listed attribute returns a value; every other attribute returns the reserved not-supported sentinel. |
| Native: `ITextProvider` shape | UIA client acquires TextPattern from a real HWND, calls `DocumentRange`, `GetSelection`, `RangeFromPoint` — asserts non-empty results and correct bounds. |
| Native: caret movement | Framework changes the caret via the controller; UIA client re-queries the selection range and asserts the expected byte offsets. |
| Native: `Text_TextChangedEventId` and `Text_TextSelectionChangedEventId` | Subscribe to both events, mutate the underlying editor, assert event delivery using the same quiescence-based drain as `whatsui_windows_uia_native_smoke_tests`. |
| Manual: Narrator matrix | A short scripted Narrator session (arrow keys, Ctrl+Home, `Read from cursor`, `Move by word`, `Move by line`) captured in the release-owner evidence. This is a **release-candidate gate**, not an automated test. |

## Rollout plan

Ship the pattern in three review-sized changes so no single PR carries the
whole surface:

1. **Model plumbing** (no UIA yet). Extend `AccessibilitySnapshotEntry` with
   `TextModel`, plumb line breaks and caret bounds through TextInput/TextArea,
   extend `snapshotsSemanticallyIdentical`. Add the unit-level text-unit
   walker library. No `IUIAutomationTextProvider` yet.
2. **Read-only pattern**. Implement `ITextProvider` + `ITextRangeProvider`
   with the read-only subset (`DocumentRange`, `GetSelection`,
   `RangeFromPoint`, `GetText`, `GetBoundingRectangles`, `Move` by all units,
   attribute white-list). Text changed/selection changed events wired into
   the diff loop. Add the native smoke test.
3. **Interactive `Select` and `ScrollIntoView`**. Add the two new
   `AccessibilityActionKind` values, the UI-thread handlers, and the round-trip
   test that the UIA client can steer selection.

Each PR MUST include the regression tests for that stage. Narrator manual
verification joins the Windows 1.0 release-candidate evidence per the
compatibility policy.

## Open questions

- Grapheme walker: bundle ICU segmenter, use HarfBuzz cluster info that
  WhatsCanvas already computes, or write a Unicode 15 grapheme table
  targeted at the CJK and emoji corpora WhatsUI already tests? Cost varies
  by an order of magnitude.
- Line-break source: does TextInput's paint pass already surface wrap
  offsets that can be reused, or does the Text pattern need its own
  measurement that mirrors the paint layout? Prefer reuse to keep the
  Narrator output identical to the visible text.
- Coordinate precision for `GetBoundingRectangles`: aggregate contiguous
  glyphs per visual line into one rectangle, or ship per-glyph rectangles?
  Narrator typically wants one rectangle per visual line; per-glyph adds
  memory pressure without observable benefit.
- Composition ranges: expose the IMM32 composition range as a UIA range
  with the `IsActiveComposition` attribute (`ITextProvider2`) or restrict
  the 1.0 pattern to committed text? Deferring to a follow-up avoids the
  `ITextProvider2` COM cast for the first release.

Answering these three questions is a prerequisite to starting stage 1.
