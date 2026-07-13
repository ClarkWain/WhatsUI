#include "wui/text_input.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"

#include <algorithm>
#include <cctype>
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

[[nodiscard]] RectF textViewport(const RectF& bounds, const Theme& current, bool focused) noexcept
{
    const float borderInset = focused ? current.controls.focusWidth : 1.0f;
    const float horizontalPadding = current.controls.horizontalPadding;
    return {bounds.x + horizontalPadding, bounds.y + borderInset,
            std::max(0.0f, bounds.width - horizontalPadding * 2.0f),
            std::max(0.0f, bounds.height - borderInset * 2.0f)};
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
    markDirty(DirtyFlag::Layout);
    notifyChanged();
    return *this;
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
    const RectF viewport = textViewport(bounds(), current, focused);
    const float caretX = measuredTextWidth(controller_.text(), controller_.selection().end,
                                            current.typography.body);
    const float offset = horizontalTextOffset(caretX, viewport.width);
    return {viewport.x + caretX - offset,
            viewport.y, 1.0f, std::max(1.0f, viewport.height)};
}

SizeF TextInput::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const auto width = std::max(measuredTextWidth(controller_.text(), controller_.text().size(), current.typography.body),
                                measuredTextWidth(placeholder_, placeholder_.size(), current.typography.body))
                       + current.controls.horizontalPadding * 2.0f;
    return constraints.clamp({width, current.controls.height});
}

