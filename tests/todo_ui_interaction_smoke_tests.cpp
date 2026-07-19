// UI-tree smoke coverage for the Todo sample.  This intentionally includes the
// sample's implementation under a test-only entry-point guard: the assertions
// drive the very same buildTodoUi() tree that the Software and GLFW examples
// use, rather than re-creating a lookalike fixture or only exercising domain
// controller methods.

#define WUI_TODO_INTERACTIVE 1
#define WUI_TODO_TESTING 1
#include "../examples/todo_app/main.cpp"

#include <cstdio>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <windows.h>
#endif
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::fputs(message, stderr);
        std::fputc('\n', stderr);
        throw std::runtime_error(message);
    }
}

class FakeSurface final : public wui::RenderSurface {
public:
    [[nodiscard]] wui::CanvasBackend backend() const noexcept override { return wui::CanvasBackend::Software; }
    [[nodiscard]] wui::SizeF framebufferSize() const noexcept override { return {800.0f, 1200.0f}; }
    void beginFrame() override {}
    void endFrame() override {}
    void resize(wui::SizeF) override {}
};

class FakeClipboard final : public wui::Clipboard {
public:
    void setText(std::string_view text) override { text_ = text; }
    [[nodiscard]] std::string getText() const override { return text_; }
    [[nodiscard]] bool hasText() const override { return !text_.empty(); }
private:
    std::string text_;
};

class FakeCursor final : public wui::CursorService {
public:
    void setCursor(wui::CursorIcon) override {}
};

class FakeTextInput final : public wui::TextInputSession {
public:
    void activate() override {}
    void deactivate() override {}
    void setCaretRect(const wui::RectF&) override {}
    void setSurroundingText(std::string_view, std::size_t, std::size_t) override {}
};

class FakeWindow final : public wui::PlatformWindow {
public:
    [[nodiscard]] wui::WindowId id() const noexcept override { return 42; }
    [[nodiscard]] wui::WindowMetrics metrics() const noexcept override
    {
        return {{800.0f, 1200.0f}, {800.0f, 1200.0f}, 1.0f};
    }
    void show() override {}
    void close() override {}
    [[nodiscard]] bool isOpen() const noexcept override { return true; }
    [[nodiscard]] bool isFocused() const noexcept override { return true; }
    void setTitle(std::string_view) override {}
    void requestRedraw() override { ++redraws; }
    [[nodiscard]] wui::RenderSurface& surface() override { return surface_; }
    [[nodiscard]] wui::Clipboard& clipboard() override { return clipboard_; }
    [[nodiscard]] wui::CursorService& cursor() override { return cursor_; }
    [[nodiscard]] wui::TextInputSession& textInput() override { return textInput_; }

    int redraws{0};

private:
    FakeSurface surface_;
    FakeClipboard clipboard_;
    FakeCursor cursor_;
    FakeTextInput textInput_;
};

template <class NodeT>
void collect(wui::Node* node, std::vector<NodeT*>& result)
{
    if (node == nullptr) return;
    if (auto* match = dynamic_cast<NodeT*>(node)) result.push_back(match);
    for (const auto& child : node->children()) collect(child.get(), result);
}

wui::PointF center(const wui::Node& node)
{
    const auto bounds = node.bounds();
    return {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f};
}

bool hasVisualState(const wui::ControlNode& control, wui::ControlVisualState state)
{
    return (control.visualStates() & wui::toMask(state)) != 0;
}

void click(wui::UiWindow& window, const wui::Node& node)
{
    const auto point = center(node);
    expect(window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Down,
                                   wui::MouseButton::Left, point, 0}),
           "Todo control should handle pointer down");
    expect(window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Up,
                                   wui::MouseButton::Left, point, 0}),
           "Todo control should handle pointer up");
}

void commitFrame(wui::UiWindow& window)
{
    window.update();
    window.layout();
}

wui::TextInput* composer(wui::Node* root)
{
    std::vector<wui::TextInput*> fields;
    collect(root, fields);
    for (auto* field : fields) {
        if (field->placeholder() == "Add a task for today") return field;
    }
    return nullptr;
}

