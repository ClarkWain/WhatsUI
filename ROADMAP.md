# WhatsUI Roadmap

WhatsUI is a small retained-mode C++ UI runtime for embedded desktop tools and
custom-rendered applications. Windows with the GLFW reference host is the
recommended platform until other platforms have equivalent input, DPI, window
lifecycle, clipboard, and IME coverage.

## Product boundary

### Primary users

- C++ desktop tools, debuggers, profilers, launchers, settings panels, and
  embedded panels in existing OpenGL/GLFW applications.

### Explicit non-goals before 1.0

- Mobile, browser-style applications, rich-text editing, office suites,
  DataGrid/TreeView/docking/WebView, ABI stability, and a claim of complete
  accessibility or cross-platform parity.

## Milestones and release gates

Each check is a verifiable gate. A milestone is not complete because a demo
looks plausible; its listed tests, examples, and documented limitations must
all be true.

### M0 — Runtime Contract (`v0.1-runtime-preview`)

- [x] Retained node ownership, safe deferred structural mutations, and teardown
  subscriptions.
- [x] Deterministic constraints/layout/paint/hit-test tests and Software
  captures.
- [x] Dirty propagation, frame-boundary state commits, and fake platform-window
  regression tests.
- [x] Lifecycle contract: explicit attach/detach callbacks and a test proving
  parent destruction detaches every child exactly once.
- [x] Mutation stress: 1,000 randomized tree mutations and nested self-removal
  without crash or leaked subscriptions.
- [x] Sanitizer-ready CI configuration (ASan/UBSan where supported; MSVC ASan
  alternative documented and gated).

### M1 — Input, Focus, and Scroll (`v0.2-input-preview`)

- [x] Pointer/keyboard routing, overlay routing, Tab traversal, dialog focus
  restoration, clipping, wheel input, and vertical ScrollView.
- [x] Pointer capture API and cancellation semantics.
- [x] Capture/target/bubble routing contract with typed event result/context.
- [x] Nested scrolling handoff and horizontal scrolling.
- [x] Settings Panel reference example: controls, scrolling, tab navigation,
  dialog, and popup menu.

### M2 — Windows Text and IME (`v0.3-text-preview`)

- [x] UTF-8 Text measurement/wrapping/ellipsis, TextInput session routing, and
  composition model lifecycle.
- [x] Windows IME adapter (TSF or IMM32): composition update, candidate window,
  caret placement, and high-DPI verification.
- [x] Text editing controller: selection by pointer/Shift, Home/End, word
  deletion, clipboard, undo/redo, and composition underline.
- [x] Shaping/fallback policy for CJK, emoji, bidi, and deterministic text cache.
- [x] Searchable command palette and editable Settings form reference examples.

### M3 — Components and Themes (`v0.4-components-preview`)

- [x] Fluent light default tokens, Button, Checkbox, TextField, Dialog, and
  ScrollView.
- [x] Dark theme and local theme override regression coverage.
- [x] Radio, Switch, Slider, ProgressBar, Divider, IconButton, SearchField,
  Menu/Popup, Tooltip, and ListView.
- [x] StateProperty-style resolution shared by component visual states.
- [x] Semantic/control accessibility model scoped to supported platforms.

### M4 — Performance and Inspector (`v0.5-performance-preview`)

- [x] Keyed virtual ListView and item reuse.
- [x] Frame/layout/paint/dirty instrumentation, with backend draw-call and
  text-cache counters reported as available only when the renderer can measure
  them.
- [x] UiInspector snapshot: node tree, final rect, dirty flags, hit path, and
  repaint summary.
- [x] Inspector constraints, resolved style values for built-in Fluent controls,
  and conservative repaint-region overlays.
- [x] Benchmarks: 1,000 controls, 100,000-row logical list, 10,000 text nodes,
  mutation storm, and overlay stress.

### M5 — Windows 1.0 release groundwork

- [x] Pre-1.0 source API policy documenting the no-ABI-guarantee preview
  contract.
- [ ] 1.0 source/ABI compatibility policy, deprecation policy, and release
  owner approval. The policy draft is available in
  [COMPATIBILITY_POLICY_1_0_DRAFT.md](doc/whatsui/COMPATIBILITY_POLICY_1_0_DRAFT.md),
  but approval and candidate-specific support tuples remain release-candidate
  gates, not preview claims.
- [x] Windows package exports for core + WhatsCanvas/GLFW path and clean
  external consumer smoke.
- [x] Release groundwork: first-party MIT LICENSE/NOTICE, changelog, and
  reproducible archive-hash rehearsal.
- [x] Windows support matrix, known limitations, upgrade guide, and contributor
  documentation.
- [x] Three maintained reference applications: Todo, Settings, and Debug
  Inspector.
- [ ] Release-candidate approval: tagged clean checkout, native application and
  IME/DPI matrix, artifact review, and third-party legal/SBOM sign-off. These
  are deliberately kept outside automated completion claims.

### M6 — Todo reference product completion

- [x] Important and optional ISO due-date metadata are available in the task
  rows and the edit dialog, with persistent storage and filtering retention.
- [x] Title, important state, and due date commit as one validated mutation and
  one Undo checkpoint; invalid input cannot partially update a task.
- [x] Compact task rows use a two-level layout at the 360 DIP breakpoint, with
  named important actions and accessibility snapshot coverage.
- [x] Collapsed conditional content does not reserve Row/Column gaps, with
  layout regression tests and narrow/regular/wide visual review captures.
- [x] Controller, interaction, storage, UI smoke, visual regression, and visual
  review tests cover the delivered metadata workflow.

## Current active focus

The Todo reference-product work is complete. The remaining release work is
release-candidate validation and owner approval. New components must not bypass
the runtime, input, and Windows text contracts.
