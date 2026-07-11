#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "wui/state.h"
#include "wui/scheduler.h"

namespace {

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
    expect(state.set(10), "Setting different value should return true");
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
    doubled.subscribe([&observed](const int& value) { observed.push_back(value); });

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
    minVal.subscribe([&notifyCount](const int&) { ++notifyCount; });

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
        derived.subscribe([&notifyCount](const int&) { ++notifyCount; });
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
    state.subscribe([&](const int&) { ++secondCalls; });

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
    state.subscribe([&](const int& value) {
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
    state.subscribe([&](const int& value) {
        events.push_back("a" + std::to_string(value));
        if (value == 1) {
            state.set(2);
        }
    });
    state.subscribe([&](const int& value) { events.push_back("b" + std::to_string(value)); });

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

} // namespace

int main()
{
    testStateDefaultConstruct();
    testStateExplicitConstruct();
    testStateSetReturnValue();
    testStateSubscribeAndNotify();
    testStateUnsubscribe();
    testStateMultipleSubscribers();
    testBindingFromState();
    testBindingCustomGetterSetter();
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
    return 0;
}
