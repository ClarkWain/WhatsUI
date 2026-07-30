#include "wui/wui.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

class Probe final : public wui::Node {
public:
    explicit Probe(wui::SizeF size) : size_(size) {}
    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override { return constraints.clamp(size_); }
    void paint(wui::PaintContext&) override {}
private:
    wui::SizeF size_;
};

class CountingTextMeasurer final : public wui::TextMeasurer {
public:
    [[nodiscard]] wui::TextExtents measureText(
        const std::string& text, float fontSize) const override
    {
        ++calls;
        return {static_cast<float>(text.size()) * fontSize * 0.5f,
                fontSize * 1.25f, fontSize * 0.8f, fontSize * 0.2f};
    }

    mutable std::size_t calls{0};
};

wui::PointerEvent scroll(float x, float y, float deltaX, float deltaY)
{
    wui::PointerEvent event;
    event.action = wui::PointerAction::Scroll;
    event.position = {x, y};
    event.scrollDelta = {deltaX, deltaY};
    return event;
}

wui::PointerEvent scroll(float y, float delta)
{
    return scroll(10.0f, y, 0.0f, delta);
}

void testViewportLayoutAndClamping()
{
    wui::ScrollView view;
    view.child(std::make_unique<Probe>(wui::SizeF{80.0f, 300.0f}));
    view.layout({0.0f, 0.0f, 100.0f, 100.0f});
    expect(view.contentSize().height == 300.0f, "ScrollView should retain unconstrained content height");
    expect(view.maxScrollOffset() == 200.0f, "ScrollView should expose document overflow");
    view.setScrollOffset(999.0f);
    expect(view.scrollOffset() == 200.0f, "ScrollView should clamp offset at document end");
    view.setScrollOffset(-1.0f);
    expect(view.scrollOffset() == 0.0f, "ScrollView should clamp offset at document start");
}

void testWheelBubblesFromContent()
{
    auto view = std::make_unique<wui::ScrollView>();
    view->child(std::make_unique<Probe>(wui::SizeF{100.0f, 300.0f}));
    view->layout({0.0f, 0.0f, 100.0f, 100.0f});
    wui::ScrollView* raw = view.get();
    wui::InputRouter router;
    router.setRoot(view.get());
    expect(router.dispatchPointer(scroll(10.0f, -60.0f)), "Wheel event should bubble from content to viewport");
    expect(raw->scrollOffset() == 60.0f, "Negative wheel delta should scroll content down");
    expect(router.dispatchPointer(scroll(10.0f, 1000.0f)), "Wheel should return the viewport to its start");
    expect(raw->scrollOffset() == 0.0f, "Positive wheel delta should clamp at start");
    expect(!router.dispatchPointer(scroll(10.0f, 10.0f)), "Scroll at start should remain unhandled for an ancestor");
}

void testHitTestingUsesDocumentCoordinates()
{
    wui::ScrollView view;
    auto content = std::make_unique<wui::Container>();
    auto child = std::make_unique<Probe>(wui::SizeF{100.0f, 300.0f});
    wui::Node* rawChild = child.get();
    content->child(std::move(child));
    view.child(std::move(content));
    view.layout({0.0f, 0.0f, 100.0f, 100.0f});
    view.setScrollOffset(80.0f);
    expect(view.hitTest({10.0f, 10.0f}) == rawChild, "Viewport hit testing should translate to document coordinates");
    expect(view.hitTest({10.0f, 110.0f}) == nullptr, "Viewport should reject points outside its clip bounds");
}

void testHorizontalViewportLayoutWheelAndHitTesting()
{
    auto view = std::make_unique<wui::ScrollView>();
    view->setAxis(wui::ScrollAxis::Horizontal);
    auto content = std::make_unique<wui::Container>();
    auto child = std::make_unique<Probe>(wui::SizeF{300.0f, 80.0f});
    wui::Node* rawChild = child.get();
    content->child(std::move(child));
    view->child(std::move(content));
    view->layout({0.0f, 0.0f, 100.0f, 100.0f});

    expect(view->contentSize().width == 300.0f, "Horizontal ScrollView should retain unconstrained content width");
    expect(view->maxScrollOffsetX() == 200.0f && view->maxScrollOffsetY() == 0.0f,
           "Horizontal ScrollView should only overflow on its enabled axis");
    view->setScrollOffset({80.0f, 20.0f});
    expect(view->scrollOffsetX() == 80.0f && view->scrollOffsetY() == 0.0f,
           "Horizontal ScrollView should clamp disabled vertical offset to zero");
    expect(view->hitTest({10.0f, 10.0f}) == rawChild,
           "Horizontal viewport hit testing should translate to document coordinates");
    expect(view->hitTest({110.0f, 10.0f}) == nullptr,
           "Horizontal viewport should clip hit testing outside its bounds");
    wui::PaintContext paint;
    view->paint(paint);
    expect(paint.saveCount() == 1, "Viewport painting should restore its clip/translation checkpoint");

    wui::InputRouter router;
    router.setRoot(view.get());
    expect(router.dispatchPointer(scroll(10.0f, 10.0f, -60.0f, 0.0f)),
           "Horizontal wheel delta should be handled by a horizontal viewport");
    expect(view->scrollOffsetX() == 140.0f, "Negative horizontal wheel delta should move toward document end");
    expect(!router.dispatchPointer(scroll(10.0f, 10.0f, 0.0f, -60.0f)),
           "A disabled axis must leave its wheel delta available to an ancestor");
}

