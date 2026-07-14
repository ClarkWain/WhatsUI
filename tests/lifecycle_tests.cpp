#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "wui/runtime.h"
#include "wui/ui.h"
#include "wui/widgets.h"

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class LifecycleProbe final : public wui::Node {
public:
    LifecycleProbe(std::string name, std::vector<std::string>& events, int& attaches, int& detaches)
        : name_(std::move(name)), events_(events), attaches_(attaches), detaches_(detaches)
    {
    }

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({1.0f, 1.0f});
    }

    void paint(wui::PaintContext&) override {}

protected:
    void onAttach() noexcept override
    {
        ++attaches_;
        events_.push_back(name_ + ".attach");
    }

    void onDetach() noexcept override
    {
        ++detaches_;
        events_.push_back(name_ + ".detach");
    }

private:
    std::string name_;
    std::vector<std::string>& events_;
    int& attaches_;
    int& detaches_;
};

class PointerProbe final : public wui::Node {
public:
    PointerProbe(int& enters, int& leaves, int& downs)
        : enters_(enters), leaves_(leaves), downs_(downs)
    {
    }

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({1.0f, 1.0f});
    }

    void paint(wui::PaintContext&) override {}

    bool onPointerEvent(const wui::PointerEvent& event) override
    {
        if (event.action == wui::PointerAction::Enter) {
            ++enters_;
        } else if (event.action == wui::PointerAction::Leave) {
            ++leaves_;
        } else if (event.action == wui::PointerAction::Down) {
            ++downs_;
        }
        return true;
    }

private:
    int& enters_;
    int& leaves_;
    int& downs_;
};

void testParentReleaseDetachesEveryAttachedDescendantExactlyOnce()
{
    std::vector<std::string> events;
    // Lifecycle hooks are noexcept by contract; avoid an allocation in this
    // probe so the test itself does not violate that contract.
    events.reserve(4);
    int parentAttaches = 0;
    int parentDetaches = 0;
    int childAttaches = 0;
    int childDetaches = 0;

    auto parent = std::make_unique<LifecycleProbe>("parent", events, parentAttaches, parentDetaches);
    auto child = std::make_unique<LifecycleProbe>("child", events, childAttaches, childDetaches);
    auto* parentRaw = parent.get();
    auto* childRaw = child.get();
    parent->appendChild(std::move(child));

    wui::UiRoot root;
    root.setContent(std::move(parent));
    expect(parentRaw->isAttached() && childRaw->isAttached(),
           "UiRoot adoption must attach a parent and every existing descendant");

    // Replacing the parent destroys the old subtree.  The explicit detach
    // phase must happen before destruction, in descendant-first order, and
    // Node's destructor fallback must not notify a second time.
    root.setContent(std::make_unique<wui::Container>());
    expect(parentAttaches == 1 && childAttaches == 1,
           "Each node must receive one attach callback for one UiRoot adoption");
    expect(parentDetaches == 1 && childDetaches == 1,
           "Destroying an attached parent must detach each descendant exactly once");
    expect(events == std::vector<std::string>{
                         "parent.attach", "child.attach", "child.detach", "parent.detach"},
           "Lifecycle order must attach parent-first and detach descendant-first");
}

void testDetachedReactiveNodeStopsReceivingStateAndSelfRemovalIsSafe()
{
    wui::State<std::string> value{"before"};
    auto textNode = wui::ui::asNode(wui::ui::Text{}.bind(value));
    auto* text = static_cast<wui::Text*>(textNode.get());
    auto parent = std::make_unique<wui::Container>();
    auto* parentRaw = parent.get();
    parent->appendChild(std::move(textNode));

    wui::UiRoot root;
    root.setContent(std::move(parent));
    expect(text->isAttached() && text->value() == "before", "The initial reactive value must be installed on attach");

    std::unique_ptr<wui::Node> retainedRemovedNode;
    const auto removalSubscription = value.subscribe([&](const std::string&) {
        // This runs while State is notifying its observer snapshot.  Removing
        // the same reactive node must synchronously detach/unsubscribe it
        // without invalidating State's current delivery iteration.
        if (!retainedRemovedNode) {
            retainedRemovedNode = parentRaw->removeChild(0);
        }
    });

    value.set("delivered-before-removal");
    expect(retainedRemovedNode.get() == text && !text->isAttached(),
           "A node removed during State notification must be safely detached and retained by its caller");
    expect(text->value() == "delivered-before-removal",
           "The node may receive the notification that caused its removal exactly once");

    value.set("must-not-reach-detached-node");
    expect(text->value() == "delivered-before-removal",
           "A detached node must unsubscribe from State and receive no later notifications");
    value.unsubscribe(removalSubscription);
}

