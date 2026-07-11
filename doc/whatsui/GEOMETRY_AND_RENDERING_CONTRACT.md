# Geometry and rendering contract

This contract applies to the foundational `Container`, `Row`, `Column`, and
their descendants.

## Layout and dirtiness

`layout(bounds)` assigns final logical bounds. A composite must lay out every
child it makes reachable and only then clear `DirtyFlag::Layout` for that
subtree. A dirty layout flag therefore means a node must not be treated as
having current geometry. Paint dirtiness is independent and is cleared after
the corresponding node has painted. Layout invalidation also invalidates paint
on the changed node and every ancestor through the root: changed geometry
cannot safely reuse a previously clean paint result. Paint-only invalidation
does not invalidate layout, but it likewise propagates to the root so the
window's redraw boundary is reached.

## Paint order and clipping

Children paint in insertion order: the first child is the back-most and the
last child is the front-most. A `Container` paints its own background before
its children. Foundational containers do not introduce implicit clipping;
children may paint outside their bounds unless a future explicit clipping
container establishes a clip. This keeps painting and input rules predictable.

## Hit testing

Hit testing first rejects points outside a node's bounds. A container then
visits children in reverse insertion order, matching the front-to-back visual
stack, and returns the first hit. If no child is hit, the container itself is
the result. Nodes have no separate visibility state in this milestone: every
attached, laid-out node is visible and hit-testable according to these bounds.
