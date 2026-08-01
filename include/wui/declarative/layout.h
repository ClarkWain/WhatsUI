#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "wui/widgets.h"
#include "wui/declarative/builder_base.h"

namespace wui {

class Spacer : public BuilderBase<Spacer, wui::SpacerNode> {
public:
    explicit Spacer(float width = 0.0f, float height = 0.0f)
        : BuilderBase(wui::SizeF{width, height})
    {
    }
};

class Box : public ContainerBuilderBase<Box, wui::BoxNode> {
public:
    Box() : ContainerBuilderBase() {}

    Box& background(Color color) &
    {
        node_->setBackground(color);
        return self();
    }

    Box&& background(Color color) &&
    {
        node_->setBackground(color);
        return std::move(self());
    }

    Box& radius(float radius) &
    {
        node_->setRadius(radius);
        return self();
    }

    Box&& radius(float radius) &&
    {
        node_->setRadius(radius);
        return std::move(self());
    }

    Box& padding(InsetsF padding) &
    {
        node_->setPadding(padding);
        return self();
    }

    Box&& padding(InsetsF padding) &&
    {
        node_->setPadding(padding);
        return std::move(self());
    }

    Box& padding(float all) &
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return self();
    }

    Box&& padding(float all) &&
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return std::move(self());
    }

    Box& contentAlign(Alignment horizontal, Alignment vertical) &
    {
        node_->setContentAlignment(horizontal, vertical);
        return self();
    }

    Box&& contentAlign(Alignment horizontal, Alignment vertical) &&
    {
        node_->setContentAlignment(horizontal, vertical);
        return std::move(self());
    }

    Box& width(float width) &
    {
        node_->setWidth(width);
        return self();
    }

    Box&& width(float width) &&
    {
        node_->setWidth(width);
        return std::move(self());
    }

    Box& height(float height) &
    {
        node_->setHeight(height);
        return self();
    }

    Box&& height(float height) &&
    {
        node_->setHeight(height);
        return std::move(self());
    }

    // Interaction — lazily attaches an InteractionArea to the underlying
    // Container. Every setter is opt-in; a Box that does not call any of them
    // paints and routes events exactly as before this API existed.
    Box& onClick(std::function<void()> handler) &
    {
        node_->setOnClick(std::move(handler));
        return self();
    }

    Box&& onClick(std::function<void()> handler) &&
    {
        node_->setOnClick(std::move(handler));
        return std::move(self());
    }

    Box& onPointerDown(std::function<bool(const wui::PointerEvent&)> handler) &
    {
        node_->setOnPointerDown(std::move(handler));
        return self();
    }

    Box&& onPointerDown(std::function<bool(const wui::PointerEvent&)> handler) &&
    {
        node_->setOnPointerDown(std::move(handler));
        return std::move(self());
    }

    Box& onPointerMove(std::function<bool(const wui::PointerEvent&)> handler) &
    {
        node_->setOnPointerMove(std::move(handler));
        return self();
    }

    Box&& onPointerMove(std::function<bool(const wui::PointerEvent&)> handler) &&
    {
        node_->setOnPointerMove(std::move(handler));
        return std::move(self());
    }

    Box& onPointerUp(std::function<bool(const wui::PointerEvent&)> handler) &
    {
        node_->setOnPointerUp(std::move(handler));
        return self();
    }

    Box&& onPointerUp(std::function<bool(const wui::PointerEvent&)> handler) &&
    {
        node_->setOnPointerUp(std::move(handler));
        return std::move(self());
    }

    Box& onHoverChange(std::function<void(bool)> handler) &
    {
        node_->setOnHoverChange(std::move(handler));
        return self();
    }

    Box&& onHoverChange(std::function<void(bool)> handler) &&
    {
        node_->setOnHoverChange(std::move(handler));
        return std::move(self());
    }

    Box& onFocusChange(std::function<void(bool)> handler) &
    {
        node_->setOnFocusChange(std::move(handler));
        return self();
    }

    Box&& onFocusChange(std::function<void(bool)> handler) &&
    {
        node_->setOnFocusChange(std::move(handler));
        return std::move(self());
    }

    Box& onKey(std::function<bool(const wui::KeyEvent&)> handler) &
    {
        node_->setOnKey(std::move(handler));
        return self();
    }

    Box&& onKey(std::function<bool(const wui::KeyEvent&)> handler) &&
    {
        node_->setOnKey(std::move(handler));
        return std::move(self());
    }

    Box& hoverBackground(Color color) &
    {
        node_->setHoverBackground(color);
        return self();
    }

    Box&& hoverBackground(Color color) &&
    {
        node_->setHoverBackground(color);
        return std::move(self());
    }

    Box& pressedBackground(Color color) &
    {
        node_->setPressedBackground(color);
        return self();
    }

    Box&& pressedBackground(Color color) &&
    {
        node_->setPressedBackground(color);
        return std::move(self());
    }

    Box& accessibleRole(wui::AccessibilityRole role) &
    {
        node_->setAccessibleRole(role);
        return self();
    }

    Box&& accessibleRole(wui::AccessibilityRole role) &&
    {
        node_->setAccessibleRole(role);
        return std::move(self());
    }

    Box& accessibleLabel(std::string label) &
    {
        node_->setAccessibleLabel(std::move(label));
        return self();
    }

    Box&& accessibleLabel(std::string label) &&
    {
        node_->setAccessibleLabel(std::move(label));
        return std::move(self());
    }
};

