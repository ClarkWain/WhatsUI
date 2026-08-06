#include "application/focus_repository.h"
#include "presentation/focus_view_model.h"
#include "presentation/focus_router.h"
#include "presentation/pages/completion_page.h"
#include "presentation/pages/focus_timer_page.h"
#include "presentation/pages/session_setup_page.h"
#include "presentation/pages/short_break_page.h"
#include "presentation/pages/task_list_page.h"
#include "wui/view.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

using namespace whatsui::focus_tomato;
using namespace whatsui::focus_tomato::presentation;

static_assert(wui::isViewLikeV<TaskListPage>);
static_assert(wui::isViewLikeV<SessionSetupPage>);
static_assert(wui::isViewLikeV<FocusTimerPage>);
static_assert(wui::isViewLikeV<CompletionPage>);
static_assert(wui::isViewLikeV<BreakTimerPage>);
static_assert(!std::is_base_of_v<wui::Node, TaskListPage>);

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

class RecordingRepository final : public FocusRepository {
public:
    RepositoryWriteResult save(const FocusData& data) override
    {
        const auto validation = validateFocusData(data);
        if (!validation.ok()) {
            return {
                RepositoryWriteStatus::ValidationRejected,
                validation,
                validation.summary(),
            };
        }
        persisted = data;
        ++saveCalls;
        return {};
    }

    FocusData persisted;
    int saveCalls{0};
};

void focusCompletionAndBreakAreOneValidatedFlow()
{
    RecordingRepository repository;
    FocusDataService service(repository);
    std::int64_t now = 10'000;
    int id = 0;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [&id] { return "flow-" + std::to_string(++id); });

    expect(viewModel.addTask("完成产品设计稿", 3).succeeded(),
           "The task-list entry should persist a validated task");
    viewModel.selectTask(service.data().tasks.front().id);
    expect(viewModel.startSelectedFocus().succeeded(),
           "The setup page should start its selected focus session");
    expect(viewModel.isRunning()
               && viewModel.remainingText().get() == "25:00",
           "The timer page should expose the active countdown");

    now += 5 * kMinuteMs;
    expect(viewModel.toggleActiveSession().succeeded()
               && !viewModel.isRunning()
               && viewModel.remainingText().get() == "20:00",
           "Pausing should preserve the deadline-derived remaining time");
    expect(viewModel.resetActiveSession().succeeded()
               && viewModel.remainingText().get() == "25:00",
           "Reset should restore the planned duration without replacing the session");
    expect(viewModel.toggleActiveSession().succeeded()
               && viewModel.isRunning(),
           "The primary timer control should resume a paused session");

    now = *viewModel.activeSession()->targetEndAtUtcMs;
    expect(viewModel.updateClock(),
           "A reached deadline should atomically finalize the focus session");
    expect(service.data().tasks.front().completedPomodoros == 1
               && !service.data().activeSessionId,
           "Completion should update task progress and clear recovery state");

    expect(viewModel.startBreak().succeeded()
               && viewModel.activeSession()->type == SessionType::ShortBreak
               && viewModel.remainingText().get() == "05:00",
           "The completion page should enter a task-free short break");
    now += kMinuteMs;
    expect(viewModel.skipActiveBreak().succeeded()
               && !service.data().activeSessionId
               && service.data().sessions.back().status
                    == SessionStatus::Skipped,
           "Skipping a break should return to tasks with a truthful terminal fact");

    expect(repository.persisted == service.data() && repository.saveCalls == 9,
           "Every visible transition should publish exactly what was persisted");
}

FocusData completionPendingData()
{
    FocusData data;
    data.tasks.push_back({
        "task-1", "恢复完成", TaskStatus::Active, 2, 0, 1024, 1,
        1'000, 1'000,
    });
    data.sessions.push_back({
        "pending-1",
        std::string{"task-1"},
        "恢复完成",
        SessionType::Focus,
        25 * kMinuteMs,
        10'000,
        std::nullopt,
        0,
        SessionStatus::CompletionPending,
        std::nullopt,
        CompletionReason::Natural,
        "pending-1",
    });
    data.activeSessionId = "pending-1";
    data.timerSnapshot = TimerSnapshot{
        kCurrentSchemaVersion,
        "pending-1",
        SessionStatus::CompletionPending,
        10'000 + 25 * kMinuteMs,
        std::nullopt,
        0,
    };
    return data;
}

