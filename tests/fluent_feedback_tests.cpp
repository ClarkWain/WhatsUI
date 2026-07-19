#include <chrono>
#include <array>
#include <memory>
#include <stdexcept>

#include "wui/accessibility.h"
#include "wui/feedback.h"
#include "wui/runtime.h"
#include "wui/scheduler.h"

namespace {
void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

wui::PointerEvent pointer(wui::PointerAction action, float x, float y)
{
    return {0, wui::PointerType::Mouse, action, wui::MouseButton::Left, {x, y}};
}

void testToastIntentActionAndPause()
{
    int action = 0;
    int dismissed = 0;
    wui::Toast toast("Saved", "Your task is available offline.");
    toast.setIntent(wui::ToastIntent::Success);
    toast.setAction("Undo", [&] { ++action; });
    toast.onDismiss([&] { ++dismissed; });
    toast.setTimeout(std::chrono::milliseconds{100});
    toast.layout({0, 0, 520, 300});
    expect(toast.intent() == wui::ToastIntent::Success && toast.actionLabel() == "Undo",
           "Toast must retain Fluent intent and optional action label");
    toast.setPaused(true);
    toast.advanceTimeout(std::chrono::milliseconds{150});
    expect(dismissed == 0, "Hovered/pressed Toast must pause its timeout");
    toast.setPaused(false);
    toast.advanceTimeout(std::chrono::milliseconds{100});
    expect(dismissed == 1, "Resumed Toast must dismiss after its remaining timeout");

    wui::Toast actionable("Network", "Retry the request");
    actionable.setAction("Retry", [&] { ++action; });
    actionable.layout({0, 0, 520, 300});
    expect(actionable.performAccessibilityAction(wui::AccessibilityActionKind::Invoke, {}) ==
               wui::AccessibilityActionStatus::Succeeded && action == 1,
           "Toast action must be invokable through accessibility without moving focus");
    const auto snapshot = wui::snapshotAccessibilityTree(actionable);
    expect(snapshot.size() == 1 && snapshot.front().properties.role == wui::AccessibilityRole::Alert &&
               snapshot.front().properties.live && snapshot.front().properties.label == "Network" &&
               snapshot.front().properties.description == "Retry the request",
           "Toast must expose a polite live alert with title and body");
}

void testToasterQueuesSafely()
{
    wui::OverlayHost host;
    wui::Toaster toaster(host, wui::ToastPosition::TopEnd);
    auto first = std::make_unique<wui::Toast>("First", "one");
    first->setTimeout(std::chrono::milliseconds{0});
    auto second = std::make_unique<wui::Toast>("Second", "two");
    second->setTimeout(std::chrono::milliseconds{0});
    toaster.show(std::move(first));
    toaster.show(std::move(second));
    host.layout({0, 0, 800, 600});
    expect(host.size() == 1 && toaster.hasActiveToast() && toaster.queuedCount() == 1,
           "Toaster must expose one visible toast and queue subsequent notifications FIFO");
    expect(toaster.activeToast()->bounds().y == 16.0f && toaster.activeToast()->bounds().x > 400.0f,
           "Top-end Toaster must position notifications against the host edge with Fluent margin");
    toaster.dismiss();
    wui::flushStructuralUpdates();
    host.layout({0, 0, 800, 600});
    expect(host.size() == 1 && toaster.activeToast() && toaster.activeToast()->title() == "Second" &&
               toaster.queuedCount() == 0,
           "Dismissal must safely advance the queued toast after overlay removal");
    toaster.clear();
    expect(host.empty() && !toaster.hasActiveToast(), "Toaster clear must remove active and queued notifications");
}

void testSpinnerSizesLabelAndReducedMotion()
{
    wui::Spinner spinner("Loading tasks");
    constexpr std::array sizes{
        wui::SpinnerSize::ExtraTiny, wui::SpinnerSize::Tiny,
        wui::SpinnerSize::ExtraSmall, wui::SpinnerSize::Small,
        wui::SpinnerSize::Medium, wui::SpinnerSize::Large,
        wui::SpinnerSize::ExtraLarge, wui::SpinnerSize::Huge,
    };
    constexpr std::array diameters{16.0f, 20.0f, 24.0f, 28.0f,
                                   32.0f, 36.0f, 40.0f, 44.0f};
    wui::Spinner bareSpinner;
    for (std::size_t index = 0; index < sizes.size(); ++index) {
        bareSpinner.setSize(sizes[index]);
        const auto measured = bareSpinner.measure({0, 100, 0, 100});
        expect(measured.width == diameters[index] &&
                   measured.height == diameters[index],
               "Spinner sizes must use the Fluent 2 indicator diameter scale");
    }
    spinner.setSize(wui::SpinnerSize::Large);
    const auto after = spinner.measure({0, 400, 0, 200});
    spinner.setLabelPosition(wui::SpinnerLabelPosition::Above);
    const auto above = spinner.measure({0, 400, 0, 200});
    expect(above.height > after.height && above.width < after.width,
           "Spinner label positions must reflow between inline and vertical Fluent layouts");
    spinner.setMotionEnabled(false);
    expect(!spinner.isMotionEnabled(), "Spinner must honor reduced-motion opt-out");
    const auto snapshot = wui::snapshotAccessibilityTree(spinner);
    expect(snapshot.size() == 1 && snapshot.front().properties.role == wui::AccessibilityRole::ProgressBar &&
               snapshot.front().properties.busy && snapshot.front().properties.label == "Loading tasks",
           "Spinner must expose a busy indeterminate progress semantic");
}
} // namespace

int main()
{
    try {
        testToastIntentActionAndPause();
        testToasterQueuesSafely();
        testSpinnerSizesLabelAndReducedMotion();
        return 0;
    } catch (const std::exception& error) {
        return (void)error, 1;
    }
}
