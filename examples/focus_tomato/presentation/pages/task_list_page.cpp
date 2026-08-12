#include "task_list_page.h"

#include "../components/common_components.h"
#include "../components/task_execution_preferences.h"
#include "../focus_style.h"
#include "wui/declarative.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace whatsui::focus_tomato::presentation {
namespace {

wui::Box taskStatus(
    bool done,
    std::string taskId,
    std::string taskTitle,
    std::function<void()> onClick)
{
    using namespace wui;
    auto status = Box()
        .background(done ? style::success : style::border)
        .radius(999.0f)
        .width(20.0f)
        .height(20.0f);
    auto control = done
        ? std::move(status)
        : std::move(status)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Box()
                .background(style::surface)
                .radius(999.0f)
                .width(16.0f)
                .height(16.0f)
        );
    if (onClick) {
        control
            .automationId("focus.tasks.toggle." + taskId)
            .accessibleRole(wui::AccessibilityRole::Button)
            .accessibleLabel(
                std::string(done ? "恢复任务：" : "完成任务：") + taskTitle)
            .onClick(std::move(onClick));
    }
    return control;
}

wui::Box buildTaskRow(
    const TaskRecord& task,
    const FocusSettings& settings,
    float width,
    std::function<void()> onClick,
    std::function<void()> onToggleCompletion,
    std::function<void()> onManage,
    std::function<void()> onRestore)
{
    using namespace wui;
    const bool done = task.status == TaskStatus::Done;
    const bool deleted = isArchivedTaskStatus(task.status);
    const bool completedBeforeDeletion =
        task.status == TaskStatus::ArchivedDone;
    const bool visuallyCompleted = done || completedBeforeDeletion;
    auto rowAction = Box()
        .background(style::surface)
        .hoverBackground(wui::Color{250, 246, 240, 255})
        .pressedBackground(style::border)
        .radius(8.0f)
        .width(32.0f)
        .height(32.0f)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .accessibleRole(wui::AccessibilityRole::Button)
        .accessibleLabel(
            deleted ? "恢复任务：" + task.title
                    : "管理任务：" + task.title)
        .automationId(
            std::string(deleted ? "focus.tasks.restore."
                                : "focus.tasks.manage.")
            + task.id)
        .onClick(deleted ? std::move(onRestore) : std::move(onManage))
        .children(
            Icon(deleted ? wui::IconName::ArrowUndo
                         : wui::IconName::MoreHorizontal)
                .size(wui::IconSize::Size16)
                .color(style::textSecondary)
        );
    auto row = Box()
        .automationId("focus.tasks.row." + task.id)
        .background(visuallyCompleted ? style::successSurface : style::surface)
        .radius(12.0f)
        .width(width)
        .height(64.0f)
        .padding({16.0f, 0.0f, 16.0f, 0.0f})
        .children(
            Row()
                .align(wui::Alignment::Center)
                .gap(12.0f)
                .children(
                    taskStatus(
                        visuallyCompleted,
                        task.id,
                        task.title,
                        deleted ? std::function<void()>{}
                                : std::move(onToggleCompletion)),
                    Column()
                        .gap(2.0f)
                        .flex(1.0f)
                        .children(
                            Text(task.title)
                                .style(style::text(13.0f, 400, 19.0f))
                                .color(visuallyCompleted || deleted
                                           ? style::textSecondary
                                           : style::textPrimary)
                                .strikethrough(visuallyCompleted),
                            Text(taskExecutionSummary(task, settings))
                                .style(style::text(10.0f, 400, 15.0f))
                                .color(style::textMuted)
                        ),
                    Box()
                        .background(style::accent)
                        .radius(999.0f)
                        .width(14.0f)
                        .height(14.0f),
                    Text("× " + std::to_string(task.estimatedPomodoros))
                        .style(style::text(11.0f, 500, 16.0f))
                        .color(style::textSecondary),
                    std::move(rowAction)
                )
        );
    if (!done && onClick) {
        return std::move(row)
            .hoverBackground(wui::Color{255, 248, 241, 255})
            .pressedBackground(style::border)
            .accessibleRole(wui::AccessibilityRole::Button)
            .accessibleLabel("选择任务：" + task.title)
            .onClick(std::move(onClick));
    }
    return std::move(row);
}

wui::Box buildFilteredEmpty(TaskFilter filter)
{
    using namespace wui;
    const std::string message = filter == TaskFilter::Deleted
        ? "没有已删除任务。删除的任务会保留历史，并可在这里恢复。"
        : "当前筛选下没有任务，可切换上方筛选条件。";
    return Box()
        .background(style::surface)
        .radius(16.0f)
        .height(112.0f)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Text(message)
                .style(style::text(12.0f, 400, 18.0f))
                .color(style::textSecondary)
        );
}

