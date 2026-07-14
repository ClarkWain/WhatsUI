#pragma once

#include <chrono>
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
        , canvasCoordinateScale_(scaleFactor_)
    {
    }

#ifdef WHATSUI_HAS_WHATSCANVAS
    // `canvasUsesDevicePixelRatio` is for a native surface whose Canvas has
    // already been configured with Canvas::setDevicePixelRatio(scaleFactor).
    // In that mode Canvas owns the logical-to-device transform, so forwarding
    // scaleFactor again with every primitive would double-scale the scene.
    // Keep scaleFactor() as the window's actual DPR for callers that need it;
    // canvasCoordinateScale() tells canvas-backed paint operations how to
    // convert their logical geometry.
    explicit PaintContext(wsc::Canvas& canvas, float scaleFactor = 1.0f,
                          bool canvasUsesDevicePixelRatio = false) noexcept
        : scaleFactor_(scaleFactor > 0.0f ? scaleFactor : 1.0f)
        , canvasCoordinateScale_(canvasUsesDevicePixelRatio ? 1.0f : scaleFactor_)
        , canvasUsesDevicePixelRatio_(canvasUsesDevicePixelRatio)
        , canvas_(&canvas)
    {
    }
#endif

    [[nodiscard]] float scaleFactor() const noexcept
    {
        return scaleFactor_;
    }

    [[nodiscard]] float canvasCoordinateScale() const noexcept
    {
        return canvasCoordinateScale_;
    }

    // WhatsCanvas exposes TOP/MIDDLE/BOTTOM text anchors rather than a
    // typographic baseline. WhatsUI therefore uses BOTTOM consistently: this
    // returns the draw y that vertically centres the renderer's real line box
    // in `lineBox`. It is intentionally shared by buttons, inputs and toggle
    // labels so Fluent controls cannot drift apart when Windows resolves a
    // different font metric than the nominal point size.
    [[nodiscard]] float centeredTextBottom(const std::string& text, const RectF& lineBox,
                                           float textSize, int fontWeight = 400) const noexcept
    {
        float lineHeight = textSize * 1.25f;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ != nullptr && !text.empty()) {
            wsc::Paint paint;
            paint.setTextSize(textSize * canvasCoordinateScale_);
            paint.setFontFamily("Segoe UI");
            paint.setFontWeight(fontWeight);
            const auto metrics = canvas_->measureTextMetrics(text, paint);
            const float physicalHeight = metrics.lineHeight > 0.0f ? metrics.lineHeight : metrics.height;
            if (physicalHeight > 0.0f) {
                lineHeight = physicalHeight / canvasCoordinateScale_;
            }
        }
#endif
        return lineBox.y + (lineBox.height + lineHeight) * 0.5f;
    }

    void setScaleFactor(float scaleFactor) noexcept
    {
        scaleFactor_ = scaleFactor > 0.0f ? scaleFactor : 1.0f;
        if (!canvasUsesDevicePixelRatio_) {
            canvasCoordinateScale_ = scaleFactor_;
        }
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
        const auto started = std::chrono::steady_clock::now();
        wsc::CanvasAdapter adapter(*canvas_);
        adapter.setFillColor(wsc::Color(color.r, color.g, color.b, color.a));
        adapter.fillRect(wsc::RectF(rect.x * canvasCoordinateScale_, rect.y * canvasCoordinateScale_,
                                    rect.width * canvasCoordinateScale_, rect.height * canvasCoordinateScale_));
        paintStats_.fillRectMilliseconds += elapsedMilliseconds(started);
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
        const auto started = std::chrono::steady_clock::now();
        wsc::Paint paint;
        paint.setStyle(wsc::Paint::Style::FILL);
        paint.setColor(wsc::Color(color.r, color.g, color.b, color.a));
        paint.setAntiAlias(true);
        canvas_->drawRoundRect(wsc::RectF(rect.x * canvasCoordinateScale_, rect.y * canvasCoordinateScale_,
                                          rect.width * canvasCoordinateScale_, rect.height * canvasCoordinateScale_),
                               radius * canvasCoordinateScale_, paint);
        paintStats_.fillRoundRectMilliseconds += elapsedMilliseconds(started);
#else
        (void)rect;
        (void)radius;
        (void)color;
#endif
    }

    void drawText(const std::string& text, float x, float y, float textSize, Color color,
                  int fontWeight = 400)
    {
        ++paintStats_.commandCount;
        ++paintStats_.textDrawCalls;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr) {
            return;
        }
        const auto started = std::chrono::steady_clock::now();
        wsc::CanvasAdapter adapter(*canvas_);
        adapter.setFillColor(wsc::Color(color.r, color.g, color.b, color.a));
        adapter.setTextSize(textSize * canvasCoordinateScale_);
        // Fluent on Windows is designed around Segoe UI. Selecting it
        // explicitly also routes through WhatsCanvas' native Windows text
        // adapter when the portable raster path is unavailable, instead of
        // falling back to its block-glyph emergency renderer.
        adapter.fillPaint().setFontFamily("Segoe UI");
        adapter.fillPaint().setFontWeight(fontWeight);
        // WhatsUI's text coordinates are baselines (the layout code computes
        // ascent/descent and vertically centers using that convention).
        // WhatsCanvas defaults to a top anchor, which applied the text size a
        // second time in its glyph-atlas path.  That put button/input labels
        // below their controls and made Text's bounds clip most glyphs.
        adapter.fillPaint().setTextBaseline(wsc::Paint::TextBaseline::BOTTOM);
        adapter.drawText(text, x * canvasCoordinateScale_, y * canvasCoordinateScale_);
        paintStats_.textDrawMilliseconds += elapsedMilliseconds(started);
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
            const auto started = std::chrono::steady_clock::now();
            canvas_->clipRect(wsc::RectF(rect.x * canvasCoordinateScale_, rect.y * canvasCoordinateScale_,
                                         rect.width * canvasCoordinateScale_, rect.height * canvasCoordinateScale_));
            paintStats_.clipRectMilliseconds += elapsedMilliseconds(started);
        }
#else
        (void)rect;
#endif
    }

    void translate(float dx, float dy) noexcept
    {
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ != nullptr) canvas_->translate(dx * canvasCoordinateScale_, dy * canvasCoordinateScale_);
#else
        (void)dx;
        (void)dy;
#endif
    }

private:
    static double elapsedMilliseconds(std::chrono::steady_clock::time_point started) noexcept
    {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    }

    float scaleFactor_{1.0f};
    float canvasCoordinateScale_{1.0f};
    bool canvasUsesDevicePixelRatio_{false};
    int saveCount_{1};
    PaintOperationStats paintStats_{};

#ifdef WHATSUI_HAS_WHATSCANVAS
    wsc::Canvas* canvas_{nullptr};
#endif
};

} // namespace wui
