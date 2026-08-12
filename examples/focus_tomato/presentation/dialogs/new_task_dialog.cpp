#include "new_task_dialog.h"

#include "../components/task_execution_preferences.h"
#include "../focus_style.h"
#include "wui/app.h"
#include "wui/scheduler.h"
#include "wui/declarative.h"

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
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
        || value < 1 || value > 99) {
        return std::nullopt;
    }
    return value;
}

class NewTaskDialogView {
public:
    NewTaskDialogView(wui::UiWindow& window,
                      FocusViewModel& viewModel,
                      TaskCreatedCallback onCreated)
        : window_(&window)
        , viewModel_(&viewModel)
        , onCreated_(std::move(onCreated))
    {
    }

    auto body()
    {
        using namespace wui;

        auto submit = [window = window_,
                       viewModel = viewModel_,
                       title = title_,
                       estimate = estimate_,
                       execution = execution_,
                       error = error_,
                       onCreated = std::move(onCreated_)]() mutable {
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
            const auto result = viewModel->addTask(
                title.get(), *parsedEstimate, *preferences);
            if (!result.succeeded()) {
                error.set(result.message.empty()
                    ? "任务没有保存，请检查名称后重试。"
                    : result.message);
                return;
            }
            (void)window->dismissTopDialog();
            if (onCreated) {
                auto refresh = std::move(onCreated);
                scheduleStructuralUpdate(
                    window,
                    [refresh = std::move(refresh)]() mutable {
                        if (refresh) refresh();
                    });
            }
        };

        auto title = TextField("例如：完成产品设计稿")
            .automationId("focus.new-task.title")
            .accessibleLabel("任务名称")
            .flex(1.0f)
            .onChange([draft = title_](const std::string& value) {
                draft.set(value);
            })
            .onSubmit(submit);
        initialFocus_ = title.node();

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
                                        Text("新建任务")
                                            .style(style::text(
                                                20.0f, 700, 29.0f))
                                            .color(style::textPrimary),
                                        Text("先写清这一轮要推进的具体结果。")
                                            .style(style::text(
                                                12.0f, 400, 18.0f))
                                            .color(style::textSecondary)
                                    ),
                                std::move(title),
                                Row()
                                    .align(wui::Alignment::Center)
                                    .gap(10.0f)
                                    .children(
                                        Text("预计番茄数")
                                            .style(style::text(
                                                12.0f, 500, 18.0f))
                                            .color(style::textSecondary),
                                        TextField("1～99")
                                            .automationId(
                                                "focus.new-task.estimate")
                                            .accessibleLabel("预计番茄数")
                                            .flex(1.0f)
                                            .onSubmit(submit)
                                            .onChange(
                                                [draft = estimate_](
                                                    const std::string& value) {
                                                    draft.set(value);
                                                })
                                    ),
                                buildTaskExecutionPreferenceFields(
                                    viewModel_->data().settings,
                                    execution_,
                                    "focus.new-task"),
                                Text()
                                    .bind(error_)
                                    .style(style::text(12.0f, 400, 18.0f))
                                    .color(style::accent),
                                Row()
                                    .align(wui::Alignment::Center)
                                    .gap(8.0f)
                                    .children(
                                        Spacer().flex(1.0f),
                                        Button("取消")
                                            .automationId(
                                                "focus.new-task.cancel")
                                            .appearance(
                                                ButtonAppearance::Outline)
                                            .onClick([window = window_] {
                                                (void)window
                                                    ->dismissTopDialog();
                                            }),
                                        Button("保存任务")
                                            .automationId(
                                                "focus.new-task.save")
                                            .appearance(
                                                ButtonAppearance::Primary)
                                            .onClick(std::move(submit))
                                    )
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
    TaskCreatedCallback onCreated_;
    wui::State<std::string> title_;
    wui::State<std::string> estimate_{"1"};
    TaskExecutionDraft execution_;
    wui::State<std::string> error_;
    wui::Node* initialFocus_{nullptr};
};

} // namespace

void showNewTaskDialog(wui::UiWindow& window,
                       FocusViewModel& viewModel,
                       TaskCreatedCallback onCreated)
{
    NewTaskDialogView dialog(window, viewModel, std::move(onCreated));
    (void)window.showDialog(dialog);
    window.focusManager().setFocused(dialog.initialFocus());
}

} // namespace whatsui::focus_tomato::presentation
