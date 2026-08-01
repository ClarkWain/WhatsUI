#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "wui/state.h"
#include "wui/scheduler.h"
#include "wui/declarative.h"
#include "wui/ui_context.h"
#include "wui/ui_dispatcher.h"

namespace {

template <typename T, typename = void>
struct HasLegacySetValue : std::false_type {
};

template <typename T>
struct HasLegacySetValue<T,
                         std::void_t<decltype(std::declval<const T&>().setValue(1))>>
    : std::true_type {
};

template <typename T, typename = void>
struct HasLegacyPostValue : std::false_type {
};

template <typename T>
struct HasLegacyPostValue<T,
                          std::void_t<decltype(std::declval<const T&>().postValue(1))>>
    : std::true_type {
};

static_assert(!HasLegacySetValue<wui::State<int>>::value,
              "State must expose set(), not the legacy setValue() alias");
static_assert(!HasLegacyPostValue<wui::State<int>>::value,
              "State must expose post(), not the legacy postValue() alias");

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// --- State<T> tests ---

void testStateDefaultConstruct()
{
    wui::State<int> state;
    expect(state.get() == 0, "Default-constructed State<int> should be 0");
}

void testStateExplicitConstruct()
{
    wui::State<std::string> state{"hello"};
    expect(state.get() == "hello", "Explicit-constructed State should hold its value");
}

void testStateSetReturnValue()
{
    wui::State<int> state{5};
    expect(!state.set(5), "Setting same value should return false");
    expect(state.set(10), "set should return true for a different value");
    expect(!state.set(10), "set should return false for the current value");
    expect(state.get() == 10, "Value should be updated after set");
}

void testStateSubscribeAndNotify()
{
    wui::State<int> state{0};
    std::vector<int> observed;

    auto id = state.subscribe([&observed](const int& value) {
        observed.push_back(value);
    });

    state.set(1);
    state.set(2);
    state.set(2); // same value, no notify
    state.set(3);

    expect(observed.size() == 3, "Should have 3 notifications (skip duplicate)");
    expect(observed[0] == 1 && observed[1] == 2 && observed[2] == 3,
           "Notifications should carry correct values");

    state.unsubscribe(id);
}

void testStateUnsubscribe()
{
    wui::State<int> state{0};
    int callCount = 0;

    auto id = state.subscribe([&callCount](const int&) {
        ++callCount;
    });

    state.set(1);
    expect(callCount == 1, "Should notify once before unsubscribe");

    state.unsubscribe(id);
    state.set(2);
    expect(callCount == 1, "Should not notify after unsubscribe");
}

void testStateMultipleSubscribers()
{
    wui::State<int> state{0};
    int countA = 0;
    int countB = 0;

    auto idA = state.subscribe([&countA](const int&) { ++countA; });
    auto idB = state.subscribe([&countB](const int&) { ++countB; });

    state.set(1);
    expect(countA == 1 && countB == 1, "Both subscribers should be notified");

    state.unsubscribe(idA);
    state.set(2);
    expect(countA == 1, "Unsubscribed A should not be notified");
    expect(countB == 2, "B should still be notified");

    state.unsubscribe(idB);
}

void testStateRejectsEmptyObserver()
{
    wui::State<int> state{0};
    bool rejected = false;
    try {
        (void)state.subscribe({});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected,
           "State must reject an empty observer at the API boundary");
}

void testStatePostRunsOnUiThread()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    const std::thread::id uiThread = std::this_thread::get_id();
    wui::State<int> state{dispatcher.context(), 0};
    std::thread::id notificationThread;
    int notifications = 0;
    wui::StatePostResult postResult = wui::StatePostResult::ContextStopped;
    (void)state.subscribe([&](const int&) {
        notificationThread = std::this_thread::get_id();
        ++notifications;
    });

    std::thread worker([&] {
        postResult = state.post(42);
    });
    worker.join();

