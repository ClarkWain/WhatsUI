#include <stdexcept>
#include <string>
#include <vector>

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

    void testLargeListExposesBoundedVisibleRange()
    {
        std::vector<wui::ListView::Item> items;
        items.reserve(100000);
        for (int index = 0; index < 100000; ++index) items.push_back({"Row " + std::to_string(index)});
        wui::ListView list(std::move(items));
        list.layout({0, 0, 240, 182});
        auto visible = list.visibleRange();
        expect(visible.first == 0 && visible.size() <= 6,
            "ListView should expose only the bounded visible viewport range");
        list.setScrollOffset(36.0f * 50000.0f);
        visible = list.visibleRange();
        expect(visible.first == 50000 && visible.size() <= 6,
            "Large ListView scroll should keep visible work bounded");
        expect(list.maximumScrollOffset() > 0.0f, "Large ListView should expose a scrollable range");
    }

    void testProviderBackedListRequestsOnlyVisibleRowsForPaint()
    {
        wui::ListView list;
        int requests = 0;
        list.setItemProvider(100000, [&requests](std::size_t index) {
            ++requests;
            return wui::ListView::Item{"Row " + std::to_string(index)};
        });
        list.layout({0, 0, 240, 182});
        wui::PaintContext context;
        list.paint(context);
        expect(requests <= 6, "Provider-backed ListView paint should request only visible rows");
        requests = 0;
        list.setScrollOffset(36.0f * 50000.0f);
        list.paint(context);
        expect(list.visibleRange().first == 50000 && requests <= 6,
            "Provider-backed ListView large scroll should keep item requests bounded");
    }

    void testProviderBackedListKeyboardUsesSelectableMetadata()
    {
        wui::ListView list;
        int itemRequests = 0;
        int selectableRequests = 0;
        list.setItemProvider(100000,
            [&itemRequests](std::size_t index) {
                ++itemRequests;
                return wui::ListView::Item{"Row " + std::to_string(index)};
            },
            [&selectableRequests](std::size_t index) {
                ++selectableRequests;
                return index == 99999;
            });
        list.layout({0, 0, 240, 182});
        expect(list.onKeyEvent({0, wui::KeyAction::Down, 35}) && list.selectedIndex() == 99999,
            "Provider-backed ListView End should use selectable metadata for navigation");
        expect(itemRequests == 0 && selectableRequests > 0,
            "Provider-backed ListView keyboard navigation must not request row payloads");
    }

    void testProviderBackedListFallsBackToItemEnabled()
    {
        wui::ListView list;
        list.setItemProvider(2, [](std::size_t index) {
            return wui::ListView::Item{
                index == 0 ? "Disabled" : "Enabled",
                index != 0};
        });
        list.layout({0, 0, 180, 72});

        list.onPointerEvent(pointer(wui::PointerAction::Down, 18.0f));
        list.onPointerEvent(pointer(wui::PointerAction::Up, 18.0f));
        expect(list.selectedIndex() == -1,
            "Provider Item.enabled=false must prevent pointer selection when no metadata provider exists");

        expect(list.onKeyEvent({0, wui::KeyAction::Down, 40}) && list.selectedIndex() == 1,
            "Provider keyboard navigation must skip Item.enabled=false rows");
    }

    void testSelectableProviderExceptionsFailClosed()
    {
        wui::ListView list;
        list.setItemProvider(
            1,
            [](std::size_t) { return wui::ListView::Item{"Row"}; },
            [](std::size_t) -> bool { throw std::runtime_error("metadata unavailable"); });
        list.setSelectedIndex(0);
        expect(list.selectedIndex() == -1,
            "A throwing selectable provider must leave the row unselected instead of terminating");
    }

    void testScrollOffsetAffectsPointerSelection()
    {
        auto list = makeList();
        list.layout({0, 0, 180, 72});
        list.setScrollOffset(72.0f);
        int selected = -1;
        list.onSelectionChanged([&](int index) { selected = index; });
        list.onPointerEvent(pointer(wui::PointerAction::Down, 18.0f));
        list.onPointerEvent(pointer(wui::PointerAction::Up, 18.0f));
        expect(list.selectedIndex() == 2 && selected == 2,
            "Pointer selection should account for ListView scroll offset");
    }

} // namespace

int main()
{
    testKeyboardSkipsDisabledRows();
    testPointerSelectsEnabledRowsOnly();
    testBindingNormalizesAndTracksExternalSelection();
    testMeasurementTracksContentAndConstraints();
    testLargeListExposesBoundedVisibleRange();
    testProviderBackedListRequestsOnlyVisibleRowsForPaint();
    testProviderBackedListKeyboardUsesSelectableMetadata();
    testProviderBackedListFallsBackToItemEnabled();
    testSelectableProviderExceptionsFailClosed();
    testScrollOffsetAffectsPointerSelection();
    return 0;
}