void testLongTextPaintsOnlyViewportLines()
{
    std::string document;
    document.reserve(120000);
    for (std::size_t index = 0; index < 10000; ++index) {
        if (index != 0) document.push_back('\n');
        document += "line " + std::to_string(index + 1);
    }

    CountingTextMeasurer measurer;
    wui::setTextMeasurer(&measurer);
    auto text = std::make_unique<wui::Text>(std::move(document));
    text->setLineHeight(20.0f);
    text->setFillAvailableWidth(true);

    wui::ScrollView view;
    view.child(std::move(text));
    view.layout({0.0f, 0.0f, 320.0f, 100.0f});
    expect(measurer.calls == 0,
           "Fill-width long Text layout must not shape every line to resolve intrinsic width");
    expect(view.contentSize().height == 200000.0f,
           "Long Text must retain the complete document height");

    wui::PaintContext paint;
    view.paint(paint);
    expect(paint.paintStats().textDrawCalls <= 7,
           "Long Text must submit only visible lines plus bounded glyph overscan");

    view.setScrollOffset(100000.0f);
    paint.resetPaintStats();
    view.paint(paint);
    expect(paint.paintStats().textDrawCalls <= 8,
           "Scrolled long Text must keep draw work independent of total line count");
    expect(!paint.currentClipBounds().has_value() && paint.saveCount() == 1,
           "ScrollView must restore logical culling state with renderer paint state");
    wui::setTextMeasurer(nullptr);
}

void testNestedViewportHandsOffOnlyRemainingWheelDelta()
{
    auto outer = std::make_unique<wui::ScrollView>();
    auto column = std::make_unique<wui::Column>();

    auto innerHost = std::make_unique<wui::Container>();
    innerHost->setHeight(100.0f);
    auto inner = std::make_unique<wui::ScrollView>();
    inner->child(std::make_unique<Probe>(wui::SizeF{100.0f, 200.0f}));
    wui::ScrollView* rawInner = inner.get();
    innerHost->child(std::move(inner));
    column->child(std::move(innerHost));
    column->child(std::make_unique<Probe>(wui::SizeF{100.0f, 300.0f}));
    outer->child(std::move(column));
    wui::ScrollView* rawOuter = outer.get();
    outer->layout({0.0f, 0.0f, 100.0f, 100.0f});

    expect(rawInner->maxScrollOffsetY() == 100.0f && rawOuter->maxScrollOffsetY() == 300.0f,
           "Nested viewports should receive independent constrained layouts");
    wui::InputRouter router;
    router.setRoot(outer.get());
    expect(router.dispatchPointer(scroll(10.0f, -150.0f)), "Nested wheel input should be handled");
    expect(rawInner->scrollOffsetY() == 100.0f,
           "Inner viewport should consume the part of a large wheel delta it can apply");
    expect(rawOuter->scrollOffsetY() == 50.0f,
           "Outer viewport should receive only the inner viewport's remaining wheel delta");

    expect(router.dispatchPointer(scroll(10.0f, 120.0f)), "Reverse nested wheel input should be handled");
    expect(rawInner->scrollOffsetY() == 0.0f,
           "Inner viewport should consume reverse delta until its document start");
    expect(rawOuter->scrollOffsetY() == 30.0f,
           "Outer viewport should receive exactly the reverse delta left after inner clamping");
}

void testNestedListViewHandsOffOnlyRemainingWheelDelta()
{
    auto outer = std::make_unique<wui::ScrollView>();
    auto column = std::make_unique<wui::Column>();

    auto listHost = std::make_unique<wui::Container>();
    listHost->setHeight(100.0f);
    std::vector<wui::ListView::Item> items;
    for (int index = 0; index < 10; ++index) {
        items.push_back({"Row " + std::to_string(index)});
    }
    auto list = std::make_unique<wui::ListView>(std::move(items));
    wui::ListView* rawList = list.get();
    listHost->child(std::move(list));
    column->child(std::move(listHost));
    column->child(std::make_unique<Probe>(wui::SizeF{100.0f, 300.0f}));
    outer->child(std::move(column));
    wui::ScrollView* rawOuter = outer.get();
    outer->layout({0.0f, 0.0f, 100.0f, 100.0f});

    const float listMaximum = rawList->maximumScrollOffset();
    expect(listMaximum > 0.0f && rawOuter->maxScrollOffsetY() == 300.0f,
           "Nested ListView regression requires independently scrollable viewports");
    wui::InputRouter router;
    router.setRoot(outer.get());
    expect(router.dispatchPointer(scroll(10.0f, -(listMaximum + 50.0f))),
           "Nested ListView wheel input should be handled");
    expect(rawList->scrollOffset() == listMaximum,
           "ListView should consume wheel input until its own document end");
    expect(rawOuter->scrollOffsetY() == 50.0f,
           "Outer viewport should receive only ListView's remaining wheel delta");

    expect(router.dispatchPointer(scroll(10.0f, listMaximum + 20.0f)),
           "Reverse nested ListView wheel input should be handled");
    expect(rawList->scrollOffset() == 0.0f,
           "ListView should consume reverse wheel input until its document start");
    expect(rawOuter->scrollOffsetY() == 30.0f,
           "Outer viewport should receive exactly the reverse delta left by ListView");
}

} // namespace

int main()
{
    testViewportLayoutAndClamping();
    testWheelBubblesFromContent();
    testHitTestingUsesDocumentCoordinates();
    testHorizontalViewportLayoutWheelAndHitTesting();
    testLongTextPaintsOnlyViewportLines();
    testNestedViewportHandsOffOnlyRemainingWheelDelta();
    testNestedListViewHandsOffOnlyRemainingWheelDelta();
    std::cout << "ScrollView tests passed\n";
}
