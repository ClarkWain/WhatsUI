#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "wui/animation.h"
#include "wui/node.h"
#include "wui/platform.h"

namespace wui {

enum class InputSize { Small, Medium, Large };
enum class InputAppearance { Outline, Underline, FilledDarker, FilledLighter };

struct TextRange {
    std::size_t start{0};
    std::size_t end{0};

    [[nodiscard]] bool empty() const noexcept
    {
        return start == end;
    }
};

// The complete, serializable state of an editable text control.  Positions
// are UTF-8 byte offsets for now; the controller intentionally owns the edit
// semantics so a future grapheme-aware text engine can replace that detail
// without changing TextInput's public contract.
struct EditingValue {
    std::string text;
    TextRange selection{};
    TextRange composition{};
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

class TextEditingController {
public:
    [[nodiscard]] const EditingValue& value() const noexcept;
    void setValue(EditingValue value);

    [[nodiscard]] const std::string& text() const noexcept;
    void setText(std::string text);

    [[nodiscard]] const TextRange& selection() const noexcept;
    void setSelection(TextRange selection) noexcept;

    [[nodiscard]] const TextRange& composition() const noexcept;
    void setComposition(TextRange composition) noexcept;
    void clearComposition() noexcept;
    void updateComposition(std::string text);

    // Selection-aware navigation. Holding Shift extends from the selection
    // anchor; a plain arrow first collapses an existing selection.
    void moveCaret(int delta, bool extendSelection = false) noexcept;
    void moveToStart(bool extendSelection = false) noexcept;
    void moveToEnd(bool extendSelection = false) noexcept;
    void setCaret(std::size_t position, bool extendSelection = false) noexcept;
    void selectAll() noexcept;

    void backspace(bool byWord = false);
    void deleteForward(bool byWord = false);
    void replaceSelection(std::string text);
    void commit(std::string text);

    [[nodiscard]] std::string selectedText() const;
    bool undo();
    bool redo();

private:
    void rememberForUndo();
    void replaceRange(TextRange range, std::string_view text, bool saveHistory);
    void collapseOrExtend(std::size_t position, bool extendSelection) noexcept;
    [[nodiscard]] std::size_t previousWordBoundary(std::size_t position) const noexcept;
    [[nodiscard]] std::size_t nextWordBoundary(std::size_t position) const noexcept;
    void normalize() noexcept;

    EditingValue value_{};
    std::size_t selectionAnchor_{0};
    std::vector<EditingValue> undoStack_;
    std::vector<EditingValue> redoStack_;
};

// Kept as a source-compatible spelling during the M2 transition. New code
// should use TextEditingController, which exposes the complete EditingValue.
using TextInputModel = TextEditingController;

class TextInput : public ControlNode {
public:
    using ChangeHandler = std::function<void(const std::string&)>;
    using SubmitHandler = std::function<void()>;
    using CancelHandler = std::function<void()>;
    TextInput() = default;
    explicit TextInput(std::string placeholder);
    ~TextInput() override;

    [[nodiscard]] TextEditingController& controller() noexcept;
    [[nodiscard]] const TextEditingController& controller() const noexcept;
    [[nodiscard]] TextInputModel& model() noexcept;
    [[nodiscard]] const TextInputModel& model() const noexcept;

    [[nodiscard]] const std::string& placeholder() const noexcept;
    TextInput& placeholder(std::string placeholder);
    void setPlaceholder(std::string placeholder);

    TextInput& text(std::string text);
    TextInput& accessibleLabel(std::string label);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    void setAccessibleLabel(std::string label);
    void setSize(InputSize size) noexcept;
    [[nodiscard]] InputSize size() const noexcept;
    void setAppearance(InputAppearance appearance) noexcept;
    [[nodiscard]] InputAppearance appearance() const noexcept;
    void setInvalid(bool invalid) noexcept;
    [[nodiscard]] bool isInvalid() const noexcept;
    void setMotionEnabled(bool enabled) noexcept;
    [[nodiscard]] bool isMotionEnabled() const noexcept;

    // TextArea reuses the same UTF-8 editing and IME controller as TextInput,
    // but opts into wrapped multi-line layout.  Keep this on the base class so
    // host integrations only need one native text-input session contract.
    void setMultiline(bool value) noexcept;
    [[nodiscard]] bool isMultiline() const noexcept;
    void setMinimumLines(std::size_t value) noexcept;
    [[nodiscard]] std::size_t minimumLines() const noexcept;
    [[nodiscard]] float verticalScrollOffset() const noexcept;
    [[nodiscard]] float maximumVerticalScrollOffset() const noexcept;

    // Applications can react to editing without polling the controller. This
    // keeps filtering and Enter/Escape form semantics widget-local.
    TextInput& onChange(ChangeHandler handler);
    TextInput& onSubmit(SubmitHandler handler);
    TextInput& onCancel(CancelHandler handler);

    void syncSession(TextInputSession& session, const RectF& caretRect) const;
    [[nodiscard]] RectF caretRect() const noexcept;

    [[nodiscard]] bool copySelection(Clipboard& clipboard) const;
    [[nodiscard]] bool cutSelection(Clipboard& clipboard);
    [[nodiscard]] bool paste(Clipboard& clipboard);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    EventResult onPointerEvent(const PointerEvent& event, EventContext& context) override;
    bool onKeyEvent(const KeyEvent& event) override;
    bool onTextInput(const TextInputEvent& event) override;
    bool onCompositionInput(const CompositionInputEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    [[nodiscard]] std::size_t caretAt(PointF point) const noexcept;
    void notifyChanged();
    void ensureCaretBlinkAnimation();
    void stopCaretBlinkAnimation() noexcept;
    void resetCaretBlink();
    void syncFocusIndicatorAnimation(bool active);
    void stopFocusIndicatorAnimation() noexcept;
    void onDetach() noexcept override;

    TextEditingController controller_;
    std::string placeholder_;
    std::string accessibleLabel_;
    ChangeHandler onChange_;
    SubmitHandler onSubmit_;
    CancelHandler onCancel_;
    bool selectingWithPointer_{false};
    InputSize size_{InputSize::Medium};
    InputAppearance appearance_{InputAppearance::Outline};
    bool invalid_{false};
    bool multiline_{false};
    std::size_t minimumLines_{1};
    float verticalScrollOffset_{0.0f};
    bool revealCaretPending_{true};
    bool hasPaintedCaret_{false};
    std::size_t lastPaintedCaret_{0};
    AnimationId caretBlinkAnimation_{0};
    bool caretVisible_{true};
    AnimationId focusIndicatorAnimation_{0};
    float focusIndicatorProgress_{0.0f};
    bool focusIndicatorTargetActive_{false};
    bool motionEnabled_{true};
};

// A genuine editable multi-line field. It intentionally inherits the same
// controller, clipboard and IME protocol as TextInput rather than faking a
// text area with a static Text node.
class TextArea final : public TextInput {
public:
    explicit TextArea(std::string placeholder = {});

    TextArea& rows(std::size_t value) noexcept;
    void setRows(std::size_t value) noexcept;
    [[nodiscard]] std::size_t rows() const noexcept;
};

} // namespace wui
