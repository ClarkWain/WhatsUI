# Collection Virtualization Baseline

Date: 2026-07-30

Status: passed with one documented sanitizer configuration limitation.

## Source Revisions

- WhatsUI commit: `007bdac`
- `third_party/WhatsCanvas` commit: `7706f4f`

The working tree includes the collection virtualization implementation and
documentation changes described in `COLLECTION_VIRTUALIZATION_ARCHITECTURE.md`
and `COLLECTION_VIRTUALIZATION_IMPLEMENTATION_PLAN.md`.

## Build Configurations

Behavior and visual validation used:

```powershell
cmake -S . -B build-virtualization-baseline `
  -DWHATSUI_WITH_WHATSCANVAS=ON `
  -DWHATSUI_BUILD_TESTS=ON `
  -DWHATSUI_BUILD_EXAMPLES=OFF
```

Sanitizer core validation used:

```powershell
cmake -S . -B build-virtualization-asan-core `
  -DWHATSUI_ENABLE_SANITIZERS=ON `
  -DWHATSUI_WITH_WHATSCANVAS=OFF `
  -DWHATSUI_BUILD_TESTS=ON `
  -DWHATSUI_BUILD_EXAMPLES=OFF
```

The ASan test process requires the MSVC ASan runtime directory on `PATH`:

```powershell
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.43.34808\bin\Hostx64\x64
```

## Debug Behavior Gate

Command:

```powershell
ctest --test-dir build-virtualization-baseline -C Debug --output-on-failure `
  -R "^(whatsui_viewport_model_tests|whatsui_virtual_list_tests|whatsui_list_view_tests|whatsui_fluent_selection_controls_tests|whatsui_fluent_table|whatsui_fluent_tree|whatsui_scroll_view_tests|whatsui_lifecycle_tests)$"
```

Result: 8/8 passed.

Covered tests:

- `whatsui_scroll_view_tests`
- `whatsui_viewport_model_tests`
- `whatsui_fluent_table`
- `whatsui_fluent_tree`
- `whatsui_fluent_selection_controls_tests`
- `whatsui_lifecycle_tests`
- `whatsui_list_view_tests`
- `whatsui_virtual_list_tests`

## Release Behavior Gate

Command:

```powershell
ctest --test-dir build-virtualization-baseline -C Release --output-on-failure `
  -R "^(whatsui_viewport_model_tests|whatsui_virtual_list_tests|whatsui_list_view_tests|whatsui_fluent_selection_controls_tests|whatsui_fluent_table|whatsui_fluent_tree|whatsui_scroll_view_tests|whatsui_lifecycle_tests)$"
```

Result: 8/8 passed.

## Release Visual Gate

Command:

```powershell
ctest --test-dir build-virtualization-baseline -C Release --output-on-failure `
  -R "^(whatsui_fluent_selection_controls_visual|whatsui_fluent_selection_controls_visual_150dpi|whatsui_fluent_table_visual|whatsui_fluent_table_visual_125dpi|whatsui_fluent_table_visual_150dpi|whatsui_fluent_table_visual_200dpi|whatsui_fluent_tree_visual|whatsui_fluent_tree_visual_125dpi|whatsui_fluent_tree_visual_150dpi|whatsui_fluent_tree_visual_200dpi)$"
```

Result: 10/10 passed.

Generated capture families:

- Fluent selection controls: 100%, 150%
- Fluent table/datagrid: 100%, 125%, 150%, 200%
- Fluent tree: 100%, 125%, 150%, 200%

## Sanitizer Ownership Gate

The full `WHATSUI_WITH_WHATSCANVAS=ON` ASan configuration did not reach test
execution on MSVC because `WhatsCanvasSoftware.lib` and the ASan-built test
objects disagreed on STL `annotate_string` / `annotate_vector` settings. This
is a build-configuration limitation at the third-party static-library boundary,
not a failing runtime sanitizer report.

Core ASan validation was run with `WHATSUI_WITH_WHATSCANVAS=OFF` to exercise the
WhatsUI ownership paths without the third-party annotation mismatch.

Command:

```powershell
ctest --test-dir build-virtualization-asan-core -C Debug --output-on-failure `
  -R "^(whatsui_lifecycle_tests|whatsui_virtual_list_tests|whatsui_list_view_tests|whatsui_fluent_table|whatsui_fluent_tree)$"
```

Result: 5/5 passed after adding the MSVC ASan runtime directory to `PATH`.

## Baseline Expectations

- `VirtualList` keeps mounted rows bounded for a 100k-row logical model; the
  existing test ceiling remains `mountedCount() <= 9` and `pooledCount() <= 18`.
- `Node::removeChild()` and `Node::clearChildren()` detach children after first
  removing them from the parent vector, so detach callbacks may re-enter the
  same parent with `clearChildren()` without invalidating the active removal or
  clear traversal.
- `VirtualList::removeChild()` and `VirtualList::clearChildren()` keep public
  child mutation synchronized with recycler mounted state, so direct child
  mutation does not leave stale mounted row pointers behind.
- `ListView` exposes a bounded `visibleRange()` for large vector-backed lists;
  the large-list test expects a 182-DIP viewport to report no more than six
  rows and to start at row 50000 after a large scroll.
- Provider-backed `ListView` paint requests no more than six items for the same
  viewport, including after a large scroll.
- Provider-backed `ListView` keyboard navigation uses selectable metadata and
  does not request row payloads while searching disabled/enabled state.
- Provider-backed `DataGrid` accessibility materialization requests no more
  than two rows for a two-row viewport, including after a large scroll to row
  50000.
- Provider-backed `DataGrid` uses the physical body viewport height, so header
  only or constrained layouts do not request off-window row payloads even when
  `maxVisibleRows()` is larger.
- Provider-backed `DataGrid` row selection and accessibility `SetValue` use
  enabled metadata instead of requesting off-window row payloads.
- Provider-backed `DataGrid` sorting delegates to `onSort`; it does not attempt
  to sort incomplete local row storage.
- `Tree` retains stable `TreeItem` identity and semantic levels while layout
  assigns real bounds only to the current viewport range. The large-tree test
  expects exactly two laid-out rows in a 64-DIP viewport before and after a
  large scroll.
- `Tree` stores roving focus by stable item id instead of a raw `TreeItem*`,
  invalidates its visible projection on mutation, and clears only previously
  laid-out rows when relayouting a viewport.
- Collapsing a branch that contains the focused descendant moves roving focus
  back to the collapsed ancestor, preventing hidden descendants from receiving
  keyboard selection.

## Known Follow-Ups

- Unify the full WhatsCanvas-enabled MSVC ASan configuration so third-party
  static libraries and test objects agree on STL annotation settings.
- Consider variable-height collection support only after the fixed-extent
  `ViewportModel` has settled across more controls.
- Add async provider policy separately; the current provider contracts are
  intentionally synchronous.