void completionPendingRetriesAfterRestart()
{
    RecordingRepository repository;
    FocusDataService service(repository, completionPendingData());
    std::int64_t now = 10'000 + 25 * kMinuteMs + 500;
    FocusViewModel viewModel(
        service, [&now] { return now; }, [] { return "unused"; });

    expect(viewModel.selectedTaskId() == std::optional<std::string>{"task-1"},
           "Restart recovery should restore the active session's selected task");
    expect(viewModel.updateClock(),
           "A persisted completion-pending checkpoint must retry finalization");
    expect(service.data().sessions.front().status == SessionStatus::Completed
               && service.data().tasks.front().completedPomodoros == 1
               && !service.data().activeSessionId,
           "Recovery retry must publish exactly one completed focus fact");
}

FocusData dataAfterCompletedFocuses(int count)
{
    FocusData data;
    for (int index = 0; index < count; ++index) {
        const std::string id = "history-" + std::to_string(index + 1);
        const std::int64_t start = 10'000 + index * 2'000'000;
        data.sessions.push_back({
            id,
            std::nullopt,
            "自由专注",
            SessionType::Focus,
            25 * kMinuteMs,
            start,
            std::nullopt,
            0,
            SessionStatus::Completed,
            start + 25 * kMinuteMs,
            CompletionReason::Natural,
            id,
        });
    }
    return data;
}

void breakPolicyHonorsSettingsAndLongBreakCycle()
{
    RecordingRepository repository;
    FocusData pausedData = dataAfterCompletedFocuses(1);
    pausedData.settings.autoStartBreak = false;
    FocusDataService pausedService(repository, pausedData);
    FocusViewModel pausedViewModel(
        pausedService, [] { return 20'000'000; }, [] { return "break-paused"; });
    expect(pausedViewModel.startBreak().succeeded(),
           "A configured break should be created");
    expect(pausedViewModel.activeSession()->status == SessionStatus::Paused
               && !pausedViewModel.activeSession()->targetEndAtUtcMs,
           "Disabling auto-start break must create one paused, recoverable break");

    FocusData longData = dataAfterCompletedFocuses(4);
    longData.settings.autoStartBreak = true;
    longData.settings.longBreakEvery = 4;
    FocusDataService longService(repository, longData);
    FocusViewModel longViewModel(
        longService, [] { return 20'000'000; }, [] { return "break-long"; });
    expect(longViewModel.startBreak().succeeded(),
           "The cycle boundary should create a break");
    expect(longViewModel.activeSession()->type == SessionType::LongBreak
               && longViewModel.activeSession()->plannedDurationMs
                    == 15 * kMinuteMs,
           "Every fourth completed focus must select the configured long break");

    FocusData overdueData = dataAfterCompletedFocuses(5);
    overdueData.settings.longBreakEvery = 4;
    FocusDataService overdueService(repository, overdueData);
    FocusViewModel overdueViewModel(
        overdueService, [] { return 22'000'000; }, [] { return "break-overdue"; });
    expect(overdueViewModel.recommendedBreakType() == SessionType::LongBreak,
           "Continuing focus at a cycle boundary must not silently lose the owed long break");
}

