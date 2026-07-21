#include <memory>
#include <iostream>
#include <stdexcept>

#include "wui/accessibility.h"
#include "wui/events.h"
#include "wui/popover.h"
#include "wui/runtime.h"

namespace {
void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

wui::PointerEvent click(float x, float y)
{
    return {0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {x, y}};
}

void popoverPlacesFlipsAndDismisses()
{
    bool dismissed = false;
    wui::Popover popover("Project settings", "Choose how members receive notifications.");
    popover.anchor({280, 184, 36, 32}).placement(wui::PopupPlacement::BelowEnd).onDismiss([&] { dismissed = true; });
    popover.layout({0, 0, 360, 240});
    expect(popover.panelBounds().y < popover.anchor().y, "Popover must flip above when below is constrained");
    expect(popover.panelBounds().x >= 0 && popover.panelBounds().x + popover.panelBounds().width <= 360,
           "Popover must clamp horizontally inside its host");
    expect(!popover.hasArrow(), "Fluent Popover arrow must be opt-in by default");
    popover.showArrow();
    expect(popover.hasArrow(), "Explicit Fluent Popover arrow must be retained");
    const auto snapshot = wui::snapshotAccessibilityTree(popover);
    expect(snapshot.size() == 1 && snapshot.front().properties.role == wui::AccessibilityRole::Group &&
               snapshot.front().properties.label == "Project settings" && !snapshot.front().properties.live,
           "Passive Popover must expose a non-live labelled Group, not an Alert");
    expect(popover.onKeyEvent({0, wui::KeyAction::Down, 27}) && dismissed,
           "Escape must dismiss a Popover");
}

void triggerOwnsFreshOverlayAndRestoresState()
{
    wui::OverlayHost host;
    wui::FocusManager focus;
    host.bindFocusManager(focus);
    wui::PopoverButton trigger("More details");
    trigger.layout({24, 24, 112, 32});
    trigger.bindOverlayHost(host).popover("Project settings", "Members can update this any time.");
    focus.setFocused(&trigger);
    trigger.openPopover();
    host.layout({0, 0, 720, 480});
    expect(trigger.isOpen() && host.size() == 1,
           "PopoverButton must create an OverlayHost-owned Popover from its factory");
    expect(host.focused() == &trigger,
           "Non-modal Popover must retain focus on its trigger rather than stealing keyboard focus");
    const auto snapshot = wui::snapshotAccessibilityTree(trigger);
    expect(snapshot.size() == 1 && snapshot.front().properties.expanded.has_value() &&
               *snapshot.front().properties.expanded && snapshot.front().properties.actions.expandCollapse,
           "Popover trigger must expose expanded ExpandCollapse semantics");
    auto* popover = dynamic_cast<wui::Popover*>(host.top()->content.get());
    expect(popover != nullptr && popover->onKeyEvent({0, wui::KeyAction::Down, 27}),
           "Overlay Popover must receive Escape dismissal");
    expect(!trigger.isOpen() && host.empty(),
           "Dismissal must collapse trigger and remove transient overlay safely");
    expect(host.focused() == &trigger, "Popover dismissal must preserve trigger focus");
}

void teachingFocusPolicyAndDialogSemantics()
{
    wui::OverlayHost host;
    wui::FocusManager focus;
    host.bindFocusManager(focus);
    wui::PopoverButton trigger("Teach me");
    trigger.layout({64, 40, 100, 32});
    trigger.bindOverlayHost(host).popoverFactory([] {
        auto teaching = std::make_unique<wui::TeachingPopover>("Keyboard help", "Use Ctrl+K to search.");
        teaching->primaryAction("Next").showArrow(true);
        return teaching;
    });
    focus.setFocused(&trigger);
    trigger.openPopover();
    host.layout({0, 0, 640, 420});
    auto* teaching = host.top() ? dynamic_cast<wui::TeachingPopover*>(host.top()->content.get()) : nullptr;
    expect(teaching != nullptr && host.focused() == teaching,
           "Default TeachingPopover must move focus to its guided dialog surface");
    expect(teaching->onKeyEvent({0, wui::KeyAction::Down, 9}),
           "TrapFocus TeachingPopover must retain Tab at its dialog boundary");
    const auto snapshot = wui::snapshotAccessibilityTree(*teaching, teaching);
    expect(snapshot.size() == 1 && snapshot.front().properties.role == wui::AccessibilityRole::Dialog &&
               snapshot.front().properties.label == "Keyboard help" &&
               snapshot.front().properties.description == "Use Ctrl+K to search." && !snapshot.front().properties.live,
           "TeachingPopover must expose a non-live Dialog title and description, not an Alert");
    expect(teaching->onKeyEvent({0, wui::KeyAction::Down, 27}) && host.empty() && host.focused() == &trigger,
           "Escape must dismiss TeachingPopover and restore trigger focus");

    trigger.openPopover();
    host.layout({0, 0, 640, 420});
    teaching = host.top() ? dynamic_cast<wui::TeachingPopover*>(host.top()->content.get()) : nullptr;
    expect(teaching != nullptr && teaching->onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Down,
                                                            wui::MouseButton::Left, {620, 400}}) &&
               host.empty() && host.focused() == &trigger,
           "Outside press must dismiss TeachingPopover and restore trigger focus");

    wui::TeachingPopover nonModal("Tip", "This does not capture focus.");
    nonModal.focusPolicy(wui::TeachingPopoverFocusPolicy::NonModal);
    expect(!nonModal.onKeyEvent({0, wui::KeyAction::Down, 9}),
           "Explicitly non-modal TeachingPopover must leave Tab routing to its page");
}

void teachingPopoverActionsAndSemantics()
{
    int primary = 0;
    int secondary = 0;
    int dismissed = 0;
    wui::TeachingPopover teaching("Try keyboard shortcuts", "Use Ctrl+K to search commands.");
    teaching.primaryAction("Next", [&] { ++primary; })
        .secondaryAction("Back", [&] { ++secondary; })
        .stepText("Step 1 of 3")
        .onDismiss([&] { ++dismissed; });
    expect(teaching.hasArrow(),
           "TeachingPopover must expose the Fluent anchored pointer by default");
    teaching.anchor({12, 12, 32, 28}).placement(wui::PopupPlacement::BelowStart);
    teaching.layout({0, 0, 480, 320});
    const auto panel = teaching.panelBounds();
    expect(teaching.measure({0, 480, 0, 320}).height > 100.0f,
           "TeachingPopover must reserve action footer space");
    expect(teaching.onPointerEvent(click(panel.x + panel.width - 168.0f, panel.y + panel.height - 28.0f)) && primary == 1 && dismissed == 1,
           "Primary action must invoke once and dismiss TeachingPopover");
    teaching.onDismiss([&] { ++dismissed; });
    teaching.onPointerEvent(click(panel.x + panel.width - 48.0f, panel.y + panel.height - 28.0f));
    expect(secondary == 1, "Secondary TeachingPopover action must be independently invokable");
}
} // namespace

int main()
{
    try {
        popoverPlacesFlipsAndDismisses();
        triggerOwnsFreshOverlayAndRestoresState();
        teachingPopoverActionsAndSemantics();
        teachingFocusPolicyAndDialogSemantics();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
