#include <memory>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wui/virtual_list.h"
#include "wui/runtime.h"

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

class ProbeRow : public wui::Node {
public:
    explicit ProbeRow(std::string key) : key(std::move(key)) {}

    wui::SizeF measure(const wui::Constraints& constraints) const override { return constraints.clamp({100.0f, 20.0f}); }
    void paint(wui::PaintContext&) override {}

    std::string key;
};

class ReentrantDetachRow final : public ProbeRow {
public:
    ReentrantDetachRow(std::string key, wui::VirtualListNode* owner, int* detached)
        : ProbeRow(std::move(key)), owner_(owner), detached_(detached) {}

protected:
    void onDetach() noexcept override
    {
        ++*detached_;
        // This is representative of a row releasing a subscription that asks
        // its view to refresh. The list must defer it until the current
        // unmount pass has no stale mounted indices left.
        owner_->refresh();
    }

private:
    wui::VirtualListNode* owner_{nullptr};
    int* detached_{nullptr};
};

class ReentrantCountChangeRow final : public ProbeRow {
public:
    ReentrantCountChangeRow(std::string key, wui::VirtualListNode* owner, bool* changed)
        : ProbeRow(std::move(key)), owner_(owner), changed_(changed) {}

protected:
    void onDetach() noexcept override
    {
        if (*changed_) return;
        *changed_ = true;
        owner_->setItemCount(10);
        owner_->setScrollOffset(0.0f);
    }

private:
    wui::VirtualListNode* owner_{nullptr};
    bool* changed_{nullptr};
};

class ReentrantBuilderResetRow final : public ProbeRow {
public:
    ReentrantBuilderResetRow(std::string key, wui::VirtualListNode* owner, bool* reset)
        : ProbeRow(std::move(key)), owner_(owner), reset_(reset) {}

protected:
    void onDetach() noexcept override
    {
        if (*reset_) return;
        *reset_ = true;
        owner_->setItemBuilder([](wui::VirtualListNode::Index, const std::string& key) {
            return std::make_unique<ProbeRow>("reset-" + key);
        });
    }

private:
    wui::VirtualListNode* owner_{nullptr};
    bool* reset_{nullptr};
};

void configureList(wui::VirtualListNode& list, std::vector<std::string>& keys)
{
    list.setKeyProvider([&keys](wui::VirtualListNode::Index index) { return keys[index]; });
    list.setItemBuilder([](wui::VirtualListNode::Index, const std::string& key) {
        return std::make_unique<ProbeRow>(key);
    });
    list.setItemCount(keys.size());
    list.layout({0.0f, 0.0f, 240.0f, 180.0f});
}

ProbeRow* findRow(wui::VirtualListNode& list, const std::string& key)
{
    for (const auto& node : list.children()) {
        auto* row = dynamic_cast<ProbeRow*>(node.get());
        if (row != nullptr && row->key == key) return row;
    }
    return nullptr;
}

void testLargeLogicalModelKeepsMountedRowsBounded()
{
    std::vector<std::string> keys;
    keys.reserve(100000);
    for (int index = 0; index < 100000; ++index) keys.push_back("row-" + std::to_string(index));
    wui::VirtualListNode list;
    configureList(list, keys);
    expect(list.visibleRange().first == 0 && list.visibleRange().last == 5,
           "Viewport range should cover only rows intersecting a 180px viewport");
    expect(list.mountedCount() <= 9, "Overscan should keep mounted rows bounded independently of 100k model rows");
    list.setScrollOffset(36.0f * 50000.0f);
    expect(list.visibleRange().first == 50000 && list.mountedCount() <= 9,
           "Large scroll should remount only a bounded new viewport");
    expect(list.pooledCount() <= 18, "Off-screen recycler pool should remain capped");
}

