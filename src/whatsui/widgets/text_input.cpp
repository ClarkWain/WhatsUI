#include "wui/text_input.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace wui {

namespace {

TextRange normalizeRange(TextRange range, std::size_t textSize) noexcept
{
    if (range.start > range.end) {
        std::swap(range.start, range.end);
    }
    range.start = std::min(range.start, textSize);
    range.end = std::min(range.end, textSize);
    return range;
}

bool isWordCharacter(char character) noexcept
{
    const auto value = static_cast<unsigned char>(character);
    return std::isalnum(value) != 0 || character == '_';
}

constexpr std::size_t kMaximumUndoEntries = 100;

[[nodiscard]] float horizontalTextPadding(const Theme& current, InputSize size,
                                          bool multiline) noexcept
{
    if (multiline) {
        switch (size) {
        case InputSize::Small: return current.spacing.horizontal.s;
        case InputSize::Large: return current.spacing.horizontal.m +
                                      current.spacing.horizontal.xxs;
        case InputSize::Medium:
        default: return current.spacing.horizontal.m;
        }
    }
    switch (size) {
    case InputSize::Small: return current.spacing.horizontal.s;
    case InputSize::Large: return current.spacing.horizontal.m +
                                  current.spacing.horizontal.sNudge;
    case InputSize::Medium:
    default: return current.controls.horizontalPadding;
    }
}

[[nodiscard]] float verticalTextPadding(const Theme& current, InputSize size,
                                        bool multiline) noexcept
{
    if (!multiline) return current.stroke.thin;
    switch (size) {
    case InputSize::Small: return current.spacing.vertical.xs;
    case InputSize::Large: return current.spacing.vertical.s;
    case InputSize::Medium:
    default: return current.spacing.vertical.sNudge;
    }
}

[[nodiscard]] RectF textViewport(const RectF& bounds, const Theme& current,
                                 bool focused, InputSize size,
                                 bool multiline) noexcept
{
    (void)focused;
    const float horizontalPadding =
        horizontalTextPadding(current, size, multiline);
    const float verticalPadding =
        verticalTextPadding(current, size, multiline);
    return {bounds.x + horizontalPadding, bounds.y + verticalPadding,
            std::max(0.0f, bounds.width - horizontalPadding * 2.0f),
            std::max(0.0f, bounds.height - verticalPadding * 2.0f)};
}

[[nodiscard]] float snapToPhysicalPixel(float logical, float scale) noexcept
{
    scale = std::max(1.0f, scale);
    return std::round(logical * scale) / scale;
}

[[nodiscard]] float snapThicknessToPhysicalPixels(float logical,
                                                  float scale) noexcept
{
    scale = std::max(1.0f, scale);
    return static_cast<float>(
               std::max(1L, std::lround(logical * scale))) /
           scale;
}

void drawBottomStroke(PaintContext& context, const RectF& bounds, float radius,
                      float thickness, Color color, float progress = 1.0f)
{
    progress = std::clamp(progress, 0.0f, 1.0f);
    if (bounds.width <= 0.0f || bounds.height <= 0.0f ||
        thickness <= 0.0f || color.a == 0 || progress <= 0.0f) {
        return;
    }
    const float scale = std::max(1.0f, context.scaleFactor());
    const float snappedThickness =
        snapThicknessToPhysicalPixels(thickness, scale);
    const float intendedWidth = bounds.width * progress;
    const float intendedLeft =
        bounds.x + (bounds.width - intendedWidth) * 0.5f;
    const float left = snapToPhysicalPixel(intendedLeft, scale);
    const float right =
        snapToPhysicalPixel(intendedLeft + intendedWidth, scale);
    const float bottom =
        snapToPhysicalPixel(bounds.y + bounds.height, scale);
    const RectF segment{
        left,
        bottom - snappedThickness,
        std::max(1.0f / scale, right - left),
        snappedThickness,
    };
    // Match Fluent's clipped ::after construction. At full width the input's
    // bottom corners define the silhouette; during scaleX animation the
    // segment retains clean rounded leading edges instead of square horns.
    const int checkpoint = context.save();
    context.clipRoundRect(bounds, radius);
    context.fillRoundRect(segment, std::min(radius, thickness * 0.5f), color);
    context.restoreTo(checkpoint);
}

[[nodiscard]] float measuredTextWidth(const std::string& text, std::size_t end,
                                      float fontSize) noexcept
{
    end = std::min(end, text.size());
    if (const TextMeasurer* measurer = textMeasurer()) {
        try {
            return std::max(0.0f, measurer->measureText(text.substr(0, end), fontSize).width);
        } catch (...) {
            // Text editing must remain usable if an optional platform
            // measurer becomes unavailable while a window is closing.
        }
    }
    return static_cast<float>(end) * (fontSize * 0.56f);
}

[[nodiscard]] float inputHeight(InputSize size) noexcept
{
    switch (size) {
    case InputSize::Small: return 24.0f;
    case InputSize::Large: return 40.0f;
    case InputSize::Medium: default: return 32.0f;
    }
}

struct EditableLine {
    std::size_t start{0};
    std::size_t length{0};
    float width{0.0f};
};

[[nodiscard]] float editableLineHeight(const Theme& current) noexcept
{
    return current.typography.body1.lineHeight;
}

// Ask the platform shaper for exactly the same wrap decisions it uses for
// normal Text nodes. The conservative fallback keeps headless tests and hosts
// without a layout provider fully editable (the editing controller itself has
// byte-offset semantics today, so fallback boundaries intentionally match it).
[[nodiscard]] std::vector<EditableLine> editableLines(const std::string& text, float fontSize,
                                                       float availableWidth, float lineHeight,
                                                       bool multiline)
{
    if (!multiline) {
        return {{0, text.size(), measuredTextWidth(text, text.size(), fontSize)}};
    }
    if (const auto* provider = dynamic_cast<const TextLayoutProvider*>(textMeasurer())) {
        try {
            const auto shaped = provider->layoutText(text, fontSize, availableWidth, lineHeight, 0, false);
            if (!shaped.empty()) {
                std::vector<EditableLine> result;
                result.reserve(shaped.size() + 1);
                for (const auto& line : shaped) result.push_back({line.sourceStart, line.sourceLength, line.width});
                // Most layout engines omit the empty visual line after a
                // terminating newline; an editor must still expose its caret.
                if (!text.empty() && text.back() == '\n') result.push_back({text.size(), 0, 0.0f});
                return result;
            }
        } catch (...) {
            // A native backend can be torn down while a window is closing.
        }
    }

    std::vector<EditableLine> result;
    const float widthLimit = std::max(1.0f, availableWidth);
    std::size_t lineStart = 0;
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        if (text[cursor] == '\n') {
            result.push_back({lineStart, cursor - lineStart,
                              measuredTextWidth(text.substr(lineStart, cursor - lineStart), cursor - lineStart, fontSize)});
            lineStart = ++cursor;
            continue;
        }
        const std::size_t next = cursor + 1;
        const float candidate = measuredTextWidth(text.substr(lineStart, next - lineStart), next - lineStart, fontSize);
        if (cursor > lineStart && candidate > widthLimit) {
            const auto length = cursor - lineStart;
            result.push_back({lineStart, length, measuredTextWidth(text.substr(lineStart, length), length, fontSize)});
            lineStart = cursor;
            continue;
        }
        cursor = next;
    }
    const auto length = text.size() - lineStart;
    result.push_back({lineStart, length, measuredTextWidth(text.substr(lineStart, length), length, fontSize)});
    return result;
}

