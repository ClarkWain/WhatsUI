#include "wui/widgets.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {

TextNode::TextNode(std::string value)
    : value_(std::move(value))
    , fontFamily_(theme().typography.familyBase.empty()
                      ? std::string(theme().typography.familyBaseFallback)
                      : std::string(theme().typography.familyBase))
{
}

const std::string& TextNode::value() const noexcept
{
    return value_;
}

TextNode& TextNode::value(std::string value)
{
    setValue(std::move(value));
    return *this;
}

void TextNode::setValue(std::string value)
{
    value_ = std::move(value);
    invalidateLineCache();
    markDirty(DirtyFlag::Layout);
}

float TextNode::fontSize() const noexcept
{
    return fontSize_;
}

void TextNode::setFontSize(float size) noexcept
{
    fontSize_ = std::max(1.0f, size);
    markDirty(DirtyFlag::Layout);
}

int TextNode::fontWeight() const noexcept
{
    return fontWeight_;
}

void TextNode::setFontWeight(int weight) noexcept
{
    fontWeight_ = std::clamp(weight, 1, 1000);
    markDirty(DirtyFlag::Layout);
}

float TextNode::lineHeight() const noexcept
{
    return lineHeight_;
}

void TextNode::setLineHeight(float height) noexcept
{
    lineHeight_ = std::max(0.0f, height);
    markDirty(DirtyFlag::Layout);
}

const std::string& TextNode::fontFamily() const noexcept
{
    return fontFamily_;
}

void TextNode::setFontFamily(std::string family)
{
    if (family.empty()) {
        family = theme().typography.familyBase.empty()
            ? std::string(theme().typography.familyBaseFallback)
            : std::string(theme().typography.familyBase);
    }
    if (fontFamily_ == family) return;
    fontFamily_ = std::move(family);
    markDirty(DirtyFlag::Layout);
}

void TextNode::setTextStyle(const TextStyleToken& style)
{
    const std::string family(style.family.empty()
                                 ? (theme().typography.familyBase.empty()
                                        ? theme().typography.familyBaseFallback
                                        : theme().typography.familyBase)
                                 : style.family);
    const float size = std::max(1.0f, style.size);
    const int weight = std::clamp(style.weight, 1, 1000);
    const float lineHeight = std::max(0.0f, style.lineHeight);
    if (fontFamily_ == family && fontSize_ == size && fontWeight_ == weight && lineHeight_ == lineHeight) return;
    fontFamily_ = family;
    fontSize_ = size;
    fontWeight_ = weight;
    lineHeight_ = lineHeight;
    markDirty(DirtyFlag::Layout);
}

void TextNode::setRole(TextRole role) noexcept { if (role_ != role) { role_ = role; markDirty(DirtyFlag::Paint); } }
TextRole TextNode::role() const noexcept { return role_; }
void TextNode::setAlignment(TextAlign alignment) noexcept { if (alignment_ != alignment) { alignment_ = alignment; markDirty(DirtyFlag::Paint); } }
TextAlign TextNode::alignment() const noexcept { return alignment_; }
void TextNode::setUnderline(bool value) noexcept { if (underline_ != value) { underline_ = value; markDirty(DirtyFlag::Paint); } }
bool TextNode::isUnderlined() const noexcept { return underline_; }
void TextNode::setStrikethrough(bool value) noexcept { if (strikethrough_ != value) { strikethrough_ = value; markDirty(DirtyFlag::Paint); } }
bool TextNode::isStrikethrough() const noexcept { return strikethrough_; }

TextWrap TextNode::wrap() const noexcept { return wrap_; }
void TextNode::setWrap(TextWrap wrap) noexcept { wrap_ = wrap; markDirty(DirtyFlag::Layout); }
std::size_t TextNode::maxLines() const noexcept { return maxLines_; }
void TextNode::setMaxLines(std::size_t lines) noexcept { maxLines_ = lines; markDirty(DirtyFlag::Layout); }
TextOverflow TextNode::overflow() const noexcept { return overflow_; }
void TextNode::setOverflow(TextOverflow overflow) noexcept { overflow_ = overflow; markDirty(DirtyFlag::Layout); }
void TextNode::setFillAvailableWidth(bool fill) noexcept
{
    if (fillAvailableWidth_ == fill) return;
    fillAvailableWidth_ = fill;
    markDirty(DirtyFlag::Layout);
}
bool TextNode::fillsAvailableWidth() const noexcept { return fillAvailableWidth_; }
std::vector<std::string> TextNode::resolvedLines(float availableWidth) const { return layoutLines(availableWidth); }

