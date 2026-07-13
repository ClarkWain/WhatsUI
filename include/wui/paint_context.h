#pragma once

#include <string>

#include "wui/frame_stats.h"
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
        : scaleFactor_(scaleFactor > 0.0f ? scaleFactor : 1.0f)
    {
    }

#ifdef WHATSUI_HAS_WHATSCANVAS
    explicit PaintContext(wsc::Canvas& canvas, float scaleFactor = 1.0f) noexcept
        : scaleFactor_(scaleFactor > 0.0f ? scaleFactor : 1.0f)
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
        scaleFactor_ = scaleFactor > 0.0f ? scaleFactor : 1.0f;
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

    // Starts a fresh framework paint-command sample. UiWindow calls this once
    // for every paint pass; standalone renderers can reset/snapshot around
    // their own subtree paint as well. Counts represent requested operations
    // at this abstraction, independent of backend batching or availability.
    void resetPaintStats() noexcept
    {
        paintStats_ = {};
    }

    [[nodiscard]] PaintOperationStats paintStats() const noexcept
    {
        return paintStats_;
    }

    void fillRect(const RectF& rect, Color color)
    {
        ++paintStats_.commandCount;
        ++paintStats_.fillRectCalls;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr) {
            return;
        }
        wsc::CanvasAdapter adapter(*canvas_);
        adapter.setFillColor(wsc::Color(color.r, color.g, color.b, color.a));
        adapter.fillRect(wsc::RectF(rect.x * scaleFactor_, rect.y * scaleFactor_,
                                    rect.width * scaleFactor_, rect.height * scaleFactor_));
#else
        (void)rect;
        (void)color;
#endif
    }

    void fillRoundRect(const RectF& rect, float radius, Color color)
    {
        ++paintStats_.commandCount;
        ++paintStats_.fillRoundRectCalls;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr) {
            return;
        }
        wsc::Paint paint;
        paint.setStyle(wsc::Paint::Style::FILL);
        paint.setColor(wsc::Color(color.r, color.g, color.b, color.a));
        paint.setAntiAlias(true);
        canvas_->drawRoundRect(wsc::RectF(rect.x * scaleFactor_, rect.y * scaleFactor_,
                                          rect.width * scaleFactor_, rect.height * scaleFactor_),
                               radius * scaleFactor_, paint);
#else
        (void)rect;
        (void)radius;
        (void)color;
#endif
    }

    void drawText(const std::string& text, float x, float y, float textSize, Color color)
    {
        ++paintStats_.commandCount;
        ++paintStats_.textDrawCalls;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr) {
            return;
        }
        wsc::CanvasAdapter adapter(*canvas_);
        adapter.setFillColor(wsc::Color(color.r, color.g, color.b, color.a));
        adapter.setTextSize(textSize * scaleFactor_);
        // WhatsUI's text coordinates are baselines (the layout code computes
        // ascent/descent and vertically centers using that convention).
        // WhatsCanvas defaults to a top anchor, which applied the text size a
        // second time in its glyph-atlas path.  That put button/input labels
        // below their controls and made Text's bounds clip most glyphs.
        adapter.fillPaint().setTextBaseline(wsc::Paint::TextBaseline::BOTTOM);
        adapter.drawText(text, x * scaleFactor_, y * scaleFactor_);
#else
        (void)text;
        (void)x;
        (void)y;
        (void)textSize;
        (void)color;
#endif
    }

    // Scoped canvas state used by viewport-like widgets. The headless build
    // records checkpoints too, so paint-state isolation remains testable
    // without a renderer.
    // Returns a checkpoint which can later be restored with restoreTo().
    // Unlike a bare restore(), restoreTo() also balances any accidental nested
    // saves made by a child widget, making it suitable for framework-owned
    // subtree boundaries.
    [[nodiscard]] int save() noexcept
    {
        const int checkpoint = saveCount_;
        ++saveCount_;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ != nullptr) canvas_->save();
#endif
        return checkpoint;
    }

    void restore() noexcept
    {
        if (saveCount_ <= 1) {
            return;
        }
        --saveCount_;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ != nullptr) canvas_->restore();
#endif
    }

    void restoreTo(int checkpoint) noexcept
    {
        checkpoint = checkpoint < 1 ? 1 : checkpoint;
        while (saveCount_ > checkpoint) {
            restore();
        }
    }

    [[nodiscard]] int saveCount() const noexcept
    {
        return saveCount_;
    }

    void clipRect(const RectF& rect) noexcept
    {
        ++paintStats_.commandCount;
        ++paintStats_.clipRectCalls;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ != nullptr) {
            canvas_->clipRect(wsc::RectF(rect.x * scaleFactor_, rect.y * scaleFactor_,
                                         rect.width * scaleFactor_, rect.height * scaleFactor_));
        }
#else
        (void)rect;
#endif
    }

    void translate(float dx, float dy) noexcept
    {
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ != nullptr) canvas_->translate(dx * scaleFactor_, dy * scaleFactor_);
#else
        (void)dx;
        (void)dy;
#endif
    }

private:
    float scaleFactor_{1.0f};
    int saveCount_{1};
    PaintOperationStats paintStats_{};

#ifdef WHATSUI_HAS_WHATSCANVAS
    wsc::Canvas* canvas_{nullptr};
#endif
};

} // namespace wui
