// Tests for Container's optional InteractionArea attachment. The attached
// component is what makes `wui::ui::Box().onClick(...)` a first-class Fluent
// row (nav rail, tiles, clickable cards) without inheriting from ControlNode
// or introducing an extra wrapper widget in the tree.

#include <memory>
#include <stdexcept>
#include <string>

#include "wui/accessibility.h"
#include "wui/events.h"
#include "wui/interaction.h"
#include "wui/theme.h"
#include "wui/widgets.h"

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

wui::PointerEvent pointer(wui::PointerAction action, wui::PointF position,
                          wui::MouseButton button = wui::MouseButton::None)
{
    wui::PointerEvent event;
    event.action = action;
    event.button = button;
    event.position = position;
    return event;
}

wui::KeyEvent key(int keyCode, wui::KeyAction action = wui::KeyAction::Down)
{
    wui::KeyEvent event;
    event.action = action;
    event.keyCode = keyCode;
    return event;
}

std::unique_ptr<wui::Container> makeBox()
{
    auto container = std::make_unique<wui::Container>();
    container->layout({0.0f, 0.0f, 100.0f, 40.0f});
    return container;
}

void testInteractionOnlyAttachesWhenAsked()
{
    wui::Container container;
    expect(container.interaction() == nullptr,
           "A fresh Container must remain a pure layout node until an "
           "interaction setter is called");
    expect(!container.accessibilityActions().invoke,
           "A pure layout Container must not advertise an accessibility invoke");
}

void testClickFiresOnPointerUpInsideBounds()
{
    auto container = makeBox();
    int clicks = 0;
    container->setOnClick([&clicks] { ++clicks; });

    expect(container->onPointerEvent(pointer(wui::PointerAction::Down, {10.0f, 10.0f},
                                             wui::MouseButton::Left)),
           "Pointer down inside an interactive Container must be consumed");
    const auto states = container->interaction()->states;
    expect((states & wui::toMask(wui::ControlVisualState::Pressed)) != 0,
           "Pointer down must set the Pressed visual state");
    expect(container->onPointerEvent(pointer(wui::PointerAction::Up, {12.0f, 12.0f},
                                             wui::MouseButton::Left)),
           "Pointer up inside bounds must be consumed");
    expect(clicks == 1, "Click handler must fire once for a matching down/up pair");
    expect((container->interaction()->states &
            wui::toMask(wui::ControlVisualState::Pressed)) == 0,
           "Pressed state must clear on pointer up");
}

void testClickCancelsWhenReleasedOutsideBounds()
{
    auto container = makeBox();
    int clicks = 0;
    container->setOnClick([&clicks] { ++clicks; });

    container->onPointerEvent(pointer(wui::PointerAction::Down, {10.0f, 10.0f},
                                      wui::MouseButton::Left));
    // Release well outside bounds: the click contract mirrors Fluent Button.
    container->onPointerEvent(pointer(wui::PointerAction::Up, {999.0f, 999.0f},
                                      wui::MouseButton::Left));
    expect(clicks == 0,
           "Releasing outside bounds must not trigger a click");
}

void testHoverAndCallback()
{
    auto container = makeBox();
    bool hoverState = false;
    int transitions = 0;
    container->setOnHoverChange([&](bool value) {
        hoverState = value;
        ++transitions;
    });

    container->onPointerEvent(pointer(wui::PointerAction::Enter, {5.0f, 5.0f}));
    expect(hoverState && transitions == 1,
           "Pointer enter must flip hover on and invoke the callback once");
    container->onPointerEvent(pointer(wui::PointerAction::Leave, {200.0f, 200.0f}));
    expect(!hoverState && transitions == 2,
           "Pointer leave must clear hover and invoke the callback once");
}

void testKeyboardActivation()
{
    auto container = makeBox();
    int clicks = 0;
    container->setOnClick([&clicks] { ++clicks; });

    expect(container->onKeyEvent(key(13)) && clicks == 1,
           "Enter must invoke the interactive Container");
    expect(container->onKeyEvent(key(32)) && clicks == 2,
           "Space must invoke the interactive Container");
    // Any other key must not implicitly activate the surface.
    expect(!container->onKeyEvent(key(65)) && clicks == 2,
           "Unrelated keys must be ignored by the default keyboard contract");
}

void testAccessibilityInvokeExposedOnlyWithClick()
{
    wui::Container container;
    container.setHoverBackground({1, 2, 3, 4});  // no click yet
    expect(!container.accessibilityActions().invoke,
           "Visual-only interaction setters must not expose invoke");

    int clicks = 0;
    container.setOnClick([&clicks] { ++clicks; });
    expect(container.accessibilityActions().invoke,
           "Attaching an onClick must expose an accessibility invoke");
    expect(container.performAccessibilityAction(
               wui::AccessibilityActionKind::Invoke, {}) ==
                   wui::AccessibilityActionStatus::Succeeded &&
               clicks == 1,
           "Accessibility Invoke must reuse the onClick handler");
    expect(container.performAccessibilityAction(
               wui::AccessibilityActionKind::Toggle, {}) ==
               wui::AccessibilityActionStatus::NotSupported,
           "Unrelated a11y actions must be reported as unsupported");
}

void testRawPointerHooksCanConsumeEventsWithoutClick()
{
    wui::Container container;
    container.layout({0.0f, 0.0f, 100.0f, 40.0f});
    int downs = 0;
    container.setOnPointerDown([&](const wui::PointerEvent&) {
        ++downs;
        return true;
    });
    expect(container.onPointerEvent(pointer(wui::PointerAction::Down, {2.0f, 2.0f})),
           "Raw pointer-down hook must consume the event when it returns true");
    expect(downs == 1,
           "Raw pointer-down hook must be invoked once per matching event");
    expect(!container.accessibilityActions().invoke,
           "Attaching only a raw pointer hook must not fabricate an invoke action");
}

} // namespace

int main()
{
    try {
        testInteractionOnlyAttachesWhenAsked();
        testClickFiresOnPointerUpInsideBounds();
        testClickCancelsWhenReleasedOutsideBounds();
        testHoverAndCallback();
        testKeyboardActivation();
        testAccessibilityInvokeExposedOnlyWithClick();
        testRawPointerHooksCanConsumeEventsWithoutClick();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "container_interaction_tests: %s\n", error.what());
        return 1;
    }
}
