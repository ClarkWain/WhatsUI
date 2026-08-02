#include "wui/ui_dispatcher.h"
#include "wui/ui_context.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

void workerPostRunsOnlyWhenOwnerDrains()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();

    const std::thread::id owner = std::this_thread::get_id();
    std::atomic<int> wakeCalls{0};
    std::thread::id executionThread;
    dispatcher.setWakeCallback([&wakeCalls] { ++wakeCalls; });

    std::thread worker([&] {
        dispatcher.post([&] { executionThread = std::this_thread::get_id(); });
    });
    worker.join();

    expect(dispatcher.hasPending(),
           "A worker post must remain pending before the UI frame drains it");
    expect(executionThread == std::thread::id{},
           "A worker must never execute UI work inline");
    expect(wakeCalls == 1,
           "A transition from an empty queue must wake the platform loop once");
    expect(dispatcher.drain() == 1 && executionThread == owner,
           "The owner thread must execute the posted task during drain");
    expect(!dispatcher.hasPending(),
           "A completed drain must leave no pending UI work");
}

void postsAreOrderedAndReentrantWorkIsDrainedSafely()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    std::vector<int> order;

    dispatcher.post([&] {
        order.push_back(1);
        dispatcher.post([&] { order.push_back(3); });
    });
    dispatcher.post([&] { order.push_back(2); });

    expect(dispatcher.drain() == 3,
           "Drain must include work posted by an executing UI task");
    expect(order == std::vector<int>({1, 2, 3}),
           "Dispatcher tasks must retain FIFO frame order");
}

void emptyTasksAreIgnored()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    dispatcher.post({});
    expect(!dispatcher.hasPending() && dispatcher.drain() == 0,
           "An empty callback must not create phantom UI work");
}

void independentDispatchersMayOwnDifferentUiThreads()
{
    wui::UiDispatcher mainDispatcher;
    mainDispatcher.bindToCurrentThread();
    wui::UiDispatcher workerDispatcher;
    std::atomic<bool> workerOwned{false};
    std::thread worker([&] {
        workerDispatcher.bindToCurrentThread();
        workerOwned = workerDispatcher.context().isCurrentThread()
            && !mainDispatcher.context().isCurrentThread();
    });
    worker.join();
    expect(workerOwned && mainDispatcher.context().isCurrentThread(),
           "Each dispatcher must own its own UI thread independently");
}

void diagnosticsRunOnTheOwningContextAndCannotEscape()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    std::vector<wui::UiDiagnostic> observed;
    dispatcher.setDiagnosticHandler(
        [&observed](const wui::UiDiagnostic& diagnostic) {
            observed.push_back(diagnostic);
            throw std::runtime_error("diagnostic handlers must be isolated");
        });

    dispatcher.context().reportDiagnostic({
        wui::UiDiagnosticCode::InvalidNodeKey,
        "empty key",
        "TaskList",
    });
    expect(observed.size() == 1
               && observed.front().code
                   == wui::UiDiagnosticCode::InvalidNodeKey,
           "Diagnostics reported on the owner thread must be observable inline");
}

void wrongThreadChecksPublishDiagnosticsOnTheOwner()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    std::vector<wui::UiDiagnostic> observed;
    dispatcher.setDiagnosticHandler(
        [&observed](const wui::UiDiagnostic& diagnostic) {
            observed.push_back(diagnostic);
        });

    std::atomic<bool> rejected{false};
    std::thread worker([&] {
        try {
            dispatcher.context().requireCurrentThread();
        } catch (const std::logic_error&) {
            rejected = true;
        }
    });
    worker.join();

    expect(rejected,
           "A wrong-thread context check must reject the operation");
    expect(observed.empty(),
           "A worker must not invoke diagnostic observers inline");
    expect(dispatcher.drain() == 1
               && observed.size() == 1
               && observed.front().code
                   == wui::UiDiagnosticCode::WrongThreadMutation,
           "Wrong-thread diagnostics must be delivered by the owner context");
}

void contextRemainsSafeAfterDispatcherShutdown()
{
    wui::UiContext context;
    {
        wui::UiDispatcher dispatcher;
        dispatcher.bindToCurrentThread();
        context = dispatcher.context();
        expect(context.isAlive(),
               "A context obtained from a live dispatcher must be alive");
        expect(context.isCurrentThread(),
               "A bound context must recognize its owner thread");
    }

    bool executed = false;
    expect(!context.isAlive(),
           "Destroying the dispatcher must stop all derived contexts");
    expect(context.post([&] { executed = true; })
               == wui::DispatchResult::Stopped,
           "A stopped context must reject new tasks explicitly");
    expect(!executed,
           "A rejected task must never execute inline or after shutdown");
}

} // namespace

int main()
{
    try {
        workerPostRunsOnlyWhenOwnerDrains();
        postsAreOrderedAndReentrantWorkIsDrainedSafely();
        emptyTasksAreIgnored();
        independentDispatchersMayOwnDifferentUiThreads();
        diagnosticsRunOnTheOwningContextAndCannotEscape();
        wrongThreadChecksPublishDiagnosticsOnTheOwner();
        contextRemainsSafeAfterDispatcherShutdown();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
