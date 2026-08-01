#include "completion_page.h"

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

std::unique_ptr<wui::Node> buildCompletionMetrics(
    const FocusData& data, float availableWidth)
{
    using namespace wui;
    const auto statistics = calculateFocusStatistics(
        data, 0, std::numeric_limits<std::int64_t>::max());
    const float cardWidth =
        std::min(180.0f, (availableWidth - 24.0f) / 3.0f);
    return Row()
        .gap(12.0f)
        .align(wui::Alignment::Start)
        .children(
            buildMetricCard(
                cardWidth, "专注时长", completedDuration(data), ""),
            buildMetricCard(cardWidth, "完成番茄", "1", "个"),
            buildMetricCard(
                cardWidth,
                "今日完成",
                std::to_string(statistics.completedFocusSessions),
                "个")
        )
        .build();
}

std::unique_ptr<wui::Node> buildCompletionActions(
    CompletionPageActions actions)
{
    using namespace wui;
    return Row()
        .gap(14.0f)
        .children(
            buildSecondaryTextButton(
                "休息一下", std::move(actions.startBreak)),
            buildPrimaryTextButton(
                "继续专注", std::move(actions.continueFocus))
        )
        .build();
}

} // namespace

std::unique_ptr<wui::Node> buildCompletionPage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float pageWidth,
    float pageHeight,
    CompletionPageActions actions)
{
    using namespace wui;
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
            buildCompletionMetrics(viewModel.data(), contentWidth),
            buildCompletionActions(std::move(actions))
        )
        .build();

    return Box()
        .background(style::canvas)
        .width(pageWidth)
        .height(pageHeight)
        .children(
            Column()
                .align(wui::Alignment::Stretch)
                .children(
                    buildWindowBar(
                        pageWidth, "FocusTomato · 完成", assets),
                    Box()
                        .height(pageHeight - 56.0f)
                        .padding({28.0f, 20.0f, 28.0f, 24.0f})
                        .contentAlign(
                            wui::Alignment::Center,
                            wui::Alignment::Center)
                        .children(
                            std::move(content)
                        )
                )
        )
        .build();
}

} // namespace whatsui::focus_tomato::presentation
