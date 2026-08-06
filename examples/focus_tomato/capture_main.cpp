#include "application/focus_data_service.h"
#include "presentation/focus_assets.h"
#include "presentation/focus_view_model.h"
#include "presentation/focus_style.h"
#include "presentation/components/common_components.h"
#include "presentation/components/task_execution_preferences.h"
#include "presentation/pages/completion_page.h"
#include "presentation/pages/focus_timer_page.h"
#include "presentation/pages/session_setup_page.h"
#include "presentation/pages/short_break_page.h"
#include "presentation/pages/task_list_page.h"

#include "wsc/Canvas.h"
#include "wui/declarative.h"
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
        {"task-deleted", "取消的临时事项", TaskStatus::Archived, 1, 0,
         6144, 2, base + 5, base + 100'005},
        {"task-deleted-done", "已经完成的旧任务", TaskStatus::ArchivedDone,
         2, 0, 7168, 2, base + 6, base + 100'006},
    };
    data.tasks[0].execution = {
        40, TaskSoundPreference::Soundscape, "forest"};
    data.tasks[1].execution = {
        std::nullopt, TaskSoundPreference::Off, {}};
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

template <
    class Content,
    std::enable_if_t<wui::isViewLikeV<Content>, int> = 0>
wui::Box captureShell(
    float width,
    std::string title,
    const FocusAssets& assets,
    Content&& content)
{
    using namespace wui;
    return Box()
        .background(style::canvas)
        .width(width)
        .children(
            Column()
                .align(Alignment::Start)
                .children(
                    buildWindowBar(
                        width, std::move(title), assets, {}, false),
                    std::forward<Content>(content)
                )
        );
}

wui::Box buildThemeControlReference(float width, float height)
{
    using namespace wui;
    auto title = TextField("任务名称")
        .text("完成主题统一检查")
        .motionEnabled(false);
    title.node()->setVisualState(ControlVisualState::Focused, true);

    return Box()
        .background(style::canvas)
        .width(width)
        .height(height)
        .contentAlign(Alignment::Center, Alignment::Center)
        .children(
            Box()
                .background(style::surface)
                .radius(16.0f)
                .width(368.0f)
                .padding({22.0f, 20.0f, 22.0f, 20.0f})
                .children(
                    Column()
                        .gap(14.0f)
                        .align(Alignment::Stretch)
                        .children(
                            Text("FocusTomato 主题控件")
                                .style(style::text(20.0f, 700, 29.0f))
                                .color(style::textPrimary),
                            Text("标准控件必须继承番茄红和暖色中性色。")
                                .style(style::text(12.0f, 400, 18.0f))
                                .color(style::textSecondary),
                            std::move(title),
                            Row()
                                .gap(8.0f)
                                .children(
                                    Button("删除任务")
                                        .appearance(ButtonAppearance::Danger),
                                    Spacer().flex(1.0f),
                                    Button("取消")
                                        .appearance(ButtonAppearance::Outline),
                                    Button("保存修改")
                                        .appearance(ButtonAppearance::Primary)
                                )
                        )
                )
        );
}

wui::Box buildTaskPreferenceReference(float width, float height)
{
    using namespace wui;
    TaskExecutionDraft execution({
        40, TaskSoundPreference::Soundscape, "forest"});
    return Box()
        .background(style::canvas)
        .width(width)
        .height(height)
        .contentAlign(Alignment::Center, Alignment::Center)
        .children(
            Box()
                .background(style::surface)
                .radius(16.0f)
                .width(392.0f)
                .padding({22.0f, 20.0f, 22.0f, 20.0f})
                .children(
                    Column()
                        .gap(14.0f)
                        .align(Alignment::Stretch)
                        .children(
                            Column()
                                .gap(4.0f)
                                .children(
                                    Text("编辑任务")
                                        .style(style::text(
                                            20.0f, 700, 29.0f))
                                        .color(style::textPrimary),
                                    Text("设置工作量，以及开始专注时采用的默认偏好。")
                                        .style(style::text(
                                            12.0f, 400, 18.0f))
                                        .color(style::textSecondary)
                                ),
                            TextField("任务名称")
                                .text("完成产品设计稿"),
                            Row()
                                .align(Alignment::Center)
                                .gap(10.0f)
                                .children(
                                    Text("预计番茄数")
                                        .style(style::text(
                                            12.0f, 500, 18.0f))
                                        .color(style::textSecondary),
                                    TextField("1～99")
                                        .text("3")
                                        .flex(1.0f)
                                ),
                            buildTaskExecutionPreferenceFields(
                                FocusSettings{},
                                execution,
                                "focus.capture-task"),
                            Row()
                                .gap(8.0f)
                                .children(
                                    Button("删除任务")
                                        .appearance(ButtonAppearance::Danger),
                                    Spacer().flex(1.0f),
                                    Button("取消")
                                        .appearance(ButtonAppearance::Outline),
                                    Button("保存修改")
                                        .appearance(ButtonAppearance::Primary)
                                )
                        )
                )
        );
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path output =
            argc > 1 ? std::filesystem::path(argv[1])
                     : std::filesystem::path("focus_tomato_visual");
        std::filesystem::create_directories(output);
        wui::setTheme(style::focusTheme());

        MemoryRepository repository;
        FocusDataService service(repository, seededData());
        constexpr std::int64_t now = 1'700'014'100'000;
        int idSequence = 0;
        FocusViewModel viewModel(
            service,
            [now] { return now; },
            [&idSequence] {
                return "capture-" + std::to_string(++idSequence);
            });
        const FocusAssets assets =
            loadFocusAssets(std::filesystem::path(FOCUS_TOMATO_ASSET_DIR));
        constexpr float captureWidth = 520.0f;
        constexpr float captureHeight = 720.0f;
        constexpr float contentHeight =
            captureHeight - kFocusWindowBarHeight;

        capture(
            buildThemeControlReference(captureWidth, captureHeight),
            520, 720, output / "00-theme-controls.ppm");
        capture(
            buildTaskPreferenceReference(captureWidth, captureHeight),
            520, 720, output / "00-task-preferences.ppm");

        capture(
            captureShell(
                captureWidth, "FocusTomato · 任务", assets,
                TaskListPage(
                    viewModel, assets, captureWidth, contentHeight,
                    {{}, {}, {}, {}, {}, {}, {}, {}})),
            520, 720, output / "01-task-list.ppm");

        viewModel.setTaskFilter(TaskFilter::Deleted);
        capture(
            captureShell(
                captureWidth, "FocusTomato · 已删除任务", assets,
                TaskListPage(
                    viewModel, assets, captureWidth, contentHeight,
                    {{}, {}, {}, {}, {}, {}, {}, {}})),
            520, 720, output / "01-deleted-task-list.ppm");
        viewModel.setTaskFilter(TaskFilter::All);

        viewModel.selectTask("task-design");
        capture(
            captureShell(
                captureWidth, "FocusTomato · 开始专注", assets,
                SessionSetupPage(
                    viewModel, assets, captureWidth, contentHeight,
                    {{}, {}, {}})),
            520, 720, output / "02-session-setup.ppm");

        const auto started = viewModel.startSelectedFocus();
        if (!started.succeeded()) {
            throw std::runtime_error(
                "could not seed active capture session: " + started.message);
        }
        capture(
            captureShell(
                captureWidth, "FocusTomato · 任务", assets,
                TaskListPage(
                    viewModel, assets, captureWidth, contentHeight,
                    {{}, {}, {}, {}, {}, {}, {}, {}})),
            520, 720, output / "03-active-task-list.ppm");
        capture(
            captureShell(
                captureWidth, "FocusTomato · 专注", assets,
                FocusTimerPage(
                    viewModel, assets, captureWidth, contentHeight,
                    {{}, {}, {}, {}})),
            520, 720, output / "03-focus-timer.ppm");

        const std::string completedSessionId =
            *service.data().activeSessionId;
        const std::int64_t deadline = now
            + service.data().sessions.back().plannedDurationMs;
        if (!service.markDeadlineReached(completedSessionId, deadline).succeeded()
            || !service.finalizeCompletion(
                    completedSessionId, deadline).succeeded()) {
            throw std::runtime_error(
                "could not seed completed capture session");
        }
        capture(
            captureShell(
                captureWidth, "FocusTomato · 完成", assets,
                CompletionPage(
                    viewModel, assets, captureWidth, contentHeight,
                    {{}, {}, {}})),
            520, 720, output / "04-completion.ppm");

        const auto breakStarted = viewModel.startBreak();
        if (!breakStarted.succeeded()) {
            throw std::runtime_error(
                "could not seed short break capture: "
                + breakStarted.message);
        }
        capture(
            captureShell(
                captureWidth, "FocusTomato · 短休息", assets,
                BreakTimerPage(
                    viewModel, assets, captureWidth, contentHeight,
                    {{}, {}, {}, {}})),
            520, 720, output / "05-short-break.ppm");
        return 0;
    } catch (const std::exception& error) {
        wui::setTextMeasurer(nullptr);
        std::cerr << "FocusTomato capture: " << error.what() << std::endl;
        return 1;
    }
}
