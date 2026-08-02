#include <limits>
#include <stdexcept>
#include <string>

#include "wui/internal/viewport_model.h"

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

void testEmptyAndZeroViewport()
{
    wui::internal::ViewportModel model;
    model.setItemExtent(36.0f);
    model.setViewportExtent(180.0f);
    expect(model.visibleRange().empty(), "Empty model should have no visible range");
    model.setItemCount(10);
    model.setViewportExtent(0.0f);
    expect(model.visibleRange().empty(), "Zero viewport should have no visible range");
    expect(model.maxScrollOffset() == 360.0f, "Zero viewport should expose full content as scrollable");
    model.scrollToIndex(0);
    expect(model.scrollOffset() == 0.0f, "Zero viewport scroll-to-index should not move the offset");
}

void testVisibleAndOverscanRanges()
{
    wui::internal::ViewportModel model;
    model.setItemCount(100);
    model.setItemExtent(36.0f);
    model.setViewportExtent(180.0f);
    model.setOverscanItems(2);
    auto visible = model.visibleRange();
    auto overscan = model.overscanRange();
    expect(visible.first == 0 && visible.last == 5, "Initial visible range should cover five rows");
    expect(overscan.first == 0 && overscan.last == 7, "Initial overscan should extend only after the viewport");

    model.setScrollOffset(36.0f * 50.0f);
    visible = model.visibleRange();
    overscan = model.overscanRange();
    expect(visible.first == 50 && visible.last == 55, "Scrolled visible range should start at row 50");
    expect(overscan.first == 48 && overscan.last == 57, "Middle overscan should extend both sides");

    model.setScrollOffset(999999.0f);
    visible = model.visibleRange();
    overscan = model.overscanRange();
    expect(model.scrollOffset() == model.maxScrollOffset(), "Oversized scroll should clamp to max offset");
    expect(visible.first == 95 && visible.last == 100, "End visible range should include final rows");
    expect(overscan.first == 93 && overscan.last == 100, "End overscan should clamp to item count");
}

void testPartialRowsAndScrollToIndex()
{
    wui::internal::ViewportModel model;
    model.setItemCount(20);
    model.setItemExtent(10.0f);
    model.setViewportExtent(25.0f);
    auto visible = model.visibleRange();
    expect(visible.first == 0 && visible.last == 3, "Partial viewport should ceil the last visible row");

    model.setScrollOffset(5.0f);
    visible = model.visibleRange();
    expect(visible.first == 0 && visible.last == 3, "Partial top row should stay visible");

    model.scrollToIndex(10);
    expect(model.scrollOffset() == 85.0f, "Scroll-to-index should reveal the target bottom edge");
    visible = model.visibleRange();
    expect(visible.first == 8 && visible.last == 11, "Scrolled target should be visible");

    model.scrollToIndex(8);
    expect(model.scrollOffset() == 80.0f, "Scroll-to-index should align above-viewport rows to top");
}

void testInvalidInputsAreDeterministic()
{
    wui::internal::ViewportModel model;
    model.setItemCount(3);
    model.setItemExtent(-4.0f);
    model.setViewportExtent(-8.0f);
    model.setScrollOffset(20.0f);
    expect(model.itemExtent() == 1.0f, "Invalid item extent should normalize to one DIP");
    expect(model.viewportExtent() == 0.0f, "Invalid viewport extent should normalize to zero");
    expect(model.scrollOffset() == model.maxScrollOffset(), "Scroll should clamp after invalid viewport normalization");
    model.setScrollOffset(std::numeric_limits<float>::quiet_NaN());
    expect(model.scrollOffset() == 0.0f, "NaN scroll should normalize to zero");
}

} // namespace

int main()
{
    testEmptyAndZeroViewport();
    testVisibleAndOverscanRanges();
    testPartialRowsAndScrollToIndex();
    testInvalidInputsAreDeterministic();
    return 0;
}
