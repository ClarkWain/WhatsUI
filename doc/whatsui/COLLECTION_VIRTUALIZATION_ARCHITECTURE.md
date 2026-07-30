# Collection Virtualization Architecture

Status: implemented and validated. This document defines how WhatsUI makes
list-like and table-like views scalable without forcing every collection
control to become a `VirtualList` clone.

## Problem

Several controls expose collection-shaped UI, but they do not all have the same
rendering contract:

- `ListView` owns a `std::vector<Item>` and currently loops over every item in
  paint, skipping off-screen rows after intersection checks. It is suitable for
  small and medium lists, but it does not recycle rows or lazily request data.
- `VirtualList` mounts only the viewport plus overscan and keeps a capped,
  key-addressable off-screen pool. It is the only current retained-node
  recycler.
- `ListBox`, `Combobox`, and `Dropdown` use value objects and already paint a
  visible option window. They do not create one retained row node per option, so
  a node recycler is not their immediate bottleneck.
- `Table` and `DataGrid` use value-based rows and columns, paint only the
  visible row window, and expose virtual accessibility entries. They still own
  the full row vector and do not yet support a lazy row or cell provider.
- `ScrollView` clips, translates, hit-tests, and propagates scroll deltas. It is
  not a recycler and should remain safe for arbitrary retained subtrees.
- `Tree` owns a retained hierarchy. It can eventually virtualize the flattened
  expanded projection, but its expand/collapse, level, selection, and
  accessibility semantics make it a later phase.

The architecture must separate three concerns that are often conflated:

1. **Windowed painting:** only draw rows that intersect the viewport.
2. **Node recycling:** keep a bounded set of retained row nodes and reuse them
   by stable key.
3. **Lazy data access:** let a control know logical item count while fetching
   visible data on demand.

## Principles

- Keep `ScrollView` as a viewport primitive. Recycling belongs to collection
  controls that understand item identity and semantics.
- Prefer value projection for simple text/options/tables. Do not create row
  nodes just to recycle them when drawing value data is cheaper and preserves
  simpler accessibility snapshots.
- Use stable keys whenever a row can carry retained state. Index-only reuse is
  acceptable only for pure value drawing with no row-owned state.
- Keep viewport math shared. Every fixed-row collection should compute visible
  range, overscan range, max scroll, and scroll-into-view in the same way.
- Keep control semantics local. Selection, disabled rows, type-ahead, grid
  focus, tree levels, and accessibility roles should stay owned by the control,
  not by the recycler.
- Make virtualization observable in tests through `visibleRange`, mounted count,
  pooled count, provider request ranges, and virtual accessibility entries.

## Proposed Layers

### ViewportModel

`ViewportModel` is an internal utility for one-dimensional fixed-extent
collections. It should be a small value type or composition object, not a Node.

Responsibilities:

- item count
- item extent
- viewport extent
- scroll offset and maximum scroll offset
- visible range
- overscan range
- `scrollToIndex(index)` / `scrollItemIntoView(index)`
- scroll delta clamping

First implementation should support fixed item extent only. The current
controls all use fixed row heights or fixed row extents. Variable-size support
can be added later with an extent cache and prefix-sum lookup without changing
the consumers' high-level contract.

Candidate API shape:

```cpp
class ViewportModel {
public:
    using Index = std::size_t;
    struct Range { Index first{0}; Index last{0}; };

    void setItemCount(Index count) noexcept;
    void setItemExtent(float extent) noexcept;
    void setViewportExtent(float extent) noexcept;
    void setOverscanItems(Index count) noexcept;
    void setScrollOffset(float offset) noexcept;

    [[nodiscard]] Index itemCount() const noexcept;
    [[nodiscard]] float itemExtent() const noexcept;
    [[nodiscard]] float scrollOffset() const noexcept;
    [[nodiscard]] float maxScrollOffset() const noexcept;
    [[nodiscard]] Range visibleRange() const noexcept;
    [[nodiscard]] Range overscanRange() const noexcept;
    void scrollToIndex(Index index) noexcept;
};
```

Initial consumers:

- `VirtualList`: replace its local visible/mounted range and scroll math.
- `ListBox`: replace local `scrollOffset_`, maximum-scroll, and visible-row
  calculations while keeping value drawing.
- `Table` / `DataGrid`: replace `firstVisibleRow`, `lastVisibleRowExclusive`,
  row scroll clamping, and row scroll-into-view math.
- `ListView`: use it before or during the larger virtualization migration.

### KeyedRecycler

`KeyedRecycler` owns retained row node lifecycle for controls that actually
mount row nodes. It should be extracted from `VirtualList` after `ViewportModel`
exists.

Responsibilities:

- track mounted entries `{index, key, Node*}`
- maintain a capped pool `{key, unique_ptr<Node>}`
- reconcile a desired overscan range against current mounted nodes
- reuse pooled nodes by stable key
- create missing nodes through an item builder
- handle duplicate keys deterministically
- guard against reentrant detach/refresh during unmount
- trim pool based on visible window size

It should not draw frames, own selection, handle keyboard policy, or decide
accessibility roles. Those remain with the control.

Candidate API shape:

```cpp
class KeyedRecycler {
public:
    using Index = std::size_t;
    using Key = std::string;
    using KeyProvider = std::function<Key(Index)>;
    using Builder = std::function<std::unique_ptr<Node>(Index, const Key&)>;

    void setKeyProvider(KeyProvider provider);
    void setBuilder(Builder builder);
    void reconcile(ContainerNode& owner, ViewportModel::Range range);
    void clear(ContainerNode& owner);
    [[nodiscard]] std::size_t mountedCount() const noexcept;
    [[nodiscard]] std::size_t pooledCount() const noexcept;
};
```