    expect(postResult == wui::StatePostResult::Scheduled,
           "The first pending value should schedule one UI task");
    expect(state.get() == 0 && notifications == 0,
           "post must not mutate State inline on a worker thread");
    expect(dispatcher.drain() == 1,
           "One posted State value should schedule one UI task");
    expect(state.get() == 42 && notifications == 1
               && notificationThread == uiThread,
           "post must publish and notify on the owning UI thread");
}

void testStatePostCoalescesBeforeDrain()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    wui::State<int> state{dispatcher.context(), 0};
    std::vector<int> observed;
    std::vector<wui::StatePostResult> results;
    (void)state.subscribe(
        [&](const int& value) { observed.push_back(value); });

    std::thread worker([&] {
        results.push_back(state.post(1));
        results.push_back(state.post(2));
        results.push_back(state.post(3));
    });
    worker.join();

    expect(results == std::vector<wui::StatePostResult>({
                          wui::StatePostResult::Scheduled,
                          wui::StatePostResult::Coalesced,
                          wui::StatePostResult::Coalesced}),
           "Only the first pending value should schedule a UI task");
    expect(dispatcher.drain() == 1,
           "Several pending values should coalesce into one UI task");
    expect(state.get() == 3 && observed == std::vector<int>({3}),
           "Coalesced post calls must publish only the latest value");
}

