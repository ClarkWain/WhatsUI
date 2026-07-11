#include "wui/text_input.h"
#include "wui/theme.h"

#include <algorithm>
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

} // namespace

const std::string& TextInputModel::text() const noexcept
{
    return text_;
}

void TextInputModel::setText(std::string text)
{
    text_ = std::move(text);
    selection_ = normalizeRange(selection_, text_.size());
    composition_ = normalizeRange(composition_, text_.size());
}

const TextRange& TextInputModel::selection() const noexcept
{
    return selection_;
}

void TextInputModel::setSelection(TextRange selection) noexcept
{
    selection_ = normalizeRange(selection, text_.size());
}

const TextRange& TextInputModel::composition() const noexcept
{
    return composition_;
}

void TextInputModel::setComposition(TextRange composition) noexcept
{
    composition_ = normalizeRange(composition, text_.size());
}

void TextInputModel::clearComposition() noexcept
{
    composition_ = {};
}

void TextInputModel::updateComposition(std::string text)
{
    const auto activeRange = composition_.empty()
        ? normalizeRange(selection_, text_.size())
        : normalizeRange(composition_, text_.size());

    text_.replace(activeRange.start, activeRange.end - activeRange.start, text);
    const auto end = activeRange.start + text.size();
    composition_ = TextRange{activeRange.start, end};
    selection_ = composition_;
}

void TextInputModel::moveCaret(int delta) noexcept
{
    selection_ = normalizeRange(selection_, text_.size());
    const auto caret = selection_.end;
    const auto next = delta < 0
        ? caret - static_cast<std::size_t>(std::min<std::size_t>(caret, static_cast<std::size_t>(-delta)))
        : std::min(text_.size(), caret + static_cast<std::size_t>(delta));
    selection_ = TextRange{next, next};
}

void TextInputModel::backspace()
{
    selection_ = normalizeRange(selection_, text_.size());
    if (!selection_.empty()) {
        replaceSelection("");
        return;
    }
    if (selection_.start == 0) {
        return;
    }

    text_.erase(selection_.start - 1, 1);
    const auto caret = selection_.start - 1;
    selection_ = TextRange{caret, caret};
    composition_ = {};
}

void TextInputModel::replaceSelection(std::string text)
{
    selection_ = normalizeRange(selection_, text_.size());
    text_.replace(selection_.start, selection_.end - selection_.start, text);
    const auto caret = selection_.start + text.size();
    selection_ = TextRange{caret, caret};
    composition_ = {};
}

void TextInputModel::commit(std::string text)
{
    replaceSelection(std::move(text));
}

TextInput::TextInput(std::string placeholder)
    : placeholder_(std::move(placeholder))
{
}

TextInputModel& TextInput::model() noexcept
{
    return model_;
}

const TextInputModel& TextInput::model() const noexcept
{
    return model_;
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
    model_.setText(std::move(text));
    markDirty(DirtyFlag::Layout);
    return *this;
}

void TextInput::syncSession(TextInputSession& session, const RectF& caretRect) const
{
    session.setCaretRect(caretRect);
    session.setSurroundingText(model_.text(), model_.selection().start, model_.selection().end);
}

RectF TextInput::caretRect() const noexcept
{
    const float kHorizontalPadding = theme().controls.horizontalPadding;
    const float kCharacterWidth = theme().typography.body * 0.56f;
    const auto caret = static_cast<float>(model_.selection().end);
    return {bounds().x + kHorizontalPadding + caret * kCharacterWidth,
            bounds().y + 6.0f, 1.0f, std::max(1.0f, bounds().height - 12.0f)};
}

SizeF TextInput::measure(const Constraints& constraints) const
{
    const auto contentLength = std::max(model_.text().size(), placeholder_.size());
    const auto& current = theme();
    const auto width = static_cast<float>(contentLength) * (current.typography.body * 0.56f) + current.controls.horizontalPadding * 2.0f;
    return constraints.clamp({width, current.controls.height});
}

void TextInput::paint(PaintContext& context)
{
    const bool showPlaceholder = model_.text().empty();
    const auto& text = showPlaceholder ? placeholder_ : model_.text();

    const auto& current = theme();
    const bool focused = (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    context.fillRoundRect(bounds(), current.radius.md, focused ? current.colors.focus : current.colors.border);
    const float inset = focused ? current.controls.focusWidth : 1.0f;
    context.fillRoundRect({bounds().x + inset, bounds().y + inset, std::max(0.0f, bounds().width - inset * 2.0f), std::max(0.0f, bounds().height - inset * 2.0f)},
                          std::max(0.0f, current.radius.md - inset), current.colors.surface);
    if (!text.empty()) {
        context.drawText(text, bounds().x + current.controls.horizontalPadding,
            bounds().y + (bounds().height + current.typography.body) * 0.5f - 2.0f, current.typography.body,
            showPlaceholder ? current.colors.textMuted : current.colors.text);
    }
    clearDirty(DirtyFlag::Paint);
}

bool TextInput::onPointerEvent(const PointerEvent& event)
{
    if (event.action == PointerAction::Down && event.button == MouseButton::Left) {
        setVisualState(ControlVisualState::Focused, true);
        return true;
    }
    return false;
}

bool TextInput::onKeyEvent(const KeyEvent& event)
{
    if (event.action != KeyAction::Down) {
        return false;
    }

    switch (event.keyCode) {
    case 8:
        model_.backspace();
        markDirty(DirtyFlag::Layout);
        return true;
    case 37:
        model_.moveCaret(-1);
        markDirty(DirtyFlag::Paint);
        return true;
    case 39:
        model_.moveCaret(1);
        markDirty(DirtyFlag::Paint);
        return true;
    default:
        return false;
    }
}

bool TextInput::onTextInput(const TextInputEvent& event)
{
    if (event.text.empty()) {
        return false;
    }

    model_.commit(event.text);
    markDirty(DirtyFlag::Layout);
    return true;
}

bool TextInput::onCompositionInput(const CompositionInputEvent& event)
{
    if (event.phase == CompositionInputEvent::Phase::End) {
        model_.clearComposition();
        markDirty(DirtyFlag::Paint);
        return true;
    }

    // Start and Update both replace the active pre-edit span. Empty pre-edit
    // text is valid and simply leaves an empty active composition range.
    model_.updateComposition(event.text);
    markDirty(DirtyFlag::Layout);
    return true;
}

} // namespace wui
