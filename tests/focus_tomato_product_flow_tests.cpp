#include "application/focus_repository.h"
#include "presentation/focus_view_model.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace whatsui::focus_tomato;
using namespace whatsui::focus_tomato::presentation;

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

    expect(viewModel.startShortBreak().succeeded()
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

} // namespace

int main()
{
    try {
        focusCompletionAndBreakAreOneValidatedFlow();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
