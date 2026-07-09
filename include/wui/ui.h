#pragma once

// Declarative builder authoring API (ADR-005).
//
// A thin, header-only, move-only builder layer that wraps the retained
// `wui::` node tree so page authors can write:
//
//     using namespace wui::ui;
//     auto page = Column()
//         .padding(16)
//         .gap(8)
//         .children(
//             Text("Settings"),
//             Button("Close").onClick([&] { nav.pop(); })
//         );
//     root.setContent(page);
//
// Ownership stays `std::unique_ptr` (single owner per node). Children are
// passed variadically (C++17 fold expression) so move-only nodes transfer
// cleanly without the `std::initializer_list` move restriction.

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "wui/node.h"
#include "wui/types.h"
#include "wui/widgets.h"

namespace wui::ui {

// A raw unique_ptr<Node> passes through unchanged.
inline std::unique_ptr<Node> asNode(std::unique_ptr<Node> node)
{
    return node;
}

// Any builder (something with `.intoNode()`) is consumed into a node.
template <class Builder, class = decltype(std::declval<Builder&&>().intoNode())>
std::unique_ptr<Node> asNode(Builder&& builder)
{
    return std::forward<Builder>(builder).intoNode();
}

// CRTP base: owns the node, provides variadic children and ownership transfer.
template <class Self, class NodeT>
class BuilderBase {
public:
    template <class... Args>
    explicit BuilderBase(Args&&... args)
        : node_(std::make_unique<NodeT>(std::forward<Args>(args)...))
    {
    }

    // Append any number of child builders (or raw unique_ptr<Node>).
    template <class... Children>
    Self&& children(Children&&... items) &&
    {
        (node_->appendChild(asNode(std::forward<Children>(items))), ...);
        return std::move(self());
    }

    // Hand ownership to a parent or to setContent().
    std::unique_ptr<Node> intoNode() { return std::move(node_); }
    operator std::unique_ptr<Node>() { return std::move(node_); }

    NodeT* operator->() noexcept { return node_.get(); }
    NodeT& operator*() noexcept { return *node_; }
    [[nodiscard]] NodeT* get() const noexcept { return node_.get(); }

protected:
    Self&& self() noexcept { return std::move(static_cast<Self&>(*this)); }

    std::unique_ptr<NodeT> node_;
};

class Text : public BuilderBase<Text, wui::Text> {
public:
    explicit Text(std::string value = {})
        : BuilderBase(std::move(value))
    {
    }

    Text&& text(std::string value) &&
    {
        node_->setValue(std::move(value));
        return std::move(self());
    }
};

class Button : public BuilderBase<Button, wui::Button> {
public:
    explicit Button(std::string label = {})
        : BuilderBase(std::move(label))
    {
    }

    Button&& label(std::string label) &&
    {
        node_->setLabel(std::move(label));
        return std::move(self());
    }

    Button&& onClick(std::function<void()> handler) &&
    {
        node_->onClick(std::move(handler));
        return std::move(self());
    }
};

class Row : public BuilderBase<Row, wui::Row> {
public:
    Row() : BuilderBase() {}

    Row&& gap(float gap) &&
    {
        node_->setGap(gap);
        return std::move(self());
    }

    Row&& padding(float all) &&
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return std::move(self());
    }

    Row&& padding(InsetsF insets) &&
    {
        node_->setPadding(insets);
        return std::move(self());
    }
};

class Column : public BuilderBase<Column, wui::Column> {
public:
    Column() : BuilderBase() {}

    Column&& gap(float gap) &&
    {
        node_->setGap(gap);
        return std::move(self());
    }

    Column&& padding(float all) &&
    {
        node_->setPadding(InsetsF{all, all, all, all});
        return std::move(self());
    }

    Column&& padding(InsetsF insets) &&
    {
        node_->setPadding(insets);
        return std::move(self());
    }
};

} // namespace wui::ui
