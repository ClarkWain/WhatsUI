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

} // namespace

int main()
{
    try {
        testParentReleaseDetachesEveryAttachedDescendantExactlyOnce();
        testDetachedReactiveNodeStopsReceivingStateAndSelfRemovalIsSafe();
    } catch (const std::exception& error) {
        // Test executables must fail normally on Windows instead of surfacing
        // an uncaught exception as an application-crash dialog.
        return (void)error, 1;
    }
    return 0;
}
