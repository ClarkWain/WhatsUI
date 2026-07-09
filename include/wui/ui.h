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
#include <vector>

#include "wui/node.h"
#include "wui/scheduler.h"
#include "wui/state.h"
#include "wui/structural.h"
#include "wui/text_input.h"
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

    // Main-axis flex weight for Row/Column layout (0 = fixed size).
    Self&& flex(float weight) &&
    {
        node_->setFlex(weight);
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

    // Reactive: re-render the text whenever `state` changes.
    template <class T, class Format>
    Text&& bind(wui::State<T>& state, Format format) &&
    {
        wui::Text* raw = node_.get();
        raw->setValue(format(state.get()));
        const auto id = state.subscribe([raw, format](const T& value) {
            raw->setValue(format(value));
        });
        raw->addTeardown([&state, id] { state.unsubscribe(id); });
        return std::move(self());
    }

    // Convenience for State<std::string>.
    Text&& bind(wui::State<std::string>& state) &&
    {
        return std::move(*this).bind(state, [](const std::string& value) { return value; });
    }
};

class Box : public BuilderBase<Box, wui::Container> {
public:
    Box() : BuilderBase() {}
};

class Spacer : public BuilderBase<Spacer, wui::Spacer> {
public:
    explicit Spacer(float width = 0.0f, float height = 0.0f)
        : BuilderBase(wui::SizeF{width, height})
    {
    }
};

class TextField : public BuilderBase<TextField, wui::TextInput> {
public:
    explicit TextField(std::string placeholder = {})
        : BuilderBase(std::move(placeholder))
    {
    }

    TextField&& placeholder(std::string value) &&
    {
        node_->setPlaceholder(std::move(value));
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

    Row&& align(Alignment align) &&
    {
        node_->setAlign(align);
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

    Column&& align(Alignment align) &&
    {
        node_->setAlign(align);
        return std::move(self());
    }
};

// Structural control: mount `then(...)` only while `state` is true.
class If : public BuilderBase<If, wui::IfNode> {
public:
    explicit If(wui::State<bool>& state)
        : BuilderBase()
        , state_(&state)
    {
    }

    // `factory` returns a builder (or unique_ptr<Node>) for the mounted subtree.
    template <class Factory>
    If&& then(Factory factory) &&
    {
        wui::IfNode* raw = node_.get();
        raw->setFactory([factory = std::move(factory)]() mutable -> std::unique_ptr<wui::Node> {
            return asNode(factory());
        });
        raw->setVisible(state_->get());
        wui::State<bool>* state = state_;
        const auto id = state->subscribe([raw, state](const bool&) {
            wui::scheduleStructuralUpdate(raw, [raw, state] { raw->setVisible(state->get()); });
        });
        raw->addTeardown([state, id] { state->unsubscribe(id); });
        return std::move(self());
    }

private:
    wui::State<bool>* state_{nullptr};
};

// Structural control: (re)generate a vertical list of children from `items`.
template <class T>
class ForEach : public BuilderBase<ForEach<T>, wui::ForEachNode> {
public:
    template <class ItemBuilder>
    ForEach(wui::State<std::vector<T>>& items, ItemBuilder itemBuilder)
        : BuilderBase<ForEach<T>, wui::ForEachNode>()
    {
        wui::ForEachNode* raw = this->node_.get();
        auto rebuild = [raw, &items, itemBuilder]() {
            raw->clearChildren();
            for (const auto& item : items.get()) {
                raw->appendChild(asNode(itemBuilder(item)));
            }
        };
        rebuild();
        wui::State<std::vector<T>>* state = &items;
        const auto id = state->subscribe([raw, rebuild](const std::vector<T>&) {
            wui::scheduleStructuralUpdate(raw, [rebuild] { rebuild(); });
        });
        raw->addTeardown([state, id] { state->unsubscribe(id); });
    }

    ForEach<T>&& gap(float gap) &&
    {
        this->node_->setGap(gap);
        return std::move(this->self());
    }

    ForEach<T>&& align(Alignment align) &&
    {
        this->node_->setAlign(align);
        return std::move(this->self());
    }
};

} // namespace wui::ui