[[nodiscard]] std::size_t lineIndexForOffset(const std::vector<EditableLine>& lines, std::size_t offset) noexcept
{
    if (lines.empty()) return 0;
    for (std::size_t index = 0; index + 1 < lines.size(); ++index) {
        if (offset <= lines[index].start + lines[index].length) return index;
    }
    return lines.size() - 1;
}

[[nodiscard]] float maximumVerticalOffset(const std::vector<EditableLine>& lines,
                                          float lineHeight, float viewportHeight) noexcept
{
    return std::max(0.0f, lineHeight * static_cast<float>(lines.size()) - viewportHeight);
}

// Reveal the complete visual caret line while retaining a user's wheel
// position whenever the caret is already visible. This makes keyboard edits
// stable (no needless snapping) and handles movement in both directions.
[[nodiscard]] float revealCaretLine(float currentOffset, std::size_t lineIndex,
                                    float lineHeight, float viewportHeight,
                                    float maximumOffset) noexcept
{
    float next = std::clamp(currentOffset, 0.0f, maximumOffset);
    const float caretTop = lineHeight * static_cast<float>(lineIndex);
    const float caretBottom = caretTop + lineHeight;
    if (caretTop < next) {
        next = caretTop;
    } else if (caretBottom > next + viewportHeight) {
        next = caretBottom - viewportHeight;
    }
    return std::clamp(next, 0.0f, maximumOffset);
}

[[nodiscard]] float linePrefixWidth(const std::string& text, const EditableLine& line,
                                    std::size_t offset, float fontSize) noexcept
{
    const auto end = std::clamp(offset, line.start, line.start + line.length);
    const auto length = end - line.start;
    return measuredTextWidth(text.substr(line.start, length), length, fontSize);
}

// This is a single-line field. Keep the active caret inside the viewport so
// long text remains editable without leaking into neighbouring controls.
[[nodiscard]] float horizontalTextOffset(float caretX, float viewportWidth) noexcept
{
    if (viewportWidth <= 1.0f) return caretX;
    return std::max(0.0f, caretX - (viewportWidth - 1.0f));
}

[[nodiscard]] std::size_t nearestTextOffset(const std::string& text, float targetX,
                                             float fontSize) noexcept
{
    if (targetX <= 0.0f || text.empty()) return 0;
    const float fullWidth = measuredTextWidth(text, text.size(), fontSize);
    if (targetX >= fullWidth) return text.size();

    // Prefix measurements preserve the shaping and fallback decisions of the
    // renderer. Binary search keeps pointer-drag selection inexpensive even
    // for long task titles.
    std::size_t firstAtOrAfter = 0;
    std::size_t low = 0;
    std::size_t high = text.size();
    while (low < high) {
        const std::size_t middle = low + (high - low) / 2;
        if (measuredTextWidth(text, middle, fontSize) < targetX) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    firstAtOrAfter = low;
    if (firstAtOrAfter == 0) return 0;

    const float before = measuredTextWidth(text, firstAtOrAfter - 1, fontSize);
    const float after = measuredTextWidth(text, firstAtOrAfter, fontSize);
    return targetX - before <= after - targetX ? firstAtOrAfter - 1 : firstAtOrAfter;
}

} // namespace

const EditingValue& TextEditingController::value() const noexcept
{
    return value_;
}

void TextEditingController::setValue(EditingValue value)
{
    value_ = std::move(value);
    normalize();
    selectionAnchor_ = value_.selection.start;
}

const std::string& TextEditingController::text() const noexcept
{
    return value_.text;
}

void TextEditingController::setText(std::string text)
{
    value_.text = std::move(text);
    normalize();
    selectionAnchor_ = value_.selection.start;
}

const TextRange& TextEditingController::selection() const noexcept
{
    return value_.selection;
}

void TextEditingController::setSelection(TextRange selection) noexcept
{
    value_.selection = normalizeRange(selection, value_.text.size());
    selectionAnchor_ = value_.selection.start;
}

const TextRange& TextEditingController::composition() const noexcept
{
    return value_.composition;
}

void TextEditingController::setComposition(TextRange composition) noexcept
{
    value_.composition = normalizeRange(composition, value_.text.size());
}

void TextEditingController::clearComposition() noexcept
{
    // updateComposition mirrors the native pre-edit range into selection so
    // IME and accessibility clients can query the active text. Once the IME
    // explicitly ends, that range is no longer a user selection. Collapse it
    // at the pre-edit end so clearing the underline cannot immediately turn
    // into a selection-highlight flash on the same characters.
    if (!value_.composition.empty()
        && value_.selection.start == value_.composition.start
        && value_.selection.end == value_.composition.end) {
        value_.selection = {value_.composition.end, value_.composition.end};
        selectionAnchor_ = value_.composition.end;
    }
    value_.composition = {};
}

void TextEditingController::updateComposition(std::string text)
{
    const auto activeRange = value_.composition.empty()
        ? normalizeRange(value_.selection, value_.text.size())
        : normalizeRange(value_.composition, value_.text.size());

    value_.text.replace(activeRange.start, activeRange.end - activeRange.start, text);
    const auto end = activeRange.start + text.size();
    value_.composition = TextRange{activeRange.start, end};
    value_.selection = value_.composition;
    selectionAnchor_ = activeRange.start;
}

