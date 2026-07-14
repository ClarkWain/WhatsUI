#pragma once

// Pluggable text measurement (ADR-002 §渲染接入). The core library measures
// text through this interface so `Text` layout can use real shaped metrics when
// a backend (e.g. WhatsCanvas) is available, and a heuristic otherwise.

#include <cstddef>
#include <string>
#include <vector>

#include "wui/types.h"

namespace wui {

// Line metrics for a run of text, in logical units.
struct TextExtents {
    float width{0.0f};
    float height{0.0f};   // full line height (leading included)
    float ascent{0.0f};   // distance from the line top down to the baseline
    float descent{0.0f};  // distance from the baseline to the glyph bottom
};

// A resolved visual line. `text` is always valid UTF-8 when supplied by a
// backend text engine, even when the original input contained malformed data.
// `sourceStart` and `sourceLength` are UTF-8 byte offsets into the submitted
// string; byte offsets deliberately match the TextInput protocol until its
// grapheme-aware controller becomes public API.
struct TextLayoutLine {
    std::string text;
    std::size_t sourceStart{0};
    std::size_t sourceLength{0};
    float width{0.0f};
    bool ellipsized{false};
};

class TextMeasurer {
public:
    virtual ~TextMeasurer() = default;

    // Measure a single run of text at the given size, in logical units.
    [[nodiscard]] virtual TextExtents measureText(const std::string& text, float fontSize) const = 0;

    // Optional weighted measurement. Existing lightweight/headless measurers
    // remain source-compatible and may use the regular metrics; native hosts
    // override this so semibold headings measure and paint with the same face.
    [[nodiscard]] virtual TextExtents measureText(const std::string& text, float fontSize,
                                                  int fontWeight) const
    {
        (void)fontWeight;
        return measureText(text, fontSize);
    }
};

// Optional extension for a renderer that owns Unicode line breaking and text
// shaping. `Text` discovers this interface at runtime, so the headless core
// remains dependency-free while a WhatsCanvas host uses one consistent engine
// for measuring, line breaking, fallback selection and painting.
//
// `availableWidth` and `lineHeight` use WhatsUI logical units. `maxLines == 0`
// means unlimited. The method is called only for word-wrapped Text nodes;
// explicit newline and no-wrap behaviour continues to be handled by Text.
class TextLayoutProvider : public TextMeasurer {
public:
    ~TextLayoutProvider() override = default;

    [[nodiscard]] virtual std::vector<TextLayoutLine> layoutText(
        const std::string& text,
        float fontSize,
        float availableWidth,
        float lineHeight,
        std::size_t maxLines,
        bool ellipsize) const = 0;

    [[nodiscard]] virtual std::vector<TextLayoutLine> layoutText(
        const std::string& text,
        float fontSize,
        int fontWeight,
        float availableWidth,
        float lineHeight,
        std::size_t maxLines,
        bool ellipsize) const
    {
        (void)fontWeight;
        return layoutText(text, fontSize, availableWidth, lineHeight, maxLines, ellipsize);
    }
};

// Install the process-wide measurer (not owned). Pass nullptr to fall back to
// the built-in heuristic. Set by the host/runtime once a canvas is available.
void setTextMeasurer(TextMeasurer* measurer) noexcept;

[[nodiscard]] TextMeasurer* textMeasurer() noexcept;

} // namespace wui
