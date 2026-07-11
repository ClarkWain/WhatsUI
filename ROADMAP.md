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
- [ ] Frame/layout/paint/dirty/draw-call/text-cache instrumentation.
- [x] UiInspector snapshot: node tree, final rect, dirty flags, hit path, and
  repaint summary.
- [ ] Inspector constraints, resolved style values, and repaint-region overlays.
- [x] Benchmarks: 1,000 controls, 100,000-row logical list, 10,000 text nodes,
  mutation storm, and overlay stress.

### M5 — Stable Windows 1.0

- [ ] Stable source API policy, documented no-ABI guarantee before 1.0, then
  explicit compatibility policy.
- [ ] Windows package exports for core + WhatsCanvas/GLFW path, clean external
  consumer smoke, LICENSE/NOTICE/SBOM, changelog, and release archive hashes.
- [ ] Windows support matrix, known limitations, upgrade guide, and contributor
  documentation.
- [ ] Three maintained reference applications: Todo, Settings, and Debug
  Inspector.

## Current active focus

The immediate sequence is M4 performance/inspector work, followed by the
Windows release contract. New components must
not bypass the runtime, input, and Windows text contracts.