void TextNode::setColor(Color color) noexcept
{
    color_ = color;
    hasColor_ = true;
    markDirty(DirtyFlag::Paint);
}

void TextNode::clearColor() noexcept
{
    hasColor_ = false;
    markDirty(DirtyFlag::Paint);
}

SizeF TextNode::measure(const Constraints& constraints) const
{
    if (wrap_ == TextWrap::NoWrap && overflow_ == TextOverflow::Clip) {
        const auto& explicitLineRanges = explicitLines();
        const std::size_t lineCount = maxLines_ == 0
            ? explicitLineRanges.size()
            : std::min(maxLines_, explicitLineRanges.size());
        float width = 0.0f;
        if (fillAvailableWidth_ && std::isfinite(constraints.maxWidth)) {
            width = constraints.maxWidth;
        } else {
            for (std::size_t index = 0; index < lineCount; ++index) {
                const auto line = explicitLineRanges[index];
                width = std::max(width, textWidth(value_.substr(line.start, line.length)));
            }
        }
        return constraints.clamp(
            {width, effectiveLineHeight() * static_cast<float>(lineCount)});
    }
    const auto lines = layoutLines(constraints.maxWidth);
    float width = 0.0f;
    for (const auto& line : lines) width = std::max(width, textWidth(line));
    return constraints.clamp({width, effectiveLineHeight() * static_cast<float>(lines.size())});
}

const std::vector<TextNode::ExplicitLine>& TextNode::explicitLines() const
{
    if (explicitLineCacheValid_) return explicitLineCache_;
    explicitLineCache_.clear();
    explicitLineCache_.reserve(
        static_cast<std::size_t>(std::count(value_.begin(), value_.end(), '\n')) + 1);
    std::size_t lineStart = 0;
    for (std::size_t index = 0; index < value_.size(); ++index) {
        if (value_[index] != '\n') continue;
        explicitLineCache_.push_back({lineStart, index - lineStart});
        lineStart = index + 1;
    }
    explicitLineCache_.push_back({lineStart, value_.size() - lineStart});
    explicitLineCacheValid_ = true;
    return explicitLineCache_;
}

void TextNode::invalidateLineCache() noexcept
{
    explicitLineCacheValid_ = false;
    explicitLineCache_.clear();
}

float TextNode::textWidth(const std::string& value) const
{
    if (const TextMeasurer* measurer = textMeasurer()) {
        return measurer->measureText(value, fontSize_, fontWeight_, fontFamily_).width;
    }
    // Fallback is deliberately codepoint-oriented so non-ASCII text does not
    // become wider merely because UTF-8 uses multiple bytes.
    std::size_t codepoints = 0;
    for (unsigned char c : value) if ((c & 0xC0u) != 0x80u) ++codepoints;
    return static_cast<float>(codepoints) * (fontSize_ * 0.5f);
}

float TextNode::effectiveLineHeight() const noexcept
{
    if (lineHeight_ > 0.0f) return lineHeight_;
    if (const TextMeasurer* measurer = textMeasurer()) {
        return measurer->measureText("M", fontSize_, fontWeight_, fontFamily_).height;
    }
    return fontSize_ * 1.25f;
}