wui::Box buildActiveSessionBanner(
    FocusViewModel& viewModel,
    float width,
    std::function<void()> onClick)
{
    using namespace wui;
    const FocusSessionRecord* session = viewModel.activeSession();
    if (session == nullptr) return Box().height(0.0f);
    const bool isFocus = session->type == SessionType::Focus;
    const bool running = session->status == SessionStatus::Running;
    const std::string status = isFocus
        ? (running ? "专注进行中" : "专注已暂停")
        : (running ? "休息进行中" : "休息已暂停");
    State<std::string> remaining = viewModel.remainingText();

    return Box()
        .automationId("focus.active-session.open")
        .background(isFocus ? wui::Color{255, 238, 232, 255}
                            : style::successSurface)
        .hoverBackground(isFocus ? wui::Color{255, 228, 220, 255}
                                 : wui::Color{224, 239, 210, 255})
        .pressedBackground(style::border)
        .radius(16.0f)
        .width(width)
        .height(78.0f)
        .padding({16.0f, 12.0f, 16.0f, 12.0f})
        .accessibleRole(wui::AccessibilityRole::Button)
        .accessibleLabel("返回当前" + std::string(isFocus ? "专注" : "休息"))
        .onClick(std::move(onClick))
        .children(
            Row()
                .align(wui::Alignment::Center)
                .gap(12.0f)
                .children(
                    Column()
                        .gap(3.0f)
                        .align(wui::Alignment::Start)
                        .flex(1.0f)
                        .children(
                            Text(status + " · " + session->titleSnapshot)
                                .style(style::text(13.0f, 600, 19.0f))
                                .color(style::textPrimary),
                            Text("计时不会因浏览任务而中断")
                                .style(style::text(11.0f, 400, 16.0f))
                                .color(style::textSecondary)
                        ),
                    Text()
                        .bind(remaining)
                        .style(style::text(20.0f, 700, 27.0f))
                        .color(isFocus ? style::accent : style::success),
                    Text("返回 ›")
                        .style(style::text(11.0f, 500, 16.0f))
                        .color(style::textSecondary)
                )
        );
}

wui::Box buildStatCard(
    float width, std::string label, std::string value, std::string unit)
{
    using namespace wui;
    return Box()
        .background(style::surface)
        .radius(16.0f)
        .width(width)
        .height(120.0f)
        .padding(18.0f)
        .children(
            Column()
                .gap(8.0f)
                .align(wui::Alignment::Start)
                .children(
                    Text(std::move(label))
                        .style(style::text(12.0f, 400, 17.0f))
                        .color(style::textSecondary),
                    Text(std::move(value))
                        .style(style::text(28.0f, 700, 34.0f))
                        .color(style::accent),
                    Text(std::move(unit))
                        .style(style::text(11.0f, 400, 16.0f))
                        .color(style::textMuted)
                )
        );
}

wui::Box buildEmptyTasks(
    std::function<void()> createTask,
    std::function<void()> secondaryAction,
    bool hasActiveSession)
{
    using namespace wui;
    return Box()
        .background(style::surface)
        .radius(16.0f)
        .height(220.0f)
        .padding(24.0f)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Column()
                .gap(12.0f)
                .align(wui::Alignment::Center)
                .children(
                    Text("还没有任务")
                        .style(style::text(20.0f, 700, 29.0f))
                        .color(style::textPrimary),
                    Text(hasActiveSession
                             ? "可以新建任务，当前计时会继续运行。"
                             : "新建一个任务，或直接开始自由专注。")
                        .style(style::text(12.0f, 400, 18.0f))
                        .color(style::textSecondary),
                    Row()
                        .gap(10.0f)
                        .children(
                            buildPrimaryTextButton(
                                "＋ 新建任务", std::move(createTask)),
                            buildSecondaryTextButton(
                                hasActiveSession ? "返回当前计时" : "直接专注",
                                std::move(secondaryAction))
                                .automationId(
                                    hasActiveSession
                                        ? "focus.active-session.empty.open"
                                        : "focus.tasks.free-focus")
                        )
                )
        );
}

} // namespace

