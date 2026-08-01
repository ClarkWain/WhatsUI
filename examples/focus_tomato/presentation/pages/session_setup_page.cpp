#include "session_setup_page.h"

#include "../components/common_components.h"
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
    std::string configuration, float width)
{
    using namespace wui;
    return Box()
        .background(style::surface)
        .radius(16.0f)
        .width(width)
        .height(68.0f)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Text(std::move(configuration))
                .style(style::text(12.0f, 500, 18.0f))
                .color(style::textSecondary)
        );
}

wui::Column buildSetupContent(
    const FocusAssets& assets,
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
            buildSessionConfiguration(
                std::move(configuration), contentWidth),
            buildGlyphControl(
                "▶", 68.0f, 22.0f, true,
                "开始专注", std::move(actions.start)),
            Text("按 Space 开始 · 开始后可自动缩为迷你模式")
                .style(style::text(11.0f, 400, 16.0f))
                .color(style::textMuted)
        );
}

} // namespace

wui::Box buildSessionSetupPage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float pageWidth,
    float pageHeight,
    SessionSetupPageActions actions)
{
    using namespace wui;
    const TaskRecord* task = viewModel.selectedTask();
    const std::string title = task ? task->title : "选择一个任务";
    const int completed = task ? task->completedPomodoros : 0;
    const int estimated = task ? task->estimatedPomodoros : 1;
    const std::string progress =
        "第 " + std::to_string(std::min(completed + 1, estimated))
        + " / " + std::to_string(estimated) + " 个番茄 · 今日优先任务";
    const std::string configuration =
        std::to_string(viewModel.data().settings.focusMinutes)
        + " 分钟    雨声    完成后休息 "
        + std::to_string(viewModel.data().settings.shortBreakMinutes) + " 分钟";
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
                if (event.keyCode == 27 && back) {
                    back();
                    return true;
                }
                return false;
            })
        .children(
            Column()
                .align(wui::Alignment::Stretch)
                .children(
                    buildWindowBar(
                        pageWidth, "FocusTomato · 准备开始", assets),
                    Box()
                        .height(pageHeight - 56.0f)
                        .padding({32.0f, 44.0f, 32.0f, 32.0f})
                        .contentAlign(
                            wui::Alignment::Center,
                            wui::Alignment::Center)
                        .children(
                            buildSetupContent(
                                assets,
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