void TextInput::paint(PaintContext& context)
{
    const bool showPlaceholder = controller_.text().empty();
    const auto& text = showPlaceholder ? placeholder_ : controller_.text();

    const auto& current = theme();
    const bool focused = (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    context.fillRoundRect(bounds(), current.radius.md, focused ? current.colors.focus : current.colors.border);
    const float inset = focused ? current.controls.focusWidth : 1.0f;
    context.fillRoundRect({bounds().x + inset, bounds().y + inset, std::max(0.0f, bounds().width - inset * 2.0f), std::max(0.0f, bounds().height - inset * 2.0f)},
                          std::max(0.0f, current.radius.md - inset), current.colors.surface);
    const auto selection = controller_.selection();
    const auto composition = controller_.composition();
    const RectF viewport = textViewport(bounds(), current, focused);
    const float selectionStartX = measuredTextWidth(controller_.text(), selection.start, current.typography.body);
    const float selectionEndX = measuredTextWidth(controller_.text(), selection.end, current.typography.body);
    const float compositionStartX = measuredTextWidth(controller_.text(), composition.start, current.typography.body);
    const float compositionEndX = measuredTextWidth(controller_.text(), composition.end, current.typography.body);
    const float horizontalOffset = showPlaceholder ? 0.0f : horizontalTextOffset(selectionEndX, viewport.width);
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
    if (focused && !selection.empty() && !selectionIsComposition) {
        const float selectionX = viewport.x + selectionStartX - horizontalOffset;
        const float selectionWidth = selectionEndX - selectionStartX;
        const auto selectionColor = Color{current.colors.focus.r, current.colors.focus.g, current.colors.focus.b, 72};
        context.fillRect({selectionX, viewport.y, selectionWidth, viewport.height}, selectionColor);
    }
    if (!text.empty()) {
        context.drawText(text, viewport.x - horizontalOffset,
            context.centeredTextBottom(text, bounds(), current.typography.body), current.typography.body,
            showPlaceholder ? current.colors.textMuted : current.colors.text);
    }
    if (focused && !composition.empty()) {
        const float compositionX = viewport.x + compositionStartX - horizontalOffset;
        const float compositionWidth = compositionEndX - compositionStartX;
        const float baseline = context.centeredTextBottom(controller_.text(), bounds(), current.typography.body);
        // Keep the marker close to the glyph baseline (rather than at the
        // input's bottom edge) so it remains correct for Fluent's compact
        // and regular control heights.
        context.fillRect({compositionX, baseline + current.controls.focusInset,
                          std::max(1.0f, compositionWidth), 1.0f}, current.colors.focus);
    }
    if (focused && selection.empty()) {
        const float caretX = viewport.x + selectionEndX - horizontalOffset;
        context.fillRect({caretX, viewport.y, 1.0f, std::max(1.0f, viewport.height)}, current.colors.focus);
    }
    context.restoreTo(checkpoint);
    clearDirty(DirtyFlag::Paint);
}

std::size_t TextInput::caretAt(PointF point) const noexcept
{
    const bool focused = (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    const RectF viewport = textViewport(bounds(), theme(), focused);
    const float caretX = measuredTextWidth(controller_.text(), controller_.selection().end,
                                            theme().typography.body);
    const float offset = horizontalTextOffset(caretX, viewport.width);
    const float localX = std::max(0.0f, point.x - viewport.x + offset);
    return nearestTextOffset(controller_.text(), localX, theme().typography.body);
}

EventResult TextInput::onPointerEvent(const PointerEvent& event, EventContext& context)
{
    if (context.phase() == EventPhase::Capture) {
        return EventResult::Ignored;
    }
    switch (event.action) {
    case PointerAction::Down:
        if (event.button != MouseButton::Left) {
            return EventResult::Ignored;
        }
        selectingWithPointer_ = true;
        controller_.setCaret(caretAt(event.position), (event.modifiers & KeyModifierShift) != 0);
        setVisualState(ControlVisualState::Focused, true);
        markDirty(DirtyFlag::Paint);
        context.requestFocus();
        context.capturePointer();
        return EventResult::Handled;
    case PointerAction::Move:
        if (!selectingWithPointer_) {
            return EventResult::Ignored;
        }
        controller_.setCaret(caretAt(event.position), true);
        markDirty(DirtyFlag::Paint);
        return EventResult::Handled;
    case PointerAction::Up:
    case PointerAction::Cancel:
        if (!selectingWithPointer_) {
            return EventResult::Ignored;
        }
        if (event.action == PointerAction::Up) {
            controller_.setCaret(caretAt(event.position), true);
        }
        selectingWithPointer_ = false;
        markDirty(DirtyFlag::Paint);
        context.releasePointer();
        return EventResult::Handled;
    default:
        return EventResult::Ignored;
    }
}

bool TextInput::onKeyEvent(const KeyEvent& event)
{
    if (event.action != KeyAction::Down) {
        return false;
    }

    const bool extendSelection = (event.modifiers & KeyModifierShift) != 0;
    const bool byWord = (event.modifiers & KeyModifierControl) != 0;
    switch (event.keyCode) {
    case 8:
    case 259: // GLFW_KEY_BACKSPACE
        controller_.backspace(byWord);
        markDirty(DirtyFlag::Layout);
        notifyChanged();
        return true;
    case 46:
    case 261: // GLFW_KEY_DELETE
        controller_.deleteForward(byWord);
        markDirty(DirtyFlag::Layout);
        notifyChanged();
        return true;
    case 36:
    case 268: // GLFW_KEY_HOME
        controller_.moveToStart(extendSelection);
        markDirty(DirtyFlag::Paint);
        return true;
    case 35:
    case 269: // GLFW_KEY_END
        controller_.moveToEnd(extendSelection);
        markDirty(DirtyFlag::Paint);
        return true;
    case 37:
    case 263: // GLFW_KEY_LEFT
        controller_.moveCaret(-1, extendSelection);
        markDirty(DirtyFlag::Paint);
        return true;
    case 39:
    case 262: // GLFW_KEY_RIGHT
        controller_.moveCaret(1, extendSelection);
        markDirty(DirtyFlag::Paint);
        return true;
    case 65:
        if (byWord) {
            controller_.selectAll();
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
            markDirty(DirtyFlag::Layout);
            notifyChanged();
            return true;
        }
        return false;
    case 89:
        if (byWord) {
            controller_.redo();
            markDirty(DirtyFlag::Layout);
            notifyChanged();
            return true;
        }
        return false;
    case 13:
    case 257: // GLFW_KEY_ENTER
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
    if (event.text.empty()) {
        return false;
    }

    controller_.commit(event.text);
    markDirty(DirtyFlag::Layout);
    notifyChanged();
    return true;
}

bool TextInput::onCompositionInput(const CompositionInputEvent& event)
{
    if (event.phase == CompositionInputEvent::Phase::End) {
        controller_.clearComposition();
        markDirty(DirtyFlag::Paint);
        return true;
    }

    // Start and Update both replace the active pre-edit span. Empty pre-edit
    // text is valid and simply leaves an empty active composition range.
    controller_.updateComposition(event.text);
    markDirty(DirtyFlag::Layout);
    notifyChanged();
    return true;
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
    markDirty(DirtyFlag::Layout);
    notifyChanged();
    return true;
}

} // namespace wui
