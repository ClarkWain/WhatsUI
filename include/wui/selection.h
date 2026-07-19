#pragma once

// Fluent selection controls.  ListBox is deliberately useful as a standalone
// control, while Combobox and Dropdown compose it into an anchored overlay.
// Options are stable value objects rather than visual children: this keeps a
// filtered combobox from rebuilding an ownership tree on every keystroke.

#include <functional>
#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "wui/overlays.h"

namespace wui {

class OverlayHost;

struct Option {
    std::string value;
    std::string text;
    std::string secondaryText;
    bool enabled{true};

    Option() = default;
    Option(std::string value, std::string text, bool enabled = true)
        : value(std::move(value)), text(std::move(text)), enabled(enabled) {}
    Option(std::string value, std::string text, std::string secondaryText, bool enabled = true)
        : value(std::move(value)), text(std::move(text)), secondaryText(std::move(secondaryText)), enabled(enabled) {}
};

enum class ListBoxSelectionMode { Single, Multiple };

// A materialized option semantic can be consumed by platform adapters that
// represent a windowed ListBox as virtual children. Bounds are set only for
// options currently visible in the viewport.
struct ListBoxOptionAccessibility {
    std::size_t index{0};
    AccessibilityProperties properties{};
};

// A Fluent listbox with keyboard roving selection.  In Multiple mode Space
// toggles the focused option and Enter commits it; in Single mode either key
// selects it.  `activeIndex` is intentionally distinct from selection so a
// combobox can preview/filter choices without mutating its committed value.
class ListBox : public ControlNode {
public:
    using SelectionHandler = std::function<void(int, const Option&)>;

    ListBox() = default;
    explicit ListBox(std::vector<Option> options);

    ListBox& addOption(Option option);
    ListBox& setOptions(std::vector<Option> options);
    ListBox& clearOptions();
    [[nodiscard]] const std::vector<Option>& options() const noexcept;

    ListBox& selectionMode(ListBoxSelectionMode value) noexcept;
    void setSelectionMode(ListBoxSelectionMode value) noexcept;
    [[nodiscard]] ListBoxSelectionMode selectionMode() const noexcept;
    [[nodiscard]] int selectedIndex() const noexcept;
    [[nodiscard]] const std::vector<int>& selectedIndices() const noexcept;
    ListBox& selectedIndex(int index);
    void setSelectedIndex(int index);
    ListBox& selectedIndices(std::vector<int> indices);
    void setSelectedIndices(std::vector<int> indices);
    [[nodiscard]] int activeIndex() const noexcept;
    void setActiveIndex(int index);
    ListBox& onSelectionChanged(SelectionHandler handler);
    ListBox& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    ListBox& maxVisibleOptions(std::size_t value) noexcept;
    void setMaxVisibleOptions(std::size_t value) noexcept;
    [[nodiscard]] std::size_t maxVisibleOptions() const noexcept;
    [[nodiscard]] float scrollOffset() const noexcept;
    void setScrollOffset(float value) noexcept;
    [[nodiscard]] float maximumScrollOffset() const noexcept;
    [[nodiscard]] std::vector<ListBoxOptionAccessibility> accessibilityOptions() const;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    [[nodiscard]] bool selectable(int index) const noexcept;
    [[nodiscard]] int nextSelectable(int from, int delta) const noexcept;
    [[nodiscard]] int optionAt(PointF point) const noexcept;
    [[nodiscard]] float rowHeight() const noexcept;
    [[nodiscard]] float preferredWidth() const noexcept;
    [[nodiscard]] bool isSelected(int index) const noexcept;
    [[nodiscard]] RectF optionBounds(int index) const noexcept;
    void scrollActiveIntoView() noexcept;
    void updateTypeAhead(char character);
    void choose(int index, bool toggle = false);

    std::vector<Option> options_;
    std::vector<int> selected_;
    int activeIndex_{-1};
    int hoveredIndex_{-1};
    int pressedIndex_{-1};
    float scrollOffset_{0.0f};
    std::size_t maxVisibleOptions_{8};
    ListBoxSelectionMode selectionMode_{ListBoxSelectionMode::Single};
    SelectionHandler onSelectionChanged_;
    std::string accessibleLabel_;
    std::string typeAheadPrefix_;
    std::chrono::steady_clock::time_point lastTypeAhead_{};
};

// Editable selection control.  The retained TextInput base owns IME, native
// text sessions and caret rendering; this class adds filtering and the popup
// lifecycle without duplicating text-editor behavior.
class Combobox : public TextInput {
public:
    using SelectionHandler = std::function<void(int, const Option&)>;