Initial consumers:

- `VirtualList`: first migration target. It already has the complete behavior
  and tests, making it the safest extraction path.
- `ListView`: only if it gains custom row nodes or expensive retained row
  content. For plain text rows, `ViewportModel` plus visible drawing is enough.
- Future `VirtualTree`: use the recycler after the tree exposes a flattened
  visible-row projection.

### Data Providers

Lazy data access should be a separate opt-in contract. A provider lets a
control keep logical count and stable keys without storing every row payload in
the widget.

Candidate contracts:

```cpp
struct ListItemProvider {
    virtual std::size_t itemCount() const noexcept = 0;
    virtual std::string keyAt(std::size_t index) const = 0;
    virtual ListView::Item itemAt(std::size_t index) const = 0;
};

struct TableRowProvider {
    virtual std::size_t rowCount() const noexcept = 0;
    virtual std::string rowKeyAt(std::size_t row) const = 0;
    virtual TableRow rowAt(std::size_t row) const = 0;
};
```

The first provider implementation should be synchronous. Async loading can be
added later through placeholder rows, request coalescing, and invalidation
callbacks. The core controls should not grow network/cache policy.

## Control Migration Plan

### Phase 1: Shared viewport math

Create `include/wui/internal/viewport_model.h` and focused tests. Migrate
`VirtualList`, `ListBox`, and `Table` / `DataGrid` to use it without changing
public behavior.

Acceptance criteria:

- existing `m4_virtual_list_tests`, selection-control tests, and table tests
  still pass
- edge cases for empty lists, zero viewport, clamped scroll, overscan at start
  and end, and scroll-to-index are covered directly
- no public API changes are required

### Phase 2: Extract retained-node recycling

Create `KeyedRecycler` from the current `VirtualList` reconcile/unmount/pool
logic. Keep `VirtualList` as the public proving ground.

Acceptance criteria:

- stable-key identity survives insertion/removal
- duplicate keys remain deterministic
- off-screen pool stays capped
- detach-triggered refresh converges without stale mounted records
- high-churn scroll remains bounded under sanitizer

### Phase 3: Modernize ListView

Move `ListView` from full traversal to viewport-based drawing. Preserve the
existing vector API and selection binding. Add optional large-data APIs only
after the internal viewport behavior is stable.

Suggested public additions:

- `setItemProvider(...)` or callback-based `setItemCount` / `setItemAt`
- `setKeyProvider(...)` for stable row identity when custom retained rows land
- `visibleRange()` for tests and diagnostics

Acceptance criteria:

- small-list behavior and measurements remain compatible unless deliberately
  versioned
- disabled-row keyboard skipping still works
- pointer selection maps through scroll offset correctly
- paint and data requests are bounded by visible range plus overscan

### Phase 4: Add provider mode to Table/DataGrid

Keep value-vector rows for simple use. Add a provider mode for large data sets.
Sorting remains local only for vector-backed rows; provider-backed grids should
delegate sorting to `onSort` or a provider refresh.

Acceptance criteria:

- DataGrid can expose 100k logical rows without storing 100k `TableRow` values
  in the control
- visible row and cell accessibility entries are still stable
- keyboard focus and selection scroll rows into view without materializing the
  full model
- provider request counts are bounded in tests

### Phase 5: Virtualize Tree projection

Flatten expanded `TreeItem` state into visible rows with stable item ids and
levels. Apply `ViewportModel` first, then `KeyedRecycler` only if tree rows
become retained custom content.

Acceptance criteria:

- expand/collapse preserves selected and focused ids where possible
- level, expanded, selected, and disabled semantics remain correct
- collapsed descendants are absent from the visible projection
- row realization remains bounded for large expanded trees

## Non-Goals

- Do not make `ScrollView` automatically recycle children.
- Do not force `ListBox` or `Table` to create row nodes merely to share a
  recycler.
- Do not add async loading policy to core widgets in the first pass.
- Do not virtualize low-count command surfaces such as Toolbar, Breadcrumb,
  TabList, or Accordion. Their scaling solution is overflow, collapse, or
  application structure, not row recycling.

## Test Matrix

- `ViewportModel` unit tests for range math, overscan, clamping, and
  scroll-to-index.
- `VirtualList` recycler tests for stable identity, duplicate keys, pool caps,
  high churn, and reentrant detach.
- `ListView` tests for bounded visible drawing/data access, selection binding,
  disabled rows, pointer hit testing, and keyboard navigation with scroll.
- `ListBox` tests for type-ahead, active row scroll-into-view, multi-select,
  virtual option bounds, and popup integration through Combobox/Dropdown.
- `Table` / `DataGrid` tests for provider-backed row count, visible-entry
  accessibility, sorting delegation, selection/focus, and scroll boundaries.
- `Tree` tests for flattened visible projection, expand/collapse, stable ids,
  levels, selection, and bounded realization.

## Implementation Notes

- Keep all new shared types internal until at least two controls consume them.
- Prefer composition over inheritance for viewport/recycler state. Controls
  already have distinct public semantics and painting contracts.
- Preserve existing public vector APIs. Provider mode should be additive.
- Instrument mounted, pooled, and requested row counts behind public diagnostic
  accessors only where tests need them.
- Maintain virtual accessibility snapshots for value-projection controls. The
  platform bridge should not require retained nodes for every logical item.