wui::Button* button(wui::Node* root, const std::string& label)
{
    std::vector<wui::Button*> buttons;
    collect(root, buttons);
    for (auto* candidate : buttons) {
        if (candidate->label() == label) return candidate;
    }
    return nullptr;
}

wui::IconButton* deleteButton(wui::Node* root)
{
    std::vector<wui::IconButton*> buttons;
    collect(root, buttons);
    for (auto* candidate : buttons) {
        if (candidate->accessibleLabel() == "Delete task") return candidate;
    }
    return nullptr;
}

wui::IconButton* importantButton(wui::Node* root, const std::string& label)
{
    std::vector<wui::IconButton*> buttons;
    collect(root, buttons);
    for (auto* candidate : buttons) {
        if (candidate->accessibleLabel() == label) return candidate;
    }
    return nullptr;
}

wui::Node* dialogRoot(wui::UiWindow& window)
{
    const auto* top = window.overlayHost().top();
    return top != nullptr ? top->content.get() : nullptr;
}

wui::Button* dialogButton(wui::UiWindow& window, const std::string& label)
{
    return button(dialogRoot(window), label);
}

bool containsText(wui::Node* root, const std::string& value)
{
    std::vector<wui::Text*> texts;
    collect(root, texts);
    for (const auto* text : texts) {
        if (text->value() == value) return true;
    }
    return false;
}

const wui::AccessibilitySnapshotEntry* semanticEntry(const wui::AccessibilitySnapshot& snapshot,
                                                      wui::AccessibilityRole role,
                                                      const std::string& label)
{
    for (const auto& entry : snapshot) {
        if (entry.properties.role == role && entry.properties.label == label) return &entry;
    }
    return nullptr;
}

struct TodoUiHarness {
    whatsui::todo::TodoController controller;
    whatsui::todo::TodoInteraction interaction{controller};
    wui::State<std::vector<Todo>> todos{controller.records()};
    wui::State<bool> isEmpty{true};
    wui::State<bool> hasNoMatches{false};
    wui::State<std::vector<Todo>> activeTodos;
    wui::State<std::vector<Todo>> completedTodos;
    wui::State<bool> hasActive{false};
    wui::State<bool> hasCompleted{false};
    wui::State<bool> hasStoredCompleted{false};
    wui::State<bool> filterAll{true};
    wui::State<bool> filterActive{false};
    wui::State<bool> filterCompleted{false};
    wui::State<bool> hasOperationMessage{false};
    wui::State<std::string> operationMessage;
    wui::State<bool> hasUndo{false};
    wui::State<std::string> undoMessage;

    [[nodiscard]] whatsui::todo::TodoFilter filter() const
    {
        if (filterActive.get()) return whatsui::todo::TodoFilter::Active;
        if (filterCompleted.get()) return whatsui::todo::TodoFilter::Completed;
        return whatsui::todo::TodoFilter::All;
    }

    void synchronize()
    {
        synchronizeTodoPresentation(controller.records(), interaction.filtered(filter()), activeTodos, completedTodos,
                                    isEmpty, hasNoMatches, hasActive, hasCompleted, hasStoredCompleted);
    }

    bool apply(const whatsui::todo::TodoActionResult& result)
    {
        if (!result.succeeded()) {
            operationMessage.set(result.message);
            hasOperationMessage.set(true);
            return false;
        }
        operationMessage.set({});
        hasOperationMessage.set(false);
        todos.set(controller.records());
        synchronize();
        const auto& presentation = interaction.undoPresentation();
        hasUndo.set(presentation.visible);
        undoMessage.set(presentation.message);
        return true;
    }

