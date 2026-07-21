#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "wui/frame_stats.h"
#include "wui/theme_access.h"
#include "wui/types.h"

#ifdef WHATSUI_HAS_WHATSCANVAS
#include "wsc/Canvas.h"
#include "wsc/CanvasAdapter.h"
#include "wsc/Color.h"
#include "wsc/Paint.h"
#include "wsc/Path.h"
#include "wsc/base.h"
#endif

namespace wui {

class PaintContext {
public:
    [[nodiscard]] std::string_view resolvedTextFamily(std::string_view requested = {}) const noexcept
    {
        const std::string_view fallback = activeTextFallbackFamily();
        const std::string_view preferred = activeTextFamily().empty()
            ? fallback : activeTextFamily();
        if (requested.empty()) requested = preferred;
#ifdef WHATSUI_HAS_WHATSCANVAS
        // The portable backend registers platform files under stable
        // WhatsCanvas aliases rather than their OS family names. Map only the
        // theme-owned defaults; explicit custom registered families remain
        // untouched.
        if (canvas_ != nullptr && canvas_->textBackend() != wsc::Canvas::TextBackend::DirectWrite) {
            if (requested == "Segoe UI Variable" || requested == "Segoe UI") {
                return wsc::FontSystem::kDefaultPrimaryFamily;
            }
            if (requested == activeMonospaceFamily()) {
                return wsc::FontSystem::kDefaultMonoFamily;
            }
        }
#endif
        return requested;
    }
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

    // Shared logical-to-physical alignment contract for controls. Components
    // used to duplicate these formulas, which let Button, Input and selection
    // controls round the same one-DIP stroke differently at 125%/150% DPR.
    [[nodiscard]] float physicalPixel() const noexcept
    {
        return 1.0f / scaleFactor_;
    }

    [[nodiscard]] float snapToPhysicalPixel(float logical) const noexcept
    {
        return std::round(logical * scaleFactor_) / scaleFactor_;
    }

    [[nodiscard]] float snapStrokeWidth(float logicalWidth) const noexcept
    {
        if (logicalWidth <= 0.0f) return 0.0f;
        return static_cast<float>(
                   std::max(1L, std::lround(logicalWidth * scaleFactor_))) /
               scaleFactor_;
    }

    [[nodiscard]] RectF snapRectEdges(const RectF& rect) const noexcept
    {
        const float left = snapToPhysicalPixel(rect.x);
        const float top = snapToPhysicalPixel(rect.y);
        const float right = snapToPhysicalPixel(rect.x + rect.width);
        const float bottom = snapToPhysicalPixel(rect.y + rect.height);
        return {left, top, std::max(0.0f, right - left),
                std::max(0.0f, bottom - top)};
    }

    [[nodiscard]] float canvasCoordinateScale() const noexcept
    {
        return canvasCoordinateScale_;
    }

    // Return the baseline which centres the font's ascent/descent box inside
    // `lineBox`. WhatsCanvas' portable backend reports ascent above the
    // baseline as negative, while DirectWrite reports its magnitude as
    // positive, so normalize both before calculating the baseline.
    //
    // Do not centre with lineHeight / 2: that treats the entire line height as
    // ascent magnitude, ignoring descent and line gap, and pushes 14-DIP
    // Fluent labels down by roughly 4 DIP inside a 32-DIP control.
    [[nodiscard]] float centeredTextBaseline(const std::string& text, const RectF& lineBox,
                                             float textSize, int fontWeight = 400,
                                             std::string_view fontFamily = {}) const noexcept
    {
        fontFamily = resolvedTextFamily(fontFamily);
        float ascent = -textSize * 0.8f;
        float descent = textSize * 0.2f;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ != nullptr && !text.empty()) {
            wsc::Paint paint;
            paint.setTextSize(textSize * canvasCoordinateScale_);
            paint.setFontFamily(std::string(fontFamily));
            paint.setFontWeight(fontWeight);
            const auto metrics = canvas_->measureTextMetrics(text, paint);
            const float measuredAscent = std::abs(metrics.ascent);
            const float measuredDescent = std::abs(metrics.descent);
            if (measuredAscent > 0.0f) {
                ascent = -measuredAscent / canvasCoordinateScale_;
            }
            if (measuredDescent > 0.0f) {
                descent = measuredDescent / canvasCoordinateScale_;
            }
        }
#endif
        const float lineCenter = lineBox.y + lineBox.height * 0.5f;
        const float fontBoxCenterFromBaseline = (ascent + descent) * 0.5f;
        return lineCenter - fontBoxCenterFromBaseline;
    }

