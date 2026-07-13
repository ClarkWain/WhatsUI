// A small but complete TODO-list application built with WhatsUI and rendered
// with the real WhatsCanvas software backend. It exercises the full product
// path: a reactive data model, a declarative reactive UI (ForEach list with
// per-item toggle/delete), and real text-measured layout.
//
// Every list action is attached to a real Button. State-driven list rebuilds are
// deferred to the frame boundary, so a toggle or delete may safely originate in
// the row that it changes.
//
// Only built when WHATSUI_WITH_WHATSCANVAS=ON.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/ui.h"
#include "wui/whatscanvas_text.h"

#include "todo_model.h"
#include "todo_controller.h"
#include "todo_interaction.h"
#include "todo_storage.h"

#ifdef WUI_TODO_INTERACTIVE
#include "wui/glfw_platform.h"
#endif

namespace {

using Todo = whatsui::todo::TodoRecord;

std::filesystem::path interactiveTodoStorePath()
{
    if (const char* localAppData = std::getenv("LOCALAPPDATA"); localAppData != nullptr && *localAppData != '\0') {
        return std::filesystem::path(localAppData) / "WhatsUI" / "Todo" / "todos.store";
    }
    // GLFW can also be hosted in a portable/sandboxed Windows environment.
    // Keep the fallback explicit and local rather than losing user work.
    return std::filesystem::current_path() / "WhatsUI" / "Todo" / "todos.store";
}

using ConfirmationRequest = std::function<void(std::string title,
                                               std::string detail,
                                               std::function<void()> confirm)>;
using EditRequest = std::function<void(int)>;

// Keep presentation groups as explicit States instead of filtering in a row
// factory.  That makes empty sections structurally absent (rather than merely
// empty-looking), and gives the destructive list action a truthful visibility
// contract. `visible` is deliberately supplied by the interaction/controller
// layer: search and filter are product state, not row-factory side effects.
void synchronizeTodoPresentation(const std::vector<Todo>& all,
                                 const std::vector<Todo>& visible,
                                 wui::State<std::vector<Todo>>& activeTodos,
                                 wui::State<std::vector<Todo>>& completedTodos,
                                 wui::State<bool>& isEmpty,
                                 wui::State<bool>& hasNoMatches,
                                 wui::State<bool>& hasActive,
                                 wui::State<bool>& hasCompleted,
                                 wui::State<bool>& hasStoredCompleted)
{
    std::vector<Todo> active;
    std::vector<Todo> completed;
    for (const auto& item : visible) {
        (item.completed ? completed : active).push_back(item);
    }
    const bool storedCompleted = std::any_of(all.begin(), all.end(), [](const Todo& item) {
        return item.completed;
    });
    activeTodos.set(std::move(active));
    completedTodos.set(std::move(completed));
    isEmpty.set(all.empty());
    hasNoMatches.set(!all.empty() && visible.empty());
    hasActive.set(!activeTodos.get().empty());
    hasCompleted.set(!completedTodos.get().empty());
    hasStoredCompleted.set(storedCompleted);
}

#ifdef WUI_TODO_INTERACTIVE
void showConfirmation(wui::UiWindow& window,
                      std::string title,
                      std::string detail,
                      std::function<void()> confirm)
{
    using namespace wui::ui;
    // The dialog is deliberately built at the point of invocation so each
    // request owns its action and the window can restore focus to the exact
    // destructive control that opened it if the user presses Escape/Cancel.
    auto dialog = Dialog().maxWidth(360.0f).content(
        Box().width(360.0f).padding(wui::InsetsF{24.0f, 20.0f, 20.0f, 20.0f}).children(
            Column().gap(16.0f).align(wui::Alignment::Stretch).children(
                Column().gap(5.0f).children(
                    Text(std::move(title)).size(18.0f).lineHeight(24.0f).color({36, 36, 36, 255}),
                    Text(std::move(detail)).wrap().size(13.0f).lineHeight(19.0f).color({97, 97, 97, 255})),
                Row().align(wui::Alignment::Center).gap(8.0f).children(
                    Spacer().flex(1.0f),
                    Button("Cancel").variant(wui::ButtonVariant::Ghost)
                        .onClick([&window] { (void)window.dismissTopDialog(); }),
                    Button("Remove").variant(wui::ButtonVariant::Danger)
                        .onClick([&window, confirm = std::move(confirm)]() mutable {
                            // Restore focus and detach the modal before the
                            // state mutation can remove its invoking row.
                            (void)window.dismissTopDialog();
                            confirm();
                        }))))).intoDialog();
    (void)window.showDialog(std::move(dialog));
}

// Editing is intentionally a compact modal rather than an in-row expansion:
// it preserves the task rail at narrow widths, keeps the IME target singular,
// and makes Save / Cancel equally discoverable on touch and desktop input.
void showEditDialog(wui::UiWindow& window,
                    std::string initialTitle,
                    std::function<whatsui::todo::TodoActionResult(std::string)> save,
                    std::function<void()> cancel)
{
    using namespace wui::ui;
    auto editor = std::make_unique<wui::TextInput>("Task title");
    auto* editorRaw = editor.get();
    editorRaw->text(std::move(initialTitle));
    editorRaw->setFlex(1.0f);

    auto error = std::make_unique<wui::Text>();
    auto* errorRaw = error.get();
    errorRaw->setFontSize(12.0f);
    errorRaw->setLineHeight(18.0f);
    errorRaw->setColor({196, 43, 28, 255});

    // Dialog dismissal also covers Escape and an enabled backdrop. A shared
    // completion marker prevents a successful Save from being interpreted as
    // cancellation when it dismisses the modal through the same window API.
    auto saved = std::make_shared<bool>(false);
    auto submit = [&window, editorRaw, errorRaw, saved, save = std::move(save)]() mutable {
        const auto result = save(editorRaw->model().text());
        if (result.succeeded()) {
            *saved = true;
            (void)window.dismissTopDialog();
            return;
        }
        // Retain focus and the user's draft. This is important for duplicate
        // and empty-title validation, which should never discard IME text.
        errorRaw->setValue(result.message);
    };
    editorRaw->onSubmit(submit);

    auto dialog = Dialog().maxWidth(420.0f)
        .onDismiss([saved, cancel] {
            if (!*saved) cancel();
        })
        .content(
        Box().width(420.0f).padding(wui::InsetsF{24.0f, 20.0f, 20.0f, 20.0f}).children(
            Column().gap(14.0f).align(wui::Alignment::Stretch).children(
                Column().gap(4.0f).children(
                    Text("Edit task").size(18.0f).lineHeight(24.0f).color({36, 36, 36, 255}),
                    Text("Choose a clear, distinct title for My day.").size(13.0f).lineHeight(19.0f).color({97, 97, 97, 255})),
                std::move(editor),
                std::move(error),
                Row().align(wui::Alignment::Center).gap(8.0f).children(
                    Spacer().flex(1.0f),
                    Button("Cancel").variant(wui::ButtonVariant::Ghost)
                        .onClick([&window] { (void)window.dismissTopDialog(); }),
                    Button("Save").variant(wui::ButtonVariant::Primary).onClick(std::move(submit)))))).intoDialog();
    (void)window.showDialog(std::move(dialog));
    // Start the modal in its only editable field so the Windows text session
    // and IME candidate placement are immediately associated with the task.
    window.focusManager().setFocused(editorRaw);
}
#endif

// Fluent-inspired, intentionally restrained: a warm neutral canvas, a single
// blue accent, 4px-derived spacing and one quiet content surface.  Keeping the
// visual language here (rather than in widget defaults) makes the sample a
// useful reference for applications that bring their own design system.
std::unique_ptr<wui::Node> buildTodoUi(wui::State<std::vector<Todo>>& todos,
                                       wui::State<bool>& isEmpty,
                                       wui::State<bool>& hasNoMatches,
                                       wui::State<std::vector<Todo>>& activeTodos,
                                       wui::State<std::vector<Todo>>& completedTodos,
                                       wui::State<bool>& hasActive,
                                       wui::State<bool>& hasCompleted,
                                       wui::State<bool>& hasStoredCompleted,
                                       wui::State<bool>& filterAll,
                                       wui::State<bool>& filterActive,
                                       wui::State<bool>& filterCompleted,
                                       wui::State<bool>& hasOperationMessage,
                                       wui::State<std::string>& operationMessage,
                                       wui::State<bool>& hasUndo,
                                       wui::State<std::string>& undoMessage,
                                       std::function<bool(std::string)> addTodo,
                                       std::function<void(int)> toggle,
                                       std::function<void(int)> remove,
                                       std::function<void()> clearCompleted,
                                       std::function<void()> undo,
                                       std::function<void(whatsui::todo::TodoFilter)> setFilter,
                                       std::function<void(std::string)> setSearchQuery,
                                       EditRequest edit,
                                       ConfirmationRequest requestConfirmation)
{
    using namespace wui::ui;
    auto summary = [](const std::vector<Todo>& items) {
        int done = 0;
        for (const auto& item : items) {
            if (item.completed) {
                ++done;
            }
        }
        return std::to_string(done) + " of " + std::to_string(items.size()) + " done";
    };

    const auto& colors = wui::theme().colors;
    const wui::Color canvas = colors.background;
    const wui::Color surface = colors.surface;
    const wui::Color ink = colors.text;
    const wui::Color muted = colors.textMuted;
    const wui::Color blue = colors.accent;
    const wui::Color blueSoft = colors.surfaceAlt;

    // The composer is deliberately a real TextInput, not a decorative Text
    // node. Its raw pointer is safe for the lifetime of this returned tree and
    // gives the Add button access to the live IME-backed model.
    auto composer = std::make_unique<wui::TextInput>("Add a task for today");
    auto* composerRaw = composer.get();
    composerRaw->setFlex(1.0f);
    auto submit = [addTodo, composerRaw] {
        const std::string value = composerRaw->model().text();
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return;
        const auto last = value.find_last_not_of(" \t\r\n");
        // Retain the draft when validation rejects it; this mirrors the edit
        // dialog and lets a user correct a duplicate without retyping.
        if (addTodo(value.substr(first, last - first + 1))) composerRaw->text("");
    };
    // Enter is the primary desktop completion path. TextInput keeps IME
    // composition separate, so this runs only after text has been committed.
    composerRaw->onSubmit(submit);
    std::unique_ptr<wui::Node> composerNode = std::move(composer);

    return Box()
        .background(canvas)
        .children(Row().align(wui::Alignment::Stretch).children(
            Spacer().flex(1.0f),
            Box().width(720.0f).children(Column()
        .padding(wui::InsetsF{28.0f, 24.0f, 24.0f, 24.0f})
        .gap(16.0f)
        .align(wui::Alignment::Stretch)
        .children(
            Row().align(wui::Alignment::Center).gap(12.0f).children(
                Column().gap(3.0f).children(
                    Text("My day").size(30.0f).lineHeight(38.0f).color(ink),
                    Text("Plan what matters today").size(13.0f).lineHeight(18.0f).color(muted)),
                Spacer().flex(1.0f),
                Box().background(blueSoft).radius(14.0f).padding({12.0f, 7.0f, 12.0f, 7.0f})
                    .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                    .children(Text().bind(todos, summary).size(12.0f).lineHeight(18.0f).color(blue))),
            Box().background(surface).radius(12.0f).padding({12.0f, 10.0f, 10.0f, 10.0f})
                .children(Row().align(wui::Alignment::Center).gap(10.0f).children(
                    std::move(composerNode),
                    Button("Add").variant(wui::ButtonVariant::Primary).onClick(submit))),
            // Search stays on its own rail above the compact view selector.
            // This prevents the filter choice from competing with the composer
            // at portrait widths, while keeping both controls within one tab
            // sequence on Windows keyboard and IME workflows.
            Box().background(surface).radius(12.0f).padding({12.0f, 8.0f, 12.0f, 8.0f})
                .children(SearchField("Search tasks").onChange(setSearchQuery)),
            Box().background(surface).radius(12.0f).padding({12.0f, 7.0f, 12.0f, 7.0f})
                .children(Row().align(wui::Alignment::Center).gap(10.0f).children(
                    Text("VIEW").size(10.0f).lineHeight(14.0f).color(muted),
                    Radio("All").bind(filterAll).onChange([setFilter](bool) {
                        setFilter(whatsui::todo::TodoFilter::All);
                    }),
                    Radio("Active").bind(filterActive).onChange([setFilter](bool) {
                        setFilter(whatsui::todo::TodoFilter::Active);
                    }),
                    Radio("Completed").bind(filterCompleted).onChange([setFilter](bool) {
                        setFilter(whatsui::todo::TodoFilter::Completed);
                    }))),
            If(hasOperationMessage).then([&operationMessage, error = colors.danger] {
                return Box().background({255, 235, 238, 255}).radius(8.0f).padding({12.0f, 8.0f, 12.0f, 8.0f})
                    .children(Text().bind(operationMessage).size(12.0f).lineHeight(18.0f).color(error));
            }),
            // A short-lived action surface turns destructive operations into a
            // recoverable workflow without moving the list or obscuring rows.
            If(hasUndo).then([&undoMessage, undo, blue, blueSoft] {
                return Box().background(blueSoft).radius(10.0f).padding({12.0f, 7.0f, 8.0f, 7.0f})
                    .children(Row().align(wui::Alignment::Center).gap(8.0f).children(
                        Text().bind(undoMessage).size(12.0f).lineHeight(18.0f).color(blue),
                        Spacer().flex(1.0f),
                        Button("Undo").variant(wui::ButtonVariant::Ghost).onClick(undo)));
            }),
            Box().background(blueSoft).radius(12.0f).padding({18.0f, 14.0f, 18.0f, 14.0f})
                .children(Row().align(wui::Alignment::Center).gap(12.0f).children(
                    Column().gap(2.0f).children(
                        Text("TODAY").size(10.0f).lineHeight(14.0f).color(blue),
                        Text().bind(todos, [summary](const std::vector<Todo>& values) { return summary(values) + " - keep going"; })
                            .size(16.0f).lineHeight(22.0f).color(ink)),
                    Spacer().flex(1.0f),
                    Text("My day").size(12.0f).lineHeight(18.0f).color(muted))),
            // Keep the list header in the same content rail as the composer
            // and focus card. The list action belongs beside its heading,
            // rather than stranded at the bottom of a tall window.
            Row().align(wui::Alignment::Center).children(
                Column().gap(1.0f).children(
                    Text("Tasks").size(16.0f).lineHeight(22.0f).color(ink),
                    Text("Keep the next important thing in view").size(11.0f).lineHeight(15.0f).color(muted)),
                Spacer().flex(1.0f),
                If(hasStoredCompleted).then([clearCompleted, requestConfirmation] {
                    return Button("Clear done").variant(wui::ButtonVariant::Ghost)
                        .onClick([clearCompleted, requestConfirmation] {
                            requestConfirmation("Clear completed tasks?",
                                                "Completed tasks will be removed from My day.",
                                                clearCompleted);
                        });
                })),
            If(isEmpty).then([muted] {
                return Box().background({255, 255, 255, 255}).radius(12.0f)
                    .height(104.0f).padding({20.0f, 24.0f, 20.0f, 24.0f})
                    .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                    .children(Column().gap(6.0f).align(wui::Alignment::Center).children(
                        Text("Nothing planned yet").size(16.0f).lineHeight(22.0f).color({36, 36, 36, 255}),
                        Text("Capture the next thing that needs your attention.").size(12.0f).lineHeight(18.0f).color(muted)));
            }),
            If(hasNoMatches).then([muted] {
                return Box().background({255, 255, 255, 255}).radius(12.0f)
                    .height(104.0f).padding({20.0f, 24.0f, 20.0f, 24.0f})
                    .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                    .children(Column().gap(6.0f).align(wui::Alignment::Center).children(
                        Text("No matching tasks").size(16.0f).lineHeight(22.0f).color({36, 36, 36, 255}),
                        Text("Try a different search or view.").size(12.0f).lineHeight(18.0f).color(muted)));
            }),
            // A section exists only when it has rows.  In particular, an
            // all-completed list must not imply that completed work is "in
            // progress", and an empty list must remain a calm empty-state
            // card rather than a heading followed by whitespace.
            If(hasActive).then([&activeTodos, toggle, remove, edit, requestConfirmation] {
                return Column().gap(10.0f).align(wui::Alignment::Stretch).children(
                    Text("IN PROGRESS").size(10.0f).lineHeight(14.0f).color({97, 97, 97, 255}),
                    ForEach<Todo>(activeTodos, [toggle, remove, edit, requestConfirmation](const Todo& item) {
                            return Box().background({255, 255, 255, 255}).radius(10.0f)
                                .padding(wui::InsetsF{14.0f, 11.0f, 10.0f, 11.0f})
                                .children(Row().align(wui::Alignment::Center).gap(10.0f).children(
                                    Checkbox("", item.completed).onChange([toggle, id = item.id](bool) { toggle(id); }),
                                    Column().gap(0.0f).flex(1.0f).children(
                                        Text(item.title).wrap().maxLines(2).ellipsis().size(15.0f).lineHeight(20.0f)
                                            .color({36, 36, 36, 255})),
                                    // Labelled commands remain unambiguous when a
                                    // renderer has no icon-font fallback. Both keep
                                    // the compact 32px Fluent control height.
                                    Button("Edit").variant(wui::ButtonVariant::Ghost).onClick([edit, id = item.id] { edit(id); }),
                                    Button("Delete").variant(wui::ButtonVariant::Ghost)
                                        .onClick([remove, id = item.id, requestConfirmation] {
                                            requestConfirmation("Remove this task?",
                                                                "This task will be removed from My day.",
                                                                [remove, id] { remove(id); });
                                        })));
                    }).gap(8.0f).align(wui::Alignment::Stretch));
            }),
            If(hasCompleted).then([&completedTodos, toggle, remove, edit, requestConfirmation] {
                return Column().gap(10.0f).align(wui::Alignment::Stretch).children(
                    Text("COMPLETED").size(10.0f).lineHeight(14.0f).color({97, 97, 97, 255}),
                    ForEach<Todo>(completedTodos, [toggle, remove, edit, requestConfirmation](const Todo& item) {
                        return Box().background({255, 255, 255, 255}).radius(10.0f)
                            .padding(wui::InsetsF{14.0f, 11.0f, 10.0f, 11.0f})
                            .children(Row().align(wui::Alignment::Center).gap(10.0f).children(
                                Checkbox("", item.completed).onChange([toggle, id = item.id](bool) { toggle(id); }),
                                Column().gap(0.0f).flex(1.0f).children(
                                    Text(item.title).wrap().maxLines(2).ellipsis().size(15.0f).lineHeight(20.0f)
                                        .color({117, 117, 117, 255})),
                                Button("Edit").variant(wui::ButtonVariant::Ghost).onClick([edit, id = item.id] { edit(id); }),
                                Button("Delete").variant(wui::ButtonVariant::Ghost)
                                    .onClick([remove, id = item.id, requestConfirmation] {
                                        requestConfirmation("Remove this task?",
                                                            "This task will be removed from My day.",
                                                            [remove, id] { remove(id); });
                                    })));
                    }).gap(8.0f).align(wui::Alignment::Stretch));
            }),
            Spacer(0.0f, 0.0f))),
            Spacer().flex(1.0f)));
}

} // namespace

