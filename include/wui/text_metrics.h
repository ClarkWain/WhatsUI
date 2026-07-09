#pragma once

// Pluggable text measurement (ADR-002 §渲染接入). The core library measures
// text through this interface so `Text` layout can use real shaped metrics when
// a backend (e.g. WhatsCanvas) is available, and a heuristic otherwise.

#include <string>

#include "wui/types.h"

namespace wui {

class TextMeasurer {
public:
    virtual ~TextMeasurer() = default;

    // Measure a single run of text at the given size, in logical units.
    [[nodiscard]] virtual SizeF measureText(const std::string& text, float fontSize) const = 0;
};

// Install the process-wide measurer (not owned). Pass nullptr to fall back to
// the built-in heuristic. Set by the host/runtime once a canvas is available.
void setTextMeasurer(TextMeasurer* measurer) noexcept;

[[nodiscard]] TextMeasurer* textMeasurer() noexcept;

} // namespace wui
