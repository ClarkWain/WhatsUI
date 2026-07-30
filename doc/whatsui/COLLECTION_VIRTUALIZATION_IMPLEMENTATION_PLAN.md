# Collection Virtualization Implementation Plan

Status: implemented and validated. Companion document to
`COLLECTION_VIRTUALIZATION_ARCHITECTURE.md`.

This plan breaks collection virtualization into small implementation steps with
focused tests, validation gates, and a final baseline. The goal is to make
WhatsUI collection controls scalable while preserving each control's public
semantics and visual contract.

## Baseline Before Any Code Change

Create a clean behavior baseline before the first implementation patch. The
baseline records current behavior and catches unrelated local failures.

Build configuration:

```powershell
cmake -S . -B build-virtualization-baseline `
  -DWHATSUI_WITH_WHATSCANVAS=ON `
  -DWHATSUI_BUILD_TESTS=ON `
  -DWHATSUI_BUILD_EXAMPLES=OFF
cmake --build build-virtualization-baseline --config Debug --parallel 4
```

Focused baseline tests:

```powershell
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_(virtual_list_tests|list_view_tests|fluent_selection_controls_tests|fluent_table|fluent_tree|scroll_view_tests)"
```

Record these baseline facts in the implementation PR or task note:

- build directory and generator
- commit for WhatsUI and `third_party/WhatsCanvas`
- pass/fail result for the focused regex
- existing unrelated failures, if any
- current `VirtualList` mounted and pooled count expectations from tests

Do not start refactoring if the focused baseline fails for reasons related to
the touched controls. Fix or explicitly record the baseline failure first.

## Step 1: Add `ViewportModel`

Implementation:

- Add an internal fixed-extent viewport model, for example
  `include/wui/internal/viewport_model.h`.
- Keep it independent from `Node`, `Theme`, and painting.
- Store item count, item extent, viewport extent, overscan count, and scroll
  offset.
- Expose visible range, overscan range, max scroll offset, clamped scroll
  offset, and scroll-to-index.
- Treat empty lists, non-positive viewport, non-finite extents, and invalid
  scroll requests deterministically.

Tests:

- Add a focused unit test target, for example `WhatsUIViewportModelTests` with
  CTest name `whatsui_viewport_model_tests`.
- Cover empty list, zero viewport, one row, exact row boundary, partial row,
  overscan at start/end, max scroll, negative scroll clamp, over-end clamp, and
  scroll-to-index.

Validation after this step:

```powershell
cmake --build build-virtualization-baseline --config Debug --target WhatsUIViewportModelTests
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_viewport_model_tests"
```

Exit criteria:

- New viewport tests pass.
- No production control consumes the model yet.
- The model API has no control-specific naming such as list, row, table, or
  option beyond neutral item/index vocabulary.

## Step 2: Migrate `VirtualList` Viewport Math

Implementation:

- Replace `VirtualList`'s local item count, row extent, scroll offset, max
  scroll, visible range, and mounted range calculations with `ViewportModel`.
- Preserve public `VirtualList` behavior and method names.
- Keep `mounted_` and `pool_` inside `VirtualList` for now. Do not extract the
  recycler in the same patch.
- Keep `overscanRows_` behavior equivalent.

Tests:

- Existing `whatsui_virtual_list_tests` must continue to pass.
- Add assertions if needed for boundary behavior now owned by `ViewportModel`.

Validation after this step:

```powershell
cmake --build build-virtualization-baseline --config Debug --target WhatsUIVirtualListTests
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_(viewport_model_tests|virtual_list_tests)"
```

Exit criteria:

- Mounted count remains bounded for 100k rows.
- Stable-key reuse and reentrant detach tests still pass.
- No `ListBox`, `Table`, or `ListView` behavior changes yet.

## Step 3: Migrate Value-Window Controls to `ViewportModel`

Implementation:

- Migrate `ListBox` scroll/range math to `ViewportModel` while keeping value
  drawing and virtual option accessibility snapshots.
- Migrate `Table` and `DataGrid` visible row range, scroll clamping, and
  scroll-row-into-view math to `ViewportModel`.
- Keep existing vector-backed data models.
- Do not introduce node recycling into these controls.

Tests:

- `whatsui_fluent_selection_controls_tests`
- `whatsui_fluent_table`
- Existing visual tests may be run after behavior tests if row positioning is
  touched.

Validation after this step:

```powershell
cmake --build build-virtualization-baseline --config Debug --target `
  WhatsUIFluentSelectionControlsTests WhatsUIFluentTableTests
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_(viewport_model_tests|virtual_list_tests|fluent_selection_controls_tests|fluent_table)"
```

