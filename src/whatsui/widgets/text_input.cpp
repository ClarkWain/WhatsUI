#include "wui/text_input.h"

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

SizeF TextInput::measure(const Constraints& constraints) const
{
    const auto contentLength = std::max(model_.text().size(), placeholder_.size());
    const auto width = static_cast<float>(contentLength) * 8.0f + 20.0f;
    return constraints.clamp({width, 32.0f});
}

void TextInput::paint(PaintContext& context)
{
    const bool showPlaceholder = model_.text().empty();
    const auto& text = showPlaceholder ? placeholder_ : model_.text();

    context.fillRoundRect(bounds(), 6.0f, Color{245, 246, 248, 255});
    if (!text.empty()) {
        context.drawText(text, bounds().x + 8.0f, bounds().y + 20.0f, 14.0f,
            showPlaceholder ? Color{132, 136, 143, 255} : Color{28, 28, 28, 255});
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
    if (event.text.empty()) {
        model_.clearComposition();
        markDirty(DirtyFlag::Paint);
        return true;
    }

    model_.updateComposition(event.text);
    markDirty(DirtyFlag::Layout);
    return true;
}

} // namespace wui