    // Compatibility alias retained for existing call sites. The returned
    // coordinate has always been passed to WhatsCanvas' BOTTOM mode, whose
    // glyph-atlas implementation treats it as the typographic baseline.
    [[nodiscard]] float centeredTextBottom(const std::string& text, const RectF& lineBox,
                                           float textSize, int fontWeight = 400,
                                           std::string_view fontFamily = {}) const noexcept
    {
        return centeredTextBaseline(text, lineBox, textSize, fontWeight, fontFamily);
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

    void strokeRoundRect(const RectF& rect, float radius, float width, Color color)
    {
        ++paintStats_.commandCount;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr || width <= 0.0f) return;
        wsc::Paint paint;
        paint.setStyle(wsc::Paint::Style::STROKE);
        paint.setStrokeWidth(width * canvasCoordinateScale_);
        paint.setColor(wsc::Color(color.r, color.g, color.b, color.a));
        paint.setAntiAlias(true);
        canvas_->drawRoundRect(wsc::RectF(rect.x * canvasCoordinateScale_, rect.y * canvasCoordinateScale_,
                                          rect.width * canvasCoordinateScale_, rect.height * canvasCoordinateScale_),
                               radius * canvasCoordinateScale_, paint);
#else
        (void)rect;
        (void)radius;
        (void)width;
        (void)color;
#endif
    }

    void fillCircle(const PointF& center, float radius, Color color)
    {
        ++paintStats_.commandCount;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr || radius <= 0.0f || color.a == 0) return;
        wsc::Paint paint;
        paint.setStyle(wsc::Paint::Style::FILL);
        paint.setColor(wsc::Color(color.r, color.g, color.b, color.a));
        paint.setAntiAlias(true);
        canvas_->drawCircle(wsc::PointF(center.x * canvasCoordinateScale_,
                                        center.y * canvasCoordinateScale_),
                            radius * canvasCoordinateScale_, paint);
#else
        (void)center;
        (void)radius;
        (void)color;
#endif
    }

    void strokeCircle(const PointF& center, float radius, float width, Color color)
    {
        ++paintStats_.commandCount;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr || radius <= 0.0f || width <= 0.0f || color.a == 0) return;
        wsc::Paint paint;
        paint.setStyle(wsc::Paint::Style::STROKE);
        paint.setStrokeWidth(width * canvasCoordinateScale_);
        paint.setStrokeJoin(wsc::Paint::StrokeJoin::ROUND);
        paint.setColor(wsc::Color(color.r, color.g, color.b, color.a));
        paint.setAntiAlias(true);
        canvas_->drawCircle(wsc::PointF(center.x * canvasCoordinateScale_,
                                        center.y * canvasCoordinateScale_),
                            radius * canvasCoordinateScale_, paint);
#else
        (void)center;
        (void)radius;
        (void)width;
        (void)color;
