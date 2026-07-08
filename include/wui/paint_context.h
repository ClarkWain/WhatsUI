#pragma once

#include <string>

#include "wui/types.h"

#ifdef WHATSUI_HAS_WHATSCANVAS
#include "wsc/Canvas.h"
#include "wsc/CanvasAdapter.h"
#include "wsc/Color.h"
#include "wsc/Paint.h"
#include "wsc/base.h"
#endif

namespace wui {

class PaintContext {
public:
    explicit PaintContext(float scaleFactor = 1.0f) noexcept
        : scaleFactor_(scaleFactor)
    {
    }

#ifdef WHATSUI_HAS_WHATSCANVAS
    explicit PaintContext(wsc::Canvas& canvas, float scaleFactor = 1.0f) noexcept
        : scaleFactor_(scaleFactor)
        , canvas_(&canvas)
    {
    }
#endif

    [[nodiscard]] float scaleFactor() const noexcept
    {
        return scaleFactor_;
    }

    void setScaleFactor(float scaleFactor) noexcept
    {
        scaleFactor_ = scaleFactor;
    }

#ifdef WHATSUI_HAS_WHATSCANVAS
    void setCanvas(wsc::Canvas& canvas) noexcept
    {
        canvas_ = &canvas;
    }

    [[nodiscard]] wsc::Canvas* canvas() const noexcept
    {
        return canvas_;
    }
#endif

    [[nodiscard]] bool hasCanvas() const noexcept
    {
#ifdef WHATSUI_HAS_WHATSCANVAS
        return canvas_ != nullptr;
#else
        return false;
#endif
    }

    void fillRect(const RectF& rect, Color color)
    {
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr) {
            return;
        }
        wsc::CanvasAdapter adapter(*canvas_);
        adapter.setFillColor(wsc::Color(color.r, color.g, color.b, color.a));
        adapter.fillRect(wsc::RectF(rect.x, rect.y, rect.width, rect.height));
#else
        (void)rect;
        (void)color;
#endif
    }

    void fillRoundRect(const RectF& rect, float radius, Color color)
    {
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr) {
            return;
        }
        wsc::Paint paint;
        paint.setStyle(wsc::Paint::Style::FILL);
        paint.setColor(wsc::Color(color.r, color.g, color.b, color.a));
        canvas_->drawRoundRect(wsc::RectF(rect.x, rect.y, rect.width, rect.height), radius, paint);
#else
        (void)rect;
        (void)radius;
        (void)color;
#endif
    }

    void drawText(const std::string& text, float x, float y, float textSize, Color color)
    {
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr) {
            return;
        }
        wsc::CanvasAdapter adapter(*canvas_);
        adapter.setFillColor(wsc::Color(color.r, color.g, color.b, color.a));
        adapter.setTextSize(textSize);
        adapter.drawText(text, x, y);
#else
        (void)text;
        (void)x;
        (void)y;
        (void)textSize;
        (void)color;
#endif
    }

private:
    float scaleFactor_{1.0f};

#ifdef WHATSUI_HAS_WHATSCANVAS
    wsc::Canvas* canvas_{nullptr};
#endif
};

} // namespace wui