void TextEditingController::collapseOrExtend(std::size_t position, bool extendSelection) noexcept
{
    position = std::min(position, value_.text.size());
    if (!extendSelection) {
        value_.selection = {position, position};
        selectionAnchor_ = position;
        return;
    }
    value_.selection = normalizeRange({selectionAnchor_, position}, value_.text.size());
}

void TextEditingController::setCaret(std::size_t position, bool extendSelection) noexcept
{
    collapseOrExtend(position, extendSelection);
}

void TextEditingController::moveCaret(int delta, bool extendSelection) noexcept
{
    value_.selection = normalizeRange(value_.selection, value_.text.size());
    std::size_t caret = value_.selection.end;
    if (!extendSelection && !value_.selection.empty()) {
        caret = delta < 0 ? value_.selection.start : value_.selection.end;
    }
    const auto next = delta < 0
        ? caret - static_cast<std::size_t>(std::min<std::size_t>(caret, static_cast<std::size_t>(-delta)))
        : std::min(value_.text.size(), caret + static_cast<std::size_t>(delta));
    if (extendSelection) {
        if (value_.selection.empty()) {
            selectionAnchor_ = caret;
        }
        collapseOrExtend(next, true);
        return;
    }
    collapseOrExtend(next, false);
}

void TextEditingController::moveToStart(bool extendSelection) noexcept
{
    if (extendSelection && value_.selection.empty()) {
        selectionAnchor_ = value_.selection.end;
    }
    collapseOrExtend(0, extendSelection);
}

void TextEditingController::moveToEnd(bool extendSelection) noexcept
{
    if (extendSelection && value_.selection.empty()) {
        selectionAnchor_ = value_.selection.end;
    }
    collapseOrExtend(value_.text.size(), extendSelection);
}

void TextEditingController::selectAll() noexcept
{
    value_.selection = {0, value_.text.size()};
    selectionAnchor_ = 0;
}

void TextEditingController::backspace(bool byWord)
{
    value_.selection = normalizeRange(value_.selection, value_.text.size());
    if (!value_.selection.empty()) {
        replaceSelection("");
        return;
    }
    if (value_.selection.start == 0) {
        return;
    }
    const auto start = byWord ? previousWordBoundary(value_.selection.start) : value_.selection.start - 1;
    replaceRange({start, value_.selection.start}, "", true);
}

void TextEditingController::deleteForward(bool byWord)
{
    value_.selection = normalizeRange(value_.selection, value_.text.size());
    if (!value_.selection.empty()) {
        replaceSelection("");
        return;
    }
    if (value_.selection.end == value_.text.size()) {
        return;
    }
    const auto end = byWord ? nextWordBoundary(value_.selection.end) : value_.selection.end + 1;
    replaceRange({value_.selection.end, end}, "", true);
}

void TextEditingController::replaceSelection(std::string text)
{
    replaceRange(value_.selection, text, true);
}

void TextEditingController::commit(std::string text)
{
    replaceRange(value_.composition.empty() ? value_.selection : value_.composition, text, true);
}

std::string TextEditingController::selectedText() const
{
    const auto selected = normalizeRange(value_.selection, value_.text.size());
    return value_.text.substr(selected.start, selected.end - selected.start);
}

bool TextEditingController::undo()
{
    if (undoStack_.empty()) {
        return false;
    }
    redoStack_.push_back(value_);
    value_ = std::move(undoStack_.back());
    undoStack_.pop_back();
    normalize();
    selectionAnchor_ = value_.selection.start;
    return true;
}

bool TextEditingController::redo()
{
    if (redoStack_.empty()) {
        return false;
    }
    undoStack_.push_back(value_);
    value_ = std::move(redoStack_.back());
    redoStack_.pop_back();
    normalize();
    selectionAnchor_ = value_.selection.start;
    return true;
}

