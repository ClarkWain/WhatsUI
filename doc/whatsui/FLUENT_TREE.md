# Fluent Tree

`Tree` and `TreeItem` implement the Fluent 2 hierarchical-navigation pattern.
Items have application-owned, stable UTF-8 identities (`id`), rather than an
index that changes while a branch is collapsed or expanded.

## Composition

```cpp
wui::Tree explorer;
auto& source = explorer.addItem("source", "Source");
source.addItem("tree", "tree.cpp");
explorer.select("tree");
```

The Tree owns its `TreeItem` children recursively. Collapsing a branch removes
its descendants from layout, hit testing, keyboard navigation and the semantic
projection, but does not recreate their objects.

## Input

Arrow Up/Down use roving focus over visible, enabled rows. Right expands a
branch or moves into its first child; Left collapses it or moves to its parent.
Home and End move to the first/final enabled visible item. Enter and Space
select the focused row. Disabled items are visible but skipped by keyboard and
cannot be selected.

## Viewport and accessibility

`maxVisibleItems` gives the Tree a deterministic viewport. It paints only
visible rows and scrolls the focused/selected item into view. The platform
accessibility bridge should project the root as `Tree` and visible rows as
`TreeItem`, with expanded, selected/checked, enabled, bounds and
Invoke/ExpandCollapse/Focus actions. Collapsed descendants must not be exposed.

## Verification

`fluent_tree_tests.cpp` covers stable identity, disclosure, disabled-item
navigation, keyboard behavior, scroll-into-view and accessibility actions.
`fluent_tree_visual_tests.cpp` produces Software renderer review captures at
100% and 150% scale with nested, collapsed, selected and disabled states.