    explicit Combobox(std::string placeholder = {});
    ~Combobox() override;

    Combobox& addOption(Option option);
    Combobox& setOptions(std::vector<Option> options);
    Combobox& clearOptions();
    [[nodiscard]] const std::vector<Option>& options() const noexcept;
    [[nodiscard]] int selectedIndex() const noexcept;
    [[nodiscard]] const std::vector<int>& selectedIndices() const noexcept;
    Combobox& selectedIndex(int index);
    void setSelectedIndex(int index);
    Combobox& multiselect(bool value = true) noexcept;
    void setMultiselect(bool value) noexcept;
    [[nodiscard]] bool isMultiselect() const noexcept;
    Combobox& selectedIndices(std::vector<int> indices);
    void setSelectedIndices(std::vector<int> indices);
    [[nodiscard]] bool isOpen() const noexcept;
    Combobox& bindOverlayHost(OverlayHost& host) noexcept;
    Combobox& onSelectionChanged(SelectionHandler handler);
    Combobox& onChange(ChangeHandler handler);
    Combobox& openOnFocus(bool value) noexcept;
    void setOpenOnFocus(bool value) noexcept;

    EventResult onPointerEvent(const PointerEvent& event, EventContext& context) override;
    bool onKeyEvent(const KeyEvent& event) override;
    bool onTextInput(const TextInputEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    void openPopup();
    void closePopup();
    void refreshPopup();
    void commit(int sourceIndex);
    void commitSelection(std::vector<int> sourceIndices, int changedSourceIndex);
    [[nodiscard]] std::vector<int> filteredIndices() const;
    [[nodiscard]] int sourceIndexForVisible(int visibleIndex) const noexcept;

    std::vector<Option> options_;
    std::vector<int> visibleIndices_;
    int selectedIndex_{-1};
    std::vector<int> selectedIndices_;
    OverlayHost* overlayHost_{nullptr};
    std::size_t overlayId_{0};
    bool open_{false};
    bool openOnFocus_{true};
    bool multiselect_{false};
    bool updatingText_{false};
    SelectionHandler onSelectionChanged_;
    ChangeHandler userOnChange_;
};

// Non-editable Fluent selection control.  It is a button-like combobox with
// the same option model and overlay behavior, exposed as an expandable field
// rather than an editable text field.
class Dropdown : public ControlNode {
public:
    using SelectionHandler = std::function<void(int, const Option&)>;

    explicit Dropdown(std::string placeholder = "Select an option");
    ~Dropdown() override;

    Dropdown& addOption(Option option);
    Dropdown& setOptions(std::vector<Option> options);
    Dropdown& clearOptions();
    [[nodiscard]] const std::vector<Option>& options() const noexcept;
    [[nodiscard]] int selectedIndex() const noexcept;
    [[nodiscard]] const std::vector<int>& selectedIndices() const noexcept;
    Dropdown& selectedIndex(int index);
    void setSelectedIndex(int index);
    Dropdown& multiselect(bool value = true) noexcept;
    void setMultiselect(bool value) noexcept;
    [[nodiscard]] bool isMultiselect() const noexcept;
    Dropdown& selectedIndices(std::vector<int> indices);
    void setSelectedIndices(std::vector<int> indices);
    [[nodiscard]] const std::string& value() const noexcept;
    [[nodiscard]] const std::string& placeholder() const noexcept;
    [[nodiscard]] bool isOpen() const noexcept;
    Dropdown& bindOverlayHost(OverlayHost& host) noexcept;
    Dropdown& onSelectionChanged(SelectionHandler handler);
    Dropdown& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    void openPopup();
    void closePopup();
    void commit(int index);
    void commitSelection(std::vector<int> indices, int changedIndex);
    [[nodiscard]] bool selectable(int index) const noexcept;
    [[nodiscard]] int nextSelectable(int from, int delta) const noexcept;

    std::vector<Option> options_;
    std::string placeholder_;
    std::string accessibleLabel_;
    int selectedIndex_{-1};
    std::vector<int> selectedIndices_;
    OverlayHost* overlayHost_{nullptr};
    std::size_t overlayId_{0};
    bool open_{false};
    bool multiselect_{false};
    SelectionHandler onSelectionChanged_;
};

} // namespace wui
