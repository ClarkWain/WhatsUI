#include <stdexcept>
#include <string>

#include "wui/list_view.h"

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

wui::PointerEvent pointer(wui::PointerAction action, float y)
{
    return {0, wui::PointerType::Mouse, action, wui::MouseButton::Left, {16.0f, y}};
}

wui::ListView makeList()
{
    return wui::ListView({{"Inbox"}, {"Archived", false}, {"Later"}, {"Done"}}, -1);
}

void testKeyboardSkipsDisabledRows()
{
    auto list = makeList();
    list.layout({0, 0, 180, 144});
    expect(list.onKeyEvent({0, wui::KeyAction::Down, 40}), "Down should be handled by a list");
    expect(list.selectedIndex() == 0, "First Down should select first enabled row");
    list.onKeyEvent({0, wui::KeyAction::Down, 40});
    expect(list.selectedIndex() == 2, "Down should skip disabled rows");
    list.onKeyEvent({0, wui::KeyAction::Down, 38});
    expect(list.selectedIndex() == 0, "Up should skip disabled rows");
    list.onKeyEvent({0, wui::KeyAction::Down, 35});
    expect(list.selectedIndex() == 3, "End should select last enabled row");
    list.onKeyEvent({0, wui::KeyAction::Down, 36});
    expect(list.selectedIndex() == 0, "Home should select first enabled row");
}

void testPointerSelectsEnabledRowsOnly()
{
    auto list = makeList();
    list.layout({0, 0, 180, 72});
    int selected = -1;
    list.onSelectionChanged([&](int index) { selected = index; });
    list.onPointerEvent(pointer(wui::PointerAction::Down, 18.0f));
    list.onPointerEvent(pointer(wui::PointerAction::Up, 18.0f));
    expect(list.selectedIndex() == 0 && selected == 0, "Pointer click should select enabled row");
    list.onPointerEvent(pointer(wui::PointerAction::Down, 54.0f));
    list.onPointerEvent(pointer(wui::PointerAction::Up, 54.0f));
    expect(list.selectedIndex() == 0 && selected == 0, "Disabled row should not replace selection or notify");
}

void testBindingNormalizesAndTracksExternalSelection()
{
    wui::State<int> selection{1};
    auto list = makeList();
    list.bind(selection);
    expect(selection.get() == -1 && list.selectedIndex() == -1,
           "Binding should normalize an initially disabled selection");
    selection.set(2);
    expect(list.selectedIndex() == 2, "List should track an enabled external selection");
    selection.set(1);
    expect(list.selectedIndex() == -1, "Disabled external selection should not be rendered as active");
}

void testMeasurementTracksContentAndConstraints()
{
    auto list = makeList();
    const auto natural = list.measure({});
    expect(natural.height == 144.0f && natural.width >= 160.0f,
           "List measurement should include every row and a usable minimum width");
    const auto constrained = list.measure({0.0f, 120.0f, 0.0f, 60.0f});
    expect(constrained.width == 120.0f && constrained.height == 60.0f,
           "List measurement should respect viewport constraints for clipped content");
}

} // namespace

int main()
{
    testKeyboardSkipsDisabledRows();
    testPointerSelectsEnabledRowsOnly();
    testBindingNormalizesAndTracksExternalSelection();
    testMeasurementTracksContentAndConstraints();
    return 0;
}
