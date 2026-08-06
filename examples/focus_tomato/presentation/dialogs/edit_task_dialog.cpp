#include "edit_task_dialog.h"

#include "../components/task_execution_preferences.h"
#include "../focus_style.h"
#include "wui/app.h"
#include "wui/declarative.h"
#include "wui/scheduler.h"

#include <algorithm>
#include <charconv>
#include <optional>
#include <utility>

namespace whatsui::focus_tomato::presentation {
namespace {

std::optional<int> parseEstimate(const std::string& text)
{
    int value = 0;
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{}
        || result.ptr != text.data() + text.size()
        || value < 1 || value > 99) {
        return std::nullopt;
    }
    return value;
}

class EditTaskDialogView {
public:
    EditTaskDialogView(
        wui::UiWindow& window,
        FocusViewModel& viewModel,
        TaskRecord task,
        bool usedByActiveSession,
        TaskEditedCallback onEdited,
        TaskDeleteRequestCallback onDeleteRequested)
        : window_(&window)
        , viewModel_(&viewModel)
        , task_(std::move(task))
        , usedByActiveSession_(usedByActiveSession)
        , onEdited_(std::move(onEdited))
        , onDeleteRequested_(std::move(onDeleteRequested))
        , title_(task_.title)
        , estimate_(std::to_string(task_.estimatedPomodoros))
        , execution_(task_.execution)
    {
    }

    auto body()
    {
        using namespace wui;
        auto submit = [window = window_,
                       viewModel = viewModel_,
                       taskId = task_.id,
                       expectedRevision = task_.revision,
                       title = title_,
                       estimate = estimate_,
                       execution = execution_,
                       error = error_,
                       onEdited = std::move(onEdited_)]() mutable {
            const auto parsedEstimate = parseEstimate(estimate.get());
            if (!parsedEstimate) {
                error.set("预计番茄数请输入 1～99 的整数。");
                return;
            }
            std::string preferenceError;
            const auto preferences = parseTaskExecutionPreferences(
                execution, preferenceError);
            if (!preferences) {
                error.set(std::move(preferenceError));
                return;
            }
            const auto result = viewModel->updateTask(
                taskId,
                title.get(),
                *parsedEstimate,
                *preferences,
                expectedRevision);
            if (!result.succeeded()) {
                error.set(result.message.empty()
                    ? "任务没有保存，请刷新后重试。"
                    : result.message);
                return;
            }
            (void)window->dismissTopDialog();
            scheduleStructuralUpdate(
                window,
                [onEdited = std::move(onEdited)]() mutable {
                    if (onEdited) onEdited();
                });
        };

        auto requestDelete = [window = window_,
                              taskId = task_.id,
                              taskTitle = task_.title,
                              expectedRevision = task_.revision,
                              callback = std::move(onDeleteRequested_)]() mutable {
            (void)window->dismissTopDialog();
            scheduleStructuralUpdate(
                window,
                [taskId = std::move(taskId),
                 taskTitle = std::move(taskTitle),
                 expectedRevision,
                 callback = std::move(callback)]() mutable {
                    if (callback) {
                        callback(
                            std::move(taskId),
                            std::move(taskTitle),
                            expectedRevision);
                    }
                });
        };

        auto titleField = TextField("任务名称")
            .text(task_.title)
            .automationId("focus.edit-task.title")
            .accessibleLabel("任务名称")
            .onChange([draft = title_](const std::string& value) {
                draft.set(value);
            })
            .onSubmit(submit);
        initialFocus_ = titleField.node();

        return Dialog()
            .maxWidth(440.0f)
            .content(
                Box()
                    .width(392.0f)
                    .background(style::surface)
                    .radius(16.0f)
                    .padding({22.0f, 20.0f, 22.0f, 20.0f})
                    .children(
                        Column()
                            .gap(14.0f)
                            .align(wui::Alignment::Stretch)
                            .children(
                                Column()
                                    .gap(4.0f)
                                    .children(
                                        Text("编辑任务")
                                            .style(style::text(
                                                20.0f, 700, 29.0f))
                                            .color(style::textPrimary),
                                        Text(usedByActiveSession_
                                                 ? "修改只影响后续专注；当前计时保留启动时快照。"
                                                 : "调整名称和工作量，不会改写历史专注记录。")
                                            .style(style::text(
                                                12.0f, 400, 18.0f))
                                            .color(style::textSecondary)
                                    ),
                                std::move(titleField),
                                Row()
                                    .align(wui::Alignment::Center)
                                    .gap(10.0f)
                                    .children(
                                        Text("预计番茄数")
                                            .style(style::text(
                                                12.0f, 500, 18.0f))
                                            .color(style::textSecondary),
                                        TextField("1～99")
                                            .text(std::to_string(
                                                task_.estimatedPomodoros))
                                            .automationId(
                                                "focus.edit-task.estimate")
                                            .accessibleLabel("预计番茄数")
                                            .flex(1.0f)
                                            .onChange(
                                                [draft = estimate_](
                                                    const std::string& value) {
                                                    draft.set(value);
                                                })
                                            .onSubmit(submit)
                                    ),
                                buildTaskExecutionPreferenceFields(
                                    viewModel_->data().settings,
                                    execution_,
                                    "focus.edit-task"),
                                Text()
                                    .bind(error_)
                                    .style(style::text(12.0f, 400, 18.0f))
                                    .color(style::accent),
                                Row()
                                    .align(wui::Alignment::Center)
                                    .gap(8.0f)
                                    .children(
                                        Button("删除任务")
                                            .automationId(
                                                "focus.edit-task.delete")
                                            .appearance(
                                                ButtonAppearance::Danger)
                                            .enabled(!usedByActiveSession_)
                                            .onClick(std::move(requestDelete)),
                                        Spacer().flex(1.0f),
                                        Button("取消")
                                            .automationId(
                                                "focus.edit-task.cancel")
                                            .appearance(
                                                ButtonAppearance::Outline)
                                            .onClick([window = window_] {
                                                (void)window
                                                    ->dismissTopDialog();
                                            }),
                                        Button("保存修改")
                                            .automationId(
                                                "focus.edit-task.save")
                                            .appearance(
                                                ButtonAppearance::Primary)
                                            .onClick(std::move(submit))
                                    ),
                                Text(usedByActiveSession_
                                         ? "结束当前计时后才能删除这个任务。"
                                         : "删除后可在“已删除”中恢复。")
                                    .style(style::text(
                                        11.0f, 400, 16.0f))
                                    .color(style::textMuted)
                            )
                    )
            );
    }

