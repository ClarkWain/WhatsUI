#include "wui/wui.h"

#include <cstdlib>
#include <iostream>
#include <memory>

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

} // namespace

int main()
{
    testViewportLayoutAndClamping();
    testWheelBubblesFromContent();
    testHitTestingUsesDocumentCoordinates();
    testHorizontalViewportLayoutWheelAndHitTesting();
    testNestedViewportHandsOffOnlyRemainingWheelDelta();
    std::cout << "ScrollView tests passed\n";
}
