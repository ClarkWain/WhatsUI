#include "task_list_page.h"

#include "../components/common_components.h"
#include "../focus_style.h"
#include "../../domain/focus_statistics.h"
#include "wui/declarative.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace whatsui::focus_tomato::presentation {
namespace {

wui::Box taskStatus(bool done)
{
    using namespace wui;
    auto status = Box()
        .background(done ? style::success : style::border)
        .radius(999.0f)
        .width(20.0f)
        .height(20.0f);
    if (done) return std::move(status);
    return std::move(status)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Box()
                .background(style::surface)
                .radius(999.0f)
                .width(16.0f)
                .height(16.0f)
        );
}

wui::Box buildTaskRow(
    const TaskRecord& task, float width, std::function<void()> onClick)
{
    using namespace wui;
    const bool done = task.status == TaskStatus::Done;
    auto row = Box()
        .background(done ? style::successSurface : style::surface)
        .radius(12.0f)
        .width(width)
        .height(56.0f)
        .padding({16.0f, 0.0f, 16.0f, 0.0f})
        .children(
            Row()
                .align(wui::Alignment::Center)
                .gap(12.0f)
                .children(
                    taskStatus(done),
                    Text(task.title)
                        .style(style::text(13.0f, 400, 19.0f))
                        .color(done ? style::textSecondary : style::textPrimary)
                        .strikethrough(done)
                        .flex(1.0f),
                    Box()
                        .background(style::accent)
                        .radius(999.0f)
                        .width(14.0f)
                        .height(14.0f),
                    Text("× " + std::to_string(task.estimatedPomodoros))
                        .style(style::text(11.0f, 500, 16.0f))
                        .color(style::textSecondary)
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

wui::Box buildEmptyTasks(std::function<void()> createTask)
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
                    Text("新建一个任务，或直接开始自由专注。")
                        .style(style::text(12.0f, 400, 18.0f))
                        .color(style::textSecondary),
                    buildPrimaryTextButton(
                        "＋ 新建任务", std::move(createTask))
                )
        );
}

} // namespace

wui::Box buildTaskListPage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float pageWidth,
    float pageHeight,
    TaskListPageActions actions)
{
    using namespace wui;
    const float contentWidth = std::max(320.0f, pageWidth - 64.0f);
    const float taskWidth = std::min(500.0f, contentWidth);
    const float statWidth = (contentWidth - 28.0f) / 3.0f;

    auto tasks = Column()
        .gap(10.0f)
        .align(wui::Alignment::Center);
    int visibleTasks = 0;
    for (const auto& task : viewModel.data().tasks) {
        if (task.status == TaskStatus::Archived) continue;
        ++visibleTasks;
        const std::string taskId = task.id;
        tasks.children(
            buildTaskRow(
                task, taskWidth,
                task.status == TaskStatus::Active && actions.selectTask
                    ? std::function<void()>(
                        [select = actions.selectTask, taskId] { select(taskId); })
                    : std::function<void()>{})
        );
    }

    const auto statistics = calculateFocusStatistics(
        viewModel.data(), 0, std::numeric_limits<std::int64_t>::max());
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
        Row()
            .align(wui::Alignment::Center)
            .gap(12.0f)
            .children(
                Text("任务列表")
                    .style(style::text(24.0f, 700, 35.0f))
                    .color(style::textPrimary)
                    .flex(1.0f),
                buildPrimaryTextButton("＋ 新建任务", actions.createTask)
            )
    );
    document.children(
        Row()
            .gap(8.0f)
            .children(
                buildPill("全部", true),
                buildPill("进行中", false),
                buildPill("已完成", false)
            )
    );
    if (visibleTasks == 0) {
        document.children(buildEmptyTasks(actions.createTask));
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
                    buildWindowBar(
                        pageWidth, "FocusTomato · 任务", assets),
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
