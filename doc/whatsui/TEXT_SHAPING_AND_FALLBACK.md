# Windows text shaping and fallback policy

## Scope and supported contract

WhatsUI source strings are UTF-8. For a Windows application built with
`WHATSUI_WITH_WHATSCANVAS=ON`, `WhatsCanvasTextMeasurer` is the supported text
bridge. It keeps the layout engine and renderer on the same public
WhatsCanvas APIs:

- `Canvas::measureTextMetrics` supplies shaped logical metrics;
- `Canvas::layoutTextBox` supplies UTF-8-safe word/CJK line breaking and
  ellipsis; and
- `Canvas::drawText` performs the matching bidi run ordering, glyph fallback
  and rendering.

`Text` automatically uses `TextLayoutProvider` when it is installed as the
process text measurer and `TextWrap::Word` has a finite width. The headless
core keeps its deterministic scalar-width fallback; it is suitable for unit
tests but **is not a product multilingual renderer**.

```cpp
wui::WhatsCanvasTextMeasurer text(canvas, dpiScale);
const auto policy = text.policyStatus();
wui::setTextMeasurer(&text);
```

Keep the measurer alive for the lifetime of the canvas/window. The GLFW host
does this automatically and updates its scale factor after a DPI change. The
constructor applies the default policy, so all existing WhatsUI Software and
GLFW entry points receive the same Windows fallback chain; calling
`installWindowsFallbackPolicy()` again is an idempotent way to retrieve it.

## Windows font policy

WhatsCanvas discovers and registers the Windows system families in this
order: `Segoe UI` (Latin/UI), `Microsoft YaHei` (CJK), Arial (Arabic/Hebrew),
`Segoe UI Symbol`, serif, and monospace. `installWindowsFallbackPolicy()`
makes that chain explicit and, when installed, appends
`C:/Windows/Fonts/seguiemj.ttf` as `WhatsUI Emoji` for U+1F000–U+1FAFF and
the common symbol range. Its return value records whether the base chain and
emoji face were actually accepted, so an embedder can surface a diagnostic on
a stripped Windows image.

The bridge only uses public `wsc::Canvas`, `wsc::FontFace`,
`wsc::FontFallbackChain`, and `wsc::FontSystem` APIs. Applications with
licensed fonts may register their own `FontFace` objects and call
`Canvas::setFontFallbackChain` before installing the bridge.

## Shaping and fallback guarantees

- UTF-8 input is normalized by WhatsCanvas; malformed sequences become U+FFFD.
- CJK lines may break without ASCII whitespace. WhatsUI does not split UTF-8
  byte sequences itself when the bridge is installed.
- WhatsCanvas resolves Unicode UAX #9 bidi runs before its per-font fallback
  segments. The bridge preserves logical UTF-8 text per resolved line and
  lets `drawText` do the corresponding visual shaping.
- The in-tree CMake default enables FreeType and the HarfBuzz OpenType adapter
  for fresh advanced-text builds. If HarfBuzz cannot be built/discovered,
  WhatsCanvas reports and uses its documented simple-shaping fallback; bidi,
  UTF-8 validation, line breaking and fallback selection remain available.
- Emoji rendering depends on the installed font and renderer capabilities.
  `WhatsCanvasTextPolicyStatus::emojiFallback` proves selection policy, while
  a missing glyph is deliberately rendered by the backend's diagnostic
  fallback rather than crashing or treating UTF-8 bytes as glyphs.

## Cache contract

`WhatsCanvasTextMeasurer` keeps a bounded, deterministic logical cache for
metrics and line-layout results. Keys contain UTF-8 text, font size, logical
width, line height, max-lines and ellipsis state. Its lexical eviction policy
is deterministic and `cacheStats()` exposes hit/miss/entry counts for tests.
It complements, rather than replaces, WhatsCanvas's persistent glyph and
shaping caches. Call `clearCache()` after font-policy changes; changing DPI
with `setScaleFactor()` clears it automatically.

## Verification

With WhatsCanvas Software enabled, run:

```powershell
ctest --test-dir build-text -C Debug -R whatsui_text_shaping_tests --output-on-failure
```

The test creates a real Software canvas and verifies CJK wrapping, emoji and
mixed Arabic/Latin metrics, valid UTF-8 line fragments, deterministic cache
hits, and `Text` delegation to the backend line breaker. It also checks the
Segoe UI Emoji policy whenever that font exists on Windows.