std::vector<std::string> TextNode::layoutLines(float availableWidth) const
{
    const bool constrained = std::isfinite(availableWidth);
    const bool canWrap = wrap_ == TextWrap::Word && constrained;
    if (!canWrap) {
        const auto& ranges = explicitLines();
        const std::size_t lineCount = maxLines_ == 0
            ? ranges.size() : std::min(maxLines_, ranges.size());
        std::vector<std::string> lines;
        lines.reserve(lineCount);
        for (std::size_t index = 0; index < lineCount; ++index) {
            lines.push_back(value_.substr(ranges[index].start, ranges[index].length));
        }
        bool truncated = lineCount < ranges.size();
        if (overflow_ == TextOverflow::Ellipsis && constrained && !lines.empty()
            && textWidth(lines.front()) > availableWidth) {
            truncated = true;
        }
        if (truncated && overflow_ == TextOverflow::Ellipsis && !lines.empty()) {
            auto fits = [&](const std::string& candidate) {
                return !constrained || textWidth(candidate) <= availableWidth;
            };
            std::string& line = lines.back();
            if (!constrained) {
                line += "...";
            } else {
                while (!line.empty() && !fits(line + "...")) {
                    std::size_t start = line.size() - 1;
                    while (start > 0
                           && (static_cast<unsigned char>(line[start]) & 0xC0u) == 0x80u) {
                        --start;
                    }
                    line.erase(start);
                }
                if (fits(line + "...")) line += "...";
            }
        }
        return lines;
    }
    if (canWrap) {
        if (const auto* provider = dynamic_cast<const TextLayoutProvider*>(textMeasurer())) {
            const auto resolved = provider->layoutText(value_, fontSize_, fontWeight_, availableWidth,
                                                       effectiveLineHeight(), maxLines_,
                                                       overflow_ == TextOverflow::Ellipsis,
                                                       fontFamily_);
            std::vector<std::string> lines;
            lines.reserve(resolved.size());
            for (const auto& line : resolved) lines.push_back(line.text);
            // An empty paragraph is still one visible logical line. Backends
            // return no lines for an empty input, while TextNode's longstanding
            // measure/paint contract reserves that line.
            if (lines.empty() && value_.empty()) lines.emplace_back();
            return lines;
        }
    }
    std::vector<std::string> lines;
    std::string current;
    bool truncated = false;
    auto appendLine = [&] { lines.push_back(current); current.clear(); };
    auto fits = [&](const std::string& candidate) { return !constrained || textWidth(candidate) <= availableWidth; };
    auto appendEllipsis = [&](std::string& line) {
        if (!constrained) { line += "..."; return; }
        while (!line.empty() && !fits(line + "...")) {
            std::size_t start = line.size() - 1;
            while (start > 0 && (static_cast<unsigned char>(line[start]) & 0xC0u) == 0x80u) --start;
            line.erase(start);
        }
        if (fits(line + "...")) line += "...";
    };
    auto finish = [&] { appendLine(); };

    for (std::size_t index = 0; index < value_.size();) {
        if (value_[index] == '\n') { finish(); ++index; continue; }
        if (!canWrap) { current += value_[index++]; continue; }

        // Consume a whole ASCII-delimited word before deciding where it goes;
        // this avoids leaving the first character of the next word on a line.
        while (index < value_.size() && (value_[index] == ' ' || value_[index] == '\t')) ++index;
        if (index == value_.size()) break;
        if (value_[index] == '\n') { finish(); ++index; continue; }
        const std::size_t wordStart = index;
        while (index < value_.size() && value_[index] != '\n' && value_[index] != ' ' && value_[index] != '\t') ++index;
        const std::string word = value_.substr(wordStart, index - wordStart);
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (fits(candidate)) { current = candidate; continue; }
        if (!current.empty()) appendLine();
        if (fits(word)) { current = word; continue; }
        // A word wider than the line has no legal whitespace break; split it
        // by UTF-8 scalar so every emitted substring remains valid UTF-8.
        for (std::size_t glyph = 0; glyph < word.size();) {
            const std::size_t begin = glyph++;
            while (glyph < word.size() && (static_cast<unsigned char>(word[glyph]) & 0xC0u) == 0x80u) ++glyph;
            const std::string next = word.substr(begin, glyph - begin);
            if (!current.empty() && !fits(current + next)) appendLine();
            current += next;
        }
    }
    finish();
    if (maxLines_ != 0 && lines.size() > maxLines_) { lines.resize(maxLines_); truncated = true; }
    // NoWrap can still truncate horizontally; ellipsis is a visual substitute
    // for that clipped suffix, without changing its single-line height.
    if (!canWrap && constrained && !lines.empty() && !fits(lines.front())) truncated = true;
    if (truncated && overflow_ == TextOverflow::Ellipsis && !lines.empty()) appendEllipsis(lines.back());
    return lines;
}

float TextNode::baselineOffset() const noexcept
{
    TextExtents extents;
    extents.height = fontSize_ * 1.25f;
    extents.ascent = fontSize_ * 0.8f;
    extents.descent = fontSize_ * 0.2f;
    if (const TextMeasurer* measurer = textMeasurer()) {
        extents = measurer->measureText(value_, fontSize_, fontWeight_, fontFamily_);
    }
    const float height = lineHeight_ > 0.0f ? lineHeight_ : extents.height;
    const float glyphHeight = extents.ascent + extents.descent;
    return std::max(0.0f, height - glyphHeight) * 0.5f + extents.ascent;
}

