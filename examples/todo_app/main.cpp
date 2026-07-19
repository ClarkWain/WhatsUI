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
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
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
#include "wui/animation.h"
#include "wui/glfw_platform.h"
#endif

namespace {

using Todo = whatsui::todo::TodoRecord;

// Keep the sample on the Fluent 2 Windows type ramp. The font family is not
// named here: TextStyleToken comes from the active Theme, which supplies the
// Windows Segoe UI Variable family (or an application's deliberate override).
// Fluent exposes Body 14/20, Caption 12/16, Subtitle 20/28 and Title 28/36
// directly; Body large 18/24 is the one application-level rung used by task
// summaries and row titles.
[[nodiscard]] wui::TextStyleToken todoBodyLarge(bool strong = false)
{
    auto style = wui::theme().typography.windows.bodyLarge;
    if (strong) style.weight = wui::theme().typography.weightSemibold;
    return style;
}

[[nodiscard]] wui::TextStyleToken todoStrong(wui::TextStyleToken style)
{
    style.weight = wui::theme().typography.weightSemibold;
    return style;
}

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
using ImportantRequest = std::function<void(int, bool)>;

#ifdef WUI_TODO_INTERACTIVE
template <class NodeT>
void collectTodoNodes(wui::Node* node, std::vector<NodeT*>& result)
{
    if (node == nullptr) return;
    if (auto* match = dynamic_cast<NodeT*>(node)) result.push_back(match);
    for (const auto& child : node->children()) {
        collectTodoNodes(child.get(), result);
    }
}

// Defined below with the shared Todo tree helpers.  The native smoke uses it
// to exercise a below-fold task row through the real document viewport.
wui::ScrollView* todoScrollView(wui::Node* node);

// A native smoke test must never appear healthy simply because the host failed
// to deliver a frame.  This watchdog exists only for --perf-smoke: ordinary
// interactive windows retain their normal event-loop lifetime.  It also gives
// CI running without a usable graphics desktop an actionable failure instead
// of an indefinitely silent process.
class PerfSmokeWatchdog {
public:
    explicit PerfSmokeWatchdog(std::chrono::milliseconds timeout)
        : worker_([this, timeout] {
            std::unique_lock<std::mutex> lock(mutex_);
            if (completed_.wait_for(lock, timeout, [this] { return completedSuccessfully_; })) {
                return;
            }
            std::fputs("Todo perf smoke failed: no completed native frame within 12 seconds; "
                       "ensure a usable Windows graphics desktop is available.\n", stderr);
            std::fflush(stderr);
            std::_Exit(124);
        })
    {
    }

    PerfSmokeWatchdog(const PerfSmokeWatchdog&) = delete;
    PerfSmokeWatchdog& operator=(const PerfSmokeWatchdog&) = delete;

    ~PerfSmokeWatchdog() { complete(); }

    void complete()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            completedSuccessfully_ = true;
        }
        completed_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

private:
    std::mutex mutex_;
    std::condition_variable completed_;
    bool completedSuccessfully_{false};
    std::thread worker_;
};

