#pragma once

// A keyed, fixed-extent virtual list.  It mounts only the visible viewport
// plus a small overscan band and retains recently off-screen rows in a capped
// key-addressable pool.  This preserves stable row identity across insertion
// and removal without making the logical item count dictate tree size.

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "wui/node.h"

namespace wui {

class VirtualList : public ControlNode {
public:
    using Index = std::size_t;
    using Key = std::string;
    using KeyProvider = std::function<Key(Index)>;
    using ItemBuilder = std::function<std::unique_ptr<Node>(Index, const Key&)>;
    using SelectionHandler = std::function<void(Index)>;

    struct Range {
        Index first{0};
        Index last{0}; // Exclusive.

        [[nodiscard]] Index size() const noexcept { return last - first; }
        [[nodiscard]] bool empty() const noexcept { return first == last; }
    };

    VirtualList();

    void setItemCount(Index count);
    [[nodiscard]] Index itemCount() const noexcept;
    void setKeyProvider(KeyProvider provider);
    void setItemBuilder(ItemBuilder builder);
    // Re-evaluates keys after an in-place data-model insertion, deletion, or
    // reordering. Keys must be stable and unique within the current model.
    void refresh();

    [[nodiscard]] float rowExtent() const noexcept;
    void setRowExtent(float extent) noexcept;
    [[nodiscard]] float scrollOffset() const noexcept;
    void setScrollOffset(float offset) noexcept;
    [[nodiscard]] float maxScrollOffset() const noexcept;
    void scrollToIndex(Index index);

    [[nodiscard]] Range visibleRange() const noexcept;
    [[nodiscard]] Index mountedCount() const noexcept;
    [[nodiscard]] Index pooledCount() const noexcept;

    [[nodiscard]] int selectedIndex() const noexcept;
    void setSelectedIndex(int index);
    VirtualList& onSelectionChanged(SelectionHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    struct Mounted {
        Index index{0};
        Key key;
        Node* node{nullptr};
    };

    struct Pooled {
        Key key;
        std::unique_ptr<Node> node;
    };

    [[nodiscard]] Range mountedRange() const noexcept;
    [[nodiscard]] Key keyFor(Index index) const;
    [[nodiscard]] int rowAt(PointF point) const noexcept;
    [[nodiscard]] int normalizedSelection(int index) const noexcept;
    void reconcile();
    void layoutMountedChildren();
    void unmount(std::size_t mountedIndex);
    [[nodiscard]] std::unique_ptr<Node> takePooled(const Key& key);
    void addToPool(Key key, std::unique_ptr<Node> node);
    void trimPool();
    void select(int index);

    Index itemCount_{0};
    float rowExtent_{36.0f};
    float scrollOffset_{0.0f};
    Index overscanRows_{2};
    int selectedIndex_{-1};
    int pressedIndex_{-1};
    KeyProvider keyProvider_;
    ItemBuilder itemBuilder_;
    SelectionHandler onSelectionChanged_;
    std::vector<Mounted> mounted_;
    std::vector<Pooled> pool_;
};

} // namespace wui