void testStableKeysPreserveMountedIdentityAfterInsertion()
{
    std::vector<std::string> keys{"a", "b", "c", "d", "e", "f"};
    wui::VirtualListNode list;
    configureList(list, keys);
    ProbeRow* const bBefore = findRow(list, "b");
    expect(bBefore != nullptr, "Visible keyed row should be mounted");
    keys.insert(keys.begin(), "new");
    list.setItemCount(keys.size());
    list.refresh();
    expect(findRow(list, "b") == bBefore,
           "A stable key should retain its mounted node when insertion shifts its index");
    keys.erase(keys.begin());
    list.setItemCount(keys.size());
    list.refresh();
    expect(findRow(list, "b") == bBefore,
           "A stable key should retain identity again after removal restores its index");
}

void testPointerAndKeyboardSelectionScrollIntoView()
{
    std::vector<std::string> keys;
    for (int index = 0; index < 100; ++index) keys.push_back(std::to_string(index));
    wui::VirtualListNode list;
    configureList(list, keys);
    int notified = -1;
    list.onSelectionChanged([&](wui::VirtualListNode::Index index) { notified = static_cast<int>(index); });
    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down, wui::MouseButton::Left, {20.0f, 54.0f}};
    auto up = down;
    up.action = wui::PointerAction::Up;
    list.onPointerEvent(down);
    list.onPointerEvent(up);
    expect(list.selectedIndex() == 1 && notified == 1, "Pointer should select the row at its viewport position");
    list.onKeyEvent({0, wui::KeyAction::Down, 35});
    expect(list.selectedIndex() == 99 && list.scrollOffset() > 0.0f,
           "End should select the final logical row and scroll it into view");
    list.onKeyEvent({0, wui::KeyAction::Down, 36});
    expect(list.selectedIndex() == 0 && list.scrollOffset() == 0.0f,
           "Home should restore the first row and its viewport position");
}

void testRecyclePoolSurvivesHighChurnAndDestruction()
{
    std::vector<std::string> keys;
    for (int index = 0; index < 512; ++index) keys.push_back("key-" + std::to_string(index));
    {
        wui::VirtualListNode list;
        configureList(list, keys);
        for (int iteration = 0; iteration < 2000; ++iteration) {
            const int logicalRow = (iteration * 37) % static_cast<int>(keys.size());
            list.setScrollOffset(static_cast<float>(logicalRow * 36));
            if (iteration % 17 == 0) {
                // Refresh is the normal model-notification path: all keys are
                // stable, but the active window is repeatedly reconciled.
                list.refresh();
            }
            expect(list.mountedCount() <= 9 && list.pooledCount() <= 18,
                   "Virtual list recycling must stay bounded during high-churn scroll reconciliation");
        }
    }
    // Scope exit destroys a populated recycler pool. This is intentionally a
    // sanitizer regression: ASan must observe every unique_ptr exactly once.
}

void testDetachRefreshIsDeferredUntilUnmountPassCompletes()
{
    std::vector<std::string> keys;
    for (int index = 0; index < 80; ++index) keys.push_back("reentrant-" + std::to_string(index));
    int detached = 0;
    auto list = std::make_unique<wui::VirtualListNode>();
    auto* raw = list.get();
    raw->setKeyProvider([&keys](wui::VirtualListNode::Index index) { return keys[index]; });
    raw->setItemBuilder([raw, &detached](wui::VirtualListNode::Index, const std::string& key) {
        return std::make_unique<ReentrantDetachRow>(key, raw, &detached);
    });
    raw->setItemCount(keys.size());

    wui::UiRoot root;
    root.setContent(std::move(list));
    root.layout({0.0f, 0.0f, 240.0f, 180.0f});
    for (int iteration = 0; iteration < 120; ++iteration) {
        raw->setScrollOffset(static_cast<float>(((iteration * 19) % 70) * 36));
        expect(raw->mountedCount() <= 9 && raw->pooledCount() <= 18,
               "Detach-triggered refresh must converge without stale mounted indices or unbounded reuse");
    }
    expect(detached > 0, "Reentry regression must actually detach rows from an attached tree");
}

