#include "session_setup_page.h"

#include "../components/common_components.h"
#include "../components/task_execution_preferences.h"
#include "../focus_style.h"
#include "wui/declarative.h"

#include <algorithm>
#include <utility>

namespace whatsui::focus_tomato::presentation {
namespace {

wui::Box buildSelectedTaskCard(
    std::string title, std::string progress, float width)
{
    using namespace wui;
    return Box()
        .background(style::surface)
        .radius(16.0f)
        .width(width)
        .height(100.0f)
        .padding({20.0f, 18.0f, 20.0f, 18.0f})
        .children(
            Column()
                .gap(7.0f)
                .align(wui::Alignment::Start)
                .children(
                    Text(std::move(title))
                        .style(style::text(16.0f, 500, 23.0f))
                        .color(style::textPrimary),
                    Text(std::move(progress))
                        .style(style::text(12.0f, 400, 17.0f))
                        .color(style::textSecondary)
                )
        );
}

wui::Box buildSessionConfiguration(
    std::string configuration,
    float width,
    std::function<void()> manageTask)
{
    using namespace wui;
    return Box()
        .background(style::surface)
        .radius(16.0f)
        .width(width)
        .height(68.0f)
        .padding({18.0f, 0.0f, 14.0f, 0.0f})
        .children(
            Row()
                .align(wui::Alignment::Center)
                .gap(10.0f)
                .children(
                    Column()
                        .gap(3.0f)
                        .flex(1.0f)
                        .children(
                            Text("本轮执行设置")
                                .style(style::text(11.0f, 500, 16.0f))
                                .color(style::textMuted),
                            Text(std::move(configuration))
                                .style(style::text(12.0f, 500, 18.0f))
                                .color(style::textSecondary)
                        ),
                    Button("调整")
                        .automationId("focus.setup.manage-task")
                        .appearance(ButtonAppearance::Outline)
                        .onClick(std::move(manageTask))
                )
        );
}

wui::Column buildSetupContent(
    FocusViewModel& viewModel,
    float contentWidth,
    std::string title,
    std::string progress,
    std::string configuration,
    SessionSetupPageActions actions)
{
    using namespace wui;
    return Column()
        .gap(18.0f)
        .align(wui::Alignment::Center)
        .children(
            Text("准备好这一轮")
                .style(style::text(26.0f, 700, 38.0f))
                .color(style::textPrimary),
            buildSelectedTaskCard(
                std::move(title), std::move(progress), contentWidth),
            buildOperationBanner(viewModel, contentWidth),
            buildSessionConfiguration(
                std::move(configuration),
                contentWidth,
                std::move(actions.manageTask)),
            buildGlyphControl(
                "▶", 68.0f, 22.0f, true,
                "开始专注", std::move(actions.start)),
            Text("按 Space 开始 · 开始后可自动缩为迷你模式")
                .style(style::text(11.0f, 400, 16.0f))
                .color(style::textMuted)
        );
}

} // namespace

wui::Box SessionSetupPage::body()
{
    using namespace wui;
    FocusViewModel& viewModel = *viewModel_;
    const float pageWidth = pageWidth_;
    const float pageHeight = pageHeight_;
    SessionSetupPageActions actions = std::move(actions_);
    const TaskRecord* task = viewModel.selectedTask();
    const std::string title = task ? task->title : "选择一个任务";
    const int completed = task ? task->completedPomodoros : 0;
    const int estimated = task ? task->estimatedPomodoros : 1;
    const std::string progress =
        "第 " + std::to_string(std::min(completed + 1, estimated))
        + " / " + std::to_string(estimated) + " 个番茄 · 今日优先任务";
    const std::string configuration = task
        ? taskExecutionSummary(*task, viewModel.data().settings)
        : std::to_string(viewModel.data().settings.focusMinutes)
            + " 分钟 · "
            + soundscapeLabel(
                viewModel.data().settings.defaultSoundscapeId);
    const float contentWidth = std::max(320.0f, pageWidth - 64.0f);
    const auto start = actions.start;
    const auto back = actions.back;

    return Box()
        .background(style::canvas)
        .width(pageWidth)
        .height(pageHeight)
        .onKey(
            [start, back](const wui::KeyEvent& event) {
                if (event.action != wui::KeyAction::Down) return false;
                if (event.keyCode == 32 && start) {
                    start();
                    return true;
                }
                if ((event.keyCode == 27
                        || (event.keyCode == 263
                            && (event.modifiers & wui::KeyModifierAlt) != 0))
                    && back) {
                    back();
                    return true;
                }
                return false;
            })
        .children(
            Column()
                .align(wui::Alignment::Stretch)
                .children(
                    buildPageNavigationAction(
                        pageWidth,
                        "← 返回任务",
                        "focus.setup.back",
                        back),
                    Box()
                        .height(pageHeight - 52.0f)
                        .padding({32.0f, 44.0f, 32.0f, 32.0f})
                        .contentAlign(
                            wui::Alignment::Center,
                            wui::Alignment::Center)
                        .children(
                            buildSetupContent(
                                viewModel,
                                contentWidth,
                                title,
                                progress,
                                configuration,
                                std::move(actions))
                        )
                )
        );
}

} // namespace whatsui::focus_tomato::presentation
