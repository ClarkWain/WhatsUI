#include "wui/overlays.h"

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
    wui::Popup popup;
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
    wui::Menu menu;
    menu.anchor({12.0f, 12.0f, 32.0f, 24.0f});
    menu.addItem({"Disabled", {}, false, [&] { invoked += 100; }})
        .addItem({"Open", "Enter", true, [&] { ++invoked; }})
        .addItem({"Delete", {}, true, [&] { invoked += 10; }})
        .onDismiss([&] { ++dismissed; });
    menu.layout({0.0f, 0.0f, 320.0f, 240.0f});

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
    wui::Tooltip tooltip;
    tooltip.text("Copy").delay(std::chrono::milliseconds{500}).showAfter(std::chrono::milliseconds{499});
    expect(!tooltip.isVisible(), "tooltip must not show before its delay");
    tooltip.showAfter(std::chrono::milliseconds{500});
    expect(tooltip.isVisible(), "tooltip should show at its configured delay");
    tooltip.hide();
    expect(!tooltip.isVisible(), "tooltip hide should be immediate");

    int changes = 0;
    wui::SearchField field;
    field.onQueryChange([&](const std::string&) { ++changes; }).query("today");
    expect(field.query() == "today", "search query should delegate to TextInput");
    expect(field.onKeyEvent({0, wui::KeyAction::Down, 27}), "escape should clear a non-empty search field");
    expect(field.query().empty() && changes == 2, "clearing search should notify query observers");
}

void iconButtonClick()
{
    int clicks = 0;
    wui::IconButton icon("+");
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

} // namespace

int main()
{
    try {
        popupPlacementAndDismiss();
        menuNavigationAndInvocation();
        tooltipDelayAndSearchEscape();
        iconButtonClick();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