int main(int argc, char** argv)
{
#ifdef WUI_TODO_INTERACTIVE
    // This target deliberately uses buildTodoUi() below rather than a separate
    // window-only tree. It is the hands-on counterpart of the captured scenes.
    whatsui::todo::TodoStorage storage(interactiveTodoStorePath());
    const auto loaded = storage.load();
    if (loaded.status == whatsui::todo::TodoLoadStatus::IoError ||
        loaded.status == whatsui::todo::TodoLoadStatus::RecoveredMalformed) {
        std::cerr << "Todo storage: " << loaded.message << std::endl;
    }
    whatsui::todo::TodoController controller(loaded.records);
    whatsui::todo::TodoInteraction interaction(controller);
    wui::State<std::vector<Todo>> todos{controller.records()};
    wui::State<bool> isEmpty{false};
    wui::State<bool> hasNoMatches{false};
    wui::State<std::vector<Todo>> activeTodos;
    wui::State<std::vector<Todo>> completedTodos;
    wui::State<bool> hasActive{true};
    wui::State<bool> hasCompleted{true};
    wui::State<bool> hasStoredCompleted{true};
    wui::State<bool> filterAll{true};
    wui::State<bool> filterActive{false};
    wui::State<bool> filterCompleted{false};
    wui::State<std::string> searchQuery;
    wui::State<bool> hasOperationMessage{false};
    wui::State<std::string> operationMessage;
    wui::State<bool> hasUndo{false};
    wui::State<std::string> undoMessage;
    auto selectedFilter = [&] {
        if (filterActive.get()) return whatsui::todo::TodoFilter::Active;
        if (filterCompleted.get()) return whatsui::todo::TodoFilter::Completed;
        return whatsui::todo::TodoFilter::All;
    };
    auto synchronize = [&] {
        const auto visible = interaction.filtered(selectedFilter(), searchQuery.get());
        synchronizeTodoPresentation(controller.records(), visible, activeTodos, completedTodos,
                                    isEmpty, hasNoMatches, hasActive, hasCompleted, hasStoredCompleted);
    };
    synchronize();
    auto setFilter = [&filterAll, &filterActive, &filterCompleted, &synchronize](whatsui::todo::TodoFilter filter) {
        filterAll.set(filter == whatsui::todo::TodoFilter::All);
        filterActive.set(filter == whatsui::todo::TodoFilter::Active);
        filterCompleted.set(filter == whatsui::todo::TodoFilter::Completed);
        synchronize();
    };
    auto setSearchQuery = [&searchQuery, &synchronize](std::string value) {
        searchQuery.set(std::move(value));
        synchronize();
    };

    auto persist = [&storage](const std::vector<Todo>& items) {
        std::string error;
        if (!storage.save(items, &error)) {
            // The in-memory operation remains valid; a user should never lose
            // a completed edit merely because the storage volume is transient.
            std::cerr << "Todo storage: " << error << std::endl;
        }
    };

    auto synchronizeUndo = [&interaction, &hasUndo, &undoMessage] {
        const auto& presentation = interaction.undoPresentation();
        hasUndo.set(presentation.visible);
        undoMessage.set(presentation.message);
    };
    auto apply = [&controller, &todos, &synchronize, &persist, &hasOperationMessage, &operationMessage, &synchronizeUndo](const whatsui::todo::TodoActionResult& result) {
        if (!result.succeeded()) {
            std::cerr << "Todo: " << result.message << std::endl;
            operationMessage.set(result.message);
            hasOperationMessage.set(true);
            return false;
        }
        operationMessage.set({});
        hasOperationMessage.set(false);
        todos.set(controller.records());
        synchronize();
        if (result.status == whatsui::todo::TodoActionStatus::Success) persist(controller.records());
        synchronizeUndo();
        return true;
    };
    auto addTodo = [&interaction, &apply](std::string text) {
        return apply(interaction.addTask(std::move(text)));
    };
    std::function<void(int)> toggle = [&interaction, &apply](int id) {
        (void)apply(interaction.toggleTask(id));
    };
    std::function<void(int)> remove = [&interaction, &apply](int id) {
        (void)apply(interaction.removeTask(id));
    };
    std::function<void()> clearCompleted = [&interaction, &apply] {
        (void)apply(interaction.clearCompleted());
    };
    std::function<void()> undo = [&interaction, &apply] {
        (void)apply(interaction.undo());
    };

    try {
        return wui::runGlfwApp("WhatsUI Todo", {640.0f, 560.0f},
                                [&todos, &isEmpty, &hasNoMatches, &activeTodos, &completedTodos, &hasActive, &hasCompleted, &hasStoredCompleted,
                                 &filterAll, &filterActive, &filterCompleted,
                                 &hasOperationMessage, &operationMessage,
                                 &hasUndo, &undoMessage,
                                 addTodo, toggle, remove, clearCompleted, undo, setFilter, setSearchQuery, &interaction, &apply](wui::UiWindow& window) {
            return buildTodoUi(todos, isEmpty, hasNoMatches, activeTodos, completedTodos, hasActive, hasCompleted, hasStoredCompleted,
                               filterAll, filterActive, filterCompleted,
                               hasOperationMessage, operationMessage, hasUndo, undoMessage,
                               addTodo, toggle, remove, clearCompleted, undo, setFilter, setSearchQuery,
                               [&window, &interaction, &apply, &hasOperationMessage, &operationMessage](int id) {
                                   const auto begin = interaction.beginEdit(id);
                                   if (!begin.succeeded()) {
                                       (void)apply(begin);
                                       return;
                                   }
                                   showEditDialog(window, interaction.editDraft(),
                                                  [&interaction, &apply](std::string title) {
                                                      const auto result = [&interaction, title = std::move(title)]() mutable {
                                                          interaction.setEditDraft(std::move(title));
                                                          return interaction.commitEdit();
                                                      }();
                                                      (void)apply(result);
                                                      return result;
                                                  },
                                                  [&interaction, &hasOperationMessage, &operationMessage] {
                                                      interaction.cancelEdit();
                                                      operationMessage.set({});
                                                      hasOperationMessage.set(false);
                                                  });
                               },
                               [&window](std::string title, std::string detail, std::function<void()> confirm) {
                                   showConfirmation(window, std::move(title), std::move(detail), std::move(confirm));
                               });
        });
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    }
#else
    int width = 640;
    int height = 560;
    constexpr float scaleFactor = 2.0f;

    // Keep capture paths explicit so a CI job can write into an empty build
    // directory and compare exactly the same four scene files each run.
    // Stable headless contract for visual review:
    //   WhatsUITodoApp <output-dir> --size <width>x<height>
    // The output directory remains the first positional argument for existing
    // CI jobs; unknown arguments fail loudly instead of silently changing a
    // golden capture.
    std::filesystem::path outputDirectory{"todo_visual"};
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--size") {
            if (++index >= argc || std::sscanf(argv[index], "%dx%d", &width, &height) != 2
                || width < 240 || height < 320) {
                std::cerr << "--size expects WIDTHxHEIGHT (minimum 240x320)" << std::endl;
                return 2;
            }
        } else if (argument.rfind("--", 0) == 0) {
            std::cerr << "unknown argument: " << argument << std::endl;
            return 2;
        } else {
            outputDirectory = argument;
        }
    }
    std::filesystem::create_directories(outputDirectory);

    wui::State<std::vector<Todo>> todos;
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
    wui::State<std::string> searchQuery;
    wui::State<bool> hasOperationMessage{false};
    wui::State<std::string> operationMessage;
    wui::State<bool> hasUndo{false};
    wui::State<std::string> undoMessage;
    int nextId = 1;

    auto selectedFilter = [&] {
        if (filterActive.get()) return whatsui::todo::TodoFilter::Active;
        if (filterCompleted.get()) return whatsui::todo::TodoFilter::Completed;
        return whatsui::todo::TodoFilter::All;
    };
    auto synchronize = [&] {
        std::vector<Todo> visible;
        const std::string query = searchQuery.get();
        std::string needle;
        needle.reserve(query.size());
        for (const unsigned char character : query) {
            if (!std::isspace(character)) needle.push_back(static_cast<char>(std::tolower(character)));
        }
        for (const Todo& item : todos.get()) {
            if (selectedFilter() == whatsui::todo::TodoFilter::Active && item.completed) continue;
            if (selectedFilter() == whatsui::todo::TodoFilter::Completed && !item.completed) continue;
            std::string title;
            title.reserve(item.title.size());
            for (const unsigned char character : item.title) title.push_back(static_cast<char>(std::tolower(character)));
            if (!needle.empty() && title.find(needle) == std::string::npos) continue;
            visible.push_back(item);
        }
        synchronizeTodoPresentation(todos.get(), visible, activeTodos, completedTodos,
                                    isEmpty, hasNoMatches, hasActive, hasCompleted, hasStoredCompleted);
    };
    synchronize();
    auto setFilter = [&filterAll, &filterActive, &filterCompleted, &synchronize](whatsui::todo::TodoFilter filter) {
        filterAll.set(filter == whatsui::todo::TodoFilter::All);
        filterActive.set(filter == whatsui::todo::TodoFilter::Active);
        filterCompleted.set(filter == whatsui::todo::TodoFilter::Completed);
        synchronize();
    };
    auto setSearchQuery = [&searchQuery, &synchronize](std::string value) {
        searchQuery.set(std::move(value));
        synchronize();
    };

    auto addTodo = [&todos, &nextId, &synchronize](std::string text) {
        auto items = todos.get();
        items.push_back({nextId++, std::move(text), false, false, std::nullopt});
        todos.set(items);
        synchronize();
        return true;
    };
    std::function<void(int)> toggle = [&todos, &synchronize](int id) {
        auto items = todos.get();
        for (auto& item : items) {
            if (item.id == id) {
                item.completed = !item.completed;
            }
        }
        todos.set(items);
        synchronize();
    };
    std::function<void(int)> remove = [&todos, &synchronize](int id) {
        auto items = todos.get();
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [id](const Todo& item) { return item.id == id; }),
                    items.end());
        todos.set(items);
        synchronize();
    };
    std::function<void()> clearCompleted = [&todos, &synchronize] {
        auto items = todos.get();
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [](const Todo& item) { return item.completed; }),
                    items.end());
        todos.set(items);
        synchronize();
    };

    auto root = buildTodoUi(todos, isEmpty, hasNoMatches, activeTodos, completedTodos, hasActive, hasCompleted, hasStoredCompleted,
                            filterAll, filterActive, filterCompleted,
                            hasOperationMessage, operationMessage, hasUndo, undoMessage,
                            addTodo, toggle, remove, clearCompleted, [] {}, setFilter, setSearchQuery, [](int) {},
                            [](std::string, std::string, std::function<void()> confirm) { confirm(); });

    int frame = 0;
    auto renderFrame = [&]() {
        // Isolate every visual-regression scene. Reusing one Software canvas
        // across pixel readbacks can retain backend target state between scenes.
        auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software,
                                          static_cast<int>(width * scaleFactor),
                                          static_cast<int>(height * scaleFactor));
        if (!canvas || !canvas->initializeContext()) {
            throw std::runtime_error("failed to create software canvas");
        }
        wui::WhatsCanvasTextMeasurer measurer(*canvas, scaleFactor);
        wui::setTextMeasurer(&measurer);

        wui::flushStructuralUpdates();
        root->layout({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});
        wui::PaintContext paint(*canvas, scaleFactor);
        root->prepare(paint);
        for (int pass = 0; pass < 2; ++pass) {
            canvas->beginFrame();
            paint.fillRect({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, wui::theme().colors.surface);
            root->paint(paint);
            canvas->endFrame();
        }
        const auto path = outputDirectory / ("todo_" + std::to_string(frame++) + ".ppm");
        if (!canvas->savePixelsPPM(path.string())) {
            throw std::runtime_error("failed to save " + path.string());
        }
        wui::setTextMeasurer(nullptr);
        std::cout << "wrote " << path << std::endl;
    };

    // Scripted product walkthrough. The first and last scenes prove that the
    // empty-state card is retained without any task section/action; the third
    // scene proves that an all-completed list renders only COMPLETED.
    renderFrame();                 // empty list: no Clear done or task groups
    addTodo("Buy milk");
    addTodo("Write WhatsUI docs");
    addTodo("Ship the release");
    renderFrame();                 // three active items: IN PROGRESS only
    toggle(1);
    toggle(2);
    toggle(3);
    renderFrame();                 // all complete: COMPLETED and Clear done only
    clearCompleted();
    renderFrame();                 // empty again after clear

    return 0;
#endif
}