    std::unique_ptr<wui::Node> build(wui::UiWindow& window)
    {
        synchronize();
        return buildTodoUi(
            todos, isEmpty, hasNoMatches, activeTodos, completedTodos, hasActive, hasCompleted, hasStoredCompleted,
            filterAll, filterActive, filterCompleted, hasOperationMessage, operationMessage, hasUndo, undoMessage,
            [this](std::string value) { return apply(interaction.addTask(std::move(value))); },
            [this](int id) { (void)apply(interaction.toggleTask(id)); },
            [this](int id) { (void)apply(interaction.removeTask(id)); },
            [this] { (void)apply(interaction.clearCompleted()); },
            [this] { (void)apply(interaction.undo()); },
            [this](whatsui::todo::TodoFilter value) {
                filterAll.set(value == whatsui::todo::TodoFilter::All);
                filterActive.set(value == whatsui::todo::TodoFilter::Active);
                filterCompleted.set(value == whatsui::todo::TodoFilter::Completed);
                synchronize();
            },
            [](std::string) {},
            [this, &window](int id) {
                const auto current = std::find_if(controller.records().begin(), controller.records().end(),
                                                  [id](const Todo& item) { return item.id == id; });
                if (current == controller.records().end() || !interaction.beginEdit(id).succeeded()) return;
                const Todo snapshot = *current;
                showEditDialog(window, interaction.editDraft(), snapshot.important, snapshot.dueDateIso,
                               [this](std::string title, bool important,
                                      std::optional<std::string> dueDate) {
                                   interaction.setEditDraft(std::move(title));
                                   const auto result = interaction.commitEditDetails(important, std::move(dueDate));
                                   (void)apply(result);
                                   return result;
                               },
                               [this] { interaction.cancelEdit(); });
            },
            [&window](std::string title, std::string detail, std::function<void()> confirm) {
                showConfirmation(window, std::move(title), std::move(detail), std::move(confirm));
            },
            [this](int id, bool important) { (void)apply(interaction.setImportant(id, important)); });
    }
};