void TextEditingController::rememberForUndo()
{
    undoStack_.push_back(value_);
    if (undoStack_.size() > kMaximumUndoEntries) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

void TextEditingController::replaceRange(TextRange range, std::string_view text, bool saveHistory)
{
    range = normalizeRange(range, value_.text.size());
    if (saveHistory) {
        rememberForUndo();
    }
    value_.text.replace(range.start, range.end - range.start, text.data(), text.size());
    const auto caret = range.start + text.size();
    value_.selection = {caret, caret};
    value_.composition = {};
    selectionAnchor_ = caret;
}

std::size_t TextEditingController::previousWordBoundary(std::size_t position) const noexcept
{
    while (position > 0 && !isWordCharacter(value_.text[position - 1])) {
        --position;
    }
    while (position > 0 && isWordCharacter(value_.text[position - 1])) {
        --position;
    }
    return position;
}

std::size_t TextEditingController::nextWordBoundary(std::size_t position) const noexcept
{
    while (position < value_.text.size() && !isWordCharacter(value_.text[position])) {
        ++position;
    }
    while (position < value_.text.size() && isWordCharacter(value_.text[position])) {
        ++position;
    }
    return position;
}

void TextEditingController::normalize() noexcept
{
    value_.selection = normalizeRange(value_.selection, value_.text.size());
    value_.composition = normalizeRange(value_.composition, value_.text.size());
}

TextInput::TextInput(std::string placeholder)
    : placeholder_(std::move(placeholder))
{
}

TextInput::~TextInput()
{
    stopCaretBlinkAnimation();
    stopFocusIndicatorAnimation();
}

void TextInput::ensureCaretBlinkAnimation()
{
    if (caretBlinkAnimation_ != 0 || !isEnabled()) return;
    // Windows' text caret is visible for the first half of each one-second
    // cycle. Keep this frame-driven so idle windows sleep normally and widget
    // destruction can synchronously cancel the callback.
    Animation blink(1.0f, [this](float phase) {
        const bool visible = phase < 0.5f;
        if (caretVisible_ == visible) return;
        caretVisible_ = visible;
        markDirty(DirtyFlag::Paint);
    });
    blink.repeat(-1);
    caretBlinkAnimation_ = Ticker::instance().add(std::move(blink));
}

void TextInput::stopCaretBlinkAnimation() noexcept
{
    if (caretBlinkAnimation_ == 0) return;
    Ticker::instance().cancel(caretBlinkAnimation_);
    caretBlinkAnimation_ = 0;
}

void TextInput::resetCaretBlink()
{
    stopCaretBlinkAnimation();
    caretVisible_ = true;
}

void TextInput::syncFocusIndicatorAnimation(bool active)
{
    if (focusIndicatorTargetActive_ == active) return;
    focusIndicatorTargetActive_ = active;
    stopFocusIndicatorAnimation();

    const float target = active ? 1.0f : 0.0f;
    if (!motionEnabled_) {
        focusIndicatorProgress_ = target;
        markDirty(DirtyFlag::Paint);
        return;
    }

    const float start = focusIndicatorProgress_;
    const float distance = std::abs(target - start);
    if (distance <= 0.0001f) {
        focusIndicatorProgress_ = target;
        return;
    }
    const auto& current = theme();
    const float fullDuration = active ? current.motion.durationNormal
                                      : current.motion.durationUltraFast;
    Animation animation(
        std::max(0.001f, fullDuration * distance),
        [this, start, target](float progress) {
            focusIndicatorProgress_ = start + (target - start) * progress;
            markDirty(DirtyFlag::Paint);
        },
        active ? easing::easeOutCubic : easing::easeInCubic);
    animation.onComplete([this, target] {
        focusIndicatorProgress_ = target;
        focusIndicatorAnimation_ = 0;
        markDirty(DirtyFlag::Paint);
    });
    focusIndicatorAnimation_ = Ticker::instance().add(std::move(animation));
}

void TextInput::stopFocusIndicatorAnimation() noexcept
{
    if (focusIndicatorAnimation_ == 0) return;
    Ticker::instance().cancel(focusIndicatorAnimation_);
    focusIndicatorAnimation_ = 0;
}

void TextInput::onDetach() noexcept
{
    stopCaretBlinkAnimation();
    stopFocusIndicatorAnimation();
}

TextEditingController& TextInput::controller() noexcept
{
    return controller_;
}

const TextEditingController& TextInput::controller() const noexcept
{
    return controller_;
}

TextInputModel& TextInput::model() noexcept
{
    return controller_;
}

const TextInputModel& TextInput::model() const noexcept
{
    return controller_;
}

const std::string& TextInput::placeholder() const noexcept
{
    return placeholder_;
}

TextInput& TextInput::placeholder(std::string placeholder)
{
    setPlaceholder(std::move(placeholder));
    return *this;
}

void TextInput::setPlaceholder(std::string placeholder)
{
    placeholder_ = std::move(placeholder);
    markDirty(DirtyFlag::Paint);
}

TextInput& TextInput::text(std::string text)
{
    controller_.setText(std::move(text));
    resetCaretBlink();
    revealCaretPending_ = true;
    markDirty(DirtyFlag::Layout);
    notifyChanged();
    return *this;
}

TextInput& TextInput::accessibleLabel(std::string label)
{
    setAccessibleLabel(std::move(label));
    return *this;
}

const std::string& TextInput::accessibleLabel() const noexcept { return accessibleLabel_; }
void TextInput::setAccessibleLabel(std::string label) { accessibleLabel_ = std::move(label); }
void TextInput::setSize(InputSize size) noexcept { if (size_ != size) { size_ = size; markDirty(DirtyFlag::Layout); } }
InputSize TextInput::size() const noexcept { return size_; }
void TextInput::setAppearance(InputAppearance appearance) noexcept { if (appearance_ != appearance) { appearance_ = appearance; markDirty(DirtyFlag::Paint); } }
InputAppearance TextInput::appearance() const noexcept { return appearance_; }
void TextInput::setInvalid(bool invalid) noexcept { if (invalid_ != invalid) { invalid_ = invalid; markDirty(DirtyFlag::Paint); } }
bool TextInput::isInvalid() const noexcept { return invalid_; }
void TextInput::setMotionEnabled(bool enabled) noexcept
{
    if (motionEnabled_ == enabled) return;
    motionEnabled_ = enabled;
    stopFocusIndicatorAnimation();
    focusIndicatorProgress_ = focusIndicatorTargetActive_ ? 1.0f : 0.0f;
    markDirty(DirtyFlag::Paint);
}
bool TextInput::isMotionEnabled() const noexcept { return motionEnabled_; }
void TextInput::setMultiline(bool value) noexcept
{
    if (multiline_ == value) return;
    multiline_ = value;
    minimumLines_ = value ? std::max<std::size_t>(minimumLines_, 1) : 1;
    verticalScrollOffset_ = 0.0f;
    revealCaretPending_ = true;
    markDirty(DirtyFlag::Layout);
}
bool TextInput::isMultiline() const noexcept { return multiline_; }
void TextInput::setMinimumLines(std::size_t value) noexcept
{
    const auto normalized = std::max<std::size_t>(1, value);
    if (minimumLines_ == normalized) return;
    minimumLines_ = normalized;
    revealCaretPending_ = true;
    markDirty(DirtyFlag::Layout);
}
std::size_t TextInput::minimumLines() const noexcept { return minimumLines_; }
float TextInput::verticalScrollOffset() const noexcept { return verticalScrollOffset_; }
float TextInput::maximumVerticalScrollOffset() const noexcept
{
    if (!multiline_) return 0.0f;
    const auto& current = theme();
    const RectF viewport =
        textViewport(bounds(), current, true, size_, multiline_);
    const float lineHeight = editableLineHeight(current);
    const auto lines = editableLines(controller_.text(), current.typography.body1.size,
                                     viewport.width, lineHeight, true);
    return maximumVerticalOffset(lines, lineHeight, viewport.height);
}

TextInput& TextInput::onChange(ChangeHandler handler)
{
    onChange_ = std::move(handler);
    return *this;
}

TextInput& TextInput::onSubmit(SubmitHandler handler)
{
    onSubmit_ = std::move(handler);
    return *this;
}

TextInput& TextInput::onCancel(CancelHandler handler)
{
    onCancel_ = std::move(handler);
    return *this;
}

void TextInput::notifyChanged()
{
    resetCaretBlink();
    if (onChange_) {
        onChange_(controller_.text());
    }
}

void TextInput::syncSession(TextInputSession& session, const RectF& caretRect) const
{
    session.setCaretRect(caretRect);
    session.setSurroundingText(controller_.text(), controller_.selection().start, controller_.selection().end);
}

RectF TextInput::caretRect() const noexcept
{
    const auto& current = theme();
    const bool focused = (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    const RectF viewport =
        textViewport(bounds(), current, focused, size_, multiline_);
    const auto lines = editableLines(controller_.text(), current.typography.body1.size, viewport.width,
                                     editableLineHeight(current), multiline_);
    const auto lineIndex = lineIndexForOffset(lines, controller_.selection().end);
    const EditableLine line = lines.empty() ? EditableLine{} : lines[lineIndex];
    const float caretX = linePrefixWidth(controller_.text(), line, controller_.selection().end,
                                         current.typography.body1.size);
    const float horizontalOffset = multiline_ ? 0.0f : horizontalTextOffset(caretX, viewport.width);
    const float lineHeight = editableLineHeight(current);
    float verticalOffset = 0.0f;
    if (multiline_) {
        const float maximum = maximumVerticalOffset(lines, lineHeight, viewport.height);
        verticalOffset = std::clamp(verticalScrollOffset_, 0.0f, maximum);
        if (revealCaretPending_ || !hasPaintedCaret_
            || lastPaintedCaret_ != controller_.selection().end) {
            verticalOffset = revealCaretLine(verticalOffset, lineIndex, lineHeight,
                                             viewport.height, maximum);
        }
    }
    const float caretTop = multiline_
        ? viewport.y + lineHeight * static_cast<float>(lineIndex) - verticalOffset
        : viewport.y + std::max(0.0f, (viewport.height - lineHeight) * 0.5f);
    return {viewport.x + caretX - horizontalOffset, caretTop,
            current.stroke.thin, std::max(1.0f, lineHeight)};
}

SizeF TextInput::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const float horizontalPadding =
        horizontalTextPadding(current, size_, multiline_);
    const float verticalPadding =
        verticalTextPadding(current, size_, multiline_);
    const float available = std::isfinite(constraints.maxWidth)
        ? std::max(1.0f, constraints.maxWidth - horizontalPadding * 2.0f)
        : 4096.0f;
    const auto lines = editableLines(controller_.text(), current.typography.body1.size, available,
                                     editableLineHeight(current), multiline_);
    float widest = measuredTextWidth(placeholder_, placeholder_.size(), current.typography.body1.size);
    for (const auto& line : lines) widest = std::max(widest, line.width);
    const float lineHeight = editableLineHeight(current);
    const float textHeight = multiline_
        ? lineHeight * static_cast<float>(std::max(minimumLines_, lines.size()))
        : inputHeight(size_);
    const float height = multiline_
        ? std::max(inputHeight(size_), textHeight + verticalPadding * 2.0f)
        : inputHeight(size_);
    return constraints.clamp({widest + horizontalPadding * 2.0f, height});
}

void TextInput::paint(PaintContext& context)
{
    const bool showPlaceholder = controller_.text().empty();
    const auto& text = showPlaceholder ? placeholder_ : controller_.text();

    const auto& current = theme();
    const bool focused = (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    const bool enabled = isEnabled();
    const bool hovered =
        (visualStates() & toMask(ControlVisualState::Hovered)) != 0;
    const bool pressed =
        (visualStates() & toMask(ControlVisualState::Pressed)) != 0;
    const float scale = std::max(1.0f, context.scaleFactor());
    const float outlineStrokeWidth =
        snapThicknessToPhysicalPixels(current.stroke.thin, scale);
    syncFocusIndicatorAnimation(focused && enabled);
    if (focused && enabled) {
        ensureCaretBlinkAnimation();
    } else {
        stopCaretBlinkAnimation();
        caretVisible_ = true;
    }
    const bool filled = appearance_ == InputAppearance::FilledDarker ||
                        appearance_ == InputAppearance::FilledLighter;
    Color outerStroke = current.colors.neutralStroke1;
    Color bottomStroke = current.colors.neutralStrokeAccessible;
    if (!enabled) {
        outerStroke = current.colors.neutralStrokeDisabled;
        bottomStroke = current.colors.neutralStrokeDisabled;
    } else if (invalid_ && !focused) {
        outerStroke = current.colors.statusDanger;
        bottomStroke = current.colors.statusDanger;
    } else if (focused || pressed) {
        outerStroke = current.colors.neutralStroke1Pressed;
        bottomStroke = current.colors.neutralStrokeAccessiblePressed;
    } else if (hovered) {
        outerStroke = current.colors.neutralStroke1Hover;
        bottomStroke = current.colors.neutralStrokeAccessibleHover;
    }
    const Color surface = !enabled ? Color{0, 0, 0, 0}
                        : appearance_ == InputAppearance::FilledDarker ? current.colors.neutralBackground3.rest
                        : appearance_ == InputAppearance::FilledLighter ? current.colors.neutralBackground1.rest
                        : appearance_ == InputAppearance::Underline ? Color{0, 0, 0, 0}
                        : current.colors.neutralBackground1.rest;

    if (appearance_ == InputAppearance::Outline || !enabled ||
        (invalid_ && !focused)) {
        context.fillStrokeRoundRect(bounds(), current.radius.medium,
                                    outlineStrokeWidth, surface, outerStroke);
    } else if (filled && surface.a != 0) {
        context.fillRoundRect(bounds(), current.radius.medium, surface);
    }
    if (!filled || !enabled || (invalid_ && !focused)) {
        drawBottomStroke(context, bounds(),
                         appearance_ == InputAppearance::Underline
                             ? current.radius.none
                             : current.radius.medium,
                         current.stroke.thin, bottomStroke);
    }
    // Fluent's focus-within pseudo-element scales from the field centre. The
    // neutral accessible bottom border remains beneath it while the 2-DIP
    // compound-brand indicator expands over 200 ms.
    if (enabled && focusIndicatorProgress_ > 0.0f) {
        const Color indicator = pressed
            ? current.colors.compoundBrandStroke.pressed
            : current.colors.compoundBrandStroke.rest;
        drawBottomStroke(context, bounds(),
                         appearance_ == InputAppearance::Underline
                             ? current.radius.none
                             : current.radius.medium,
                         current.stroke.thick, indicator,
                         focusIndicatorProgress_);
    }
    const auto selection = controller_.selection();
    const auto composition = controller_.composition();
    const RectF viewport =
        textViewport(bounds(), current, focused, size_, multiline_);
    const float lineHeight = editableLineHeight(current);
    const auto lines = editableLines(controller_.text(), current.typography.body1.size, viewport.width,
                                     lineHeight, multiline_);
    const auto caretLineIndex = lineIndexForOffset(lines, selection.end);
    const EditableLine caretLine = lines.empty() ? EditableLine{} : lines[caretLineIndex];
    const float selectionEndX = linePrefixWidth(controller_.text(), caretLine, selection.end,
                                                current.typography.body1.size);
    const float horizontalOffset = (!multiline_ && !showPlaceholder)
        ? horizontalTextOffset(selectionEndX, viewport.width) : 0.0f;
    float verticalOffset = 0.0f;
    if (multiline_) {
        const float maximum = maximumVerticalOffset(lines, lineHeight, viewport.height);
        verticalOffset = std::clamp(verticalScrollOffset_, 0.0f, maximum);
        if (revealCaretPending_ || !hasPaintedCaret_
            || lastPaintedCaret_ != selection.end) {
            verticalOffset = revealCaretLine(verticalOffset, caretLineIndex, lineHeight,
                                             viewport.height, maximum);
        }
        verticalScrollOffset_ = verticalOffset;
    }
    const int checkpoint = context.save();
    context.clipRect(viewport);
    // Win32 pre-edit text carries a selection covering its composition span.
    // That is editing state, not a user selection: rendering it with the
    // normal translucent selection fill makes active IME input look selected
    // and hides the conventional composition affordance. Paint an underline
    // for the exact pre-edit range instead; a genuine selection remains
    // visible while no composition is active.
    const bool selectionIsComposition = !composition.empty()
        && selection.start == composition.start
        && selection.end == composition.end;
    const auto selectionColor = Color{current.colors.brandForeground1.r, current.colors.brandForeground1.g,
                                      current.colors.brandForeground1.b, 72};
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto& line = lines[index];
        const float lineY = multiline_
            ? viewport.y + lineHeight * static_cast<float>(index) - verticalOffset
            : viewport.y + std::max(0.0f, (viewport.height - lineHeight) * 0.5f);
        const RectF lineBox{viewport.x, lineY, viewport.width, lineHeight};
        const auto lineEnd = line.start + line.length;
        if (focused && enabled && !selection.empty() && !selectionIsComposition) {
            const auto selectedStart = std::max(selection.start, line.start);
            const auto selectedEnd = std::min(selection.end, lineEnd);
            if (selectedEnd > selectedStart) {
                const float startX = linePrefixWidth(controller_.text(), line, selectedStart,
                                                     current.typography.body1.size);
                const float endX = linePrefixWidth(controller_.text(), line, selectedEnd,
                                                   current.typography.body1.size);
                context.fillRect({viewport.x + startX, multiline_ ? lineY : viewport.y,
                                  std::max(1.0f, endX - startX), multiline_ ? lineHeight : viewport.height}, selectionColor);
            }
        }
        if (!showPlaceholder && line.length > 0) {
            const std::string lineText = controller_.text().substr(line.start, line.length);
            context.drawText(lineText, viewport.x,
                             context.centeredTextBottom(lineText, lineBox,
                                                        current.typography.body1.size,
                                                        current.typography.body1.weight,
                                                        current.typography.body1.family),
                             current.typography.body1.size,
                             enabled ? current.colors.text : current.colors.textDisabled,
                             current.typography.body1.weight,
                             current.typography.body1.family);
        }
        if (focused && enabled && !composition.empty()) {
            const auto compositionStart = std::max(composition.start, line.start);
            const auto compositionEnd = std::min(composition.end, lineEnd);
            if (compositionEnd > compositionStart) {
                const float startX = linePrefixWidth(controller_.text(), line, compositionStart,
                                                     current.typography.body1.size);
                const float endX = linePrefixWidth(controller_.text(), line, compositionEnd,
                                                   current.typography.body1.size);
                const float physicalPixel = 1.0f / scale;
                const float underlineLeft =
                    snapToPhysicalPixel(viewport.x + startX, scale);
                const float underlineRight =
                    snapToPhysicalPixel(viewport.x + endX, scale);
                const float underlineY = snapToPhysicalPixel(
                    context.centeredTextBottom(
                        "Ag", lineBox, current.typography.body1.size,
                        current.typography.body1.weight,
                        current.typography.body1.family) +
                        current.controls.focusInset,
                    scale);
                context.fillRect(
                    {underlineLeft, underlineY,
                     std::max(physicalPixel,
                              underlineRight - underlineLeft),
                     physicalPixel},
                    current.colors.brandForeground1);
            }
        }
    }
    if (showPlaceholder && !placeholder_.empty()) {
        const RectF placeholderBox{viewport.x, multiline_ ? viewport.y
            : viewport.y + std::max(0.0f, (viewport.height - lineHeight) * 0.5f), viewport.width, lineHeight};
        context.drawText(placeholder_, viewport.x, context.centeredTextBottom(placeholder_, placeholderBox,
                         current.typography.body1.size, current.typography.body1.weight,
                         current.typography.body1.family), current.typography.body1.size,
                         enabled ? current.colors.textMuted : current.colors.textDisabled,
                         current.typography.body1.weight, current.typography.body1.family);
    }
    if (focused && enabled && selection.empty() && caretVisible_) {
        const float caretX = viewport.x + selectionEndX - horizontalOffset;
        const float caretY = multiline_
            ? viewport.y + lineHeight * static_cast<float>(caretLineIndex) - verticalOffset
            : viewport.y + std::max(0.0f, (viewport.height - lineHeight) * 0.5f);
        const float physicalPixel = 1.0f / scale;
        const float snappedX = snapToPhysicalPixel(caretX, scale);
        const float snappedTop = snapToPhysicalPixel(caretY, scale);
        const float snappedBottom =
            snapToPhysicalPixel(caretY + lineHeight, scale);
        context.fillRect({snappedX, snappedTop, physicalPixel,
                          std::max(physicalPixel, snappedBottom - snappedTop)},
                         current.colors.brandForeground1);
    }
    context.restoreTo(checkpoint);
    revealCaretPending_ = false;
    hasPaintedCaret_ = true;
    lastPaintedCaret_ = selection.end;
    clearDirty(DirtyFlag::Paint);
}

std::size_t TextInput::caretAt(PointF point) const noexcept
{
    const bool focused = (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    const RectF viewport =
        textViewport(bounds(), theme(), focused, size_, multiline_);
    const float lineHeight = editableLineHeight(theme());
    const auto lines = editableLines(controller_.text(), theme().typography.body1.size, viewport.width,
                                     lineHeight, multiline_);
    if (lines.empty()) return 0;
    const float verticalOffset = multiline_
        ? std::clamp(verticalScrollOffset_, 0.0f,
                     maximumVerticalOffset(lines, lineHeight, viewport.height))
        : 0.0f;
    const auto localLine = multiline_
        ? static_cast<std::size_t>(std::max(0.0f, std::floor((point.y - viewport.y + verticalOffset) / lineHeight)))
        : 0u;
    const auto lineIndex = std::min(localLine, lines.size() - 1);
    const auto& line = lines[lineIndex];
    const float existingCaretX = linePrefixWidth(controller_.text(), line, controller_.selection().end,
                                                 theme().typography.body1.size);
    const float horizontalOffset = multiline_ ? 0.0f : horizontalTextOffset(existingCaretX, viewport.width);
    const float localX = std::max(0.0f, point.x - viewport.x + horizontalOffset);
    const auto lineText = controller_.text().substr(line.start, line.length);
    return line.start + nearestTextOffset(lineText, localX, theme().typography.body1.size);
}

EventResult TextInput::onPointerEvent(const PointerEvent& event, EventContext& context)
{
    if (!isEnabled()) return EventResult::Ignored;
    if (context.phase() == EventPhase::Capture) {
        return EventResult::Ignored;
    }
    switch (event.action) {
    case PointerAction::Enter:
        setVisualState(ControlVisualState::Hovered, true);
        return EventResult::Handled;
    case PointerAction::Down:
        if (event.button != MouseButton::Left) {
            return EventResult::Ignored;
        }
        selectingWithPointer_ = true;
        setVisualState(ControlVisualState::Pressed, true);
        resetCaretBlink();
        controller_.setCaret(caretAt(event.position), (event.modifiers & KeyModifierShift) != 0);
        revealCaretPending_ = true;
        setVisualState(ControlVisualState::Focused, true);
        markDirty(DirtyFlag::Paint);
        context.requestFocus();
        context.capturePointer();
        return EventResult::Handled;
    case PointerAction::Move:
        setVisualState(ControlVisualState::Hovered,
                       bounds().contains(event.position));
        if (!selectingWithPointer_) {
            return EventResult::Handled;
        }
        controller_.setCaret(caretAt(event.position), true);
        resetCaretBlink();
        revealCaretPending_ = true;
        markDirty(DirtyFlag::Paint);
        return EventResult::Handled;
    case PointerAction::Scroll:
        if (!multiline_) return EventResult::Ignored;
        {
            const float lineHeight = editableLineHeight(theme());
            const RectF viewport =
                textViewport(bounds(), theme(), true, size_, multiline_);
            const auto lines = editableLines(controller_.text(), theme().typography.body1.size,
                                             viewport.width, lineHeight, true);
            const float maximum = maximumVerticalOffset(lines, lineHeight, viewport.height);
            const float previous = verticalScrollOffset_;
            const float next = std::clamp(verticalScrollOffset_ - event.scrollDelta.y, 0.0f, maximum);
            verticalScrollOffset_ = next;
            revealCaretPending_ = false;
            context.setRemainingScrollDelta({event.scrollDelta.x,
                                             event.scrollDelta.y + next - previous});
            if (next == previous) return EventResult::Ignored;
            markDirty(DirtyFlag::Paint);
            return EventResult::Handled;
        }
    case PointerAction::Up:
    case PointerAction::Cancel:
        setVisualState(ControlVisualState::Pressed, false);
        if (!selectingWithPointer_) {
            return EventResult::Handled;
        }
        if (event.action == PointerAction::Up) {
            controller_.setCaret(caretAt(event.position), true);
            resetCaretBlink();
            revealCaretPending_ = true;
        }
        selectingWithPointer_ = false;
        markDirty(DirtyFlag::Paint);
        context.releasePointer();
        return EventResult::Handled;
    case PointerAction::Leave:
        setVisualState(ControlVisualState::Hovered, false);
        if (!selectingWithPointer_) {
            setVisualState(ControlVisualState::Pressed, false);
        }
        return EventResult::Handled;
    default:
        return EventResult::Ignored;
    }
}

bool TextInput::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled()) return false;
    if (event.action != KeyAction::Down) {
        return false;
    }

    // Any accepted keyboard editing/navigation action restarts the visible
    // caret phase. Cases that return false do not leave a running blink reset.
    const auto restartCaret = [this]() { resetCaretBlink(); };

    const bool extendSelection = (event.modifiers & KeyModifierShift) != 0;
    const bool byWord = (event.modifiers & KeyModifierControl) != 0;
    const auto moveToLineBoundary = [this, extendSelection](bool end) {
        if (!multiline_) {
            if (end) controller_.moveToEnd(extendSelection); else controller_.moveToStart(extendSelection);
            return;
        }
        const auto& current = theme();
        const RectF viewport =
            textViewport(bounds(), current, true, size_, multiline_);
        const auto lines = editableLines(controller_.text(), current.typography.body1.size, viewport.width,
                                         editableLineHeight(current), true);
        if (lines.empty()) return;
        const auto index = lineIndexForOffset(lines, controller_.selection().end);
        const auto& line = lines[index];
        controller_.setCaret(end ? line.start + line.length : line.start, extendSelection);
    };
    const auto moveVertically = [this, extendSelection](int direction) {
        const auto& current = theme();
        const RectF viewport =
            textViewport(bounds(), current, true, size_, multiline_);
        const auto lines = editableLines(controller_.text(), current.typography.body1.size, viewport.width,
                                         editableLineHeight(current), true);
        if (lines.empty()) return;
        const auto currentIndex = lineIndexForOffset(lines, controller_.selection().end);
        const auto targetIndex = direction < 0
            ? currentIndex - std::min<std::size_t>(currentIndex, 1)
            : std::min(lines.size() - 1, currentIndex + 1);
        const float x = linePrefixWidth(controller_.text(), lines[currentIndex], controller_.selection().end,
                                        current.typography.body1.size);
        const auto lineText = controller_.text().substr(lines[targetIndex].start, lines[targetIndex].length);
        controller_.setCaret(lines[targetIndex].start
                                 + nearestTextOffset(lineText, x, current.typography.body1.size),
                             extendSelection);
    };
    switch (event.keyCode) {
    case 8:
    case 259: // GLFW_KEY_BACKSPACE
        controller_.backspace(byWord);
        restartCaret();
        revealCaretPending_ = true;
        markDirty(DirtyFlag::Layout);
        notifyChanged();
        return true;
    case 46:
    case 261: // GLFW_KEY_DELETE
        controller_.deleteForward(byWord);
        restartCaret();
        revealCaretPending_ = true;
        markDirty(DirtyFlag::Layout);
        notifyChanged();
        return true;
    case 36:
    case 268: // GLFW_KEY_HOME
        moveToLineBoundary(false);
        restartCaret();
        revealCaretPending_ = true;
        markDirty(DirtyFlag::Paint);
        return true;
    case 35:
    case 269: // GLFW_KEY_END
        moveToLineBoundary(true);
        restartCaret();
        revealCaretPending_ = true;
        markDirty(DirtyFlag::Paint);
        return true;
    case 37:
    case 263: // GLFW_KEY_LEFT
        controller_.moveCaret(-1, extendSelection);
        restartCaret();
        revealCaretPending_ = true;
        markDirty(DirtyFlag::Paint);
        return true;
    case 39:
    case 262: // GLFW_KEY_RIGHT
        controller_.moveCaret(1, extendSelection);
        restartCaret();
        revealCaretPending_ = true;
        markDirty(DirtyFlag::Paint);
        return true;
    case 38:
    case 265: // GLFW_KEY_UP
        if (!multiline_) return false;
        moveVertically(-1);
        restartCaret();
        revealCaretPending_ = true;
        markDirty(DirtyFlag::Paint);
        return true;
    case 40:
    case 264: // GLFW_KEY_DOWN
        if (!multiline_) return false;
        moveVertically(1);
        restartCaret();
        revealCaretPending_ = true;
        markDirty(DirtyFlag::Paint);
        return true;
    case 65:
        if (byWord) {
            controller_.selectAll();
            restartCaret();
            revealCaretPending_ = true;
            markDirty(DirtyFlag::Paint);
            return true;
        }
        return false;
    case 90:
        if (byWord) {
            if (extendSelection) {
                controller_.redo();
            } else {
                controller_.undo();
            }
            revealCaretPending_ = true;
            restartCaret();
            markDirty(DirtyFlag::Layout);
            notifyChanged();
            return true;
        }
        return false;
    case 89:
        if (byWord) {
            controller_.redo();
            restartCaret();
            revealCaretPending_ = true;
            markDirty(DirtyFlag::Layout);
            notifyChanged();
            return true;
        }
        return false;
    case 13:
    case 257: // GLFW_KEY_ENTER
        if (multiline_) {
            controller_.commit("\n");
            restartCaret();
            revealCaretPending_ = true;
            markDirty(DirtyFlag::Layout);
            notifyChanged();
            return true;
        }
        if (onSubmit_) {
            onSubmit_();
            return true;
        }
        return false;
    case 27:
    case 256: // GLFW_KEY_ESCAPE
        if (onCancel_) {
            onCancel_();
            return true;
        }
        return false;
    default:
        return false;
    }
}

