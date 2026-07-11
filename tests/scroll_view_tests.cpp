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

wui::PointerEvent scroll(float y, float delta)
{
    wui::PointerEvent event;
    event.action = wui::PointerAction::Scroll;
    event.position = {10.0f, y};
    event.scrollDelta = {0.0f, delta};
    return event;
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

} // namespace

int main()
{
    testViewportLayoutAndClamping();
    testWheelBubblesFromContent();
    testHitTestingUsesDocumentCoordinates();
    std::cout << "ScrollView tests passed\n";
}