void testConcurrentPostProducersRemainCoalesced()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    wui::State<int> state{dispatcher.context(), 0};
    std::vector<std::thread> workers;

    for (int worker = 0; worker < 8; ++worker) {
        workers.emplace_back([&, worker] {
            for (int index = 0; index < 100; ++index) {
                (void)state.post(worker * 100 + index + 1);
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    expect(state.post(9999) == wui::StatePostResult::Coalesced,
           "A deterministic final value should join the pending publication");
    expect(dispatcher.drain() == 1 && state.get() == 9999,
           "Concurrent producers must remain one coalesced UI publication");
}

void testDestroyedPostedStateIsSafe()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    {
        auto state = std::make_unique<wui::State<std::string>>(
            dispatcher.context(), "before");
        (void)state->post("after");
    }
    expect(dispatcher.drain() == 1,
           "A queued update may be drained after its State is destroyed");
}

void testNewerSynchronousValueSupersedesOlderPostedValue()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    wui::State<std::string> state{dispatcher.context(), "initial"};
    std::vector<std::string> observed;
    (void)state.subscribe(
        [&](const std::string& value) { observed.push_back(value); });

    expect(state.post("stale worker result")
               == wui::StatePostResult::Scheduled,
           "The worker result should initially be scheduled");
    expect(state.set("new user value"),
           "A newer synchronous value should be committed immediately");
    expect(dispatcher.drain() == 1,
           "The superseded posted task should still drain safely");
    expect(state.get() == "new user value",
           "An older posted value must not overwrite a newer UI value");
    expect(observed == std::vector<std::string>({"new user value"}),
           "Observers must not receive a superseded posted value");
}

void testPostReportsStoppedContext()
{
    auto dispatcher = std::make_unique<wui::UiDispatcher>();
    dispatcher->bindToCurrentThread();
    wui::State<int> state{dispatcher->context(), 0};
    int notifications = 0;
    (void)state.subscribe([&](const int&) { ++notifications; });
    dispatcher.reset();

    expect(state.post(1) == wui::StatePostResult::ContextStopped,
           "Posting through a stopped UI context must be reported");
    expect(notifications == 0,
           "A stopped context must not notify State observers");
}

void testPostRequiresBoundContext()
{
    wui::State<int> state{0};
    bool rejected = false;
    try {
        (void)state.post(1);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    expect(rejected,
           "post must reject State without an owning UiContext");
}

void testBoundStateRejectsWorkerSet()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    wui::State<int> state{dispatcher.context(), 0};
    bool rejected = false;

    std::thread worker([&] {
        try {
            state.set(1);
        } catch (const std::logic_error&) {
            rejected = true;
        }
    });
    worker.join();

    expect(rejected && state.get() == 0,
           "A bound State must reject synchronous worker mutations");
}

void testObserverExceptionDoesNotPoisonState()
{
    wui::State<int> state{0};
    int successfulCalls = 0;
    bool throwOnce = true;
    (void)state.subscribe([&](const int&) {
        if (throwOnce) {
            throwOnce = false;
            throw std::runtime_error("observer failure");
        }
    });
    (void)state.subscribe([&](const int&) { ++successfulCalls; });

    bool propagated = false;
    try {
        state.set(1);
    } catch (const std::runtime_error&) {
        propagated = true;
    }
    expect(propagated && successfulCalls == 1,
           "Observer failures should propagate after the remaining observers run");

    expect(state.set(2),
           "State must remain usable after an observer throws");
    expect(successfulCalls == 2 && state.get() == 2,
           "A prior observer failure must not poison later notifications");
}

void testSubscriptionCanOutliveState()
{
    auto subscription = std::make_unique<wui::StateSubscription<int>>();
    {
        auto state = std::make_unique<wui::State<int>>(0);
        subscription->subscribe(*state, [](const int&) {});
    }

    subscription.reset();
}

void testBoundSubscriptionCanResetFromWorker()
{
    wui::UiDispatcher dispatcher;
    dispatcher.bindToCurrentThread();
    wui::State<int> state{dispatcher.context(), 0};
    auto subscription = std::make_unique<wui::StateSubscription<int>>();
    int notifications = 0;
    subscription->subscribe(
        state, [&](const int&) { ++notifications; });

    std::thread worker([&] { subscription->reset(); });
    worker.join();

    state.set(1);
    expect(notifications == 0,
           "Reset must deactivate its observer before UI cleanup runs");
    expect(dispatcher.drain() == 1,
           "Worker reset should schedule one UI-side observer cleanup");
}

// --- Binding<T> tests ---

void testBindingFromState()
{
    wui::State<int> state{42};
    wui::Binding<int> binding(state);

    expect(binding.get() == 42, "Binding should read from State");
    binding.set(100);
    expect(state.get() == 100, "Binding set should write to State");
}

void testBindingCustomGetterSetter()
{
    int storage = 7;
    wui::Binding<int> binding(
        [&storage]() -> const int& { return storage; },
        [&storage](int value) { storage = value * 2; });

    expect(binding.get() == 7, "Custom binding should read from getter");
    binding.set(5);
    expect(storage == 10, "Custom binding should write through setter");
}

void testStateCopiesShareReactiveIdentity()
{
    wui::State<int> original{1};
    wui::State<int> copy = original;
    int observed = 0;
    (void)original.subscribe(
        [&](const int& value) { observed = value; });

    expect(copy.set(2),
           "A copied State handle should update its shared reactive core");
    expect(original.get() == 2 && observed == 2,
           "All State handles must observe the same reactive identity");
}

void testBindingCanOutliveStateHandle()
{
    std::unique_ptr<wui::Binding<int>> binding;
    {
        wui::State<int> state{7};
        binding = std::make_unique<wui::Binding<int>>(state);
    }

    expect(binding->get() == 7,
           "Binding should retain StateCore after the original handle dies");
    binding->set(9);
    expect(binding->get() == 9,
           "A retained Binding should remain writable");
}

void testBoundTextCanOutliveStateHandle()
{
    std::unique_ptr<wui::Node> text;
    {
        wui::State<std::string> state{"retained"};
        text = wui::Text().bind(state).build();
    }

    text.reset();
}

// --- Computed<T> tests ---

void testComputedInitialValue()
{
    wui::State<int> a{3};
    wui::State<int> b{4};
    wui::Computed<int> product([&a, &b] { return a.get() * b.get(); }, a, b);

    expect(product.get() == 12, "Computed should hold initial derived value");
}

void testComputedRecomputes()
{
    wui::State<int> a{2};
    wui::State<int> b{3};
    wui::Computed<int> sum([&a, &b] { return a.get() + b.get(); }, a, b);

    a.set(10);
    expect(sum.get() == 13, "Computed should recompute when source a changes");

    b.set(7);
    expect(sum.get() == 17, "Computed should recompute when source b changes");
}

void testComputedNotifiesObservers()
{
    wui::State<int> x{1};
    wui::Computed<int> doubled([&x] { return x.get() * 2; }, x);

    std::vector<int> observed;
    (void)doubled.subscribe([&observed](const int& value) { observed.push_back(value); });

    x.set(5);
    x.set(5); // same computed value, should not notify
    x.set(3);

    expect(observed.size() == 2, "Computed should notify only when derived value changes");
    expect(observed[0] == 10 && observed[1] == 6, "Computed notifications should carry correct values");
}

void testComputedSkipsSameValue()
{
    wui::State<int> a{4};
    wui::State<int> b{6};
    // min(a, b) — changing b from 6 to 7 won't change min
    wui::Computed<int> minVal([&a, &b] { return std::min(a.get(), b.get()); }, a, b);

    int notifyCount = 0;
    (void)minVal.subscribe([&notifyCount](const int&) { ++notifyCount; });

    b.set(7); // min stays 4
    expect(notifyCount == 0, "Computed should not notify when result unchanged");
    expect(minVal.get() == 4, "Computed value should stay at 4");

    a.set(2); // min changes to 2
    expect(notifyCount == 1, "Computed should notify when result changes");
    expect(minVal.get() == 2, "Computed value should update to 2");
}

void testComputedUnsubscribesOnDestruction()
{
    wui::State<int> src{1};
    int notifyCount = 0;

    {
        wui::Computed<int> derived([&src] { return src.get(); }, src);
        (void)derived.subscribe([&notifyCount](const int&) { ++notifyCount; });
        src.set(2);
        expect(notifyCount == 1, "Should notify while Computed is alive");
    }

    // After Computed is destroyed, State should not crash when set
    src.set(3);
    // If unsubscribe didn't work, this would crash or UB
}

void testComputedChain()
{
    wui::State<int> base{5};
    wui::Computed<int> doubled([&base] { return base.get() * 2; }, base);
    wui::Computed<int> quadrupled([&doubled] { return doubled.get() * 2; }, doubled);

    expect(quadrupled.get() == 20, "Chained Computed should derive correctly");

    base.set(3);
    expect(doubled.get() == 6, "First Computed should update");
    expect(quadrupled.get() == 12, "Chained Computed should propagate");
}

void testStateObserverCanUnsubscribeDuringNotification()
{
    wui::State<int> state{0};
    int firstCalls = 0;
    int secondCalls = 0;
    wui::State<int>::SubscriptionId firstId = 0;
    firstId = state.subscribe([&](const int&) {
        ++firstCalls;
        state.unsubscribe(firstId);
    });
    (void)state.subscribe([&](const int&) { ++secondCalls; });

    state.set(1);
    state.set(2);
    expect(firstCalls == 1, "An observer should be able to unsubscribe itself while notified");
    expect(secondCalls == 2, "Other observers should continue receiving state updates");
}

void testStateObserverCanRemoveLaterObserver()
{
    wui::State<int> state{0};
    int removedCalls = 0;
    wui::State<int>::SubscriptionId removedId = 0;
    const auto removerId = state.subscribe([&](const int&) { state.unsubscribe(removedId); });
    (void)removerId;
    removedId = state.subscribe([&](const int&) { ++removedCalls; });

    state.set(1);
    expect(removedCalls == 0, "An observer removed during notification must not receive that update");
}

void testNestedStateUpdateRemainsWellDefined()
{
    wui::State<int> state{0};
    std::vector<int> observed;
    (void)state.subscribe([&](const int& value) {
        observed.push_back(value);
        if (value == 1) {
            state.set(2);
        }
    });

    state.set(1);
    expect(state.get() == 2, "Nested state changes should commit the latest value");
    expect(observed.size() == 2 && observed[0] == 1 && observed[1] == 2,
           "Nested state changes should notify each committed value exactly once");
}

void testNestedStateUpdatePreservesDeliveryOrderForAllObservers()
{
    wui::State<int> state{0};
    std::vector<std::string> events;
    (void)state.subscribe([&](const int& value) {
        events.push_back("a" + std::to_string(value));
        if (value == 1) {
            state.set(2);
        }
    });
    (void)state.subscribe([&](const int& value) { events.push_back("b" + std::to_string(value)); });

    state.set(1);
    expect(events == std::vector<std::string>{"a1", "b1", "a2", "b2"},
           "Nested state commits must finish the current observer batch before delivering the next value");
}

void testStructuralUpdatesCoalesceByKey()
{
    int calls = 0;
    int value = 0;
    const int key = 1;
    wui::scheduleStructuralUpdate(&key, [&] { ++calls; value = 1; });
    wui::scheduleStructuralUpdate(&key, [&] { ++calls; value = 2; });

    wui::flushStructuralUpdates();
    expect(calls == 1 && value == 2, "Repeated structural work for one node should coalesce to its latest action");
    expect(!wui::hasPendingStructuralUpdates(), "Flushing should empty the structural update queue");
}

void testStructuralUpdatesDrainReentrantWork()
{
    std::vector<int> order;
    const int firstKey = 1;
    const int secondKey = 2;
    wui::scheduleStructuralUpdate(&firstKey, [&] {
        order.push_back(1);
        wui::scheduleStructuralUpdate(&secondKey, [&] { order.push_back(2); });
    });

    wui::flushStructuralUpdates();
    expect(order.size() == 2 && order[0] == 1 && order[1] == 2,
           "Work scheduled during a structural flush should commit in the same frame after its current batch");
}

// This is deliberately deterministic: when a lifecycle or subscription
// regression appears, the same operation number is reproducible locally and
// in CI.  It mixes State notification reentrancy with structural work that is
// allowed to outlive the tree node that originally queued it.
void testDeterministicMutationStress()
{
    using namespace wui;

    constexpr int kOperations = 1200;
    std::uint32_t random = 0xC0FFEEu;
    const auto nextRandom = [&random] {
        random = random * 1664525u + 1013904223u;
        return random;
    };

    wui::State<bool> visible{true};
    wui::State<std::vector<int>> items{{1, 2, 3}};
    wui::State<int> value{0};
    wui::ColumnNode root;
    int mountedBranches = 0;
    int generatedRows = 0;

    // A notification that removes itself also performs one nested state
    // mutation.  This exercises both stable observer snapshots and the
    // notification queue while the tree churns around it.
    int selfRemovalCalls = 0;
    bool nestedMutationSent = false;
    wui::State<int>::SubscriptionId selfRemovalId = 0;
    selfRemovalId = value.subscribe([&](const int& current) {
        ++selfRemovalCalls;
        value.unsubscribe(selfRemovalId);
        if (!nestedMutationSent) {
            nestedMutationSent = true;
            value.set(current + 10000);
        }
    });

    const auto makeBranch = [&]() -> std::unique_ptr<wui::Node> {
        return Column()
            .gap(2.0f)
            .children(
                If(visible).then([&] {
                    ++mountedBranches;
                    return ForEach<int>(items, [&](const int& item) {
                        ++generatedRows;
                        return Text().bind(value, [item](const int& current) {
                            return std::to_string(item) + ":" + std::to_string(current);
                        });
                    });
                }),
                Text().bind(value, [](const int& current) {
                    return std::string("value:") + std::to_string(current);
                })
            )
            .build();
    };

    for (int operation = 0; operation < kOperations; ++operation) {
        switch (nextRandom() % 6u) {
        case 0: // Add a tree that owns reactive and structural subscriptions.
            if (root.children().size() < 24) {
                root.appendChild(makeBranch());
            }
            break;
        case 1: // Delete a possibly recently queued tree before the safe point.
            if (!root.children().empty()) {
                const auto index = nextRandom() % root.children().size();
                (void)root.removeChild(index);
            }
            break;
        case 2:
            visible.set(!visible.get());
            break;
        case 3: {
            auto next = items.get();
            next.push_back(static_cast<int>(nextRandom() % 100u));
            items.set(std::move(next));
            break;
        }
        case 4: {
            auto next = items.get();
            if (!next.empty()) {
                next.erase(next.begin() + static_cast<std::ptrdiff_t>(nextRandom() % next.size()));
            }
            items.set(std::move(next));
            break;
        }
        default:
            value.set(static_cast<int>(nextRandom() % 100000u));
            break;
        }

        // Leave some work pending long enough for removals to invalidate it,
        // then require every frame boundary to converge completely.
        if (operation % 3 == 2 || operation + 1 == kOperations) {
            wui::flushStructuralUpdates();
            expect(!wui::hasPendingStructuralUpdates(),
                   "Mutation stress structural queue must converge at every frame boundary");
        }
    }

    expect(selfRemovalCalls == 1,
           "A nested self-removing observer must be delivered exactly once during mutation stress");
    expect(nestedMutationSent, "Mutation stress must exercise a nested State update");
    expect(mountedBranches > 0 && generatedRows > 0,
           "Mutation stress must mount branches and generate list rows");

    // Clearing the tree must synchronously release every structural
    // subscription.  Mutating the source states afterwards must not schedule
    // stale work or recreate branches that no longer exist.
    root.clearChildren();
    wui::flushStructuralUpdates();
    const int branchesBeforeReleaseCheck = mountedBranches;
    visible.set(!visible.get());
    items.set({7, 8, 9});
    value.set(value.get() + 1);
    expect(!wui::hasPendingStructuralUpdates(),
           "Destroyed stress branches must release structural subscriptions immediately");
    wui::flushStructuralUpdates();
    expect(mountedBranches == branchesBeforeReleaseCheck,
           "Destroyed stress branches must not be rebuilt by later State mutations");
}

} // namespace

