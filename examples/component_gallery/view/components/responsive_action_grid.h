#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "wui/node.h"

namespace whatsui::gallery::view::components {

// Rebuilds small, self-contained action samples into a grid that follows its
// own laid-out width.  This deliberately lives above the framework Row: the
// component gallery can opt in without changing the semantics of all Rows.
class ResponsiveActionGrid final : public wui::ContainerNode {
public:
    using ItemFactory = std::function<std::unique_ptr<wui::Node>()>;

    explicit ResponsiveActionGrid(std::vector<ItemFactory> factories,
                                  float compactBreakpoint = 520.0f,
                                  float singleColumnBreakpoint = 210.0f)
        : factories_(std::move(factories))
        , compactBreakpoint_(compactBreakpoint)
        , singleColumnBreakpoint_(singleColumnBreakpoint)
    {
    }

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        constexpr float kItemHeight = 32.0f;
        constexpr float kGap = 8.0f;
        const auto columns = columnsFor(constraints.maxWidth);
        const auto rows = (factories_.size() + columns - 1) / columns;
        const float height = rows == 0 ? 0.0f : rows * kItemHeight + (rows - 1) * kGap;
        const float width = columns == 4 ? std::min(constraints.maxWidth, 440.0f)
                                         : constraints.maxWidth;
        return constraints.clamp({width, height});
    }

    void layout(const wui::RectF& bounds) override
    {
        wui::Node::layout(bounds);
        const auto columns = columnsFor(bounds.width);
        if (contentColumns_ != columns) rebuild(columns);
        constexpr float kGap = 8.0f;
        const float gridWidth = columns == 4 ? std::min(bounds.width, 440.0f) : bounds.width;
        const float itemWidth = columns == 0 ? gridWidth
            : std::max(0.0f, (gridWidth - (columns - 1) * kGap) / columns);
        const float gridX = bounds.x + (bounds.width - gridWidth) / 2.0f;
        std::vector<float> rowHeights((children().size() + columns - 1) / columns, 0.0f);
        for (std::size_t index = 0; index < children().size(); ++index) {
            const auto measured = children()[index]->measureWithConstraints(
                {0.0f, itemWidth, 0.0f, 1000000.0f});
            rowHeights[index / columns] = std::max(rowHeights[index / columns], measured.height);
        }
        float y = bounds.y;
        for (std::size_t row = 0; row < rowHeights.size(); ++row) {
            for (std::size_t column = 0; column < columns; ++column) {
                const std::size_t index = row * columns + column;
                if (index >= children().size()) break;
                children()[index]->layout(
                    {gridX + column * (itemWidth + kGap), y, itemWidth, rowHeights[row]});
            }
            y += rowHeights[row] + kGap;
        }
        setBounds({bounds.x, bounds.y, bounds.width,
                   std::max(0.0f, y - bounds.y - (rowHeights.empty() ? 0.0f : kGap))});
        clearLayoutDirtyRecursively();
    }

    void paint(wui::PaintContext& context) override
    {
        for (const auto& child : children()) child->paint(context);
        clearDirty(wui::DirtyFlag::Paint);
    }

    [[nodiscard]] wui::Node* hitTest(wui::PointF point) override
    {
        if (!bounds().contains(point)) return nullptr;
        for (const auto& child : children()) {
            if (auto* hit = child->hitTest(point)) return hit;
        }
        return this;
    }

private:
    [[nodiscard]] std::size_t columnsFor(float width) const noexcept
    {
        if (width < singleColumnBreakpoint_) return 1;
        if (width < compactBreakpoint_) return 2;
        return 4;
    }

    void rebuild(std::size_t columns)
    {
        clearChildren();
        contentColumns_ = columns;
        for (const auto& factory : factories_) appendChild(factory());
    }

    std::vector<ItemFactory> factories_;
    float compactBreakpoint_{520.0f};
    float singleColumnBreakpoint_{210.0f};
    std::size_t contentColumns_{0};
};

} // namespace whatsui::gallery::view::components
