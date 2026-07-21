# Fluent icon system

WhatsUI bundles the Regular and Filled font faces from Microsoft Fluent UI
System Icons 1.1.328. The project uses a semantic C++ API rather than exposing
private-use codepoints to application code.

## Why a font

The font faces provide a consistent visual grammar across controls, preserve
sharp vector outlines at fractional Windows DPI, and let icons share the text
renderer’s color, clipping, and baseline machinery. The default set replaces
ASCII stand-ins and one-off geometry such as `x`, `+`, slash separators, and
hand-built chevrons.

### Visible-ink centring

An icon font's em box is not its visible vector outline. Fluent System Icons
reserve asymmetric vertical font space, so centring only the font
ascent/descent box placed a 20-DIP Star or Delete outline 2 DIP above the
centre of a 32-DIP IconButton. `drawIcon()` applies the font's 10% semantic-size
optical correction after baseline centring. This shared correction covers
standalone Icon, IconButton, Button and ToggleButton icon content rather than
adding per-widget offsets.

The native OpenGL Button visual gate measures the actual rasterized Star and
Delete ink at 100%, 125%, 150%, and 200%. Both axes must remain within one
physical pixel of the IconButton centre.

The supported optical sizes are 16, 20, and 24 DIP. Choose the size matching
the component slot instead of scaling a single glyph:

```cpp
#include <wui/icons.h>

wui::Icon deleteIcon(wui::IconName::Delete);
deleteIcon.size(wui::IconSize::Size20);

wui::IconButton deleteButton(
    wui::IconName::Delete, "Delete task");
deleteButton.setIconStyle(wui::IconStyle::Regular);
```

Decorative `Icon` nodes remain absent from the accessibility tree. Interactive
icon-only controls must use `IconButton` and supply an accessible label.

## Public semantic set

The initial stable set covers Add, Delete, Dismiss, Edit, Star, StarOff,
Checkmark, CheckmarkCircle, Info, Warning, ErrorCircle, directional chevrons,
Search, horizontal/vertical More, Undo, Calendar, Clock, Important, and
TaskList. `iconCodepoint()` and `iconUtf8()` exist for renderer integration and
testing; applications should prefer `Icon`, `IconButton`, or `drawIcon()`.

## Runtime and packaging

`WhatsCanvasTextMeasurer` registers both faces when a Canvas is installed.
Development builds resolve the repository assets. Installed applications
resolve:

```text
<prefix>/share/WhatsUI/fonts/fluent-system-icons/
```

The install rules include both TTF files, their upstream JSON maps, license,
and notice. CMake verifies the reviewed TTF SHA-256 values at configure time.
The 100% and 150% DPI icon-gallery tests verify that every exposed gallery
glyph is loaded and produces visible pixels.

## Version and license

- Upstream: `https://github.com/microsoft/fluentui-system-icons`
- Reviewed release: `1.1.328`
- License: MIT
- Vendored evidence: `assets/fonts/fluent-system-icons/LICENSE` and `NOTICE`

See [SBOM](SBOM.md) for checksums and distribution inventory.
