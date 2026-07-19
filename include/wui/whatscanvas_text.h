#pragma once

// WhatsCanvas-backed text measurement. Only active when WhatsUI is built with
// WHATSUI_WITH_WHATSCANVAS=ON (which defines WHATSUI_HAS_WHATSCANVAS). Install
// it once a measurement canvas exists:
//
//     wui::WhatsCanvasTextMeasurer measurer(canvas);
//     wui::setTextMeasurer(&measurer);

#include "wui/text_metrics.h"
#include "wui/theme.h"
#include "wui/icons.h"

#ifdef WHATSUI_HAS_WHATSCANVAS

#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "wsc/Canvas.h"
#include "wsc/Font.h"
#include "wsc/Paint.h"

namespace wui {

// Cache observability is intentionally small and deterministic. It describes
// only this adapter's logical measure/layout cache; WhatsCanvas additionally
// owns its persistent glyph atlas and shaping caches.
struct TextCacheStats {
    std::size_t entries{0};
    std::size_t hits{0};
    std::size_t misses{0};
};

// Result of explicitly applying the documented Windows-first fallback policy.
// `defaultFallbackChain` covers Latin, CJK, Arabic/Hebrew and symbols. On
// Windows `emojiFallback` reports whether Segoe UI Emoji was found and added.
struct WhatsCanvasTextPolicyStatus {
    bool defaultFallbackChain{false};
    bool emojiFallback{false};
    bool regularIconFont{false};
    bool filledIconFont{false};
};

class WhatsCanvasTextMeasurer : public TextLayoutProvider {
public:
    explicit WhatsCanvasTextMeasurer(wsc::Canvas& canvas, float scaleFactor = 1.0f) noexcept
        : canvas_(&canvas)
        , scaleFactor_(scaleFactor > 0.0f ? scaleFactor : 1.0f)
    {
        policyStatus_ = installWindowsFallbackPolicy();
    }

    void setScaleFactor(float scaleFactor) noexcept
    {
        const float sanitized = scaleFactor > 0.0f ? scaleFactor : 1.0f;
        if (std::fabs(scaleFactor_ - sanitized) < 0.0001f) return;
        scaleFactor_ = sanitized;
        clearCache();
    }

    [[nodiscard]] float scaleFactor() const noexcept { return scaleFactor_; }

    // Make the font policy explicit at application startup. WhatsCanvas also
    // installs its portable default fallback chain itself; this call makes the
    // contract auditable and augments it with Segoe UI Emoji on Windows.
    [[nodiscard]] WhatsCanvasTextPolicyStatus installWindowsFallbackPolicy()
    {
        if (policyInstalled_) return policyStatus_;
        WhatsCanvasTextPolicyStatus result;
        if (canvas_ == nullptr) return result;

        const auto icons = registerDefaultIconFonts(*canvas_);
        result.regularIconFont = icons.regular;
        result.filledIconFont = icons.filled;
        wsc::FontFallbackChain chain = wsc::FontSystem::defaultFallbackChain();
#if defined(_WIN32)
        constexpr const char* emojiFamily = "WhatsUI Emoji";
        constexpr const char* emojiPath = "C:/Windows/Fonts/seguiemj.ttf";
        if (wsc::FontSystem::fileExists(emojiPath)) {
            wsc::FontFace emoji = wsc::FontFace::fromFile(wsc::FontDescriptor(emojiFamily), emojiPath);
            emoji.addCodepointRange(0x1F000, 0x1FAFF);
            emoji.addCodepointRange(0x2600, 0x27BF);
            if (canvas_->registerFontFace(emoji)) {
                chain.addFallbackFamily(emojiFamily);
                result.emojiFallback = true;
            }
        }
#endif
        result.defaultFallbackChain = canvas_->setFontFallbackChain(chain);
        policyInstalled_ = true;
        policyStatus_ = result;
        clearCache();
        return result;
    }

    [[nodiscard]] WhatsCanvasTextPolicyStatus policyStatus() const noexcept { return policyStatus_; }

    [[nodiscard]] TextExtents measureText(const std::string& text, float fontSize) const override
    {
        return measureText(text, fontSize, 400);
    }

    [[nodiscard]] TextExtents measureText(const std::string& text, float fontSize,
                                          int fontWeight) const override
    {
        return measureText(text, fontSize, fontWeight, {});
    }

    [[nodiscard]] TextExtents measureText(const std::string& text, float fontSize,
                                          int fontWeight,
                                          std::string_view requestedFontFamily) const override
    {
        const std::string fontFamily = resolvedFontFamily(requestedFontFamily);
        const MeasureKey key{text, fontFamily, fontSize, fontWeight};
        if (const auto found = measureCache_.find(key); found != measureCache_.end()) {
            ++cacheHits_;
            return found->second;
        }
        ++cacheMisses_;
        wsc::Paint paint;
        paint.setTextSize(fontSize * scaleFactor_);
        paint.setFontWeight(fontWeight);
        paint.setFontFamily(fontFamily);
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
        insert(measureCache_, key, extents);
        return extents;
    }

    [[nodiscard]] std::vector<TextLayoutLine> layoutText(
        const std::string& text,
        float fontSize,
        float availableWidth,
        float lineHeight,
        std::size_t maxLines,
        bool ellipsize) const override
    {
        return layoutText(text, fontSize, 400, availableWidth, lineHeight, maxLines, ellipsize);
    }