// Runs only when WhatsUITodoGlfw receives --perf-smoke. It is deliberately a
// native GLFW/OpenGL scenario, rather than a headless approximation: composer
// entry/submission, task toggling and radio activation all traverse the same
// event, layout, text and present pipeline used by a person at the window.
void installTodoPerformanceSmoke(wui::UiWindow& window)
{
    constexpr auto smokeTimeout = std::chrono::seconds{12};
    auto watchdog = std::make_shared<PerfSmokeWatchdog>(smokeTimeout);
    std::cout << "Todo perf smoke: started (native timeout=12s)" << std::endl;
    struct ActionMetric {
        const char* name{nullptr};
        bool dispatched{false};
        bool accepted{false};
        bool sampled{false};
        std::uint64_t sourceFrame{0};
        std::chrono::steady_clock::time_point started{};
        double latencyMilliseconds{0.0};
        double layoutMilliseconds{0.0};
        double paintMilliseconds{0.0};
    };
    struct Sample {
        bool reported{false};
        double maxUpdate{0.0};
        double maxLayout{0.0};
        double maxPrepare{0.0};
        double maxPaint{0.0};
        ActionMetric composer{"composer-submit"};
        ActionMetric checkbox{"checkbox-toggle"};
        ActionMetric radio{"radio-select"};
    };
    auto sample = std::make_shared<Sample>();
    wui::Animation animation(1.4f, [&window, sample](float progress) {
        const auto& frame = window.frameStats();
        sample->maxUpdate = std::max(sample->maxUpdate, frame.updateMilliseconds);
        sample->maxLayout = std::max(sample->maxLayout, frame.layoutMilliseconds);
        sample->maxPrepare = std::max(sample->maxPrepare, frame.prepareMilliseconds);
        sample->maxPaint = std::max(sample->maxPaint, frame.paintMilliseconds);
        auto sampleAction = [&frame](ActionMetric& action) {
            if (!action.dispatched || action.sampled || frame.frameNumber <= action.sourceFrame) return;
            action.sampled = true;
            action.latencyMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - action.started).count();
            action.layoutMilliseconds = frame.layoutMilliseconds;
            action.paintMilliseconds = frame.paintMilliseconds;
        };
        sampleAction(sample->composer);
        sampleAction(sample->checkbox);
        sampleAction(sample->radio);

        if (!sample->composer.dispatched && progress >= 0.20f) {
            std::vector<wui::TextInput*> fields;
            collectTodoNodes(window.root(), fields);
            const auto composer = std::find_if(fields.begin(), fields.end(), [](const wui::TextInput* field) {
                return field->placeholder() == "Add a task for today";
            });
            if (composer != fields.end() && (*composer)->bounds().width > 0.0f) {
                const auto& bounds = (*composer)->bounds();
                const wui::PointF point{bounds.x + 8.0f, bounds.y + bounds.height * 0.5f};
                const bool pointerDown = window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Down,
                                                                 wui::MouseButton::Left, point, 0});
                const bool pointerUp = window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Up,
                                                               wui::MouseButton::Left, point, 0});
                const bool focused = pointerDown && pointerUp;
                bool textAccepted = true;
                for (const char character : std::string{"Native perf smoke task"}) {
                    textAccepted = window.dispatchTextInput({window.id(), std::string(1, character)}) && textAccepted;
                }
                sample->composer.sourceFrame = frame.frameNumber;
                sample->composer.started = std::chrono::steady_clock::now();
                sample->composer.accepted = focused && textAccepted
                    && window.dispatchKey({window.id(), wui::KeyAction::Down, 13, 0, false});
                sample->composer.dispatched = true;
            }
        }

        if (!sample->checkbox.dispatched && sample->composer.dispatched && progress >= 0.48f) {
            std::vector<wui::Checkbox*> checkboxes;
            collectTodoNodes(window.root(), checkboxes);
            // Task rows live inside the document ScrollView. Move to its end
            // and translate the row's document bounds back into viewport
            // coordinates before dispatching native-style pointer input.
            // This makes the toggle path valid even at the compact 640x560
            // window size, where a freshly submitted row begins below fold.
            auto* scroll = todoScrollView(window.root());
            if (scroll != nullptr) scroll->setScrollOffset(scroll->maxScrollOffset());
            const float scrollOffset = scroll != nullptr ? scroll->scrollOffset() : 0.0f;
            const auto checkbox = std::find_if(checkboxes.rbegin(), checkboxes.rend(), [scroll, scrollOffset](const wui::Checkbox* node) {
                const auto& bounds = node->bounds();
                const wui::PointF viewportPoint{bounds.x + bounds.width * 0.5f,
                                                bounds.y + bounds.height * 0.5f - scrollOffset};
                return bounds.width > 0.0f && (scroll == nullptr || scroll->bounds().contains(viewportPoint));
            });
            if (checkbox != checkboxes.rend()) {
                const auto& bounds = (*checkbox)->bounds();
                const wui::PointF point{bounds.x + bounds.width * 0.5f,
                                        bounds.y + bounds.height * 0.5f - scrollOffset};
                sample->checkbox.sourceFrame = frame.frameNumber;
                sample->checkbox.started = std::chrono::steady_clock::now();
                const bool pointerDown = window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Down,
                                                                 wui::MouseButton::Left, point, 0});
                const bool pointerUp = window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Up,
                                                               wui::MouseButton::Left, point, 0});
                sample->checkbox.accepted = pointerDown && pointerUp;
                sample->checkbox.dispatched = true;
            }
        }

        if (!sample->radio.dispatched && progress >= 0.70f) {
            std::vector<wui::Radio*> radios;
            collectTodoNodes(window.root(), radios);
            if (auto* scroll = todoScrollView(window.root()); scroll != nullptr) {
                scroll->setScrollOffset(0.0f);
            }
            if (radios.size() >= 2 && radios[1]->bounds().width > 0.0f) {
                const auto& bounds = radios[1]->bounds();
                const wui::PointF point{bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f};
                sample->radio.sourceFrame = frame.frameNumber;
                sample->radio.started = std::chrono::steady_clock::now();
                const bool pointerDown = window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Down,
                                                                 wui::MouseButton::Left, point, 0});
                const bool pointerUp = window.dispatchPointer({window.id(), wui::PointerType::Mouse, wui::PointerAction::Up,
                                                               wui::MouseButton::Left, point, 0});
                sample->radio.accepted = pointerDown && pointerUp;
                sample->radio.dispatched = true;
            }
        }

        if (!sample->reported && progress >= 0.9f) {
            sample->reported = true;
            const auto printAction = [](const ActionMetric& action) {
                std::cout << "Todo perf smoke: " << action.name << "="
                          << (action.dispatched && action.accepted && action.sampled ? "ok" : "failed");
                if (action.sampled) {
                    std::cout << " latency=" << action.latencyMilliseconds
                              << "ms layout=" << action.layoutMilliseconds
                              << "ms paint=" << action.paintMilliseconds << "ms";
                }
                std::cout << std::endl;
            };
            std::cout << std::fixed << std::setprecision(3);
            std::cout << "Todo perf smoke: max update=" << sample->maxUpdate
                      << "ms layout=" << sample->maxLayout
                      << "ms prepare=" << sample->maxPrepare
                      << "ms paint=" << sample->maxPaint << "ms" << std::endl;
            printAction(sample->composer);
            printAction(sample->checkbox);
            printAction(sample->radio);
        }
    });
    // Closing from the update callback leaves that callback registered for
    // the next host tick while UiWindow has already been destroyed. Close
    // only from the completion hook, after Animation::tick removes itself.
    animation.onComplete([&window, watchdog] {
        watchdog->complete();
        window.platformWindow().close();
    });
    (void)wui::Ticker::instance().add(std::move(animation));
}
#endif

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

