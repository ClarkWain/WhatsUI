#include "focus_timer_page.h"

#include "../components/common_components.h"
#include "../focus_style.h"
#include "wui/declarative.h"

#include <algorithm>
#include <utility>

namespace whatsui::focus_tomato::presentation {
namespace {

const TaskRecord* findSessionTask(
    const FocusData& data, const FocusSessionRecord& session)
{
    if (!session.taskId) return nullptr;
    const auto task = std::find_if(
        data.tasks.begin(),
        data.tasks.end(),
        [&session](const TaskRecord& item) {
            return item.id == *session.taskId;
        });
    return task == data.tasks.end() ? nullptr : &*task;
}

wui::Box buildTaskContext(
    const FocusSessionRecord& session, const TaskRecord* task, float width)
{
    using namespace wui;
    const int estimated = task ? task->estimatedPomodoros : 1;
    const int completed = task ? task->completedPomodoros : 0;
    const std::string progress =
        std::to_string(std::min(completed + 1, estimated))
        + " / " + std::to_string(estimated);

    return Box()
        .background(style::surface)
        .radius(14.0f)
        .width(width)
        .height(64.0f)
        .padding({14.0f, 12.0f, 14.0f, 12.0f})
        .children(
            Row()
                .align(wui::Alignment::Center)
                .gap(10.0f)
                .children(
                    Column()
                        .gap(3.0f)
                        .align(wui::Alignment::Start)
                        .flex(1.0f)
                        .children(
                            Text(session.titleSnapshot)
                                .style(style::text(13.0f, 500, 19.0f))
                                .color(style::textPrimary),
                            Text("今日优先任务 · 预计 "
                                 + std::to_string(estimated)
                                 + " 个番茄")
                                .style(style::text(11.0f, 400, 16.0f))
                                .color(style::textSecondary)
                        ),
                    Box()
                        .background(style::accent)
                        .radius(999.0f)
                        .padding({10.0f, 6.0f, 10.0f, 6.0f})
                        .children(
                            Text(progress)
                                .style(style::text(11.0f, 500, 16.0f))
                                .color(style::surface)
                        )
                )
        );
}

wui::Box buildTimerStage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float width,
    bool running,
    int remainingPomodoros)
{
    using namespace wui;
    return Box()
        .width(width)
        .height(340.0f)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Column()
                .gap(8.0f)
                .align(wui::Alignment::Center)
                .children(
                    Row()
                        .gap(8.0f)
                        .children(
                            buildPill("番茄钟", true),
                            buildPill(
                                running ? "专注中" : "已暂停", false)
                        ),
                    buildFixedImage(
                        assets.mascotFocus,
                        200.0f,
                        200.0f,
                        "戴耳机专注的番茄吉祥物",
                        true),
                    Text()
                        .bind(viewModel.remainingText())
                        .style(style::text(52.0f, 700, 58.0f))
                        .color(style::textPrimary),
                    Text("本轮完成后，任务还剩 "
                         + std::to_string(remainingPomodoros)
                         + " 个番茄")
                        .style(style::text(11.0f, 400, 16.0f))
                        .color(style::textSecondary)
                )
        );
}

wui::Box buildTimerControls(
    const FocusAssets& assets,
    float width,
    bool running,
    FocusTimerPageActions& actions)
{
    using namespace wui;
    return Box()
        .width(width)
        .height(76.0f)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Row()
                .align(wui::Alignment::Center)
                .gap(20.0f)
                .children(
                    buildGlyphControl(
                        "↻",
                        40.0f,
                        21.0f,
                        false,
                        "重置本轮",
                        std::move(actions.reset)),
                    buildGlyphControl(
                        running ? "▮▮" : "▶",
                        68.0f,
                        running ? 15.0f : 22.0f,
                        true,
                        running ? "暂停专注" : "继续专注",
                        std::move(actions.toggle)),
                    buildGlyphControl(
                        "▶|",
                        40.0f,
                        13.0f,
                        false,
                        "提前结束",
                        std::move(actions.abort))
                )
        );
}

wui::Box buildFocusFooter(
    int shortBreakMinutes,
    float width,
    std::function<void()> recordInterruption)
{
    using namespace wui;
    return Box()
        .width(width)
        .height(48.0f)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Column()
                .gap(4.0f)
                .align(wui::Alignment::Center)
                .children(
                    Box()
                        .accessibleRole(wui::AccessibilityRole::Button)
                        .accessibleLabel("记录一次打断")
                        .onClick(std::move(recordInterruption))
                        .children(
                            Text("＋ 记录一次打断")
                                .style(style::text(12.0f, 500, 17.0f))
                                .color(style::accent)
                        ),
                    Text("完成本轮 → 短休息 "
                         + std::to_string(shortBreakMinutes)
                         + " 分钟 → 返回任务")
                        .style(style::text(10.0f, 400, 15.0f))
                        .color(style::textMuted)
                )
        );
}

wui::Box buildMissingSessionPage(
    float pageWidth, float pageHeight)
{
    using namespace wui;
    return Box()
        .background(style::canvas)
        .width(pageWidth)
        .height(pageHeight)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Text("没有活动中的专注会话")
                .style(style::text(18.0f, 700, 26.0f))
                .color(style::textPrimary)
        );
}

} // namespace

wui::Box FocusTimerPage::body()
{
    using namespace wui;
    FocusViewModel& viewModel = *viewModel_;
    const FocusAssets& assets = *assets_;
    const float pageWidth = pageWidth_;
    const float pageHeight = pageHeight_;
    FocusTimerPageActions actions = std::move(actions_);
    const FocusSessionRecord* session = viewModel.activeSession();
    if (session == nullptr) {
        return buildMissingSessionPage(pageWidth, pageHeight);
    }

    const TaskRecord* task = findSessionTask(viewModel.data(), *session);
    const float contentWidth = std::min(424.0f, pageWidth - 56.0f);
    const bool running = viewModel.isRunning();
    const int estimated = task ? task->estimatedPomodoros : 1;
    const int completed = task ? task->completedPomodoros : 0;
    const int remainingPomodoros =
        std::max(0, estimated - completed - 1);

    auto content = Column()
        .gap(12.0f)
        .align(wui::Alignment::Center)
        .children(
            buildTaskContext(*session, task, contentWidth),
            buildTimerStage(
                viewModel,
                assets,
                contentWidth,
                running,
                remainingPomodoros),
            buildTimerControls(
                assets, contentWidth, running, actions),
            buildFocusFooter(
                viewModel.data().settings.shortBreakMinutes,
                contentWidth,
                std::move(actions.recordInterruption))
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
                        pageWidth, "FocusTomato · 专注", assets),
                    Box()
                        .height(pageHeight - 56.0f)
                        .padding({28.0f, 20.0f, 28.0f, 20.0f})
                        .contentAlign(
                            wui::Alignment::Center,
                            wui::Alignment::Center)
                        .children(
                            std::move(content)
                        )
                )
        );
}

} // namespace whatsui::focus_tomato::presentation