    [[nodiscard]] std::vector<TextLayoutLine> layoutText(
        const std::string& text,
        float fontSize,
        int fontWeight,
        float availableWidth,
        float lineHeight,
        std::size_t maxLines,
        bool ellipsize) const override
    {
        return layoutText(text, fontSize, fontWeight, availableWidth, lineHeight, maxLines,
                          ellipsize, {});
    }

    [[nodiscard]] std::vector<TextLayoutLine> layoutText(
        const std::string& text,
        float fontSize,
        int fontWeight,
        float availableWidth,
        float lineHeight,
        std::size_t maxLines,
        bool ellipsize,
        std::string_view requestedFontFamily) const override
    {
        if (canvas_ == nullptr || !std::isfinite(availableWidth) || availableWidth <= 0.0f) return {};
        const std::string fontFamily = resolvedFontFamily(requestedFontFamily);
        const LayoutKey key{text, fontFamily, fontSize, fontWeight, availableWidth,
                            lineHeight, maxLines, ellipsize};
        if (const auto found = layoutCache_.find(key); found != layoutCache_.end()) {
            ++cacheHits_;
            return found->second;
        }
        ++cacheMisses_;

        wsc::Paint paint;
        paint.setTextSize(fontSize * scaleFactor_);
        paint.setFontWeight(fontWeight);
        paint.setFontFamily(fontFamily);
        const float physicalLineHeight = (lineHeight > 0.0f ? lineHeight : fontSize * 1.25f) * scaleFactor_;
        const float physicalHeight = std::max(physicalLineHeight,
            physicalLineHeight * static_cast<float>(maxLines == 0 ? 65536 : maxLines));
        const auto lines = canvas_->layoutTextBox(
            text,
            wsc::RectF(0.0f, 0.0f, availableWidth * scaleFactor_, physicalHeight),
            physicalLineHeight,
            maxLines > static_cast<std::size_t>(std::numeric_limits<int>::max())
                ? std::numeric_limits<int>::max() : static_cast<int>(maxLines),
            ellipsize,
            paint);
        std::vector<TextLayoutLine> result;
        result.reserve(lines.size());
        for (const auto& line : lines) {
            result.push_back({line.text, line.sourceStart, line.sourceLength,
                              line.width / scaleFactor_, line.ellipsized});
        }
        insert(layoutCache_, key, result);
        return result;
    }

    [[nodiscard]] TextCacheStats cacheStats() const noexcept
    {
        return {measureCache_.size() + layoutCache_.size(), cacheHits_, cacheMisses_};
    }

    void clearCache() const noexcept
    {
        measureCache_.clear();
        layoutCache_.clear();
        cacheHits_ = 0;
        cacheMisses_ = 0;
    }

private:
    [[nodiscard]] std::string resolvedFontFamily(std::string_view requested = {}) const
    {
        const auto& typography = theme().typography;
        const std::string_view preferred = requested.empty()
            ? (typography.familyBase.empty() ? typography.familyBaseFallback
                                             : typography.familyBase)
            : requested;
        if (canvas_ != nullptr
            && canvas_->textBackend() != wsc::Canvas::TextBackend::DirectWrite) {
            if (preferred == kFluentWindowsFontFamily
                || preferred == kFluentWindowsFontFallback) {
                return wsc::FontSystem::kDefaultPrimaryFamily;
            }
            if (preferred == typography.familyMonospace) {
                return wsc::FontSystem::kDefaultMonoFamily;
            }
        }
        return std::string(preferred);
    }

    struct MeasureKey {
        std::string text;
        std::string fontFamily;
        float fontSize{0.0f};
        int fontWeight{400};
        [[nodiscard]] bool operator<(const MeasureKey& other) const noexcept
        {
            return std::tie(text, fontFamily, fontSize, fontWeight)
                < std::tie(other.text, other.fontFamily, other.fontSize, other.fontWeight);
        }
    };

    struct LayoutKey {
        std::string text;
        std::string fontFamily;
        float fontSize{0.0f};
        int fontWeight{400};
        float availableWidth{0.0f};
        float lineHeight{0.0f};
        std::size_t maxLines{0};
        bool ellipsize{false};
        [[nodiscard]] bool operator<(const LayoutKey& other) const noexcept
        {
            return std::tie(text, fontFamily, fontSize, fontWeight, availableWidth,
                            lineHeight, maxLines, ellipsize)
                < std::tie(other.text, other.fontFamily, other.fontSize, other.fontWeight,
                           other.availableWidth, other.lineHeight, other.maxLines,
                           other.ellipsize);
        }
    };

    template <typename Map, typename Key, typename Value>
    static void insert(Map& cache, Key key, Value value)
    {
        // A deterministic lexical eviction rule prevents a long-lived window
        // from retaining every transient label ever measured.
        constexpr std::size_t kMaxEntries = 512;
        if (cache.size() >= kMaxEntries) cache.erase(cache.begin());
        cache.emplace(std::move(key), std::move(value));
    }

    wsc::Canvas* canvas_{nullptr};
    float scaleFactor_{1.0f};
    mutable std::map<MeasureKey, TextExtents> measureCache_;
    mutable std::map<LayoutKey, std::vector<TextLayoutLine>> layoutCache_;
    mutable std::size_t cacheHits_{0};
    mutable std::size_t cacheMisses_{0};
    bool policyInstalled_{false};
    WhatsCanvasTextPolicyStatus policyStatus_{};
};

} // namespace wui

#endif // WHATSUI_HAS_WHATSCANVAS
