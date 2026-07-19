# Fluent Drawer

`wui::Drawer` implements Fluent 2's edge-attached navigation/detail surface.
It supports `Inline` and `Overlay` modes, Start/End/Bottom placement, Small
(320), Medium (480), Large (640), Full and explicit logical-pixel sizing.

Overlay drawers are modal by default: they paint a scrim, consume background
input, trap Tab at the drawer boundary, close on Escape, and optionally close
from an outside press. `modal(false)` creates a non-modal overlay; inline
drawers never paint a scrim or take input outside their bounds.

The title/subtitle header has a close affordance; primary and secondary footer
actions are optional. The body is clipped and wheel-scrollable when its child
is taller than the available content viewport. `contentBounds()`,
`contentScrollOffset()` and `maxContentScrollOffset()` are exposed for tests
and automation.

Platform integration projects a Drawer as an accessibility `Dialog` with the
title as its accessible name and a focus action. `OverlayHost::show()`
automatically focuses a modal Drawer, installs its ownership dismissal hook,
and restores the prior focused control (including when a Drawer child had
focus) after close. `UiWindow` makes that modal Drawer the active keyboard and
accessibility subtree, so Escape cannot leak to the dimmed page.
