#include "wui/overlays.h"
#include "wui/runtime.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void popupPlacementAndDismiss()
{
    bool dismissed = false;
    wui::PopupNode popup;
    popup.anchor({90.0f, 95.0f, 40.0f, 24.0f})
         .preferredSize({140.0f, 80.0f})
         .onDismiss([&] { dismissed = true; });
    popup.layout({0.0f, 0.0f, 180.0f, 160.0f});

    // It cannot fit below the anchor, so a below-placement popup flips above.
    expect(popup.panelBounds().y < popup.anchor().y, "popup should flip above when the lower edge is constrained");
    expect(popup.panelBounds().x + popup.panelBounds().width <= 180.0f, "popup should remain inside host horizontally");

    wui::PointerEvent outside;
    outside.action = wui::PointerAction::Down;
    outside.button = wui::MouseButton::Left;
    outside.position = {4.0f, 4.0f};
    expect(popup.onPointerEvent(outside), "outside popup press should be consumed");
    expect(dismissed, "outside popup press should dismiss by default");
}

void menuNavigationAndInvocation()
{
    int invoked = 0;
    int dismissed = 0;
    wui::MenuNode menu;
    menu.anchor({12.0f, 12.0f, 32.0f, 24.0f});
    menu.addItem({"Disabled", {}, false, [&] { invoked += 100; }})
        .addItem({"Open", "Enter", true, [&] { ++invoked; }})
        .addItem({"Delete", {}, true, [&] { invoked += 10; }})
        .onDismiss([&] { ++dismissed; });
    menu.layout({0.0f, 0.0f, 320.0f, 240.0f});
    const auto menuSize = menu.measure({0.0f, 320.0f, 0.0f, 240.0f});
    expect(menuSize.width >= 138.0f && menuSize.width <= 300.0f &&
               menuSize.height == 108.0f,
           "menu geometry should use 4-DIP surface padding, 32-DIP rows and 2-DIP row gaps");

    expect(menu.selectedIndex() == 1, "menu should select the first enabled item");
    expect(menu.onKeyEvent({0, wui::KeyAction::Down, 40}), "down should navigate a menu");
    expect(menu.selectedIndex() == 2, "down should skip to next enabled item");
    expect(menu.onKeyEvent({0, wui::KeyAction::Down, 38}), "up should navigate a menu");
    expect(menu.selectedIndex() == 1, "up should skip disabled menu items");
    expect(menu.onKeyEvent({0, wui::KeyAction::Down, 13}), "enter should invoke selected menu item");
    expect(invoked == 1 && dismissed == 1, "invoke should call the action then dismiss");
}

void tooltipDelayAndSearchEscape()
{
    wui::TooltipNode tooltip;
    tooltip.text("Copy").delay(std::chrono::milliseconds{500}).showAfter(std::chrono::milliseconds{499});
    expect(!tooltip.isVisible(), "tooltip must not show before its delay");
    tooltip.showAfter(std::chrono::milliseconds{500});
    expect(tooltip.isVisible(), "tooltip should show at its configured delay");
    const auto tooltipSize =
        tooltip.measure({0.0f, 400.0f, 0.0f, 200.0f});
    expect(tooltipSize.width <= 240.0f && tooltipSize.height == 28.0f,
           "tooltip must use the Fluent 240-DIP cap and 6-DIP vertical padding");
    tooltip.appearance(wui::TooltipAppearance::Brand);
    expect(tooltip.appearance() == wui::TooltipAppearance::Brand,
           "tooltip must retain its Fluent appearance variant");
    tooltip.hide();
    expect(!tooltip.isVisible(), "tooltip hide should be immediate");

    int changes = 0;
    wui::SearchFieldNode field;
    field.onQueryChange([&](const std::string&) { ++changes; }).query("today");
    expect(field.query() == "today", "search query should delegate to TextInput");
    expect(field.onKeyEvent({0, wui::KeyAction::Down, 27}), "escape should clear a non-empty search field");
    expect(field.query().empty() && changes == 2, "clearing search should notify query observers");
}

