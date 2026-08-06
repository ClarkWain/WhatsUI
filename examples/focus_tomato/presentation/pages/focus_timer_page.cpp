#include "focus_timer_page.h"

#include "../components/common_components.h"
#include "../focus_style.h"
#include "wui/declarative.h"

#include <algorithm>
#include <optional>
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
    const std::string progress = task
        ? std::to_string(std::min(completed + 1, estimated))
            + " / " + std::to_string(estimated)
        : "自由";
    const std::string detail = task
        ? "今日优先任务 · 预计 " + std::to_string(estimated) + " 个番茄"
        : "不关联任务 · 仍会计入今日专注统计";

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
                            Text(detail)
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
    std::optional<int> remainingPomodoros)
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
                    Text(remainingPomodoros
                             ? "本轮完成后，任务还剩 "
                                 + std::to_string(*remainingPomodoros)
                                 + " 个番茄"
                             : "本轮完成后会计入今日专注统计")
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
                        std::move(actions.reset))
                        .automationId("focus.timer.reset"),
                    buildGlyphControl(
                        running ? "▮▮" : "▶",
                        68.0f,
                        running ? 15.0f : 22.0f,
                        true,
                        running ? "暂停专注" : "继续专注",
                        std::move(actions.toggle))
                        .automationId("focus.timer.toggle"),
                    buildGlyphControl(
                        "▶|",
                        40.0f,
                        13.0f,
                        false,
                        "提前结束",
                        std::move(actions.abort))
                        .automationId("focus.timer.abort")
                )
        );
}

wui::Box buildFocusFooter(float width)
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
                    Text("完成本轮 → 进入休息 → 返回任务")
                        .style(style::text(12.0f, 500, 17.0f))
                        .color(style::accent),
                    Text("计时、结算和休息状态会自动保存")
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
    const auto minimizeShortcut = actions.minimize;
    const auto toggleShortcut = actions.toggle;
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
            buildOperationBanner(viewModel, contentWidth),
            buildTimerStage(
                viewModel,
                assets,
                contentWidth,
                running,
                task ? std::optional<int>{remainingPomodoros}
                     : std::nullopt),
            buildTimerControls(
                assets, contentWidth, running, actions),
            buildFocusFooter(contentWidth)
        );

    return Box()
        .background(style::canvas)
        .width(pageWidth)
        .height(pageHeight)
        .onKey([minimizeShortcut, toggleShortcut](const wui::KeyEvent& event) {
            if (event.action != wui::KeyAction::Down) return false;
            if ((event.keyCode == 27
                    || (event.keyCode == 263
                        && (event.modifiers & wui::KeyModifierAlt) != 0))
                && minimizeShortcut) {
                minimizeShortcut();
                return true;
            }
            if (event.keyCode == 32 && toggleShortcut) {
                toggleShortcut();
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
                        "← 返回任务 · 计时继续",
                        "focus.timer.minimize",
                        minimizeShortcut),
                    Box()
                        .height(pageHeight - 52.0f)
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