Visual validation if paint geometry changed:

```powershell
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_fluent_(selection_controls_visual|selection_controls_visual_150dpi|table_visual|table_visual_125dpi|table_visual_150dpi|table_visual_200dpi)"
```

Exit criteria:

- ListBox type-ahead, popup behavior, multiselect, and virtual option bounds
  remain unchanged.
- Table/DataGrid visible rows, selection, focus, sorting, and accessibility
  entries remain unchanged.
- No new retained row nodes are introduced for value-projection controls.

## Step 4: Extract `KeyedRecycler`

Implementation:

- Move `VirtualList` mounted/pool/reconcile logic into an internal
  `KeyedRecycler` helper.
- Keep duplicate-key handling, pool trimming, and reentrant detach protection.
- Let `VirtualList` continue to own frame painting, selection, hit testing, and
  keyboard policy.
- Keep `KeyedRecycler` independent from viewport math except for accepting a
  desired range.

Tests:

- Add direct recycler tests only if they can be written without duplicating all
  `VirtualList` behavior.
- Keep `whatsui_virtual_list_tests` as the primary integration safety net.
- Run with sanitizer configuration before considering this step complete,
  because ownership and detach behavior are the risk center.

Validation after this step:

```powershell
cmake --build build-virtualization-baseline --config Debug --target WhatsUIVirtualListTests
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_(viewport_model_tests|virtual_list_tests)"
```

Sanitizer validation:

```powershell
cmake -S . -B build-virtualization-asan -DWHATSUI_ENABLE_SANITIZERS=ON
cmake --build build-virtualization-asan --config Debug --target WhatsUIVirtualListTests
ctest --test-dir build-virtualization-asan -C Debug --output-on-failure `
  -R "whatsui_virtual_list_tests"
```

Exit criteria:

- No ownership regressions under sanitizer.
- Public `VirtualList` API and test expectations remain stable.
- `KeyedRecycler` has no drawing, selection, or accessibility responsibilities.

## Step 5: Modernize `ListView`

Implementation:

- Add viewport state to `ListView` and move paint traversal from full vector
  scan to visible range iteration.
- Add scroll handling if `ListView` is expected to own its viewport; otherwise
  document that it still relies on parent `ScrollView` and only optimizes
  constrained painting.
- Preserve existing vector API, selection binding, disabled-row behavior,
  row height, and measurement compatibility unless a deliberate breaking
  change is approved.
- Add diagnostics such as `visibleRange()` only if tests need direct evidence.
- Add provider-mode API only after visible-range drawing is stable.

Tests:

- Extend `m3_list_view_tests.cpp` for constrained viewport iteration,
  scroll-aware pointer hit testing if scroll is added, keyboard selection, and
  disabled-row skipping.
- Add a large-list test that proves paint/data requests are bounded by visible
  range plus overscan. Use a probe provider or instrumentation rather than
  measuring wall-clock time.

Validation after this step:

```powershell
cmake --build build-virtualization-baseline --config Debug --target WhatsUIListViewTests
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_(viewport_model_tests|list_view_tests|scroll_view_tests)"
```

Exit criteria:

- Existing `ListView` selection and binding tests pass.
- Large-list test proves bounded visible work.
- Parent `ScrollView` interaction remains valid if `ListView` does not own
  scrolling itself.

## Step 6: Add Provider Mode for Large Data

Implementation:

- Add an opt-in provider contract for `ListView` first.
- Add `Table` / `DataGrid` provider mode after `ListView` validates the API
  shape.
- Keep vector APIs as convenience adapters over provider-like internal access.
- Keep provider calls synchronous in the first pass.
- Add invalidation hooks for model refresh, item count change, and stable-key
  change.

Tests:

- Provider-backed `ListView` with 100k logical rows and bounded visible item
  requests.
- Provider-backed `DataGrid` with 100k logical rows, visible accessibility
  entries, selection, focus, and sort delegation.
- Tests for provider count shrink/grow while selection or focus points near the
  end of the model.

Validation after this step:

```powershell
cmake --build build-virtualization-baseline --config Debug --target `
  WhatsUIListViewTests WhatsUIFluentTableTests
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_(list_view_tests|fluent_table|virtual_list_tests)"
```