void iconButtonClick()
{
    int clicks = 0;
    wui::IconButtonNode icon("+");
    icon.onClick([&] { ++clicks; });
    icon.layout({10.0f, 10.0f, 32.0f, 32.0f});
    wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down, wui::MouseButton::Left, {20.0f, 20.0f}};
    auto up = down;
    up.action = wui::PointerAction::Up;
    expect(icon.onPointerEvent(down) && icon.onPointerEvent(up), "icon button should handle a primary click");
    expect(clicks == 1, "icon button should invoke click handler exactly once");
    expect(icon.onKeyEvent({0, wui::KeyAction::Down, 13}), "enter should activate an enabled icon button");
    expect(icon.onKeyEvent({0, wui::KeyAction::Down, 32}), "space should activate an enabled icon button");
    expect(clicks == 3, "keyboard activation should invoke the same click handler");
    icon.setEnabled(false);
    expect(!icon.onKeyEvent({0, wui::KeyAction::Down, 13}), "disabled icon buttons must not activate from the keyboard");
    expect(clicks == 3, "disabled icon buttons must not invoke click handlers");
}

// Regression: overlay dismissal must be deferred while an input dispatch has
// captured a raw-pointer path into the overlay's subtree. Directly destroying
// an overlay in dismiss() during event dispatch dangled the InputRouter's
// captured Node* path and crashed deep in the CRT (msvcp140!_Rng_abort). The
// fix moved deferral into OverlayHost itself so every widget inherits it.
void overlayDeferralOutsideDispatchIsImmediate()
{
    wui::OverlayHost host;
    expect(!host.isDeferringDismissals(), "OverlayHost defaults to immediate dismissal");

    const auto id = host.show(std::make_unique<wui::PopupNode>());
    expect(host.size() == 1, "show should add exactly one overlay");
    auto removed = host.dismiss(id);
    expect(removed != nullptr, "dismiss outside a deferral scope returns the removed overlay");
    expect(host.size() == 0, "the overlay list should be empty after direct dismiss");
}

void overlayDeferralQueuesInsideScope()
{
    wui::OverlayHost host;
    const auto id = host.show(std::make_unique<wui::PopupNode>());

    host.beginDeferredDismissals();
    expect(host.isDeferringDismissals(), "begin should mark the host as deferring");
    auto removed = host.dismiss(id);
    expect(removed == nullptr, "dismiss during deferral must return nullptr");
    expect(host.size() == 1, "deferral must keep the overlay in the list until scope unwinds");

    host.endDeferredDismissals();
    expect(!host.isDeferringDismissals(), "end should clear the deferring flag at depth zero");
    expect(host.size() == 0, "endDeferredDismissals must drain queued ids");
}

void overlayDeferralCoalescesRepeatedDismiss()
{
    wui::OverlayHost host;
    const auto id = host.show(std::make_unique<wui::PopupNode>());

    host.beginDeferredDismissals();
    (void)host.dismiss(id);
    (void)host.dismiss(id);      // repeated request must be a no-op
    (void)host.dismissTop();     // pointing at the same overlay too
    expect(host.size() == 1, "duplicate dismiss requests must not double-enqueue");
    host.endDeferredDismissals();
    expect(host.size() == 0, "the overlay is dismissed exactly once at scope exit");
}

void overlayDeferralNestedScopesDrainAtOutermost()
{
    wui::OverlayHost host;
    const auto id = host.show(std::make_unique<wui::PopupNode>());

    host.beginDeferredDismissals();      // outer
    host.beginDeferredDismissals();      // inner
    (void)host.dismiss(id);
    host.endDeferredDismissals();        // inner: MUST NOT drain
    expect(host.isDeferringDismissals(), "inner end must leave the outer scope active");
    expect(host.size() == 1, "nested inner end must not destroy the overlay");
    host.endDeferredDismissals();        // outer: drains
    expect(host.size() == 0, "outer end drains the deferred queue");
}

void overlayDeferralDismissTopEnqueuesTopId()
{
    wui::OverlayHost host;
    const auto first = host.show(std::make_unique<wui::PopupNode>());
    const auto second = host.show(std::make_unique<wui::PopupNode>());
    (void)first;
    (void)second;

    host.beginDeferredDismissals();
    auto removed = host.dismissTop();
    expect(removed == nullptr, "dismissTop during deferral must not destroy the top overlay");
    expect(host.size() == 2, "both overlays remain until the scope drains");
    host.endDeferredDismissals();
    expect(host.size() == 1, "only the previously-top overlay is dismissed on unwind");
}

} // namespace

int main()
{
    try {
        popupPlacementAndDismiss();
        menuNavigationAndInvocation();
        tooltipDelayAndSearchEscape();
        iconButtonClick();
        overlayDeferralOutsideDispatchIsImmediate();
        overlayDeferralQueuesInsideScope();
        overlayDeferralCoalescesRepeatedDismiss();
        overlayDeferralNestedScopesDrainAtOutermost();
        overlayDeferralDismissTopEnqueuesTopId();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
