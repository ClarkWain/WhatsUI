#include "short_break_page.h"

#include "../components/common_components.h"
#include "../focus_style.h"
#include "wui/declarative.h"

#include <algorithm>
#include <utility>

namespace whatsui::focus_tomato::presentation {
namespace {

wui::Box buildBreakContext(
    const FocusViewModel& viewModel,
    const FocusSessionRecord& session,
    float width)
{
    using namespace wui;
    const TaskRecord* task = viewModel.selectedTask();
    const std::string nextTitle =
        task ? task->title : "选择下一项任务";
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
                            Text("下一轮：" + nextTitle)
                                .style(style::text(13.0f, 500, 19.0f))
                                .color(style::textPrimary),
                             Text(session.type == SessionType::LongBreak
                                      ? "完成一轮专注周期 · 充分放松后继续"
                                      : "刚完成 1 个番茄 · 休息后继续")
                                .style(style::text(11.0f, 400, 16.0f))
                                .color(style::textSecondary)
                        ),
                    Box()
                        .background(style::accent)
                        .radius(999.0f)
                        .padding({10.0f, 6.0f, 10.0f, 6.0f})
                        .children(
                            Text("休息中")
                                .style(style::text(11.0f, 500, 16.0f))
                                .color(style::surface)
                        )
                )
        );
}

wui::Box buildBreakStage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    const FocusSessionRecord& session,
    float width)
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
                            buildPill(
                                session.type == SessionType::LongBreak
                                    ? "长休息" : "短休息",
                                true),
                            buildPill(
                                std::to_string(
                                    session.plannedDurationMs / kMinuteMs)
                                    + " 分钟",
                                false)
                        ),
                    buildFixedImage(
                        assets.mascotBreak,
                        200.0f,
                        200.0f,
                        "正在休息的番茄吉祥物",
                        true),
                    Text()
                        .bind(viewModel.remainingText())
                        .style(style::text(52.0f, 700, 58.0f))
                        .color(style::textPrimary),
                    Text("喝水 · 站起来 · 看向远处")
                        .style(style::text(11.0f, 400, 16.0f))
                        .color(style::textSecondary)
                )
        );
}

wui::Box buildBreakControls(
    float width,
    bool running,
    BreakTimerPageActions actions)
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
                        "重置休息",
                        std::move(actions.reset))
                        .automationId("focus.break.reset"),
                    buildGlyphControl(
                        running ? "▮▮" : "▶",
                        68.0f,
                        running ? 15.0f : 22.0f,
                        true,
                        running ? "暂停休息" : "继续休息",
                        std::move(actions.toggle))
                        .automationId("focus.break.toggle"),
                    buildGlyphControl(
                        "▶|",
                        40.0f,
                        13.0f,
                        false,
                        "提前结束休息",
                        std::move(actions.skip))
                        .automationId("focus.break.skip")
                )
        );
}

wui::Box buildBreakFooter(
    const FocusViewModel& viewModel, float width)
{
    using namespace wui;
    const TaskRecord* task = viewModel.selectedTask();
    const std::string nextTitle =
        task ? task->title : "任务列表";
    return Box()
        .width(width)
        .height(48.0f)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Column()
                .gap(4.0f)
                .align(wui::Alignment::Center)
                .children(
                    Text("休息也可以提前结束")
                        .style(style::text(12.0f, 500, 17.0f))
                        .color(style::accent),
                    Text("休息结束 → 回到任务 · " + nextTitle)
                        .style(style::text(10.0f, 400, 15.0f))
                        .color(style::textMuted)
                )
        );
}

} // namespace

wui::Box BreakTimerPage::body()
{
    using namespace wui;
    FocusViewModel& viewModel = *viewModel_;
    const FocusAssets& assets = *assets_;
    const float pageWidth = pageWidth_;
    const float pageHeight = pageHeight_;
    BreakTimerPageActions actions = std::move(actions_);
    const auto minimizeShortcut = actions.minimize;
    const auto toggleShortcut = actions.toggle;
    const FocusSessionRecord* session = viewModel.activeSession();
    if (session == nullptr || session->type == SessionType::Focus) {
        return Box()
            .background(style::canvas)
            .width(pageWidth)
            .height(pageHeight)
            .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
            .children(
                Text("没有活动中的休息会话")
                    .style(style::text(18.0f, 700, 26.0f))
                    .color(style::textPrimary)
            );
    }
    const float contentWidth = std::min(424.0f, pageWidth - 56.0f);
    auto content = Column()
        .gap(12.0f)
        .align(wui::Alignment::Center)
        .children(
            buildBreakContext(viewModel, *session, contentWidth),
            buildOperationBanner(viewModel, contentWidth),
            buildBreakStage(viewModel, assets, *session, contentWidth),
            buildBreakControls(
                contentWidth, viewModel.isRunning(), std::move(actions)),
            buildBreakFooter(viewModel, contentWidth)
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
                        "← 返回任务 · 休息继续",
                        "focus.break.minimize",
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
