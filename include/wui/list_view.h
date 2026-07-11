#pragma once

// A Fluent-styled, selectable list for small and medium data sets.  ListView
// deliberately owns and paints every item; M4's virtualized list layer will
// add keyed reuse for very large data sets without changing this interaction
// contract.

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "wui/node.h"
#include "wui/state.h"

namespace wui {

class ListView : public ControlNode {
public:
    struct Item {
        std::string label;
        bool enabled{true};
    };

    using SelectionHandler = std::function<void(int)>;

    explicit ListView(std::vector<Item> items = {}, int selectedIndex = -1);

    [[nodiscard]] const std::vector<Item>& items() const noexcept;
    void setItems(std::vector<Item> items);
    void appendItem(Item item);
    void clearItems();

    [[nodiscard]] int selectedIndex() const noexcept;
    ListView& selectedIndex(int index);
    void setSelectedIndex(int index);
    ListView& bind(State<int>& state);
    ListView& onSelectionChanged(SelectionHandler handler);

    [[nodiscard]] float rowHeight() const noexcept;
    void setRowHeight(float value) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    [[nodiscard]] bool isSelectable(int index) const noexcept;
    [[nodiscard]] int normalizedSelection(int index) const noexcept;
    [[nodiscard]] int rowAt(PointF point) const noexcept;
    [[nodiscard]] int nextEnabled(int from, int direction) const noexcept;
    [[nodiscard]] float preferredWidth() const noexcept;
    void select(int index);

    std::vector<Item> items_;
    int selectedIndex_{-1};
    int hoveredIndex_{-1};
    int pressedIndex_{-1};
    float rowHeight_{36.0f};
    std::optional<Binding<int>> binding_;
    bool hasBinding_{false};
    SelectionHandler onSelectionChanged_;
};

} // namespace wui