void testDetachingFocusedNodeClearsFocusBeforeDestruction()
{
    auto content = std::make_unique<wui::Container>();
    auto focusedButton = std::make_unique<wui::Button>("Removed");
    auto nextButton = std::make_unique<wui::Button>("Next");
    auto* contentRaw = content.get();
    auto* focusedRaw = focusedButton.get();
    auto* nextRaw = nextButton.get();
    int nextClicks = 0;
    nextButton->onClick([&nextClicks] { ++nextClicks; });
    content->appendChild(std::move(focusedButton));
    content->appendChild(std::move(nextButton));

    wui::UiRoot root;
    root.setContent(std::move(content));

    wui::FocusManager focus;
    wui::InputRouter router(&focus);
    router.setRoot(contentRaw);
    focus.setFocused(focusedRaw);
    expect(focus.focused() == focusedRaw,
           "A focused attached control must be reported before its subtree is removed");

    // Keyed collection reconciliation removes an attached old row before its
    // unique_ptr is released.  The detach itself must clear the non-owning
    // focus target; otherwise the next focus change dereferences freed memory.
    auto removed = contentRaw->removeChild(0);
    expect(focus.focused() == nullptr,
           "Detaching the focused node must synchronously clear FocusManager's target");
    expect((focusedRaw->visualStates() & wui::toMask(wui::ControlVisualState::Focused)) == 0,
           "Detaching the focused control must also clear its visual focus state");
    removed.reset();

    focus.setFocused(nextRaw);
    expect(focus.focused() == nextRaw,
           "Focus must transfer to a surviving control after the old node is destroyed");
    expect(router.dispatchKey({0, wui::KeyAction::Down, 32, 0, false}),
           "Keyboard routing after focused-node destruction must remain safe");
    expect(nextClicks == 1,
           "Keyboard activation must reach the newly focused surviving control");
}

void testFocusDetachCallbackDoesNotOutliveFocusManager()
{
    auto content = std::make_unique<wui::Container>();
    auto button = std::make_unique<wui::Button>("Temporary manager");
    auto* contentRaw = content.get();
    auto* buttonRaw = button.get();
    content->appendChild(std::move(button));

    wui::UiRoot root;
    root.setContent(std::move(content));
    {
        wui::FocusManager shortLivedFocus;
        shortLivedFocus.setFocused(buttonRaw);
    }

    // The node now owns a detach callback registered by a manager that no
    // longer exists.  Destruction must be a no-op instead of a callback into
    // the dead manager.
    contentRaw->removeChild(0).reset();
}

void testDetachingHoveredNodeClearsRouterBeforeDestruction()
{
    int removedEnters = 0;
    int removedLeaves = 0;
    int removedDowns = 0;
    int survivorEnters = 0;
    int survivorLeaves = 0;
    int survivorDowns = 0;

    auto content = std::make_unique<wui::Container>();
    auto removed = std::make_unique<PointerProbe>(removedEnters, removedLeaves, removedDowns);
    auto survivor = std::make_unique<PointerProbe>(survivorEnters, survivorLeaves, survivorDowns);
    auto* contentRaw = content.get();
    auto* removedRaw = removed.get();
    auto* survivorRaw = survivor.get();
    content->appendChild(std::move(removed));
    content->appendChild(std::move(survivor));
    contentRaw->layout({0.0f, 0.0f, 200.0f, 100.0f});
    removedRaw->layout({0.0f, 0.0f, 100.0f, 100.0f});
    survivorRaw->layout({100.0f, 0.0f, 100.0f, 100.0f});

    wui::UiRoot root;
    root.setContent(std::move(content));
    wui::InputRouter router;
    router.setRoot(contentRaw);

    expect(router.dispatchPointer({0, wui::PointerType::Mouse, wui::PointerAction::Move,
                                   wui::MouseButton::None, {10.0f, 10.0f}}),
           "Hovering a child must deliver Enter before it is removed");
    expect(router.hovered() == removedRaw && removedEnters == 1,
           "InputRouter must retain the currently hovered attached node");

    // Todo keyed reconciliation can remove the active row from the tree on a
    // click. The router must clear its non-owning hover target synchronously,
    // before the row's unique_ptr is released.
    contentRaw->removeChild(0).reset();
    expect(router.hovered() == nullptr,
           "Detaching a hovered node must synchronously clear InputRouter hover state");

    expect(router.dispatchPointer({0, wui::PointerType::Mouse, wui::PointerAction::Down,
                                   wui::MouseButton::Left, {150.0f, 10.0f}}),
           "The next pointer Down after hovered-node destruction must remain safe");
    expect(router.hovered() == survivorRaw && survivorEnters == 1 && survivorDowns == 1,
           "Subsequent input must route to the surviving hit target without a stale Leave");
    expect(removedLeaves == 0,
           "A detached hover target must not receive a synthetic Leave on the next event");
}

} // namespace

int main()
{
    try {
        testParentReleaseDetachesEveryAttachedDescendantExactlyOnce();
        testDetachedReactiveNodeStopsReceivingStateAndSelfRemovalIsSafe();
        testDetachingFocusedNodeClearsFocusBeforeDestruction();
        testFocusDetachCallbackDoesNotOutliveFocusManager();
        testDetachingHoveredNodeClearsRouterBeforeDestruction();
    } catch (const std::exception& error) {
        // Test executables must fail normally on Windows instead of surfacing
        // an uncaught exception as an application-crash dialog.
        return (void)error, 1;
    }
    return 0;
}