wui::Box TaskListPage::body()
{
    using namespace wui;
    FocusViewModel& viewModel = *viewModel_;
    const float pageWidth = pageWidth_;
    const float pageHeight = pageHeight_;
    TaskListPageActions actions = std::move(actions_);
    const float contentWidth = std::max(320.0f, pageWidth - 64.0f);
    const float taskWidth = std::min(500.0f, contentWidth);
    const float statWidth = (contentWidth - 28.0f) / 3.0f;
    const bool hasActiveSession = viewModel.activeSession() != nullptr;

    auto tasks = Column()
        .gap(10.0f)
        .align(wui::Alignment::Center);
    int visibleTasks = 0;
    for (const auto& task : viewModel.data().tasks) {
        if (!viewModel.isTaskVisible(task)) continue;
        ++visibleTasks;
        const std::string taskId = task.id;
        tasks.children(
            buildTaskRow(
                task, viewModel.data().settings, taskWidth,
                !hasActiveSession
                    && task.status == TaskStatus::Active
                    && actions.selectTask
                    ? std::function<void()>(
                        [select = actions.selectTask, taskId] { select(taskId); })
                    : std::function<void()>{},
                actions.toggleCompletion
                    ? std::function<void()>(
                        [toggle = actions.toggleCompletion, taskId] {
                            toggle(taskId);
                        })
                    : std::function<void()>{},
                actions.manageTask
                    ? std::function<void()>(
                        [manage = actions.manageTask, taskId] {
                            manage(taskId);
                        })
                    : std::function<void()>{},
                actions.restoreTask
                    ? std::function<void()>(
                        [restore = actions.restoreTask,
                         taskId,
                         revision = task.revision] {
                            restore(taskId, revision);
                        })
                    : std::function<void()>{})
        );
    }

    const auto statistics = viewModel.todayStatistics();
    int completedTasks = 0;
    for (const auto& task : viewModel.data().tasks) {
        if (task.status == TaskStatus::Done) ++completedTasks;
    }
    const double focusedHours =
        static_cast<double>(statistics.focusedDurationMs) / (60.0 * 60.0 * 1000.0);
    std::ostringstream focusedTextStream;
    focusedTextStream << std::fixed << std::setprecision(1) << focusedHours;
    const std::string focusedText = focusedTextStream.str();

    auto document = Column()
        .gap(18.0f)
        .padding({32.0f, 28.0f, 32.0f, 28.0f})
        .align(wui::Alignment::Stretch);
    document.children(
        buildOperationBanner(viewModel, contentWidth)
    );
    document.children(
        Row()
            .align(wui::Alignment::Center)
            .gap(12.0f)
            .children(
                Text("任务列表")
                    .style(style::text(24.0f, 700, 35.0f))
                    .color(style::textPrimary)
                    .flex(1.0f),
                buildPrimaryTextButton("＋ 新建任务", actions.createTask)
                    .automationId("focus.tasks.create")
            )
    );
    if (hasActiveSession) {
        document.children(
            buildActiveSessionBanner(
                viewModel,
                contentWidth,
                actions.openActiveSession)
        );
    }
    document.children(
        Row()
            .gap(8.0f)
            .children(
                buildPill(
                    "全部",
                    viewModel.taskFilter() == TaskFilter::All,
                    [setFilter = actions.setFilter] {
                        if (setFilter) setFilter(TaskFilter::All);
                    })
                    .automationId("focus.tasks.filter.all"),
                buildPill(
                    "进行中",
                    viewModel.taskFilter() == TaskFilter::Active,
                    [setFilter = actions.setFilter] {
                        if (setFilter) setFilter(TaskFilter::Active);
                    })
                    .automationId("focus.tasks.filter.active"),
                buildPill(
                    "已完成",
                    viewModel.taskFilter() == TaskFilter::Completed,
                    [setFilter = actions.setFilter] {
                        if (setFilter) setFilter(TaskFilter::Completed);
                    })
                    .automationId("focus.tasks.filter.completed"),
                buildPill(
                    "已删除",
                    viewModel.taskFilter() == TaskFilter::Deleted,
                    [setFilter = actions.setFilter] {
                        if (setFilter) setFilter(TaskFilter::Deleted);
                    })
                    .automationId("focus.tasks.filter.deleted")
            )
    );
    if (visibleTasks == 0) {
        document.children(
            viewModel.data().tasks.empty()
                && viewModel.taskFilter() == TaskFilter::All
                ? buildEmptyTasks(
                    actions.createTask,
                    hasActiveSession
                        ? actions.openActiveSession
                        : actions.startFreeFocus,
                    hasActiveSession)
                : buildFilteredEmpty(viewModel.taskFilter())
        );
    } else {
        document.children(std::move(tasks));
    }
    document.children(
        Row()
            .align(wui::Alignment::Start)
            .gap(14.0f)
            .children(
                buildStatCard(
                    statWidth,
                    "今日番茄",
                    std::to_string(statistics.completedFocusSessions),
                    "个"),
                buildStatCard(
                    statWidth, "专注时间", focusedText, "小时"),
                buildStatCard(
                    statWidth,
                    "完成任务",
                    std::to_string(completedTasks),
                    "项")
            )
    );

    return Box()
        .background(style::canvas)
        .width(pageWidth)
        .height(pageHeight)
        .children(
            Column()
                .align(wui::Alignment::Stretch)
                .children(
                    ScrollView()
                        .axis(wui::ScrollAxis::Vertical)
                        .flex(1.0f)
                        .content(
                            std::move(document)
                        )
                )
        );
}

} // namespace whatsui::focus_tomato::presentation