// The Todo page owns one document viewport.  Keeping its lookup local to this
// sample lets the deterministic Software walkthrough capture the bottom of a
// compact page, which proves task rows remain reachable rather than merely
// painted below the 640x560 viewport.
wui::ScrollView* todoScrollView(wui::Node* node)
{
    if (node == nullptr) return nullptr;
    if (auto* scroll = dynamic_cast<wui::ScrollView*>(node)) return scroll;
    for (const auto& child : node->children()) {
        if (auto* scroll = todoScrollView(child.get())) return scroll;
    }
    return nullptr;
}

#ifdef WUI_TODO_INTERACTIVE
void showConfirmation(wui::UiWindow& window,
                      std::string title,
                      std::string detail,
                      std::function<void()> confirm)
{
    using namespace wui::ui;
    const auto& type = wui::theme().typography;
    // The dialog is deliberately built at the point of invocation so each
    // request owns its action and the window can restore focus to the exact
    // destructive control that opened it if the user presses Escape/Cancel.
    auto dialog = Dialog().maxWidth(360.0f).content(
        Box().width(360.0f).padding(wui::InsetsF{24.0f, 20.0f, 20.0f, 20.0f}).children(
            Column().gap(16.0f).align(wui::Alignment::Stretch).children(
                Column().gap(5.0f).children(
                    Text(std::move(title)).style(type.windows.subtitle).color({36, 36, 36, 255}),
                    Text(std::move(detail)).wrap().style(type.windows.body).color({97, 97, 97, 255})),
                Row().align(wui::Alignment::Center).gap(8.0f).children(
                    Spacer().flex(1.0f),
                    Button("Cancel").accessibilityId("todo.confirm.cancel").variant(wui::ButtonVariant::Ghost)
                        .onClick([&window] { (void)window.dismissTopDialog(); }),
                    Button("Remove").accessibilityId("todo.confirm.remove").variant(wui::ButtonVariant::Danger)
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
                    bool initialImportant,
                    std::optional<std::string> initialDueDate,
                    std::function<whatsui::todo::TodoActionResult(
                        std::string, bool, std::optional<std::string>)> save,
                    std::function<void()> cancel)
{
    using namespace wui::ui;
    const auto& type = wui::theme().typography;
    auto editor = std::make_unique<wui::TextInput>("Task title");
    auto* editorRaw = editor.get();
    editorRaw->setAccessibilityId("todo.edit.title");
    editorRaw->text(std::move(initialTitle));
    editorRaw->setFlex(1.0f);

    auto important = std::make_unique<wui::Checkbox>("Important", initialImportant);
    auto* importantRaw = important.get();
    importantRaw->setAccessibilityId("todo.edit.important");

    auto dueDate = std::make_unique<wui::TextInput>("YYYY-MM-DD (optional)");
    auto* dueDateRaw = dueDate.get();
    dueDateRaw->setAccessibilityId("todo.edit.due-date");
    dueDateRaw->text(initialDueDate.value_or(""));
    dueDateRaw->setFlex(1.0f);

    auto error = std::make_unique<wui::Text>();
    auto* errorRaw = error.get();
    errorRaw->setTextStyle(type.windows.body);
    errorRaw->setColor({196, 43, 28, 255});

    // Dialog dismissal also covers Escape and an enabled backdrop. A shared
    // completion marker prevents a successful Save from being interpreted as
    // cancellation when it dismisses the modal through the same window API.
    auto saved = std::make_shared<bool>(false);
    auto submit = [&window, editorRaw, importantRaw, dueDateRaw, errorRaw, saved, save = std::move(save)]() mutable {
        const std::string dueDateText = dueDateRaw->model().text();
        const auto first = dueDateText.find_first_not_of(" \t\r\n");
        std::optional<std::string> dueDateValue;
        if (first != std::string::npos) {
            const auto last = dueDateText.find_last_not_of(" \t\r\n");
            dueDateValue = dueDateText.substr(first, last - first + 1);
        }
        const auto result = save(editorRaw->model().text(), importantRaw->isChecked(), std::move(dueDateValue));
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
    dueDateRaw->onSubmit(submit);

    auto dialog = Dialog().maxWidth(420.0f)
        .onDismiss([saved, cancel] {
            if (!*saved) cancel();
        })
        .content(
        Box().width(320.0f).padding(wui::InsetsF{20.0f, 18.0f, 18.0f, 18.0f}).children(
            Column().gap(14.0f).align(wui::Alignment::Stretch).children(
                Column().gap(4.0f).children(
                    Text("Edit task").style(type.windows.subtitle).color({36, 36, 36, 255}),
                    Text("Update its title, priority, and due date.").wrap().style(type.windows.body).color({97, 97, 97, 255})),
                std::move(editor),
                std::move(important),
                Column().gap(4.0f).align(wui::Alignment::Stretch).children(
                    Text("DUE DATE").style(todoStrong(type.windows.caption)).color({97, 97, 97, 255}),
                    std::move(dueDate)),
                std::move(error),
                Row().align(wui::Alignment::Center).gap(8.0f).children(
                    Spacer().flex(1.0f),
                    Button("Cancel").accessibilityId("todo.edit.cancel").variant(wui::ButtonVariant::Ghost)
                        .onClick([&window] { (void)window.dismissTopDialog(); }),
                    Button("Save").accessibilityId("todo.edit.save").variant(wui::ButtonVariant::Primary).onClick(std::move(submit)))))).intoDialog();
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
                                       ConfirmationRequest requestConfirmation,
                                       ImportantRequest setImportant = {})
{
    using namespace wui::ui;
    if (!setImportant) setImportant = [](int, bool) {};
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
    const auto& type = wui::theme().typography;
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
    composerRaw->setAccessibleLabel("Add a task");
    composerRaw->setAccessibilityId("todo.composer");
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
            Box().width(720.0f).children(
                // The entire page scrolls as one document.  This keeps the
                // title/composer visually coherent at desktop sizes while
                // making every task row reachable at the 640x560 acceptance
                // viewport and on compact portrait windows.
                ScrollView().axis(wui::ScrollAxis::Vertical).children(Column()
        .padding(wui::InsetsF{28.0f, 24.0f, 24.0f, 24.0f})
        .gap(16.0f)
        .align(wui::Alignment::Stretch)
        .children(
            Row().align(wui::Alignment::Center).gap(12.0f).children(
                Column().gap(3.0f).children(
                    Text("My day").style(type.windows.title).color(ink),
                    Text("Plan what matters today").style(type.windows.body).color(muted)),
                Spacer().flex(1.0f),
                Box().background(blueSoft).radius(14.0f).padding({12.0f, 7.0f, 12.0f, 7.0f})
                    .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                    .children(Text().bind(todos, summary).style(type.windows.body).color(blue))),
            Box().background(surface).radius(12.0f).padding({12.0f, 10.0f, 10.0f, 10.0f})
                .children(Row().align(wui::Alignment::Center).gap(10.0f).children(
                    std::move(composerNode),
                    Button("Add").accessibilityId("todo.add").variant(wui::ButtonVariant::Primary).onClick(submit))),
            // Search stays on its own rail above the compact view selector.
            // This prevents the filter choice from competing with the composer
            // at portrait widths, while keeping both controls within one tab
            // sequence on Windows keyboard and IME workflows.
            Box().background(surface).radius(12.0f).padding({12.0f, 8.0f, 12.0f, 8.0f})
                .children(SearchField("Search tasks").accessibilityId("todo.search").onChange(setSearchQuery)),
            Box().background(surface).radius(12.0f).padding({12.0f, 7.0f, 12.0f, 7.0f})
                .children(Row().align(wui::Alignment::Center).gap(10.0f).children(
                    Text("VIEW").style(todoStrong(type.windows.caption)).color(muted),
                    Radio("All").bind(filterAll).onChange([setFilter](bool) {
                        setFilter(whatsui::todo::TodoFilter::All);
                    }),
                    Radio("Active").bind(filterActive).onChange([setFilter](bool) {
                        setFilter(whatsui::todo::TodoFilter::Active);
                    }),
                    Radio("Completed").bind(filterCompleted).onChange([setFilter](bool) {
                        setFilter(whatsui::todo::TodoFilter::Completed);
                    }))),
            If(hasOperationMessage).then([&operationMessage, error = colors.danger, body = type.windows.body] {
                return Box().background({255, 235, 238, 255}).radius(8.0f).padding({12.0f, 8.0f, 12.0f, 8.0f})
                    .children(Text().bind(operationMessage).style(body).color(error));
            }),
            // A short-lived action surface turns destructive operations into a
            // recoverable workflow without moving the list or obscuring rows.
            If(hasUndo).then([&undoMessage, undo, blue, blueSoft, body = type.windows.body] {
                return Box().background(blueSoft).radius(10.0f).padding({12.0f, 7.0f, 8.0f, 7.0f})
                    .children(Row().align(wui::Alignment::Center).gap(8.0f).children(
                        Text().bind(undoMessage).style(body).color(blue),
                        Spacer().flex(1.0f),
                        Button("Undo").accessibilityId("todo.undo").variant(wui::ButtonVariant::Ghost).onClick(undo)));
            }),
            Card().appearance(wui::CardAppearance::FilledAlternative)
                .children(Row().align(wui::Alignment::Center).gap(12.0f).children(
                    Column().gap(2.0f).children(
                        Text("TODAY").style(todoStrong(type.windows.caption)).color(blue),
                        Text().bind(todos, [summary](const std::vector<Todo>& values) { return summary(values) + " - keep going"; })
                            .style(todoBodyLarge()).color(ink)),
                    Spacer().flex(1.0f),
                    Text("My day").style(type.windows.body).color(muted))),
            // Keep the list header in the same content rail as the composer
            // and focus card. The list action belongs beside its heading,
            // rather than stranded at the bottom of a tall window.
            Row().align(wui::Alignment::Center).children(
                Column().gap(1.0f).children(
                    Text("Tasks").style(type.windows.subtitle).color(ink),
                    Text("Keep the next important thing in view").style(type.windows.body).color(muted)),
                Spacer().flex(1.0f),
                If(hasStoredCompleted).then([clearCompleted, requestConfirmation] {
                    return Button("Clear done").accessibilityId("todo.clear-completed").variant(wui::ButtonVariant::Ghost)
                        .onClick([clearCompleted, requestConfirmation] {
                            requestConfirmation("Clear completed tasks?",
                                                "Completed tasks will be removed from My day.",
                                                clearCompleted);
                        });
                })),
            If(isEmpty).then([muted, body = type.windows.body, bodyLarge = todoBodyLarge(true)] {
                return Box().background({255, 255, 255, 255}).radius(12.0f)
                    .height(104.0f).padding({20.0f, 24.0f, 20.0f, 24.0f})
                    .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                    .children(Column().gap(6.0f).align(wui::Alignment::Center).children(
                        Text("Nothing planned yet").style(bodyLarge).color({36, 36, 36, 255}),
                        Text("Capture the next thing that needs your attention.").style(body).color(muted)));
            }),
            If(hasNoMatches).then([muted, body = type.windows.body, bodyLarge = todoBodyLarge(true)] {
                return Box().background({255, 255, 255, 255}).radius(12.0f)
                    .height(104.0f).padding({20.0f, 24.0f, 20.0f, 24.0f})
                    .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                    .children(Column().gap(6.0f).align(wui::Alignment::Center).children(
                        Text("No matching tasks").style(bodyLarge).color({36, 36, 36, 255}),
                        Text("Try a different search or view.").style(body).color(muted)));
            }),
            // A section exists only when it has rows.  In particular, an
            // all-completed list must not imply that completed work is "in
            // progress", and an empty list must remain a calm empty-state
            // card rather than a heading followed by whitespace.
            If(hasActive).then([&activeTodos, toggle, remove, setImportant, edit, requestConfirmation,
                                caption = todoStrong(type.windows.caption), bodyLarge = todoBodyLarge(), metadataStyle = type.windows.caption] {
                return Column().gap(10.0f).align(wui::Alignment::Stretch).children(
                    Text("IN PROGRESS").style(caption).color({97, 97, 97, 255}),
                    KeyedForEach<Todo>(activeTodos,
                        [](const Todo& item) { return std::to_string(item.id); },
                        [toggle, remove, setImportant, edit, requestConfirmation, bodyLarge, metadataStyle](const Todo& item) {
                            const std::string metadata = item.dueDateIso ? "Due " + *item.dueDateIso : std::string{};
                            const std::string automationPrefix = "todo.task." + std::to_string(item.id);
                            return Box().background({255, 255, 255, 255}).radius(10.0f)
                                .padding(wui::InsetsF{14.0f, 11.0f, 10.0f, 11.0f})
                                .children(Row().align(wui::Alignment::Start).gap(10.0f).children(
                                    // Match the 32-DIP primary row (title +
                                    // star action). A 40-DIP wrapper placed
                                    // the completion indicator 4 DIP below the
                                    // title's visual centre.
                                    Box().height(32.0f)
                                        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                                        .children(Checkbox("", item.completed).accessibilityId(automationPrefix + ".complete").accessibleLabel(item.title)
                                            .onChange([toggle, id = item.id](bool) { toggle(id); })),
                                    Column().gap(4.0f).align(wui::Alignment::Stretch).flex(1.0f).children(
                                        // The 4-DIP vertical inset makes a
                                        // 24-DIP title line occupy the same
                                        // 32-DIP primary rail as the
                                        // completion and important actions.
                                        // For a two-line title the rail stays
                                        // top-aligned, so all three controls
                                        // continue to share the first-line
                                        // centre instead of drifting toward
                                        // the full text block's midpoint.
                                        Row().align(wui::Alignment::Start).gap(8.0f).children(
                                            Box().padding(wui::InsetsF{0.0f, 4.0f, 0.0f, 4.0f}).flex(1.0f)
                                                .children(Text(item.title).wrap().maxLines(2).ellipsis().style(bodyLarge)
                                                    .color({36, 36, 36, 255})),
                                            IconButton(wui::IconName::Star,
                                                       (item.important ? "Remove important from " : "Mark important: ") + item.title)
                                                .iconStyle(item.important ? wui::IconStyle::Filled
                                                                          : wui::IconStyle::Regular)
                                                .accessibilityId(automationPrefix + ".important")
                                                .checked(item.important)
                                                .onClick([setImportant, id = item.id, important = item.important] {
                                                    setImportant(id, !important);
                                                })),
                                        // Keep secondary actions on their own compact
                                        // rail so a 360px window never squeezes the
                                        // title and completion affordance into slivers.
                                        Row().align(wui::Alignment::Center).gap(6.0f).children(
                                            Text(metadata).maxLines(1).ellipsis().style(metadataStyle)
                                                .color(item.important ? wui::Color{0, 95, 184, 255} : wui::Color{97, 97, 97, 255}).flex(1.0f),
                                            Button("Edit").accessibilityId(automationPrefix + ".edit").variant(wui::ButtonVariant::Ghost)
                                                .onClick([edit, id = item.id] { edit(id); }),
                                            IconButton(wui::IconName::Delete, "Delete task").accessibilityId(automationPrefix + ".delete")
                                                .onClick([remove, id = item.id, requestConfirmation] {
                                                    requestConfirmation("Remove this task?",
                                                                        "This task will be removed from My day.",
                                                                        [remove, id] { remove(id); });
                                                })))));
                    }).gap(8.0f).align(wui::Alignment::Stretch));
            }),
            If(hasCompleted).then([&completedTodos, toggle, remove, setImportant, edit, requestConfirmation,
                                   caption = todoStrong(type.windows.caption), bodyLarge = todoBodyLarge(), metadataStyle = type.windows.caption] {
                return Column().gap(10.0f).align(wui::Alignment::Stretch).children(
                    Text("COMPLETED").style(caption).color({97, 97, 97, 255}),
                    KeyedForEach<Todo>(completedTodos,
                        [](const Todo& item) { return std::to_string(item.id); },
                        [toggle, remove, setImportant, edit, requestConfirmation, bodyLarge, metadataStyle](const Todo& item) {
                        const std::string metadata = item.dueDateIso ? "Due " + *item.dueDateIso : std::string{};
                        const std::string automationPrefix = "todo.task." + std::to_string(item.id);
                        return Box().background({255, 255, 255, 255}).radius(10.0f)
                            .padding(wui::InsetsF{14.0f, 11.0f, 10.0f, 11.0f})
                            .children(Row().align(wui::Alignment::Start).gap(10.0f).children(
                                Box().height(32.0f)
                                    .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                                    .children(Checkbox("", item.completed).accessibilityId(automationPrefix + ".complete").accessibleLabel(item.title)
                                        .onChange([toggle, id = item.id](bool) { toggle(id); })),
                                Column().gap(4.0f).align(wui::Alignment::Stretch).flex(1.0f).children(
                                    Row().align(wui::Alignment::Start).gap(8.0f).children(
                                        Box().padding(wui::InsetsF{0.0f, 4.0f, 0.0f, 4.0f}).flex(1.0f)
                                            .children(Text(item.title).wrap().maxLines(2).ellipsis().style(bodyLarge)
                                                .color({117, 117, 117, 255})),
                                        IconButton(wui::IconName::Star,
                                                   (item.important ? "Remove important from " : "Mark important: ") + item.title)
                                            .iconStyle(item.important ? wui::IconStyle::Filled
                                                                      : wui::IconStyle::Regular)
                                            .accessibilityId(automationPrefix + ".important")
                                            .checked(item.important)
                                            .onClick([setImportant, id = item.id, important = item.important] {
                                                setImportant(id, !important);
                                            })),
                                    Row().align(wui::Alignment::Center).gap(6.0f).children(
                                        Text(metadata).maxLines(1).ellipsis().style(metadataStyle)
                                            .color({117, 117, 117, 255}).flex(1.0f),
                                        Button("Edit").accessibilityId(automationPrefix + ".edit").variant(wui::ButtonVariant::Ghost)
                                            .onClick([edit, id = item.id] { edit(id); }),
                                        IconButton(wui::IconName::Delete, "Delete task").accessibilityId(automationPrefix + ".delete")
                                            .onClick([remove, id = item.id, requestConfirmation] {
                                                requestConfirmation("Remove this task?",
                                                                    "This task will be removed from My day.",
                                                                    [remove, id] { remove(id); });
                                            })))));
                    }).gap(8.0f).align(wui::Alignment::Stretch));
            }),
            Spacer(0.0f, 0.0f)))),
            Spacer().flex(1.0f)));
}

} // namespace

#if !defined(WUI_TODO_TESTING)
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
    ImportantRequest setImportant = [&interaction, &apply](int id, bool important) {
        (void)apply(interaction.setImportant(id, important));
    };
    std::function<void()> clearCompleted = [&interaction, &apply] {
        (void)apply(interaction.clearCompleted());
    };
    std::function<void()> undo = [&interaction, &apply] {
        (void)apply(interaction.undo());
    };

    const bool perfSmoke = std::any_of(argv + 1, argv + argc, [](const char* argument) {
        return std::string(argument) == "--perf-smoke";
    });
    try {
        return wui::runGlfwApp("WhatsUI Todo", {640.0f, 560.0f},
                                [&todos, &isEmpty, &hasNoMatches, &activeTodos, &completedTodos, &hasActive, &hasCompleted, &hasStoredCompleted,
                                 &filterAll, &filterActive, &filterCompleted,
                                 &hasOperationMessage, &operationMessage,
                                 &hasUndo, &undoMessage,
                                 addTodo, toggle, remove, setImportant, clearCompleted, undo, setFilter, setSearchQuery,
                                 &controller, &interaction, &apply, perfSmoke](wui::UiWindow& window) {
            auto root = buildTodoUi(todos, isEmpty, hasNoMatches, activeTodos, completedTodos, hasActive, hasCompleted, hasStoredCompleted,
                               filterAll, filterActive, filterCompleted,
                               hasOperationMessage, operationMessage, hasUndo, undoMessage,
                               addTodo, toggle, remove, clearCompleted, undo, setFilter, setSearchQuery,
                               [&window, &controller, &interaction, &apply, &hasOperationMessage, &operationMessage](int id) {
                                   const auto current = std::find_if(controller.records().begin(), controller.records().end(),
                                                                     [id](const Todo& item) { return item.id == id; });
                                   if (current == controller.records().end()) return;
                                   const Todo snapshot = *current;
                                   const auto begin = interaction.beginEdit(id);
                                   if (!begin.succeeded()) {
                                       (void)apply(begin);
                                       return;
                                   }
                                   showEditDialog(window, interaction.editDraft(), snapshot.important, snapshot.dueDateIso,
                                                  [&interaction, &apply](std::string title,
                                                                         bool important,
                                                                         std::optional<std::string> dueDate) {
                                                      interaction.setEditDraft(std::move(title));
                                                      const auto result = interaction.commitEditDetails(
                                                          important, std::move(dueDate));
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
                               },
                               setImportant);
            if (perfSmoke) installTodoPerformanceSmoke(window);
            return root;
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

    auto makeRoot = [&] {
        return buildTodoUi(todos, isEmpty, hasNoMatches, activeTodos, completedTodos,
                           hasActive, hasCompleted, hasStoredCompleted,
                           filterAll, filterActive, filterCompleted,
                           hasOperationMessage, operationMessage, hasUndo, undoMessage,
                           addTodo, toggle, remove, clearCompleted, [] {}, setFilter,
                           setSearchQuery, [](int) {},
                           [](std::string, std::string, std::function<void()> confirm) { confirm(); },
                           [](int, bool) {});
    };
    auto root = makeRoot();

    // A visual walkthrough is a sequence of frames on one render target, just
    // like the real window. Recreating the Software canvas per scene discards
    // the background that WhatsCanvas' native ClearType compositor samples
    // while its glyph commands are flushed, which can turn later text bounds
    // into opaque black rectangles. Keep one canvas/context for the complete
    // walkthrough and explicitly repaint the surface for every capture.
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software,
                                      static_cast<int>(width * scaleFactor),
                                      static_cast<int>(height * scaleFactor));
    if (!canvas || !canvas->initializeContext()) {
        throw std::runtime_error("failed to create software canvas");
    }
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scaleFactor);
    wui::setTextMeasurer(&measurer);

    int frame = 0;
    auto renderFrame = [&](const char* namedCapture = nullptr, bool scrollToEnd = false) {
        wui::flushStructuralUpdates();
        // Each golden is a complete product state, not an incremental dirty
        // region. Rebuild the retained widget tree from the current States so
        // removed ForEach/If branches cannot leave renderer-owned text/clip
        // resources in the following capture.
        root = makeRoot();
        wui::flushStructuralUpdates();
        root->layout({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});
        if (scrollToEnd) {
            auto* scroll = todoScrollView(root.get());
            if (scroll == nullptr || scroll->maxScrollOffsetY() <= 0.0f) {
                throw std::runtime_error("Todo compact viewport did not expose a vertical scroll range");
            }
            scroll->setScrollOffset(scroll->maxScrollOffsetY());
        }
        wui::PaintContext paint(*canvas, scaleFactor);
        root->prepare(paint);
        for (int pass = 0; pass < 2; ++pass) {
            canvas->beginFrame();
            paint.fillRect({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, wui::theme().colors.surface);
            root->paint(paint);
            canvas->endFrame();
        }
        const auto path = outputDirectory / (namedCapture != nullptr
            ? std::string{"todo_"} + namedCapture + ".ppm"
            : "todo_" + std::to_string(frame++) + ".ppm");
        if (!canvas->savePixelsPPM(path.string())) {
            throw std::runtime_error("failed to save " + path.string());
        }
        if (scrollToEnd) {
            // The next scripted product state starts at the same top-of-page
            // position a user sees after an add/toggle rebuild.
            if (auto* scroll = todoScrollView(root.get())) scroll->setScrollOffset(0.0f);
        }
        std::cout << "wrote " << path << std::endl;
    };

    // Scripted product walkthrough. The first and last scenes prove that the
    // empty-state card is retained without any task section/action; the third
    // scene proves that an all-completed list renders only COMPLETED.
    renderFrame();                 // empty list: no Clear done or task groups
    addTodo("Buy milk");
    addTodo("Write WhatsUI docs");
    addTodo("Ship the release");
    // Keep priority and due-date presentation in every visual-review matrix
    // (360/640/1180), rather than limiting metadata coverage to an interactive
    // dialog that the headless walkthrough cannot open.
    {
        auto items = todos.get();
        items[0].important = true;
        items[0].dueDateIso = "2026-07-15";
        items[1].dueDateIso = "2026-07-18";
        todos.set(std::move(items));
        synchronize();
    }
    renderFrame();                 // three active items: IN PROGRESS only
    if (width == 640 && height == 560) {
        // The regular Windows acceptance viewport is deliberately shorter
        // than the three-row walkthrough.  Capture its document end as a
        // focused regression artifact without making wide/narrow reviews
        // render an unnecessary duplicate scene.
        renderFrame("scroll_end", true);
    }
    toggle(1);
    toggle(2);
    toggle(3);
    renderFrame();                 // all complete: COMPLETED and Clear done only
    clearCompleted();
    renderFrame();                 // empty again after clear

    wui::setTextMeasurer(nullptr);

    return 0;
#endif
}
#endif // !defined(WUI_TODO_TESTING)
