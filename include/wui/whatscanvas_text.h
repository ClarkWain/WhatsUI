#pragma once

// WhatsCanvas-backed text measurement. Only active when WhatsUI is built with
// WHATSUI_WITH_WHATSCANVAS=ON (which defines WHATSUI_HAS_WHATSCANVAS). Install
// it once a measurement canvas exists:
//
//     wui::WhatsCanvasTextMeasurer measurer(canvas);
//     wui::setTextMeasurer(&measurer);

#include "wui/text_metrics.h"

#ifdef WHATSUI_HAS_WHATSCANVAS

#include <string>

#include "wsc/Canvas.h"
#include "wsc/Paint.h"

namespace wui {

class WhatsCanvasTextMeasurer : public TextMeasurer {
public:
    explicit WhatsCanvasTextMeasurer(wsc::Canvas& canvas, float scaleFactor = 1.0f) noexcept
        : canvas_(&canvas)
        , scaleFactor_(scaleFactor > 0.0f ? scaleFactor : 1.0f)
    {
    }

    [[nodiscard]] TextExtents measureText(const std::string& text, float fontSize) const override
    {
        wsc::Paint paint;
        paint.setTextSize(fontSize * scaleFactor_);
        const wsc::Canvas::TextMetrics metrics = canvas_->measureTextMetrics(text, paint);
        TextExtents extents;
        extents.width = metrics.width / scaleFactor_;
        extents.height = (metrics.lineHeight > 0.0f ? metrics.lineHeight : metrics.height) / scaleFactor_;
        extents.ascent = (metrics.ascent >= 0.0f ? metrics.ascent : -metrics.ascent) / scaleFactor_;
        extents.descent = (metrics.descent >= 0.0f ? metrics.descent : -metrics.descent) / scaleFactor_;
        if (extents.height <= 0.0f) {
            extents.height = fontSize * 1.25f;
        }
        if (extents.ascent <= 0.0f) {
            extents.ascent = fontSize * 0.8f;
        }
        if (extents.descent <= 0.0f) {
            extents.descent = fontSize * 0.2f;
        }
        return extents;
    }

private:
    wsc::Canvas* canvas_{nullptr};
    float scaleFactor_{1.0f};
};

} // namespace wui

#endif // WHATSUI_HAS_WHATSCANVAS