#endif
    }

    // A circular control outline shares one outer silhouette just like
    // fillStrokeRoundRect().  Keeping the stroke centre half a width inside
    // the requested radius prevents Radio/Slider rings from growing beyond
    // their layout box or being clipped at fractional DPR.
    void fillStrokeCircle(const PointF& center, float radius, float strokeWidth,
                          Color fillColor, Color strokeColor)
    {
        if (radius <= 0.0f) return;
        if (fillColor.a != 0) fillCircle(center, radius, fillColor);
        if (strokeWidth <= 0.0f || strokeColor.a == 0) return;
        strokeCircle(center, std::max(0.0f, radius - strokeWidth * 0.5f),
                     strokeWidth, strokeColor);
    }

    // Paint a rounded surface with an inset stroke.  A surprising number of
    // controls used to emulate a one-DIP border by drawing a stroke-coloured
    // rounded rectangle and then a smaller fill on top.  At fractional DPR
    // values (especially 150%) both silhouettes were independently
    // antialiased, leaving a soft or stair-stepped seam around the corners.
    //
    // Keeping the fill at the requested outer bounds and centring the stroke
    // half a stroke width inside those bounds gives the renderer one shared
    // outer silhouette.  The stroke's outer edge lands exactly on the
    // surface edge while its inner coverage is composited over the fill.
    void fillStrokeRoundRect(const RectF& rect, float radius, float strokeWidth,
                             Color fillColor, Color strokeColor)
    {
        if (rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }
        if (fillColor.a != 0) {
            fillRoundRect(rect, radius, fillColor);
        }
        if (strokeWidth <= 0.0f || strokeColor.a == 0) {
            return;
        }
        const float inset = strokeWidth * 0.5f;
        strokeRoundRect(
            {rect.x + inset, rect.y + inset,
             std::max(0.0f, rect.width - strokeWidth),
             std::max(0.0f, rect.height - strokeWidth)},
            std::max(0.0f, radius - inset), strokeWidth, strokeColor);
    }

    // Backend-neutral open arc used by progress indicators. Rounded caps
    // avoid the cut-off ends produced by a polygonal approximation at compact
    // Fluent spinner sizes.
    void strokeArc(const RectF& bounds, float startRadians, float sweepRadians,
                   float width, Color color)
    {
        ++paintStats_.commandCount;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr || width <= 0.0f || color.a == 0 ||
            bounds.width <= 0.0f || bounds.height <= 0.0f) {
            return;
        }
        wsc::Paint paint;
        paint.setStyle(wsc::Paint::Style::STROKE);
        paint.setStrokeWidth(width * canvasCoordinateScale_);
        paint.setStrokeCap(wsc::Paint::StrokeCap::ROUND);
        paint.setColor(wsc::Color(color.r, color.g, color.b, color.a));
        paint.setAntiAlias(true);
        canvas_->drawArc(
            wsc::RectF(bounds.x * canvasCoordinateScale_,
                       bounds.y * canvasCoordinateScale_,
                       bounds.width * canvasCoordinateScale_,
                       bounds.height * canvasCoordinateScale_),
            startRadians, sweepRadians, wsc::Canvas::ArcMode::OPEN, paint);
#else
        (void)bounds;
        (void)startRadians;
        (void)sweepRadians;
        (void)width;
        (void)color;
#endif
    }

    // Backend-neutral polygon primitives used by icon-like controls. Keeping
    // these in PaintContext preserves DPR conversion and lets headless paint
    // statistics count the operation even when no Canvas is attached.
    void fillPolygon(const std::vector<PointF>& points, Color color)
    {
        ++paintStats_.commandCount;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr || points.size() < 3) return;
        wsc::Path path;
        path.moveTo(points.front().x * canvasCoordinateScale_, points.front().y * canvasCoordinateScale_);
        for (std::size_t index = 1; index < points.size(); ++index) {
            path.lineTo(points[index].x * canvasCoordinateScale_, points[index].y * canvasCoordinateScale_);
        }
        path.close();
        wsc::Paint paint;
        paint.setStyle(wsc::Paint::Style::FILL);
        paint.setColor(wsc::Color(color.r, color.g, color.b, color.a));
        paint.setAntiAlias(true);
        canvas_->drawPath(path, paint);
#else
        (void)points;
        (void)color;
#endif
    }

    void strokePolygon(const std::vector<PointF>& points, float width, Color color)
    {
        ++paintStats_.commandCount;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr || points.size() < 2 || width <= 0.0f) return;
        wsc::Path path;
        path.moveTo(points.front().x * canvasCoordinateScale_, points.front().y * canvasCoordinateScale_);
        for (std::size_t index = 1; index < points.size(); ++index) {
            path.lineTo(points[index].x * canvasCoordinateScale_, points[index].y * canvasCoordinateScale_);
        }
        path.close();
        wsc::Paint paint;
        paint.setStyle(wsc::Paint::Style::STROKE);
        paint.setStrokeWidth(width * canvasCoordinateScale_);
        paint.setColor(wsc::Color(color.r, color.g, color.b, color.a));
        paint.setAntiAlias(true);
        canvas_->drawPath(path, paint);
#else
        (void)points;
        (void)width;
        (void)color;