void startupRouteRestoresEveryActiveSessionType()
{
    RecordingRepository repository;
    FocusData focusData = completionPendingData();
    FocusDataService focusService(repository, focusData);
    FocusViewModel focusViewModel(
        focusService, [] { return 2'000'000; }, [] { return "unused"; });
    expect(focusInitialRoute(focusViewModel) == FocusRoute::Timer,
           "A restored focus must reopen its timer instead of a conflicting task list");

    FocusData breakData;
    breakData.sessions.push_back({
        "break-1", std::nullopt, "长休息", SessionType::LongBreak,
        15 * kMinuteMs, 10'000, std::nullopt, 15 * kMinuteMs,
        SessionStatus::Paused, std::nullopt, CompletionReason::None, "break-1",
    });
    breakData.activeSessionId = "break-1";
    breakData.timerSnapshot = TimerSnapshot{
        kCurrentSchemaVersion, "break-1", SessionStatus::Paused,
        10'000, std::nullopt, 15 * kMinuteMs,
    };
    FocusDataService breakService(repository, breakData);
    FocusViewModel breakViewModel(
        breakService, [] { return 20'000; }, [] { return "unused"; });
    expect(focusInitialRoute(breakViewModel) == FocusRoute::Break,
           "A restored short or long break must reopen the break timer");

    FocusDataService idleService(repository);
    FocusViewModel idleViewModel(
        idleService, [] { return 20'000; }, [] { return "unused"; });
    expect(focusInitialRoute(idleViewModel) == FocusRoute::Tasks,
           "An idle restart should open the task list");
}

void backwardWallClockJumpPausesWithoutGrantingExtraTime()
{
    RecordingRepository repository;
    FocusDataService service(repository);
    std::int64_t wall = 10'000'000;
    std::int64_t monotonic = 10'000;
    int id = 0;
    FocusViewModel viewModel(
        service,
        [&wall] { return wall; },
        [&id] { return "clock-" + std::to_string(++id); },
        [&monotonic] { return monotonic; });
    expect(viewModel.addTask("校准时钟", 1).succeeded(),
           "Clock test task should be created");
    viewModel.selectTask(service.data().tasks.front().id);
    expect(viewModel.startSelectedFocus().succeeded(),
           "Clock test focus should start");

    wall += 5 * kMinuteMs;
    monotonic += 5 * kMinuteMs;
    expect(!viewModel.updateClock()
               && viewModel.remainingText().get() == "20:00",
           "Normal wall and monotonic progress should keep running");

    wall -= 30 * kMinuteMs;
    monotonic += kMinuteMs;
    expect(!viewModel.updateClock(),
           "A backward wall-clock jump must not complete the focus");
    expect(viewModel.activeSession()->status == SessionStatus::Paused
               && viewModel.remainingText().get() == "19:00",
           "The safe recovery path must pause using monotonic elapsed time");
    expect(viewModel.hasOperationMessage().get(),
           "Clock recovery must be visible instead of silently changing time");
}

void taskFiltersAndCompletionUseOneViewModelPolicy()
{
    RecordingRepository repository;
    FocusData data;
    data.tasks = {
        {"active", "进行中", TaskStatus::Active, 1, 0, 1024, 1, 1'000, 1'000},
        {"done", "已完成", TaskStatus::Done, 1, 0, 2048, 1, 1'000, 1'000},
        {"archived", "已归档", TaskStatus::Archived, 1, 0, 3072, 1, 1'000, 1'000},
    };
    FocusDataService service(repository, data);
    FocusViewModel viewModel(
        service, [] { return 2'000; }, [] { return "unused"; });

    viewModel.setTaskFilter(TaskFilter::Active);
    expect(viewModel.isTaskVisible(service.data().tasks[0])
               && !viewModel.isTaskVisible(service.data().tasks[1])
               && !viewModel.isTaskVisible(service.data().tasks[2]),
           "The active filter must not confuse a filtered empty result with archived data");
    viewModel.setTaskFilter(TaskFilter::Completed);
    expect(!viewModel.isTaskVisible(service.data().tasks[0])
               && viewModel.isTaskVisible(service.data().tasks[1]),
           "The completed filter must expose only completed, non-archived tasks");

    expect(viewModel.toggleTaskCompletion("active").succeeded(),
           "The task-row completion control should use the validated service command");
    expect(service.data().tasks[0].status == TaskStatus::Done,
           "The view-model completion command must publish persisted state");
}

void freeFocusCanStartCompleteAndContinueWithoutATask()
{
    RecordingRepository repository;
    FocusDataService service(repository);
    std::int64_t now = 10'000;
    int id = 0;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [&id] { return "free-" + std::to_string(++id); });
    expect(viewModel.startFreeFocus().succeeded(),
           "The empty-task page should be able to start a real free focus");
    expect(viewModel.activeSession()->type == SessionType::Focus
               && !viewModel.activeSession()->taskId
               && viewModel.activeSession()->titleSnapshot == "自由专注",
           "Free focus must remain task-free while preserving a useful snapshot title");

    now = *viewModel.activeSession()->targetEndAtUtcMs;
    expect(viewModel.updateClock(), "Free focus should complete normally");
    const auto statistics = calculateFocusStatistics(
        service.data(), 1, now + 1);
    expect(statistics.completedFreeFocusSessions == 1,
           "Completed free focus must be counted without modifying any task cache");
    expect(viewModel.continueLastFocus().succeeded()
               && !viewModel.activeSession()->taskId,
           "Continue focus should repeat a completed free-focus context");
}

} // namespace

int main()
{
    try {
        focusCompletionAndBreakAreOneValidatedFlow();
        completionPendingRetriesAfterRestart();
        breakPolicyHonorsSettingsAndLongBreakCycle();
        startupRouteRestoresEveryActiveSessionType();
        backwardWallClockJumpPausesWithoutGrantingExtraTime();
        taskFiltersAndCompletionUseOneViewModelPolicy();
        freeFocusCanStartCompleteAndContinueWithoutATask();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