bool TextInput::onTextInput(const TextInputEvent& event)
{
    if (!isEnabled()) return false;
    if (event.text.empty()) {
        return false;
    }

    controller_.commit(event.text);
    resetCaretBlink();
    revealCaretPending_ = true;
    markDirty(DirtyFlag::Layout);
    notifyChanged();
    return true;
}

bool TextInput::onCompositionInput(const CompositionInputEvent& event)
{
    if (!isEnabled()) return false;
    if (event.phase == CompositionInputEvent::Phase::End) {
        controller_.clearComposition();
        resetCaretBlink();
        revealCaretPending_ = true;
        markDirty(DirtyFlag::Paint);
        return true;
    }

    // Start and Update both replace the active pre-edit span. Empty pre-edit
    // text is valid and simply leaves an empty active composition range.
    controller_.updateComposition(event.text);
    resetCaretBlink();
    revealCaretPending_ = true;
    markDirty(DirtyFlag::Layout);
    notifyChanged();
    return true;
}

AccessibilityActionCapabilities TextInput::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.setValue = true;
    return actions;
}

AccessibilityActionStatus TextInput::performAccessibilityAction(
    AccessibilityActionKind kind, std::string_view value)
{
    if (kind != AccessibilityActionKind::SetValue) return AccessibilityActionStatus::NotSupported;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    text(std::string(value));
    return AccessibilityActionStatus::Succeeded;
}

