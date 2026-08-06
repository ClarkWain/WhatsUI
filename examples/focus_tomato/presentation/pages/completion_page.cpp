#include "completion_page.h"

#include "../components/common_components.h"
#include "../focus_style.h"
#include "wui/declarative.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace whatsui::focus_tomato::presentation {
namespace {

std::string completedDuration(const FocusData& data)
{
    const auto session = std::find_if(
        data.sessions.rbegin(),
        data.sessions.rend(),
        [](const FocusSessionRecord& item) {
            return item.type == SessionType::Focus
                && item.status == SessionStatus::Completed;
        });
    const std::int64_t duration = session == data.sessions.rend()
        ? static_cast<std::int64_t>(data.settings.focusMinutes) * kMinuteMs
        : session->plannedDurationMs;
    const std::int64_t totalSeconds = duration / 1000;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << totalSeconds / 60
           << ':' << std::setw(2) << totalSeconds % 60;
    return output.str();
}

wui::Row buildCompletionMetrics(
    const FocusViewModel& viewModel, float availableWidth)
{
    using namespace wui;
    const auto statistics = viewModel.todayStatistics();
    const float cardWidth =
        std::min(180.0f, (availableWidth - 24.0f) / 3.0f);
    return Row()
        .gap(12.0f)
        .align(wui::Alignment::Start)
        .children(
            buildMetricCard(
                cardWidth,
                "专注时长",
                completedDuration(viewModel.data()),
                ""),
            buildMetricCard(cardWidth, "完成番茄", "1", "个"),
            buildMetricCard(
                cardWidth,
                "今日完成",
                std::to_string(statistics.completedFocusSessions),
                "个")
        );
}

wui::Row buildCompletionActions(
    const FocusViewModel& viewModel,
    CompletionPageActions actions)
{
    using namespace wui;
    const SessionType breakType = viewModel.recommendedBreakType();
    const int minutes = breakType == SessionType::LongBreak
        ? viewModel.data().settings.longBreakMinutes
        : viewModel.data().settings.shortBreakMinutes;
    const std::string breakLabel =
        (breakType == SessionType::LongBreak ? "长休息 " : "短休息 ")
        + std::to_string(minutes) + " 分钟";
    return Row()
        .gap(14.0f)
        .children(
            buildSecondaryTextButton(
                breakLabel, std::move(actions.startBreak)),
            buildPrimaryTextButton(
                "继续专注", std::move(actions.continueFocus))
        );
}

} // namespace

wui::Box CompletionPage::body()
{
    using namespace wui;
    FocusViewModel& viewModel = *viewModel_;
    const FocusAssets& assets = *assets_;
    const float pageWidth = pageWidth_;
    const float pageHeight = pageHeight_;
    CompletionPageActions actions = std::move(actions_);
    const auto returnToTasks = actions.returnToTasks;
    const float contentWidth = std::max(320.0f, pageWidth - 56.0f);
    auto content = Column()
        .gap(14.0f)
        .align(wui::Alignment::Center)
        .children(
            buildFixedImage(
                assets.mascotComplete,
                std::min(280.0f, contentWidth),
                std::min(280.0f, contentWidth),
                "庆祝完成番茄钟的番茄吉祥物",
                true),
            Text("太棒了！")
                .style(style::text(30.0f, 700, 43.0f))
                .color(style::textPrimary),
            Text("一个番茄钟完成了，注意力被好好保存下来。")
                .style(style::text(13.0f, 400, 19.0f))
                .color(style::textSecondary),
            buildOperationBanner(viewModel, contentWidth),
            buildCompletionMetrics(viewModel, contentWidth),
            buildCompletionActions(viewModel, std::move(actions))
        );

    return Box()
        .background(style::canvas)
        .width(pageWidth)
        .height(pageHeight)
        .onKey([returnToTasks](const wui::KeyEvent& event) {
            if (event.action == wui::KeyAction::Down
                && (event.keyCode == 27
                    || (event.keyCode == 263
                        && (event.modifiers & wui::KeyModifierAlt) != 0))
                && returnToTasks) {
                returnToTasks();
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
                        "focus.completion.back",
                        returnToTasks),
                    Box()
                        .height(pageHeight - 52.0f)
                        .padding({28.0f, 20.0f, 28.0f, 24.0f})
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
