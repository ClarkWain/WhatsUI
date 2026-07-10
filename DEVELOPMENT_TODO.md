# WhatsUI Development TODO

This file is the implementation checklist for the first usable WhatsUI milestone.
The scope is intentionally limited to the core runtime and five foundational
widgets: `Text`, `Image`, `Container`, `Column`, and `Row`.

## Definition of done

- Public APIs compile both with and without WhatsCanvas enabled.
- Every foundational widget has deterministic measure, layout, paint, and hit-test behavior.
- State changes are safe during event dispatch and are committed at a defined frame boundary.
- A window owns one coherent UI domain: root, navigator, overlays, input routing, and frame updates.
- Unit tests cover behavior and geometry; the Software backend covers rendered output.
- The Todo demo uses only the milestone runtime and foundational widgets for its visual structure.

## Phase 1 - Foundational widgets

- [x] Text: intrinsic measurement, color, size, paint.
- [x] Image: RGBA source, intrinsic size, fill/contain/cover, alignment, paint.
- [x] Container: background, radius, padding, multi-child overlay layout.
- [x] Row: constrained measure, flex layout, baseline/cross-axis alignment, hit testing.
- [x] Column: constrained measure, flex layout, alignment, hit testing.
- [x] Add focused tests for all five widgets.

## Phase 2 - App, Window, Navigator

- [x] Add a single `UiWindow::update/layout/paint` frame entry point.
- [x] Keep the input root synchronized with the active page/root.
- [x] Make Navigator activation observable by its owning window.
- [x] Implement page retention semantics instead of storing metadata only.
- [ ] Route input through overlays above page content (layout, paint, and overlay hit testing are complete).
- [x] Use real frame delta for animations.
- [ ] Add window/runtime tests with a fake platform host.

## Phase 3 - State and update flow

- [ ] Define event dispatch and state-commit boundaries.
- [ ] Make structural updates safe under nested/re-entrant state changes.
- [ ] Coalesce duplicate structural updates per frame.
- [ ] Propagate layout and paint invalidation to the correct boundary.
- [ ] Request a redraw when observable UI state changes.
- [ ] Add teardown, re-entrancy, and self-removal regression tests.

## Phase 4 - Geometry and rendering contract

- [x] Document constraints passed by each foundational container.
- [x] Ensure measure results are always clamped and non-negative.
- [ ] Ensure layout clears layout dirtiness recursively where appropriate.
- [ ] Define paint order and clipping behavior.
- [x] Prepare image/backend resources before beginning the paint pass.
- [ ] Share immutable image resources across rebuilt nodes.
- [x] Isolate Software-backend visual-regression scenes with one canvas per capture.
- [ ] Define reverse-paint-order hit testing and visibility rules.
- [ ] Add layout snapshots for nested and constrained compositions.
- [ ] Add Software-backend visual regression scenes.
- [x] Apply device-pixel ratio consistently to geometry, images, and text.
- [x] Enable analytic anti-aliasing for rounded geometry.

## Phase 5 - Todo demo

- [x] Build a polished header, summary card, task list, and footer.
- [x] Provide useful empty, active, completed, and mixed states.
- [ ] Support add, toggle, delete, and clear-completed actions.
- [ ] Keep event callbacks safe when they mutate list structure.
- [ ] Produce deterministic Software-backend screenshots.
- [ ] Provide an interactive GLFW version using the same UI tree.

## Deferred

- Text input and IME refinement.
- Advanced focus traversal and accessibility.
- Additional controls such as Button, Checkbox, ScrollView, Menu, and Dialog.
- Animation and compositing polish beyond what the Todo demo requires.