Exit criteria:

- Provider request counts are bounded and asserted.
- Vector-backed behavior remains compatible.
- Provider-backed sorting is delegated rather than silently sorting incomplete
  local data.

## Step 7: Virtualize Tree Projection

Implementation:

- Build a flattened visible projection of expanded tree items with stable ids,
  levels, expansion state, selection state, and disabled state.
- Apply `ViewportModel` to visible rows.
- Use `KeyedRecycler` only if tree rows become retained custom row nodes.
- Preserve retained `TreeItem` ownership for application identity unless a new
  provider-backed tree API is explicitly introduced.

Tests:

- Extend `fluent_tree_tests.cpp` for large expanded trees, collapsed branch
  removal from visible range, stable selected/focused ids after expand/collapse,
  and bounded visible realization.
- Run tree visual tests if indentation or row placement changes.

Validation after this step:

```powershell
cmake --build build-virtualization-baseline --config Debug --target WhatsUIFluentTreeTests
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_(fluent_tree|viewport_model_tests)"
```

Visual validation if geometry changed:

```powershell
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_fluent_tree_visual"
```

Exit criteria:

- Tree level and selection semantics remain correct.
- Collapsed descendants are absent from visible projection and accessibility
  snapshots.
- Large expanded trees do not require full visible-row painting or retained row
  realization.

## Final Verification Gate

Run this gate after all implementation phases are complete and before calling
the work done.

Debug behavior gate:

```powershell
cmake --build build-virtualization-baseline --config Debug --parallel 4
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "whatsui_(viewport_model_tests|virtual_list_tests|list_view_tests|fluent_selection_controls_tests|fluent_table|fluent_tree|scroll_view_tests)"
```

Release behavior gate:

```powershell
cmake --build build-virtualization-baseline --config Release --parallel 4
ctest --test-dir build-virtualization-baseline -C Release --output-on-failure `
  -R "whatsui_(viewport_model_tests|virtual_list_tests|list_view_tests|fluent_selection_controls_tests|fluent_table|fluent_tree|scroll_view_tests)"
```

Visual gate when paint geometry changed:

```powershell
ctest --test-dir build-virtualization-baseline -C Release --output-on-failure `
  -R "whatsui_fluent_(selection_controls_visual|selection_controls_visual_150dpi|table_visual|table_visual_125dpi|table_visual_150dpi|table_visual_200dpi|tree_visual|tree_visual_125dpi|tree_visual_150dpi|tree_visual_200dpi)"
```

Sanitizer ownership gate:

```powershell
cmake --build build-virtualization-asan --config Debug --parallel 4
ctest --test-dir build-virtualization-asan -C Debug --output-on-failure `
  -R "whatsui_(virtual_list_tests|list_view_tests|fluent_table|fluent_tree)"
```

## Final Baseline Record

After the final gate passes, record a new virtualization baseline. This becomes
the comparison point for later provider, async-loading, or variable-size row
work.

Required baseline fields:

- WhatsUI commit and `third_party/WhatsCanvas` commit
- build directories and CMake options
- Debug behavior gate result
- Release behavior gate result
- sanitizer ownership gate result
- visual gate result and generated capture paths, if applicable
- `VirtualList` maximum mounted count and pooled count in the 100k-row test
- `ListView` large-list visible/request count expectation
- provider-backed `DataGrid` visible row/cell accessibility count expectation,
  once provider mode exists
- known limitations intentionally left for future work

The final baseline should be added to the implementation PR, release note, or a
short follow-up record in `TESTING_AND_VALIDATION.md` if the project wants a
durable repository-local validation history.

## Stop Conditions

Pause and re-evaluate instead of continuing if any of these happens:

- A shared helper starts owning visual states, focus policy, or accessibility
  roles that belong to a specific control.
- `ScrollView` needs collection-specific knowledge to make a phase pass.
- A value-projection control starts allocating retained row nodes only to reuse
  the recycler.
- Provider mode changes vector-backed behavior.
- Tests need timing assertions instead of deterministic counts or ranges.