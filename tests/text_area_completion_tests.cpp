#include "wui/wui.h"

#include <cmath>
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

bool near(float lhs, float rhs, float tolerance = 0.01f)
{
    return std::fabs(lhs - rhs) <= tolerance;
}

wui::PointerEvent pointer(wui::PointerAction action, float x, float y,
                          float deltaY = 0.0f)
{
    wui::PointerEvent event;
    event.action = action;
    event.button = action == wui::PointerAction::Down || action == wui::PointerAction::Up
        ? wui::MouseButton::Left : wui::MouseButton::None;
    event.position = {x, y};
    event.scrollDelta = {0.0f, deltaY};
    return event;
}

class WheelAncestor final : public wui::ContainerNode {
public:
    explicit WheelAncestor(std::unique_ptr<wui::TextArea> area)
    {
        appendChild(std::move(area));
    }

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({200.0f, 64.0f});
    }

    void layout(const wui::RectF& bounds) override
    {
        wui::Node::layout(bounds);
        children().front()->layout(bounds);
    }

    void paint(wui::PaintContext& context) override
    {
        children().front()->paint(context);
    }

    wui::EventResult onPointerEvent(const wui::PointerEvent& event,
                                    wui::EventContext& context) override
    {
        if (context.phase() == wui::EventPhase::Bubble
            && event.action == wui::PointerAction::Scroll) {
            lastRemainingDelta = event.scrollDelta.y;
            ++bubbleCount;
        }
        return wui::EventResult::Ignored;
    }

    float lastRemainingDelta{0.0f};
    int bubbleCount{0};
};

std::unique_ptr<wui::TextArea> longArea()
{
    auto area = std::make_unique<wui::TextArea>("Notes");
    area->text("00\n11\n22\n33\n44\n55\n66\n77\n88\n99");
    return area;
}

void testWheelConsumesOnlyAvailableDistanceAndCaretFollowsBothWays()
{
    auto area = longArea();
    wui::TextArea* rawArea = area.get();
    WheelAncestor root(std::move(area));
    root.layout({0.0f, 0.0f, 200.0f, 64.0f});

    wui::InputRouter router;
    router.setRoot(&root);
    const float maximum = rawArea->maximumVerticalScrollOffset();
    expect(maximum > 60.0f, "A constrained long TextArea must expose internal overflow");

    wui::PaintContext paint;
    root.paint(paint);

    expect(router.dispatchPointer(pointer(wui::PointerAction::Scroll, 20.0f, 20.0f,
                                          -(maximum + 30.0f))),
           "A TextArea must handle the wheel distance it can consume");
    expect(near(rawArea->verticalScrollOffset(), maximum),
           "A large wheel delta must clamp the TextArea at its document end");
    expect(root.bubbleCount == 1 && near(root.lastRemainingDelta, -30.0f),
           "Only wheel delta beyond the TextArea edge may bubble to an ancestor");
    root.paint(paint);
    expect(near(rawArea->verticalScrollOffset(), maximum),
           "Painting after a manual wheel gesture must not snap back to an off-screen caret");

    expect(router.dispatchPointer(pointer(wui::PointerAction::Down, 13.0f, 5.0f)),
           "Pointer down must focus the scrolled TextArea");
    expect(router.dispatchPointer(pointer(wui::PointerAction::Up, 13.0f, 5.0f)),
           "Pointer up must finish a TextArea selection gesture");
    expect(rawArea->controller().selection().end >= 18,
           "Pointer hit testing after a wheel scroll must resolve into a visible late line");

    rawArea->controller().moveToStart();
    rawArea->paint(paint);
    expect(near(rawArea->verticalScrollOffset(), 0.0f),
           "Moving the caret to the first line must reveal the document start");
    rawArea->controller().moveToEnd();
    rawArea->paint(paint);
    expect(near(rawArea->verticalScrollOffset(), maximum),
           "Moving the caret to the last line must reveal the document end");

    root.lastRemainingDelta = 0.0f;
    expect(router.dispatchPointer(pointer(wui::PointerAction::Scroll, 20.0f, 20.0f,
                                          maximum + 12.0f)),
           "Reverse wheel input must scroll the TextArea toward its start");
    expect(near(rawArea->verticalScrollOffset(), 0.0f)
           && near(root.lastRemainingDelta, 12.0f),
           "Reverse edge handoff must preserve only the unconsumed distance");
}

void testPointerSelectionAndCompositionAcrossVisualLines()
{
    auto area = std::make_unique<wui::TextArea>("Notes");
    area->text("first line\nsecond line\nthird line\nfourth line");
    wui::TextArea* rawArea = area.get();
    WheelAncestor root(std::move(area));
    root.layout({0.0f, 0.0f, 128.0f, 64.0f});
    wui::FocusManager focus;
    wui::InputRouter router(&focus);
    router.setRoot(&root);

    expect(router.dispatchPointer(pointer(wui::PointerAction::Down, 13.0f, 5.0f)),
           "TextArea pointer selection must start inside the first line");
    expect(router.dispatchPointer(pointer(wui::PointerAction::Move, 62.0f, 45.0f)),
           "Captured pointer movement must extend selection across lines");
    expect(router.dispatchPointer(pointer(wui::PointerAction::Up, 62.0f, 45.0f)),
           "TextArea pointer selection must release cleanly");
    const auto selected = rawArea->controller().selection();
    expect(selected.start < 3 && selected.end > 20,
           "A drag across visual lines must create one continuous source selection");

    expect(rawArea->onCompositionInput({0, "IME text wrapping over multiple visual lines",
                                        wui::CompositionInputEvent::Phase::Start}),
           "IME start must replace a multi-line TextArea selection");
    const auto composition = rawArea->controller().composition();
    expect(!composition.empty() && composition.start == selected.start
           && composition.end > composition.start,
           "TextArea must retain the exact active multi-line composition span");
    expect(rawArea->controller().selection().start == composition.start
           && rawArea->controller().selection().end == composition.end,
           "Active composition must remain queryable through editing selection state");
    expect(rawArea->onCompositionInput({0, "", wui::CompositionInputEvent::Phase::End}),
           "IME end must close a TextArea pre-edit session");
    expect(rawArea->controller().composition().empty()
           && rawArea->controller().selection().empty(),
           "Ending TextArea composition must clear its underline range and collapse the caret");
}

void testDisabledTextAreaRejectsAllEditingRoutes()
{
    wui::TextArea area("Disabled notes");
    area.text("unchanged");
    area.layout({0.0f, 0.0f, 180.0f, 64.0f});
    area.setEnabled(false);

    expect(!area.onTextInput({0, "x"}), "Disabled TextArea must reject committed text");
    expect(!area.onKeyEvent({0, wui::KeyAction::Down, 259}),
           "Disabled TextArea must reject editing keys");
    expect(!area.onCompositionInput({0, "x", wui::CompositionInputEvent::Phase::Start}),
           "Disabled TextArea must reject native IME updates");
    expect(area.controller().text() == "unchanged",
           "Disabled editing routes must not mutate TextArea content");
}

} // namespace

int main()
{
    testWheelConsumesOnlyAvailableDistanceAndCaretFollowsBothWays();
    testPointerSelectionAndCompositionAcrossVisualLines();
    testDisabledTextAreaRejectsAllEditingRoutes();
    std::cout << "WhatsUI TextArea completion tests passed\n";
}