void testDetachCountChangeUsesLatestRange()
{
    std::vector<std::string> keys;
    for (int index = 0; index < 80; ++index) keys.push_back("count-" + std::to_string(index));
    bool changed = false;
    auto list = std::make_unique<wui::VirtualListNode>();
    auto* raw = list.get();
    raw->setKeyProvider([&keys](wui::VirtualListNode::Index index) { return keys[index]; });
    raw->setItemBuilder([raw, &changed](wui::VirtualListNode::Index, const std::string& key) {
        return std::make_unique<ReentrantCountChangeRow>(key, raw, &changed);
    });
    raw->setItemCount(keys.size());

    wui::UiRoot root;
    root.setContent(std::move(list));
    root.layout({0.0f, 0.0f, 240.0f, 180.0f});
    raw->setScrollOffset(36.0f * 40.0f);
    expect(changed && raw->itemCount() == 10 && raw->visibleRange().last <= 10,
           "Detach-triggered count changes must reconcile against the latest bounded range");
    expect(raw->mountedCount() <= 9, "Count-change reentry must keep mounted rows bounded");
}

void testDetachBuilderResetIsDeferredUntilRemoveCompletes()
{
    std::vector<std::string> keys;
    for (int index = 0; index < 80; ++index) keys.push_back("builder-" + std::to_string(index));
    bool reset = false;
    auto list = std::make_unique<wui::VirtualListNode>();
    auto* raw = list.get();
    raw->setKeyProvider([&keys](wui::VirtualListNode::Index index) { return keys[index]; });
    raw->setItemBuilder([raw, &reset](wui::VirtualListNode::Index, const std::string& key) {
        return std::make_unique<ReentrantBuilderResetRow>(key, raw, &reset);
    });

    raw->setItemCount(keys.size());
    wui::UiRoot root;
    root.setContent(std::move(list));
    root.layout({0.0f, 0.0f, 240.0f, 180.0f});
    raw->setScrollOffset(36.0f * 20.0f);
    expect(reset && raw->mountedCount() <= 9,
           "Detach-triggered builder reset must defer owner clear until removal completes");
}

    void testPublicChildMutationDoesNotLeaveRecyclerStalePointers()
    {
        std::vector<std::string> keys;
        for (int index = 0; index < 100; ++index) keys.push_back("public-" + std::to_string(index));
        wui::VirtualListNode list;
        configureList(list, keys);
        const auto mountedBeforeRemove = list.mountedCount();
        expect(mountedBeforeRemove > 0, "Public mutation regression needs mounted rows");
        auto removed = list.removeChild(0);
        expect(removed != nullptr && list.mountedCount() + 1 == mountedBeforeRemove,
            "VirtualList public removeChild must forget the removed recycler row");
        list.refresh();
        list.layout({0.0f, 0.0f, 240.0f, 180.0f});
        expect(list.mountedCount() <= 9 && list.children().size() == list.mountedCount(),
            "VirtualList must reconcile after public removeChild without stale mounted pointers");

        list.clearChildren();
        expect(list.mountedCount() == 0 && list.children().empty(),
            "VirtualList public clearChildren must clear recycler state and owned children together");
        list.refresh();
        list.layout({0.0f, 0.0f, 240.0f, 180.0f});
        expect(list.mountedCount() <= 9 && list.children().size() == list.mountedCount(),
            "VirtualList must reconcile after public clearChildren without stale mounted pointers");
    }

} // namespace

int main()
{
    try {
        testLargeLogicalModelKeepsMountedRowsBounded();
        testStableKeysPreserveMountedIdentityAfterInsertion();
        testPointerAndKeyboardSelectionScrollIntoView();
        testRecyclePoolSurvivesHighChurnAndDestruction();
        testDetachRefreshIsDeferredUntilUnmountPassCompletes();
        testDetachCountChangeUsesLatestRange();
        testDetachBuilderResetIsDeferredUntilRemoveCompletes();
        testPublicChildMutationDoesNotLeaveRecyclerStalePointers();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "VirtualList test failure: " << error.what() << '\n';
        return 1;
    }
}
