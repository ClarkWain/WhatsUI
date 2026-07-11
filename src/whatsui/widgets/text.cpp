#include "wui/widgets.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {

Text::Text(std::string value)
    : value_(std::move(value))
{
}

const std::string& Text::value() const noexcept
{
    return value_;
}

Text& Text::value(std::string value)
{
    setValue(std::move(value));
    return *this;
}

void Text::setValue(std::string value)
{
    value_ = std::move(value);
    markDirty(DirtyFlag::Layout);
}

float Text::fontSize() const noexcept
{
    return fontSize_;
}

void Text::setFontSize(float size) noexcept
{
    fontSize_ = std::max(1.0f, size);
    markDirty(DirtyFlag::Layout);
}

float Text::lineHeight() const noexcept
{
    return lineHeight_;
}

void Text::setLineHeight(float height) noexcept
{
    lineHeight_ = std::max(0.0f, height);
    markDirty(DirtyFlag::Layout);
}

TextWrap Text::wrap() const noexcept { return wrap_; }
void Text::setWrap(TextWrap wrap) noexcept { wrap_ = wrap; markDirty(DirtyFlag::Layout); }
std::size_t Text::maxLines() const noexcept { return maxLines_; }
void Text::setMaxLines(std::size_t lines) noexcept { maxLines_ = lines; markDirty(DirtyFlag::Layout); }
TextOverflow Text::overflow() const noexcept { return overflow_; }
void Text::setOverflow(TextOverflow overflow) noexcept { overflow_ = overflow; markDirty(DirtyFlag::Layout); }
std::vector<std::string> Text::resolvedLines(float availableWidth) const { return layoutLines(availableWidth); }

void Text::setColor(Color color) noexcept
{
    color_ = color;
    hasColor_ = true;
    markDirty(DirtyFlag::Paint);
}

void Text::clearColor() noexcept
{
    hasColor_ = false;
    markDirty(DirtyFlag::Paint);
}

SizeF Text::measure(const Constraints& constraints) const
{
    const auto lines = layoutLines(constraints.maxWidth);
    float width = 0.0f;
    for (const auto& line : lines) width = std::max(width, textWidth(line));
    return constraints.clamp({width, effectiveLineHeight() * static_cast<float>(lines.size())});
}

float Text::textWidth(const std::string& value) const
{
    if (const TextMeasurer* measurer = textMeasurer()) return measurer->measureText(value, fontSize_).width;
    // Fallback is deliberately codepoint-oriented so non-ASCII text does not
    // become wider merely because UTF-8 uses multiple bytes.
    std::size_t codepoints = 0;
    for (unsigned char c : value) if ((c & 0xC0u) != 0x80u) ++codepoints;
    return static_cast<float>(codepoints) * (fontSize_ * 0.5f);
}

float Text::effectiveLineHeight() const noexcept
{
    if (lineHeight_ > 0.0f) return lineHeight_;
    if (const TextMeasurer* measurer = textMeasurer()) return measurer->measureText("M", fontSize_).height;
    return fontSize_ * 1.25f;
}

std::vector<std::string> Text::layoutLines(float availableWidth) const
{
    const bool constrained = std::isfinite(availableWidth);
    const bool canWrap = wrap_ == TextWrap::Word && constrained;
    if (canWrap) {
        if (const auto* provider = dynamic_cast<const TextLayoutProvider*>(textMeasurer())) {
            const auto resolved = provider->layoutText(value_, fontSize_, availableWidth,
                                                       effectiveLineHeight(), maxLines_,
                                                       overflow_ == TextOverflow::Ellipsis);
            std::vector<std::string> lines;
            lines.reserve(resolved.size());
            for (const auto& line : resolved) lines.push_back(line.text);
            // An empty paragraph is still one visible logical line. Backends
            // return no lines for an empty input, while Text's longstanding
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

float Text::baselineOffset() const noexcept
{
    TextExtents extents;
    extents.height = fontSize_ * 1.25f;
    extents.ascent = fontSize_ * 0.8f;
    extents.descent = fontSize_ * 0.2f;
    if (const TextMeasurer* measurer = textMeasurer()) {
        extents = measurer->measureText(value_, fontSize_);
    }
    const float height = lineHeight_ > 0.0f ? lineHeight_ : extents.height;
    const float glyphHeight = extents.ascent + extents.descent;
    return std::max(0.0f, height - glyphHeight) * 0.5f + extents.ascent;
}

void Text::paint(PaintContext& context)
{
    const auto lines = layoutLines(bounds().width);
    if (!lines.empty()) {
        const Color color = hasColor_ ? color_ : theme().colors.text;
        const float lineHeight = effectiveLineHeight();
        const float baseline = bounds().y + baselineOffset();
        // Most text is already contained by wrapping/ellipsis, so adding a
        // clip for every run is redundant. More importantly, the Software
        // backend batches glyph-atlas draws after paint has returned; a large
        // structural update can otherwise leave those independent per-run
        // clips associated with the wrong batch. Clip only when Clip overflow
        // can actually expose pixels outside this node's bounds.
        bool needsClip = false;
#ifndef WHATSUI_HAS_WHATSCANVAS
        // The headless contract retains literal Clip semantics. WhatsCanvas
        // itself batches glyphs at endFrame, so its platform viewport (or the
        // window edge) owns clipping rather than an ephemeral clip around a
        // single text draw. See the Software structural regression below.
        needsClip = overflow_ == TextOverflow::Clip
            && static_cast<float>(lines.size()) * lineHeight > bounds().height + 0.01f;
        if (!needsClip) {
            for (const auto& line : lines) {
                if (textWidth(line) > bounds().width + 0.01f) {
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
        for (std::size_t index = 0; index < lines.size(); ++index) {
            context.drawText(lines[index], bounds().x, baseline + lineHeight * static_cast<float>(index), fontSize_, color);
        }
        if (needsClip) {
            context.restore();
        }
    }
    clearDirty(DirtyFlag::Paint);
}

Spacer::Spacer(SizeF size) noexcept
    : size_(size)
{
}

SizeF Spacer::size() const noexcept
{
    return size_;
}

void Spacer::setSize(SizeF size) noexcept
{
    size_ = size;
    markDirty(DirtyFlag::Layout);
}

SizeF Spacer::measure(const Constraints& constraints) const
{
    return constraints.clamp(size_);
}

void Spacer::paint(PaintContext& context)
{
    (void)context;
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
