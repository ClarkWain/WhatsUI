# Fluent P2 — Disclosure, Surfaces, and Identity

Status: complete (2026-07-18). This batch implements WhatsUI-native counterparts to the
Fluent UI React v9 Accordion, Drawer, and Persona component families.

## Acceptance gates

- [x] Accordion: semantic heading/region relationship, single and multiple
  expansion policies, disabled items, pointer/keyboard activation, stable
  content measurement, and 100%/150% visual state coverage.
- [x] Drawer: inline and overlay forms, start/end/bottom placement, size,
  overlay dismissal policy, Escape/focus restoration, scroll-safe content,
  title/body/action layout, Dialog semantics, and 100%/150% visual coverage.
- [x] Persona: avatar/person metadata, text hierarchy, presence/status,
  size variants, overflow handling, interactive policy, accessible naming,
  and 100%/150% visual coverage.
- [x] Public headers are exported through `wui.h`, `ui.h`, and CMake; Windows
  UI Automation mappings and semantic snapshots expose the final roles.
- [x] Dedicated behaviour tests, Software visual captures, focused P2 CTest,
  full `^whatsui_` CTest, and `git diff --check` all pass.

## Component contracts

### Accordion

The group owns expansion policy and keyboard navigation. Items own a header
trigger and a content region. Collapsed content is neither measured nor painted
and is excluded from the semantic tree. Decorative chevrons must never be the
only expansion affordance.

### Drawer

An inline drawer participates in the containing layout; an overlay drawer is
an explicit surface hosted by `OverlayHost`. Modal overlay drawers trap focus
and restore it to their trigger on dismissal. Non-modal drawers preserve normal
page navigation. Surface placement is logical start/end/bottom rather than a
hardcoded physical side.

### Persona

Persona composes an Avatar and optional presence indicator with primary,
secondary, tertiary, and quaternary text. The aggregate has one useful spoken
name; decorative avatar imagery is not announced twice. Text must truncate
before it can overlap presence or action affordances.

## Verification record

- Built the complete `Release` tree with WhatsCanvas, examples, and tests.
- P2-focused CTest: 9/9 passed (behaviour plus Software captures at 100% and
  150% for every component family).
- Full `ctest -C Release -R '^whatsui_'`: 72/72 passed.
- Personally reviewed the three 150% Software captures for clipping, overlap,
  state clarity, elevation and text hierarchy.
- `git diff --check` passed. The sole console note is the repository's
  existing CRLF normalization advisory for `windows_uia_provider.cpp`.