int runTests()
{
    testStateDefaultConstruct();
    testStateExplicitConstruct();
    testStateSetReturnValue();
    testStateSubscribeAndNotify();
    testStateUnsubscribe();
    testStateMultipleSubscribers();
    testStateRejectsEmptyObserver();
    testStatePostRunsOnUiThread();
    testStatePostCoalescesBeforeDrain();
    testConcurrentPostProducersRemainCoalesced();
    testDestroyedPostedStateIsSafe();
    testNewerSynchronousValueSupersedesOlderPostedValue();
    testPostReportsStoppedContext();
    testPostRequiresBoundContext();
    testBoundStateRejectsWorkerSet();
    testObserverExceptionDoesNotPoisonState();
    testSubscriptionCanOutliveState();
    testBoundSubscriptionCanResetFromWorker();
    testBindingFromState();
    testBindingCustomGetterSetter();
    testStateCopiesShareReactiveIdentity();
    testBindingCanOutliveStateHandle();
    testBoundTextCanOutliveStateHandle();
    testComputedInitialValue();
    testComputedRecomputes();
    testComputedNotifiesObservers();
    testComputedSkipsSameValue();
    testComputedUnsubscribesOnDestruction();
    testComputedChain();
    testStateObserverCanUnsubscribeDuringNotification();
    testStateObserverCanRemoveLaterObserver();
    testNestedStateUpdateRemainsWellDefined();
    testNestedStateUpdatePreservesDeliveryOrderForAllObservers();
    testStructuralUpdatesCoalesceByKey();
    testStructuralUpdatesDrainReentrantWork();
    testDeterministicMutationStress();
    return 0;
}

int main()
{
    try {
        return runTests();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