bool TextInput::copySelection(Clipboard& clipboard) const
{
    const auto selected = controller_.selectedText();
    if (selected.empty()) {
        return false;
    }
    clipboard.setText(selected);
    return true;
}

bool TextInput::cutSelection(Clipboard& clipboard)
{
    if (!copySelection(clipboard)) {
        return false;
    }
    controller_.replaceSelection("");
    resetCaretBlink();
    revealCaretPending_ = true;
    markDirty(DirtyFlag::Layout);
    notifyChanged();
    return true;
}

bool TextInput::paste(Clipboard& clipboard)
{
    if (!clipboard.hasText()) {
        return false;
    }
    controller_.commit(clipboard.getText());
    resetCaretBlink();
    revealCaretPending_ = true;
    markDirty(DirtyFlag::Layout);
    notifyChanged();
    return true;
}

TextArea::TextArea(std::string placeholder)
    : TextInput(std::move(placeholder))
{
    setMultiline(true);
    setRows(3);
}

TextArea& TextArea::rows(std::size_t value) noexcept
{
    setRows(value);
    return *this;
}

void TextArea::setRows(std::size_t value) noexcept
{
    setMinimumLines(std::max<std::size_t>(1, value));
}

std::size_t TextArea::rows() const noexcept
{
    return minimumLines();
}

} // namespace wui
