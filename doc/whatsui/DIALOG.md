# Dialog / Modal

`wui::Dialog` is a window-sized modal overlay. It dims the active page, centers
one content subtree, and consumes all pointer input outside that subtree. The
panel itself is painted by the dialog; provide the interior using ordinary
layout nodes.

```cpp
using namespace wui::ui;

auto dialog = Dialog()
    .maxWidth(360)
    .dismissOnBackdrop()
    .content(Column().padding(20).gap(12).children(
        Text("Discard draft?").size(18),
        Button("Cancel"),
        Button("Discard").variant(wui::ButtonVariant::Danger)
    ));
window.showDialog(dialog.intoDialog());
```

Use `UiWindow::showDialog` and `dismissDialog`, rather than treating a dialog
as a generic overlay. This records the current focus, clears it while modal
input is active, restores it after dismissal, and makes `Escape` close the top
dialog. `dismissOnBackdrop()` is opt-in; the backdrop always blocks click-through.

Dialogs are stacked. Escape closes the most recently shown dialog. Focus
traversal within a dialog is intentionally deferred to the window-level
keyboard-navigation work; pointer focus works for controls inside the panel.
