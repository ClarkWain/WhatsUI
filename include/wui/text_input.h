#pragma once

#include <cstddef>
#include <string>

#include "wui/node.h"
#include "wui/platform.h"

namespace wui {

struct TextRange {
    std::size_t start{0};
    std::size_t end{0};

    [[nodiscard]] bool empty() const noexcept
    {
        return start == end;
    }
};

enum class TextInputEventType {
    Commit,
    CompositionStart,
    CompositionUpdate,
    CompositionEnd,
};

struct TextCommitEvent {
    std::string text;
};

struct CompositionEvent {
    TextInputEventType type{TextInputEventType::CompositionUpdate};
    std::string text;
    TextRange range{};
};

class TextInputModel {
public:
    [[nodiscard]] const std::string& text() const noexcept;
    void setText(std::string text);

    [[nodiscard]] const TextRange& selection() const noexcept;
    void setSelection(TextRange selection) noexcept;

    [[nodiscard]] const TextRange& composition() const noexcept;
    void setComposition(TextRange composition) noexcept;
    void clearComposition() noexcept;
    void updateComposition(std::string text);

    void moveCaret(int delta) noexcept;
    void backspace();
    void replaceSelection(std::string text);
    void commit(std::string text);

private:
    std::string text_;
    TextRange selection_{};
    TextRange composition_{};
};

class TextInput : public ControlNode {
public:
    TextInput() = default;
    explicit TextInput(std::string placeholder);

    [[nodiscard]] TextInputModel& model() noexcept;
    [[nodiscard]] const TextInputModel& model() const noexcept;

    [[nodiscard]] const std::string& placeholder() const noexcept;
    TextInput& placeholder(std::string placeholder);
    void setPlaceholder(std::string placeholder);

    TextInput& text(std::string text);

    void syncSession(TextInputSession& session, const RectF& caretRect) const;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    bool onTextInput(const TextInputEvent& event) override;
    bool onCompositionInput(const CompositionInputEvent& event) override;

private:
    TextInputModel model_;
    std::string placeholder_;
};

} // namespace wui