class Row : public ContainerBuilderBase<Row, wui::RowNode> {
public:
    Row() : ContainerBuilderBase() {}

    Row& gap(float gap) &
    {
        node_->setGap(gap);
        return self();
    }

    Row&& gap(float gap) &&
    {
        node_->setGap(gap);
        return std::move(self());
    }

    Row& padding(float all) &
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return self();
    }

    Row&& padding(float all) &&
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return std::move(self());
    }

    Row& padding(InsetsF insets) &
    {
        node_->setPadding(insets);
        return self();
    }

    Row&& padding(InsetsF insets) &&
    {
        node_->setPadding(insets);
        return std::move(self());
    }

    Row& align(Alignment align) &
    {
        node_->setAlign(align);
        return self();
    }

    Row&& align(Alignment align) &&
    {
        node_->setAlign(align);
        return std::move(self());
    }
};

class Column : public ContainerBuilderBase<Column, wui::ColumnNode> {
public:
    Column() : ContainerBuilderBase() {}

    Column& gap(float gap) &
    {
        node_->setGap(gap);
        return self();
    }

    Column&& gap(float gap) &&
    {
        node_->setGap(gap);
        return std::move(self());
    }

    Column& padding(float all) &
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return self();
    }

    Column&& padding(float all) &&
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return std::move(self());
    }

    Column& padding(InsetsF insets) &
    {
        node_->setPadding(insets);
        return self();
    }

    Column&& padding(InsetsF insets) &&
    {
        node_->setPadding(insets);
        return std::move(self());
    }

    Column& align(Alignment align) &
    {
        node_->setAlign(align);
        return self();
    }

    Column&& align(Alignment align) &&
    {
        node_->setAlign(align);
        return std::move(self());
    }
};

class ScrollView : public SingleContentBuilderBase<ScrollView, wui::ScrollViewNode> {
public:
    ScrollView() : SingleContentBuilderBase() {}

    ScrollView& axis(wui::ScrollAxis axis) &
    {
        node_->setAxis(axis);
        return self();
    }

    ScrollView&& axis(wui::ScrollAxis axis) &&
    {
        node_->setAxis(axis);
        return std::move(self());
    }

    ScrollView& offset(float value) &
    {
        node_->setScrollOffset(value);
        return self();
    }

    ScrollView&& offset(float value) &&
    {
        node_->setScrollOffset(value);
        return std::move(self());
    }

    ScrollView& offset(wui::PointF value) &
    {
        node_->setScrollOffset(value);
        return self();
    }

    ScrollView&& offset(wui::PointF value) &&
    {
        node_->setScrollOffset(value);
        return std::move(self());
    }
};

} // namespace wui