#endif
    }

    void strokePolyline(const std::vector<PointF>& points, float width,
                        Color color)
    {
        ++paintStats_.commandCount;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr || points.size() < 2 || width <= 0.0f) {
            return;
        }
        wsc::Path path;
        path.moveTo(points.front().x * canvasCoordinateScale_,
                    points.front().y * canvasCoordinateScale_);
        for (std::size_t index = 1; index < points.size(); ++index) {
            path.lineTo(points[index].x * canvasCoordinateScale_,
                        points[index].y * canvasCoordinateScale_);
        }
        wsc::Paint paint;
        paint.setStyle(wsc::Paint::Style::STROKE);
        paint.setStrokeWidth(width * canvasCoordinateScale_);
        paint.setStrokeCap(wsc::Paint::StrokeCap::ROUND);
        paint.setStrokeJoin(wsc::Paint::StrokeJoin::ROUND);
        paint.setColor(wsc::Color(color.r, color.g, color.b, color.a));
        paint.setAntiAlias(true);
        canvas_->drawPath(path, paint);
#else
        (void)points;
        (void)width;
        (void)color;
#endif
    }

    // Elevation is recorded independently from fills so performance tools can
    // identify an accidental proliferation of expensive blurred surfaces.
    // Callers pass a semantic Theme::elevation token rather than ad-hoc blur
    // values; the context only performs the logical-to-device conversion.
    void drawBoxShadow(const RectF& rect, float radius, float blur, float offsetX,
                       float offsetY, float spread, Color color)
    {
        ++paintStats_.commandCount;
        ++paintStats_.boxShadowCalls;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr || color.a == 0 || blur <= 0.0f) {
            return;
        }
        const auto started = std::chrono::steady_clock::now();
        canvas_->drawBoxShadow(
            wsc::RectF(rect.x * canvasCoordinateScale_, rect.y * canvasCoordinateScale_,
                       rect.width * canvasCoordinateScale_, rect.height * canvasCoordinateScale_),
            radius * canvasCoordinateScale_, spread * canvasCoordinateScale_,
            blur * canvasCoordinateScale_, offsetX * canvasCoordinateScale_,
            offsetY * canvasCoordinateScale_, wsc::Color(color.r, color.g, color.b, color.a));
        paintStats_.boxShadowMilliseconds += elapsedMilliseconds(started);
#else
        (void)rect;
        (void)radius;
        (void)blur;
        (void)offsetX;
        (void)offsetY;
        (void)spread;
        (void)color;
#endif
    }

    void drawText(const std::string& text, float x, float y, float textSize, Color color,
                  int fontWeight = 400, std::string_view fontFamily = {})
    {
        ++paintStats_.commandCount;
        ++paintStats_.textDrawCalls;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ == nullptr) {
            return;
        }
        fontFamily = resolvedTextFamily(fontFamily);
        const auto started = std::chrono::steady_clock::now();
        wsc::CanvasAdapter adapter(*canvas_);
        adapter.setFillColor(wsc::Color(color.r, color.g, color.b, color.a));
        adapter.setTextSize(textSize * canvasCoordinateScale_);
        // Fluent defaults to Segoe UI on Windows. The explicit family is also
        // routed through WhatsCanvas' native text adapter, so a custom theme
        // can select another installed family without falling back to block
        // glyphs on the portable raster path.
        adapter.fillPaint().setFontFamily(std::string(fontFamily));
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
        (void)fontFamily;
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

    // Rounded clipping is essential for media previews: painting an opaque
    // rounded overlay only hides corners on one background and fails for
    // transparent/video content. Use the backend's actual clip path instead.
    void clipRoundRect(const RectF& rect, float radius) noexcept
    {
        ++paintStats_.commandCount;
        ++paintStats_.clipRectCalls;
#ifdef WHATSUI_HAS_WHATSCANVAS
        if (canvas_ != nullptr) {
            const auto started = std::chrono::steady_clock::now();
            wsc::Path path;
            path.addRoundRect(wsc::RectF(rect.x * canvasCoordinateScale_, rect.y * canvasCoordinateScale_,
                                         rect.width * canvasCoordinateScale_, rect.height * canvasCoordinateScale_),
                              std::max(0.0f, radius) * canvasCoordinateScale_);
            canvas_->clipPath(path);
            paintStats_.clipRectMilliseconds += elapsedMilliseconds(started);
        }
#else
        (void)rect;
        (void)radius;
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