void testTodoUiTreeInputAndDestructiveConfirmation()
{
    wui::UiWindow window(std::make_unique<FakeWindow>());
    TodoUiHarness harness;
    window.setRoot(harness.build(window));
    commitFrame(window);

    auto* input = composer(window.root());
    expect(input != nullptr, "Todo tree should expose its real composer TextInput");
    click(window, *input);
    expect(window.dispatchTextInput({window.id(), "Review Windows Todo"}),
           "Composer should receive routed committed text");
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 13, 0, false}),
           "Composer Enter should submit through its real TextInput handler");
    commitFrame(window);

    expect(harness.controller.records().size() == 1
               && harness.controller.records().front().title == "Review Windows Todo",
           "Composer input plus Enter should add through the Todo UI callback");
    expect(containsText(window.root(), "0 of 1 done"),
           "Todo summary text should reflect the added task through the rendered tree");

    auto* markImportant = importantButton(window.root(), "Mark important: Review Windows Todo");
    expect(markImportant != nullptr, "A new Todo should expose a named important toggle");
    click(window, *markImportant);
    commitFrame(window);
    expect(harness.controller.records().front().important,
           "Important toggle should update Todo data through the real row callback");
    auto* removeImportant = importantButton(window.root(), "Remove important from Review Windows Todo");
    expect(removeImportant != nullptr,
           "An important Todo should rebuild its toggle with the current accessible action");
    window.focusManager().setFocused(removeImportant);
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 32, 0, false}),
           "Focused important toggle should activate from Space");
    commitFrame(window);
    expect(!harness.controller.records().front().important,
           "Keyboard activation should remove the important state through the real row callback");

    auto* editDetails = button(window.root(), "Edit");
    expect(editDetails != nullptr, "Todo row should expose its details editor");
    click(window, *editDetails);
    commitFrame(window);
    expect(window.hasDialog(), "Edit should open the real Todo details dialog");
    std::vector<wui::TextInput*> detailFields;
    collect(dialogRoot(window), detailFields);
    auto* dueDateField = static_cast<wui::TextInput*>(nullptr);
    for (auto* field : detailFields) {
        if (field->placeholder() == "YYYY-MM-DD (optional)") dueDateField = field;
    }
    expect(dueDateField != nullptr, "Todo details dialog should expose its due-date field");
    dueDateField->text("2023-02-29");
    std::vector<wui::Checkbox*> dialogChecks;
    collect(dialogRoot(window), dialogChecks);
    expect(dialogChecks.size() == 1 && dialogChecks.front()->label() == "Important",
           "Todo details dialog should expose one named Important checkbox");
    click(window, *dialogChecks.front());
    auto* saveDetails = dialogButton(window, "Save");
    expect(saveDetails != nullptr, "Todo details dialog should expose Save");
    click(window, *saveDetails);
    expect(window.hasDialog() && containsText(dialogRoot(window), "Use a valid date in YYYY-MM-DD format."),
           "Invalid due date should keep the details dialog open with inline validation");
    expect(harness.controller.records().front().title == "Review Windows Todo"
               && !harness.controller.records().front().important
               && !harness.controller.records().front().dueDateIso,
           "Invalid details Save must not partially commit any task field");
    dueDateField->text("2026-07-20");
    window.focusManager().setFocused(dueDateField);
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 13, 0, false}),
           "Due-date Enter should submit the complete task details form");
    commitFrame(window);
    expect(!window.hasDialog() && harness.controller.records().front().important
               && harness.controller.records().front().dueDateIso
               == std::optional<std::string>{"2026-07-20"},
           "Corrected details should commit priority and due date together and close the dialog");
    expect(containsText(window.root(), "Due 2026-07-20"),
           "Committed due date should appear in the real task row metadata rail");
    auto* undoDetails = button(window.root(), "Undo");
    expect(undoDetails != nullptr, "Atomic details Save should expose one Undo action");
    click(window, *undoDetails);
    commitFrame(window);
    expect(!harness.controller.records().front().important
               && !harness.controller.records().front().dueDateIso,
           "One Todo Undo should restore all details changed by the dialog Save");

    // Exercise the real primary Todo command rather than an isolated Button.
    // The pressed gesture deliberately ends outside the control: this is the
    // desktop drag-out path which must clear transient state without adding a
    // second task.
    auto* add = button(window.root(), "Add");
    expect(add != nullptr, "Todo composer should expose its real primary Add command");
    const auto addCenter = center(*add);
    const wui::PointF addOutside{add->bounds().x - 2.0f, addCenter.y};
    expect(window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Move,
                                   wui::MouseButton::None, addCenter, 0}),
           "Moving over Todo Add should route hover through the real UI tree");
    expect(hasVisualState(*add, wui::ControlVisualState::Hovered),
           "Todo Add should expose its Fluent hovered visual state");
    expect(window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Down,
                                   wui::MouseButton::Left, addCenter, 0}),
           "Todo Add should enter its pressed state on pointer down");
    expect(hasVisualState(*add, wui::ControlVisualState::Pressed)
               && hasVisualState(*add, wui::ControlVisualState::Focused),
           "Pressed Todo Add should retain both pressed and visible focus states");
    expect(window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Move,
                                   wui::MouseButton::None, addOutside, 0}),
           "Captured Todo Add should receive pointer moves after the cursor leaves it");
    expect(!hasVisualState(*add, wui::ControlVisualState::Hovered)
               && hasVisualState(*add, wui::ControlVisualState::Pressed),
           "Todo Add drag-out should clear hover while retaining pressed until pointer up");
    expect(window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Up,
                                   wui::MouseButton::Left, addOutside, 0}),
           "Todo Add should finish a drag-out pointer sequence");
    expect(!hasVisualState(*add, wui::ControlVisualState::Pressed)
               && harness.controller.records().size() == 1,
           "Todo Add drag-out must clear pressed state without invoking the command");

    // Disabled behavior is verified on the same primary Todo command. This
    // guards both pointer routing and keyboard traversal; it is intentionally
    // not a stand-alone generic-control fixture.
    add->setEnabled(false);
    expect(!add->isEnabled() && hasVisualState(*add, wui::ControlVisualState::Disabled),
           "Disabled Todo Add should expose its Fluent disabled visual state");
    expect(!window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Down,
                                    wui::MouseButton::Left, addCenter, 0}),
           "Disabled Todo Add must reject pointer activation at the window route");
    expect(!hasVisualState(*add, wui::ControlVisualState::Pressed)
               && harness.controller.records().size() == 1,
           "Disabled Todo Add must not become pressed or mutate Todo data");
    window.focusManager().setFocused(input);
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 9, 0, false}),
           "Tab should continue through the Todo tree when Add is disabled");
    expect(window.focusManager().focused() != add,
           "Disabled Todo Add must be skipped by real Todo keyboard traversal");
    add->setEnabled(true);
    expect(add->isEnabled() && !hasVisualState(*add, wui::ControlVisualState::Disabled),
           "Todo Add should restore from its deliberate disabled smoke state");

    // Real Tab traversal must move focus between controls, expose the Fluent
    // focus state, and let Shift+Tab walk back to the preceding command.
    window.focusManager().clear();
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 9, 0, false}),
           "Tab should focus the first Todo control through UiWindow routing");
    auto* firstTabFocus = window.focusManager().focused();
    auto* firstTabControl = dynamic_cast<wui::ControlNode*>(firstTabFocus);
    expect(firstTabControl != nullptr
               && (firstTabControl->visualStates() & wui::toMask(wui::ControlVisualState::Focused)) != 0,
           "Tab-focused Todo control should expose its visible Fluent focus state");
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 9, 0, false}),
           "Second Tab should advance through the Todo control sequence");
    auto* secondTabFocus = window.focusManager().focused();
    auto* secondTabControl = dynamic_cast<wui::ControlNode*>(secondTabFocus);
    expect(secondTabFocus != nullptr && secondTabFocus != firstTabFocus,
           "Second Tab should move focus to a distinct Todo control");
    expect((firstTabControl->visualStates() & wui::toMask(wui::ControlVisualState::Focused)) == 0
               && secondTabControl != nullptr
               && (secondTabControl->visualStates() & wui::toMask(wui::ControlVisualState::Focused)) != 0,
           "Tab should clear the old focus state and visibly focus the next Todo control");
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 9, wui::KeyModifierShift, false}),
           "Shift+Tab should reverse through the Todo control sequence");
    expect(window.focusManager().focused() == firstTabFocus
               && (firstTabControl->visualStates() & wui::toMask(wui::ControlVisualState::Focused)) != 0,
           "Shift+Tab should restore the preceding Todo control and its visible focus state");

    std::vector<wui::Checkbox*> checkboxes;
    collect(window.root(), checkboxes);
    expect(checkboxes.size() == 1, "Added Todo row should expose a real checkbox");

    // The row checkbox is the Todo-specific primary completion affordance.
    // Validate its hover/pressed transitions and drag-out cancellation before
    // testing the normal click path below.
    auto* taskCheckbox = checkboxes.front();
    const auto checkboxCenter = center(*taskCheckbox);
    const wui::PointF checkboxOutside{taskCheckbox->bounds().x - 2.0f, checkboxCenter.y};
    expect(window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Move,
                                   wui::MouseButton::None, checkboxCenter, 0}),
           "Moving over a Todo checkbox should route hover through the rendered row");
    expect(hasVisualState(*taskCheckbox, wui::ControlVisualState::Hovered),
           "Todo checkbox should expose its Fluent hovered visual state");
    expect(window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Down,
                                   wui::MouseButton::Left, checkboxCenter, 0}),
           "Todo checkbox should enter its pressed state on pointer down");
    expect(hasVisualState(*taskCheckbox, wui::ControlVisualState::Pressed),
           "Todo checkbox should expose its pressed visual state during a pointer gesture");
    expect(window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Move,
                                   wui::MouseButton::None, checkboxOutside, 0}),
           "Captured Todo checkbox should receive pointer moves after drag-out");
    expect(!hasVisualState(*taskCheckbox, wui::ControlVisualState::Hovered)
               && hasVisualState(*taskCheckbox, wui::ControlVisualState::Pressed),
           "Todo checkbox drag-out should clear hover while preserving pressed until pointer up");
    expect(window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Up,
                                   wui::MouseButton::Left, checkboxOutside, 0}),
           "Todo checkbox should complete its drag-out pointer sequence");
    expect(!hasVisualState(*taskCheckbox, wui::ControlVisualState::Pressed)
               && !harness.controller.records().front().completed,
           "Todo checkbox drag-out must clear pressed state without toggling completion");

    // Pointer activation is the ordinary desktop path. A Todo checkbox moves
    // between separately keyed active/completed sections, so this explicitly
    // verifies the real mouse route and its resulting presentation update.
    click(window, *checkboxes.front());
    commitFrame(window);
    expect(harness.controller.records().front().completed, "Checkbox mouse click should complete Todo work");
    expect(containsText(window.root(), "1 of 1 done"),
           "Todo summary text should update after the mouse checkbox toggle");

    auto* deleteTask = deleteButton(window.root());
    expect(deleteTask != nullptr, "Completed Todo row should retain its Delete task icon button");
    click(window, *deleteTask);
    expect(window.hasDialog(), "Delete must show a confirmation dialog before mutating Todo data");
    expect(harness.controller.records().size() == 1, "Delete confirmation must not remove a task before approval");
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 27, 0, false}),
           "Escape should dismiss the Todo delete confirmation");
    expect(!window.hasDialog() && harness.controller.records().size() == 1,
           "Escape should cancel deletion without losing the task");
    expect(window.focusManager().focused() == deleteTask,
           "Escape should restore focus to the destructive Todo command that opened the dialog");

    auto* clearDone = button(window.root(), "Clear done");
    expect(clearDone != nullptr, "Completed work should reveal the real Clear done action");
    click(window, *clearDone);
    expect(window.hasDialog(), "Clear done must show confirmation before removing completed tasks");
    expect(harness.controller.records().size() == 1, "Clear confirmation must leave completed data intact before approval");
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 27, 0, false}),
           "Escape should dismiss the Todo clear confirmation");
    expect(!window.hasDialog() && harness.controller.records().size() == 1,
           "Escape should cancel Clear done without losing completed work");
    expect(window.focusManager().focused() == clearDone,
           "Escape should restore focus to the Clear done command");

    // Confirming Delete removes the row that originally held focus.  The
    // real keyed tree must detach that row and clear focus rather than retain
    // a pointer to its now-destroyed destructive command.
    deleteTask = deleteButton(window.root());
    expect(deleteTask != nullptr, "Completed Todo row should still expose Delete after cancellation");
    click(window, *deleteTask);
    commitFrame(window);
    auto* remove = dialogButton(window, "Remove");
    expect(remove != nullptr, "Todo delete confirmation should expose its real Remove command");
    click(window, *remove);
    commitFrame(window);
    expect(!window.hasDialog(), "Confirming Todo deletion should close its dialog");
    expect(harness.controller.records().empty(), "Confirming Delete should remove the task from Todo data");
    expect(containsText(window.root(), "0 of 0 done"),
           "Todo summary should update after confirming the last-task deletion");
    expect(deleteButton(window.root()) == nullptr,
           "Confirmed deletion should remove the destructive row command from the rendered tree");
    expect(window.focusManager().focused() == nullptr,
           "Confirmed deletion must not leave focus pointing at the removed Todo command");

    // Recreate one completed task through the real composer and verify the
    // distinct Clear done confirmation path.  This covers the bulk mutation
    // whose invoking button is also removed as part of the state update.
    input = composer(window.root());
    expect(input != nullptr, "Empty Todo state should retain the real composer TextInput");
    click(window, *input);
    expect(window.dispatchTextInput({window.id(), "Clear completed task"}),
           "Recreated Todo composer should receive routed committed text");
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 13, 0, false}),
           "Recreated Todo composer Enter should add through its real handler");
    commitFrame(window);
    checkboxes.clear();
    collect(window.root(), checkboxes);
    expect(checkboxes.size() == 1, "Recreated Todo row should expose one real checkbox");
    window.focusManager().setFocused(checkboxes.front());
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 32, 0, false}),
           "Recreated Todo checkbox should complete from Space through UiWindow routing");
    commitFrame(window);
    expect(containsText(window.root(), "1 of 1 done"),
           "Todo summary should reflect the recreated completed task before clearing");

    // Add a separate active row to verify the Fluent checkbox keyboard
    // contract without reusing a control that has moved sections: Enter is
    // ignored and Space toggles the focused checkbox.
    input = composer(window.root());
    expect(input != nullptr, "Todo composer should remain available after Space completion");
    click(window, *input);
    expect(window.dispatchTextInput({window.id(), "Enter completed task"}),
           "Second Todo composer should receive routed committed text");
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 13, 0, false}),
           "Second Todo composer Enter should add a task through its real handler");
    commitFrame(window);
    checkboxes.clear();
    collect(window.root(), checkboxes);
    expect(checkboxes.size() == 2, "One completed and one active Todo should expose two real checkboxes");
    auto* enterCheckbox = static_cast<wui::Checkbox*>(nullptr);
    for (auto* checkbox : checkboxes) {
        if (!checkbox->isChecked()) {
            enterCheckbox = checkbox;
            break;
        }
    }
    expect(enterCheckbox != nullptr, "Second Todo should remain an active checkbox before keyboard activation");
    window.focusManager().setFocused(enterCheckbox);
    expect(!window.dispatchKey({window.id(), wui::KeyAction::Down, 13, 0, false}),
           "Focused Todo checkbox must ignore Enter through UiWindow routing");
    commitFrame(window);
    expect(harness.controller.records().size() == 2
               && harness.controller.records()[0].completed && !harness.controller.records()[1].completed,
           "Checkbox Enter must leave Todo completion unchanged through the rendered tree");
    expect(containsText(window.root(), "1 of 2 done"),
           "Todo summary should remain unchanged after ignored checkbox Enter");
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 32, 0, false}),
           "Focused Todo checkbox should complete from Space through UiWindow routing");
    commitFrame(window);
    expect(harness.controller.records().size() == 2
               && harness.controller.records()[0].completed && harness.controller.records()[1].completed,
           "Checkbox Space should update Todo completion through the rendered tree");
    expect(containsText(window.root(), "2 of 2 done"),
           "Todo summary should reflect the checkbox Space completion");

    clearDone = button(window.root(), "Clear done");
    expect(clearDone != nullptr, "Completed Todo work should expose Clear done before approval");
    click(window, *clearDone);
    commitFrame(window);
    remove = dialogButton(window, "Remove");
    expect(remove != nullptr, "Clear done confirmation should expose its real Remove command");
    click(window, *remove);
    commitFrame(window);
    expect(!window.hasDialog(), "Confirming Clear done should close its dialog");
    expect(harness.controller.records().empty(), "Confirming Clear done should remove completed Todo data");
    expect(containsText(window.root(), "0 of 0 done"),
           "Todo summary should return to the empty count after confirmed Clear done");
    expect(button(window.root(), "Clear done") == nullptr,
           "Confirmed Clear done should remove its command from the rendered tree");
    expect(window.focusManager().focused() == nullptr,
           "Confirmed Clear done must not leave focus pointing at its removed command");
    // The declarative tree owns State subscriptions. Detach it while this
    // harness still owns those States; otherwise local destruction order
    // would deliberately test a dangling subscription rather than UI input.
    window.setRoot({});
}

