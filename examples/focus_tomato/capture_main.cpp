#include "application/focus_data_service.h"
#include "presentation/focus_assets.h"
#include "presentation/focus_view_model.h"
#include "presentation/focus_style.h"
#include "presentation/pages/completion_page.h"
#include "presentation/pages/focus_timer_page.h"
#include "presentation/pages/session_setup_page.h"
#include "presentation/pages/short_break_page.h"
#include "presentation/pages/task_list_page.h"

#include "wsc/Canvas.h"
#include "wui/paint_context.h"
#include "wui/runtime.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using namespace whatsui::focus_tomato;
using namespace whatsui::focus_tomato::presentation;

class MemoryRepository final : public FocusRepository {
public:
    RepositoryWriteResult save(const FocusData& data) override
    {
        const auto validation = validateFocusData(data);
        if (!validation.ok()) {
            return {RepositoryWriteStatus::ValidationRejected, validation,
                    validation.summary()};
        }
        data_ = data;
        return {};
    }

private:
    FocusData data_;
};

FocusData seededData()
{
    FocusData data;
    constexpr std::int64_t base = 1'700'000'000'000;
    data.tasks = {
        {"task-design", "完成产品设计稿", TaskStatus::Active, 3, 0,
         1024, 1, base, base},
        {"task-book", "阅读专业书籍", TaskStatus::Active, 2, 0,
         2048, 1, base + 1, base + 1},
        {"task-health", "健身锻炼", TaskStatus::Active, 1, 0,
         3072, 1, base + 2, base + 2},
        {"task-skill", "学习新技能", TaskStatus::Active, 1, 0,
         4096, 1, base + 3, base + 3},
        {"task-docs", "整理工作文档", TaskStatus::Done, 2, 0,
         5120, 1, base + 4, base + 4},
    };
    for (int index = 0; index < 7; ++index) {
        const std::string id = "history-" + std::to_string(index + 1);
        const std::int64_t startedAt =
            base + 10'000 + static_cast<std::int64_t>(index) * 2'000'000;
        data.sessions.push_back({
            id,
            std::nullopt,
            "自由专注",
            SessionType::Focus,
            25 * kMinuteMs,
            startedAt,
            std::nullopt,
            0,
            SessionStatus::Completed,
            startedAt + 25 * kMinuteMs,
            CompletionReason::Natural,
            id,
        });
    }
    return data;
}

template <
    class Content,
    std::enable_if_t<wui::isViewLikeV<Content>, int> = 0>
void capture(Content&& content,
             int width,
             int height,
             const std::filesystem::path& path)
{
    constexpr float scale = 2.0f;
    auto canvas = wsc::Canvas::create(
        wsc::Canvas::Backend::Software,
        static_cast<int>(width * scale),
        static_cast<int>(height * scale));
    if (!canvas || !canvas->initializeContext()) {
        throw std::runtime_error("failed to create FocusTomato capture canvas");
    }
    wui::WhatsCanvasTextMeasurer text(*canvas, scale);
    wui::setTextMeasurer(&text);
    wui::UiRoot root;
    root.setContent(std::forward<Content>(content));
    root.layout({0.0f, 0.0f, static_cast<float>(width),
                 static_cast<float>(height)});
    wui::PaintContext paint(*canvas, scale);
    for (int pass = 0; pass < 2; ++pass) {
        canvas->beginFrame();
        paint.fillRect(
            {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)},
            style::canvas);
        root.prepare(paint);
        root.paint(paint);
        canvas->endFrame();
    }
    if (!canvas->savePixelsPPM(path.string())) {
        throw std::runtime_error("failed to save " + path.string());
    }
    wui::setTextMeasurer(nullptr);
    std::cout << "wrote " << path << std::endl;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path output =
            argc > 1 ? std::filesystem::path(argv[1])
                     : std::filesystem::path("focus_tomato_visual");
        std::filesystem::create_directories(output);
        wui::setTheme(wui::Theme{});

        MemoryRepository repository;
        FocusDataService service(repository, seededData());
        constexpr std::int64_t now = 1'800'000'000'000;
        int idSequence = 0;
        FocusViewModel viewModel(
            service,
            [now] { return now; },
            [&idSequence] {
                return "capture-" + std::to_string(++idSequence);
            });
        const FocusAssets assets =
            loadFocusAssets(std::filesystem::path(FOCUS_TOMATO_ASSET_DIR));

        capture(
            TaskListPage(
                viewModel, assets, 640.0f, 820.0f, {{}, {}}),
            640, 820, output / "01-task-list.ppm");

        viewModel.selectTask("task-design");
        capture(
            SessionSetupPage(
                viewModel, assets, 520.0f, 720.0f, {{}, {}}),
            520, 720, output / "02-session-setup.ppm");

        const auto started = viewModel.startSelectedFocus();
        if (!started.succeeded()) {
            throw std::runtime_error(
                "could not seed active capture session: " + started.message);
        }
        capture(
            FocusTimerPage(
                viewModel, assets, 480.0f, 720.0f, {{}, {}, {}, {}}),
            480, 720, output / "03-focus-timer.ppm");

        const std::string completedSessionId =
            *service.data().activeSessionId;
        const std::int64_t deadline = now + 25 * kMinuteMs;
        if (!service.markDeadlineReached(completedSessionId, deadline).succeeded()
            || !service.finalizeCompletion(
                    completedSessionId, deadline).succeeded()) {
            throw std::runtime_error(
                "could not seed completed capture session");
        }
        capture(
            CompletionPage(
                viewModel, assets, 640.0f, 820.0f, {{}, {}}),
            640, 820, output / "04-completion.ppm");

        const auto breakStarted = viewModel.startShortBreak();
        if (!breakStarted.succeeded()) {
            throw std::runtime_error(
                "could not seed short break capture: "
                + breakStarted.message);
        }
        capture(
            ShortBreakPage(
                viewModel, assets, 480.0f, 720.0f, {{}, {}, {}}),
            480, 720, output / "05-short-break.ppm");
        return 0;
    } catch (const std::exception& error) {
        wui::setTextMeasurer(nullptr);
        std::cerr << "FocusTomato capture: " << error.what() << std::endl;
        return 1;
    }
}