void TextNode::paint(PaintContext& context)
{
    const Color color = hasColor_ ? color_ : theme().colors.text;
    const float lineHeight = effectiveLineHeight();
    const auto activeClip = context.currentClipBounds();

    auto paintRange = [&](std::size_t lineCount, auto&& lineAt) {
        if (lineCount == 0) return;
        std::size_t first = 0;
        std::size_t last = lineCount;
        if (activeClip) {
            const float visibleTop = std::max(bounds().y, activeClip->y);
            const float visibleBottom = std::min(
                bounds().y + lineHeight * static_cast<float>(lineCount),
                activeClip->y + activeClip->height);
            if (visibleBottom <= visibleTop || activeClip->width <= 0.0f) {
                return;
            }
            first = std::min(
                lineCount,
                static_cast<std::size_t>(std::max(
                    0.0f, std::floor((visibleTop - bounds().y) / lineHeight))));
            last = std::min(
                lineCount,
                static_cast<std::size_t>(std::max(
                    0.0f, std::ceil((visibleBottom - bounds().y) / lineHeight))));
            // Retain one line of overscan for scripts whose glyph ink exceeds
            // the nominal ascent/descent box.
            if (first > 0) --first;
            if (last < lineCount) ++last;
        }

        bool needsClip = false;
#ifndef WHATSUI_HAS_WHATSCANVAS
        // WhatsCanvas batches glyphs after paint returns, so native viewport
        // clipping remains owned by the enclosing viewport. The headless
        // contract still records TextNode's literal overflow clip when an
        // ancestor is not already clipping inside the TextNode bounds.
        const bool ancestorClipsWidth = activeClip
            && activeClip->x >= bounds().x - 0.01f
            && activeClip->x + activeClip->width
                <= bounds().x + bounds().width + 0.01f;
        needsClip = overflow_ == TextOverflow::Clip
            && static_cast<float>(lineCount) * lineHeight > bounds().height + 0.01f;
        if (!needsClip && overflow_ == TextOverflow::Clip && !ancestorClipsWidth) {
            for (std::size_t index = first; index < last; ++index) {
                if (textWidth(lineAt(index)) > bounds().width + 0.01f) {
                    needsClip = true;
                    break;
                }
            }
        }
#endif
        if (needsClip) {
            (void)context.save();
            context.clipRect({bounds().x, bounds().y - fontSize_, bounds().width,
                              bounds().height + fontSize_ * 2.0f});
        }
        for (std::size_t index = first; index < last; ++index) {
            const std::string line = lineAt(index);
            const RectF lineBox{
                bounds().x,
                bounds().y + lineHeight * static_cast<float>(index),
                bounds().width,
                lineHeight};
            const bool needsWidth = alignment_ != TextAlign::Start
                || underline_ || strikethrough_;
            const float width = needsWidth ? textWidth(line) : 0.0f;
            float x = bounds().x;
            if (alignment_ == TextAlign::Center) {
                x += std::max(0.0f, (bounds().width - width) * 0.5f);
            }
            if (alignment_ == TextAlign::End) {
                x += std::max(0.0f, bounds().width - width);
            }
            const float baseline = context.centeredTextBottom(
                line, lineBox, fontSize_, fontWeight_, fontFamily_);
            context.drawText(line, x, baseline, fontSize_, color,
                             fontWeight_, fontFamily_);
            if (underline_ && width > 0.0f) {
                context.fillRect(
                    {x, baseline + theme().stroke.thin, width, theme().stroke.thin},
                    color);
            }
            if (strikethrough_ && width > 0.0f) {
                context.fillRect(
                    {x, lineBox.y + lineBox.height * 0.5f,
                     width, theme().stroke.thin},
                    color);
            }
        }
        if (needsClip) context.restore();
    };

    if (wrap_ == TextWrap::NoWrap && overflow_ == TextOverflow::Clip) {
        const auto& ranges = explicitLines();
        const std::size_t lineCount = maxLines_ == 0
            ? ranges.size() : std::min(maxLines_, ranges.size());
        paintRange(lineCount, [&](std::size_t index) {
            return value_.substr(ranges[index].start, ranges[index].length);
        });
    } else {
        const auto lines = layoutLines(bounds().width);
        paintRange(lines.size(), [&](std::size_t index) {
            return lines[index];
        });
    }
    clearDirty(DirtyFlag::Paint);
}

SpacerNode::SpacerNode(SizeF size) noexcept
    : size_(size)
{
}

SizeF SpacerNode::size() const noexcept
{
    return size_;
}

void SpacerNode::setSize(SizeF size) noexcept
{
    size_ = size;
    markDirty(DirtyFlag::Layout);
}

SizeF SpacerNode::measure(const Constraints& constraints) const
{
    return constraints.clamp(size_);
}

void SpacerNode::paint(PaintContext& context)
{
    (void)context;
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