void testTodoAccessibilityProjectionUsesRealTaskTree()
{
    wui::UiWindow window(std::make_unique<FakeWindow>());
    TodoUiHarness harness;
    expect(harness.apply(harness.interaction.addTask("Accessible Todo task")),
           "Accessibility fixture should add a real Todo record");
    window.setRoot(harness.build(window));
    commitFrame(window);

    auto snapshot = window.accessibilitySnapshot();
    expect(snapshot.size() > 1 && snapshot.front().properties.role == wui::AccessibilityRole::Application,
           "Todo window snapshot should begin with the application semantic root");
    const auto* composerEntry = semanticEntry(snapshot, wui::AccessibilityRole::TextField, "Add a task");
    expect(composerEntry != nullptr && composerEntry->properties.value && composerEntry->properties.value->empty(),
           "Todo composer should expose its placeholder name and current editable value");
    const auto* taskEntry = semanticEntry(snapshot, wui::AccessibilityRole::CheckBox, "Accessible Todo task");
    expect(taskEntry != nullptr && taskEntry->properties.checked && !*taskEntry->properties.checked,
           "Todo task checkbox should expose a task-name semantic label and unchecked state");
    expect(semanticEntry(snapshot, wui::AccessibilityRole::Button, "Delete task") != nullptr,
           "Todo compact delete affordance should retain its accessible command name");
    const auto* importantEntry = semanticEntry(snapshot, wui::AccessibilityRole::CheckBox,
                                                "Mark important: Accessible Todo task");
    expect(importantEntry != nullptr && importantEntry->properties.checked
               && !*importantEntry->properties.checked,
           "Todo important affordance should expose a task-specific name and unchecked state");

    auto* important = importantButton(window.root(), "Mark important: Accessible Todo task");
    expect(important != nullptr, "Accessibility fixture should expose the real important affordance");
    click(window, *important);
    commitFrame(window);
    snapshot = window.accessibilitySnapshot();
    importantEntry = semanticEntry(snapshot, wui::AccessibilityRole::CheckBox,
                                   "Remove important from Accessible Todo task");
    expect(importantEntry != nullptr && importantEntry->properties.checked
               && *importantEntry->properties.checked,
           "Important state changes should update the accessible name and checked state");

    std::vector<wui::Checkbox*> checkboxes;
    collect(window.root(), checkboxes);
    expect(checkboxes.size() == 1, "Accessibility fixture should expose the real Todo checkbox");
    click(window, *checkboxes.front());
    commitFrame(window);

    snapshot = window.accessibilitySnapshot();
    taskEntry = semanticEntry(snapshot, wui::AccessibilityRole::CheckBox, "Accessible Todo task");
    expect(taskEntry != nullptr && taskEntry->properties.checked && *taskEntry->properties.checked,
           "Todo accessibility snapshot should update checked state after the real completion gesture");

    auto* remove = deleteButton(window.root());
    expect(remove != nullptr, "Completed Todo should retain the real delete command for modal coverage");
    click(window, *remove);
    expect(window.hasDialog(), "Todo delete command should open its actual confirmation dialog");

    snapshot = window.accessibilitySnapshot();
    expect(semanticEntry(snapshot, wui::AccessibilityRole::Dialog, "") != nullptr,
           "Active Todo confirmation should project its dialog semantic root");
    expect(semanticEntry(snapshot, wui::AccessibilityRole::Button, "Remove") != nullptr
               && semanticEntry(snapshot, wui::AccessibilityRole::Button, "Cancel") != nullptr,
           "Active Todo confirmation should expose only its actionable dialog commands");
    expect(semanticEntry(snapshot, wui::AccessibilityRole::TextField, "Add a task") == nullptr
               && semanticEntry(snapshot, wui::AccessibilityRole::CheckBox, "Accessible Todo task") == nullptr,
           "An active Todo modal must isolate background controls from the accessibility projection");

    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 27, 0, false}),
           "Escape should dismiss the Todo accessibility modal fixture");
    expect(!window.hasDialog(), "Escape should restore the Todo page after the accessibility modal fixture");
    snapshot = window.accessibilitySnapshot();
    expect(semanticEntry(snapshot, wui::AccessibilityRole::CheckBox, "Accessible Todo task") != nullptr,
           "Dismissed Todo modal should restore page semantics to the accessibility projection");
    window.setRoot({});
}

} // namespace

int main()
{
#ifdef _MSC_VER
    // A test failure must be observable by CTest/CI, never block an unattended
    // developer session behind the Debug CRT assertion UI.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    try {
        testTodoUiTreeInputAndDestructiveConfirmation();
        testTodoAccessibilityProjectionUsesRealTaskTree();
        std::puts("WhatsUI Todo UI interaction smoke tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "WhatsUI Todo UI interaction smoke tests failed: %s\n", error.what());
        return 1;
    }
}