    [[nodiscard]] wui::Node* initialFocus() const noexcept
    {
        return initialFocus_;
    }

private:
    wui::UiWindow* window_;
    FocusViewModel* viewModel_;
    TaskRecord task_;
    bool usedByActiveSession_{false};
    TaskEditedCallback onEdited_;
    TaskDeleteRequestCallback onDeleteRequested_;
    wui::State<std::string> title_;
    wui::State<std::string> estimate_;
    TaskExecutionDraft execution_;
    wui::State<std::string> error_;
    wui::Node* initialFocus_{nullptr};
};

} // namespace

bool showEditTaskDialog(
    wui::UiWindow& window,
    FocusViewModel& viewModel,
    const std::string& taskId,
    TaskEditedCallback onEdited,
    TaskDeleteRequestCallback onDeleteRequested)
{
    const auto task = std::find_if(
        viewModel.data().tasks.begin(), viewModel.data().tasks.end(),
        [&taskId](const TaskRecord& item) { return item.id == taskId; });
    if (task == viewModel.data().tasks.end()
        || isArchivedTaskStatus(task->status)) {
        return false;
    }
    const auto* session = viewModel.activeSession();
    const bool usedByActiveSession = session != nullptr
        && session->taskId && *session->taskId == taskId;
    EditTaskDialogView dialog(
        window,
        viewModel,
        *task,
        usedByActiveSession,
        std::move(onEdited),
        std::move(onDeleteRequested));
    (void)window.showDialog(dialog);
    window.focusManager().setFocused(dialog.initialFocus());
    return true;
}

} // namespace whatsui::focus_tomato::presentation
