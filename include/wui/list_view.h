#pragma once

// A Fluent-styled, selectable, fixed-row list. Vector-backed lists retain
// their item values, while provider-backed lists request only visible values;
// both modes paint only the current viewport. ListViewNode does not retain row
// Nodes. Use VirtualListNode when keyed Node identity and recycling are required.

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "wui/node.h"
#include "wui/state.h"

namespace wui {

class ListViewNode : public ControlNode {
public:
    struct Item {
        std::string label;
        bool enabled{true};
    };

    using SelectionHandler = std::function<void(int)>;
    using ItemProvider = std::function<Item(std::size_t)>;
    // Optional authoritative enabled/selectable state for provider-backed
    // rows. When omitted, Item.enabled supplies the same state.
    using SelectableProvider = std::function<bool(std::size_t)>;

    struct Range {
        std::size_t first{0};
        std::size_t last{0};

        [[nodiscard]] std::size_t size() const noexcept { return last - first; }
        [[nodiscard]] bool empty() const noexcept { return first == last; }
    };

    explicit ListViewNode(std::vector<Item> items = {}, int selectedIndex = -1);
    ~ListViewNode() override;

    [[nodiscard]] const std::vector<Item>& items() const noexcept;
    [[nodiscard]] std::size_t itemCount() const noexcept;
    void setItems(std::vector<Item> items);
    void setItemProvider(std::size_t count, ItemProvider provider, SelectableProvider selectable = {});
    void appendItem(Item item);
    void clearItems();

    [[nodiscard]] int selectedIndex() const noexcept;
    ListViewNode& selectedIndex(int index);
    void setSelectedIndex(int index);
    ListViewNode& bind(State<int>& state);
    ListViewNode& onSelectionChanged(SelectionHandler handler);

    [[nodiscard]] float rowHeight() const noexcept;
    void setRowHeight(float value) noexcept;
    [[nodiscard]] float scrollOffset() const noexcept;
    void setScrollOffset(float value) noexcept;
    [[nodiscard]] float maximumScrollOffset() const noexcept;
    [[nodiscard]] Range visibleRange() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    EventResult onPointerEvent(const PointerEvent& event, EventContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    struct State;

    [[nodiscard]] bool isSelectable(int index) const noexcept;
    [[nodiscard]] int normalizedSelection(int index) const noexcept;
    [[nodiscard]] int rowAt(PointF point) const noexcept;
    [[nodiscard]] int nextEnabled(int from, int direction) const noexcept;
    [[nodiscard]] float preferredWidth() const noexcept;
    [[nodiscard]] Item itemAt(std::size_t index) const;
    [[nodiscard]] bool providerItemSelectable(std::size_t index) const noexcept;
    void syncViewport() noexcept;
    void select(int index);

    std::vector<Item> items_;
    int selectedIndex_{-1};
    int hoveredIndex_{-1};
    int pressedIndex_{-1};
    float rowHeight_{36.0f};
    std::unique_ptr<State> state_;
    ItemProvider itemProvider_;
    SelectableProvider selectableProvider_;
    bool usesItemProvider_{false};
    std::size_t providerItemCount_{0};
    std::optional<Binding<int>> binding_;
    bool hasBinding_{false};
    SelectionHandler onSelectionChanged_;
};

} // namespace wui
