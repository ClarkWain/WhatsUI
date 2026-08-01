#pragma once

// Declarative builder authoring API (ADR-005 and ADR-006).
//
// A thin, header-only, move-only builder layer that wraps the retained
// `wui::` node tree so page authors can write:
//
//     using namespace wui;
//     auto page = Column()
//         .padding(16)
//         .gap(8)
//         .children(
//             Text("Settings"),
//             Button("Close").onClick([&] { nav.pop(); })
//         );
//     root.setContent(std::move(page).build());
//
// Ownership stays `std::unique_ptr` (single owner per node). Children are
// passed variadically (C++17 fold expression) so move-only nodes transfer
// cleanly without the `std::initializer_list` move restriction.

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "wui/node.h"
#include "wui/accordion.h"
#include "wui/avatar.h"
#include "wui/basic_controls.h"
#include "wui/feedback.h"
#include "wui/form_feedback.h"
#include "wui/icons.h"
#include "wui/drawer.h"
#include "wui/badge.h"
#include "wui/date_time.h"
#include "wui/list_view.h"
#include "wui/navigation.h"
#include "wui/overlays.h"
#include "wui/popover.h"
#include "wui/persona.h"
#include "wui/rating.h"
#include "wui/selection.h"
#include "wui/virtual_list.h"
#include "wui/scheduler.h"
#include "wui/state.h"
#include "wui/structural.h"
#include "wui/table.h"
#include "wui/text_input.h"
#include "wui/tree.h"
#include "wui/types.h"
#include "wui/widgets.h"

namespace wui {

namespace detail {

struct BuilderMarker {
};

template <class NodeT>
class BuilderNodeOwner {
public:
    template <class... Args>
    explicit BuilderNodeOwner(Args&&... args)
        : value_(std::make_unique<NodeT>(std::forward<Args>(args)...))
    {
    }

    BuilderNodeOwner(BuilderNodeOwner&&) noexcept = default;
    BuilderNodeOwner& operator=(BuilderNodeOwner&&) noexcept = default;
    BuilderNodeOwner(const BuilderNodeOwner&) = delete;
    BuilderNodeOwner& operator=(const BuilderNodeOwner&) = delete;

    [[nodiscard]] bool empty() const noexcept { return value_ == nullptr; }
    [[nodiscard]] NodeT* raw() const noexcept { return value_.get(); }

    [[nodiscard]] NodeT* get() const
    {
        requireValue();
        return value_.get();
    }

    [[nodiscard]] NodeT* operator->() const { return get(); }

    [[nodiscard]] std::unique_ptr<NodeT> take()
    {
        requireValue();
        return std::move(value_);
    }

private:
    void requireValue() const
    {
        if (!value_) {
            throw std::logic_error("cannot use an empty WhatsUI Builder");
        }
    }

    std::unique_ptr<NodeT> value_;
};

} // namespace detail

// CRTP base: owns one runtime node and exposes only common Builder capabilities.
template <class Self, class NodeT>
class BuilderBase : public detail::BuilderMarker {
    static_assert(std::is_base_of_v<Node, NodeT>,
                  "A WhatsUI Builder must construct a wui::Node type");

public:
    using node_type = NodeT;

    template <class... Args>
    explicit BuilderBase(Args&&... args)
        : node_(std::forward<Args>(args)...)
    {
    }

    BuilderBase(BuilderBase&&) noexcept = default;

    BuilderBase& operator=(BuilderBase&& other) noexcept
    {
        if (this != &other) {
            node_ = std::move(other.node_);
        }
        return *this;
    }

    BuilderBase(const BuilderBase&) = delete;
    BuilderBase& operator=(const BuilderBase&) = delete;

    Self& flex(float weight) &
    {
        node_->setFlex(weight);
        return self();
    }

    Self&& flex(float weight) &&
    {
        node_->setFlex(weight);
        return std::move(self());
    }

    Self& accessibilityId(std::string id) &
    {
        node_->setAccessibilityId(std::move(id));
        return self();
    }

    Self&& accessibilityId(std::string id) &&
    {
        node_->setAccessibilityId(std::move(id));
        return std::move(self());
    }

    [[nodiscard]] bool empty() const noexcept { return node_.empty(); }

    [[nodiscard]] NodeT* node() & noexcept { return node_.raw(); }
    [[nodiscard]] const NodeT* node() const & noexcept { return node_.raw(); }
    NodeT* node() && = delete;
    const NodeT* node() const && = delete;

    [[nodiscard]] NodePtr build() && { return NodePtr(node_.take()); }

protected:
    [[nodiscard]] Self& self() & noexcept { return static_cast<Self&>(*this); }
    [[nodiscard]] Self&& self() && noexcept { return std::move(static_cast<Self&>(*this)); }

    detail::BuilderNodeOwner<NodeT> node_;
};

template <class NodeT,
          std::enable_if_t<std::is_base_of_v<Node, NodeT>, int> = 0>
NodePtr asNode(std::unique_ptr<NodeT>&& node)
{
    if (!node) {
        throw std::invalid_argument("cannot consume a null WhatsUI node");
    }
    return NodePtr(std::move(node));
}

template <class Builder,
          std::enable_if_t<
              std::is_base_of_v<detail::BuilderMarker, std::decay_t<Builder>>,
              int> = 0>
NodePtr asNode(Builder&& builder)
{
    static_assert(!std::is_lvalue_reference_v<Builder>,
                  "WhatsUI node consumers require an rvalue Builder; use std::move for a named Builder");
    return NodePtr(std::move(builder).build());
}

template <class Self, class NodeT>
class ContainerBuilderBase : public BuilderBase<Self, NodeT> {
public:
    using BuilderBase<Self, NodeT>::BuilderBase;

    template <class... Children>
    Self& children(Children&&... items) &
    {
        appendChildren(std::forward<Children>(items)...);
        return this->self();
    }

    template <class... Children>
    Self&& children(Children&&... items) &&
    {
        appendChildren(std::forward<Children>(items)...);
        return std::move(this->self());
    }

private:
    template <class... Children>
    void appendChildren(Children&&... items)
    {
        // Validate the parent before consuming any child values.
        (void)this->node_.get();
        std::vector<NodePtr> nodes;
        nodes.reserve(sizeof...(Children));
        (nodes.push_back(asNode(std::forward<Children>(items))), ...);
        this->node_->appendChildren(std::move(nodes));
    }
};

class Text : public BuilderBase<Text, wui::TextNode> {
public:
    explicit Text(std::string value = {})
        : BuilderBase(std::move(value))
    {
    }

    Text& text(std::string value) &
    {
        node_->setValue(std::move(value));
        return self();
    }

    Text&& text(std::string value) &&
    {
        node_->setValue(std::move(value));
        return std::move(self());
    }

    Text& size(float fontSize) &
    {
        node_->setFontSize(fontSize);
        return self();
    }

    Text&& size(float fontSize) &&
    {
        node_->setFontSize(fontSize);
        return std::move(self());
    }

    Text& weight(int fontWeight) &
    {
        node_->setFontWeight(fontWeight);
        return self();
    }

    Text&& weight(int fontWeight) &&
    {
        node_->setFontWeight(fontWeight);
        return std::move(self());
    }

    Text& lineHeight(float height) &
    {
        node_->setLineHeight(height);
        return self();
    }

    Text&& lineHeight(float height) &&
    {
        node_->setLineHeight(height);
        return std::move(self());
    }

    Text& fillWidth(bool fill = true) &
    {
        node_->setFillAvailableWidth(fill);
        return self();
    }

    Text&& fillWidth(bool fill = true) &&
    {
        node_->setFillAvailableWidth(fill);
        return std::move(self());
    }

    Text& style(const wui::TextStyleToken& value) &
    {
        node_->setTextStyle(value);
        return self();
    }

    Text&& style(const wui::TextStyleToken& value) &&
    {
        node_->setTextStyle(value);
        return std::move(self());
    }

    Text& role(wui::TextRole value) & { node_->setRole(value); return self(); }

    Text&& role(wui::TextRole value) && { node_->setRole(value); return std::move(self()); }
    Text& align(wui::TextAlign value) & { node_->setAlignment(value); return self(); }

    Text&& align(wui::TextAlign value) && { node_->setAlignment(value); return std::move(self()); }
    Text& underline(bool value = true) & { node_->setUnderline(value); return self(); }

    Text&& underline(bool value = true) && { node_->setUnderline(value); return std::move(self()); }
    Text& strikethrough(bool value = true) & { node_->setStrikethrough(value); return self(); }

    Text&& strikethrough(bool value = true) && { node_->setStrikethrough(value); return std::move(self()); }

    Text& wrap(wui::TextWrap value = wui::TextWrap::Word) &
    {
        node_->setWrap(value);
        return self();
    }

    Text&& wrap(wui::TextWrap value = wui::TextWrap::Word) &&
    {
        node_->setWrap(value);
        return std::move(self());
    }

    Text& maxLines(std::size_t value) &
    {
        node_->setMaxLines(value);
        return self();
    }

    Text&& maxLines(std::size_t value) &&
    {
        node_->setMaxLines(value);
        return std::move(self());
    }

    Text& ellipsis(bool enabled = true) &
    {
        node_->setOverflow(enabled ? wui::TextOverflow::Ellipsis : wui::TextOverflow::Clip);
        return self();
    }

    Text&& ellipsis(bool enabled = true) &&
    {
        node_->setOverflow(enabled ? wui::TextOverflow::Ellipsis : wui::TextOverflow::Clip);
        return std::move(self());
    }

    Text& color(Color color) &
    {
        node_->setColor(color);
        return self();
    }

    Text&& color(Color color) &&
    {
        node_->setColor(color);
        return std::move(self());
    }

    // Reactive: re-render the text whenever the observable source (State or
    // Computed) changes.
    template <class T, class Format>
    Text& bind(wui::State<T>& source, Format format) &
    {
        wui::TextNode* raw = node_.get();
        wui::State<T> retained = source;
        struct Subscription {
            std::size_t id{0};
            bool active{false};
        };
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, retained, format, subscription] {
            raw->setValue(format(retained.get()));
            if (subscription->active) {
                return;
            }
            subscription->id = retained.subscribe(
                [raw, format](const T& value) {
                    raw->setValue(format(value));
                });
            subscription->active = true;
        };
        auto disconnect = [retained, subscription] {
            if (!subscription->active) {
                return;
            }
            retained.unsubscribe(subscription->id);
            subscription->active = false;
        };
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown(disconnect);
        return self();
    }

    template <class T, class Format>
    Text&& bind(wui::State<T>& source, Format format) &&
    {
        wui::TextNode* raw = node_.get();
        wui::State<T> retained = source;
        struct Subscription {
            std::size_t id{0};
            bool active{false};
        };
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, retained, format, subscription] {
            raw->setValue(format(retained.get()));
            if (subscription->active) {
                return;
            }
            subscription->id = retained.subscribe(
                [raw, format](const T& value) {
                    raw->setValue(format(value));
                });
            subscription->active = true;
        };
        auto disconnect = [retained, subscription] {
            if (!subscription->active) {
                return;
            }
            retained.unsubscribe(subscription->id);
            subscription->active = false;
        };
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown(disconnect);
        return std::move(self());
    }

    template <class Observable, class Format>
    Text& bind(Observable& source, Format format) &
    {
        wui::TextNode* raw = node_.get();
        struct Subscription {
            std::size_t id{0};
            bool active{false};
        };
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, source = &source, format, subscription] {
            raw->setValue(format(source->get()));
            if (subscription->active) {
                return;
            }
            subscription->id = source->subscribe([raw, format](const auto& value) {
                raw->setValue(format(value));
            });
            subscription->active = true;
        };
        auto disconnect = [source = &source, subscription] {
            if (!subscription->active) {
                return;
            }
            source->unsubscribe(subscription->id);
            subscription->active = false;
        };
        // Bind immediately for detached construction (useful for headless
        // composition), then pause updates whenever the node leaves a live
        // UI tree. Reattachment refreshes from the source before reconnecting.
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown(disconnect);
        return self();
    }

    template <class Observable, class Format>
    Text&& bind(Observable& source, Format format) &&
    {
        wui::TextNode* raw = node_.get();
        struct Subscription {
            std::size_t id{0};
            bool active{false};
        };
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, source = &source, format, subscription] {
            raw->setValue(format(source->get()));
            if (subscription->active) {
                return;
            }
            subscription->id = source->subscribe([raw, format](const auto& value) {
                raw->setValue(format(value));
            });
            subscription->active = true;
        };
        auto disconnect = [source = &source, subscription] {
            if (!subscription->active) {
                return;
            }
            source->unsubscribe(subscription->id);
            subscription->active = false;
        };
        // Bind immediately for detached construction (useful for headless
        // composition), then pause updates whenever the node leaves a live
        // UI tree. Reattachment refreshes from the source before reconnecting.
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown(disconnect);
        return std::move(self());
    }

    // Convenience for State<std::string>.
    Text& bind(wui::State<std::string>& state) &
    {
        return this->bind(state, [](const std::string& value) { return value; });
    }

    Text&& bind(wui::State<std::string>& state) &&
    {
        return std::move(*this).bind(state, [](const std::string& value) { return value; });
    }
};

class Icon : public BuilderBase<Icon, wui::IconNode> {
public:
    explicit Icon(wui::IconName name = wui::IconName::Info)
        : BuilderBase(name) {}
    Icon& name(wui::IconName value) & { node_->setName(value); return self(); }

    Icon&& name(wui::IconName value) && { node_->setName(value); return std::move(self()); }
    Icon& size(wui::IconSize value) & { node_->setSize(value); return self(); }

    Icon&& size(wui::IconSize value) && { node_->setSize(value); return std::move(self()); }
    Icon& style(wui::IconStyle value) & { node_->setStyle(value); return self(); }

    Icon&& style(wui::IconStyle value) && { node_->setStyle(value); return std::move(self()); }
    Icon& color(Color value) & { node_->setColor(value); return self(); }

    Icon&& color(Color value) && { node_->setColor(value); return std::move(self()); }
};

class Image : public BuilderBase<Image, wui::ImageNode> {
public:
    Image() : BuilderBase() {}

    Image(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
        : BuilderBase(std::move(rgbaPixels), pixelWidth, pixelHeight)
    {
    }

    Image& source(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight) &
    {
        node_->setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
        return self();
    }

    Image&& source(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight) &&
    {
        node_->setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
        return std::move(self());
    }

    Image& fallback(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight) &
    {
        node_->fallback(std::move(rgbaPixels), pixelWidth, pixelHeight);
        return self();
    }

    Image&& fallback(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight) &&
    {
        node_->fallback(std::move(rgbaPixels), pixelWidth, pixelHeight);
        return std::move(self());
    }

    Image& fit(wui::ImageFit fit) &
    {
        node_->setFit(fit);
        return self();
    }

    Image&& fit(wui::ImageFit fit) &&
    {
        node_->setFit(fit);
        return std::move(self());
    }

    Image& align(float x, float y) &
    {
        node_->setAlignment(x, y);
        return self();
    }

    Image&& align(float x, float y) &&
    {
        node_->setAlignment(x, y);
        return std::move(self());
    }

    Image& shape(wui::ImageShape value) & { node_->setShape(value); return self(); }

    Image&& shape(wui::ImageShape value) && { node_->setShape(value); return std::move(self()); }
    Image& bordered(bool value = true) & { node_->setBordered(value); return self(); }

    Image&& bordered(bool value = true) && { node_->setBordered(value); return std::move(self()); }
    Image& shadow(bool value = true) & { node_->setShadow(value); return self(); }

    Image&& shadow(bool value = true) && { node_->setShadow(value); return std::move(self()); }
    Image& block(bool value = true) & { node_->setBlock(value); return self(); }

    Image&& block(bool value = true) && { node_->setBlock(value); return std::move(self()); }
    Image& alt(std::string value) & { node_->setAlt(std::move(value)); return self(); }

    Image&& alt(std::string value) && { node_->setAlt(std::move(value)); return std::move(self()); }
    Image& decorative(bool value = true) & { node_->setDecorative(value); return self(); }

    Image&& decorative(bool value = true) && { node_->setDecorative(value); return std::move(self()); }
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

class Spacer : public BuilderBase<Spacer, wui::SpacerNode> {
public:
    explicit Spacer(float width = 0.0f, float height = 0.0f)
        : BuilderBase(wui::SizeF{width, height})
    {
    }
};

class TextField : public BuilderBase<TextField, wui::TextFieldNode> {
public:
    explicit TextField(std::string placeholder = {})
        : BuilderBase(std::move(placeholder))
    {
    }

    TextField& placeholder(std::string value) &
    {
        node_->setPlaceholder(std::move(value));
        return self();
    }

    TextField&& placeholder(std::string value) &&
    {
        node_->setPlaceholder(std::move(value));
        return std::move(self());
    }

    TextField& onChange(wui::TextFieldNode::ChangeHandler handler) &
    {
        node_->onChange(std::move(handler));
        return self();
    }

    TextField&& onChange(wui::TextFieldNode::ChangeHandler handler) &&
    {
        node_->onChange(std::move(handler));
        return std::move(self());
    }

    TextField& onSubmit(wui::TextFieldNode::SubmitHandler handler) &
    {
        node_->onSubmit(std::move(handler));
        return self();
    }

    TextField&& onSubmit(wui::TextFieldNode::SubmitHandler handler) &&
    {
        node_->onSubmit(std::move(handler));
        return std::move(self());
    }

    TextField& onCancel(wui::TextFieldNode::CancelHandler handler) &
    {
        node_->onCancel(std::move(handler));
        return self();
    }

    TextField&& onCancel(wui::TextFieldNode::CancelHandler handler) &&
    {
        node_->onCancel(std::move(handler));
        return std::move(self());
    }

    TextField& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    TextField&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    TextField& size(wui::InputSize value) & { node_->setSize(value); return self(); }

    TextField&& size(wui::InputSize value) && { node_->setSize(value); return std::move(self()); }
    TextField& appearance(wui::InputAppearance value) & { node_->setAppearance(value); return self(); }

    TextField&& appearance(wui::InputAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    TextField& invalid(bool value = true) & { node_->setInvalid(value); return self(); }

    TextField&& invalid(bool value = true) && { node_->setInvalid(value); return std::move(self()); }
    TextField& motionEnabled(bool value = true) & { node_->setMotionEnabled(value); return self(); }

    TextField&& motionEnabled(bool value = true) && { node_->setMotionEnabled(value); return std::move(self()); }
};

class TextArea : public BuilderBase<TextArea, wui::TextAreaNode> {
public:
    explicit TextArea(std::string placeholder = {})
        : BuilderBase(std::move(placeholder))
    {
    }

    TextArea& placeholder(std::string value) & { node_->setPlaceholder(std::move(value)); return self(); }

    TextArea&& placeholder(std::string value) && { node_->setPlaceholder(std::move(value)); return std::move(self()); }
    TextArea& onChange(wui::TextFieldNode::ChangeHandler handler) & { node_->onChange(std::move(handler)); return self(); }

    TextArea&& onChange(wui::TextFieldNode::ChangeHandler handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    TextArea& onCancel(wui::TextFieldNode::CancelHandler handler) & { node_->onCancel(std::move(handler)); return self(); }

    TextArea&& onCancel(wui::TextFieldNode::CancelHandler handler) && { node_->onCancel(std::move(handler)); return std::move(self()); }
    TextArea& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    TextArea&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    TextArea& size(wui::InputSize value) & { node_->setSize(value); return self(); }

    TextArea&& size(wui::InputSize value) && { node_->setSize(value); return std::move(self()); }
    TextArea& appearance(wui::InputAppearance value) & { node_->setAppearance(value); return self(); }

    TextArea&& appearance(wui::InputAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    TextArea& invalid(bool value = true) & { node_->setInvalid(value); return self(); }

    TextArea&& invalid(bool value = true) && { node_->setInvalid(value); return std::move(self()); }
    TextArea& motionEnabled(bool value = true) & { node_->setMotionEnabled(value); return self(); }

    TextArea&& motionEnabled(bool value = true) && { node_->setMotionEnabled(value); return std::move(self()); }
    TextArea& rows(std::size_t value) & { node_->setRows(value); return self(); }

    TextArea&& rows(std::size_t value) && { node_->setRows(value); return std::move(self()); }
};

class Card : public ContainerBuilderBase<Card, wui::CardNode> {
public:
    Card() : ContainerBuilderBase() {}
    Card& appearance(wui::CardAppearance value) & { node_->setAppearance(value); return self(); }

    Card&& appearance(wui::CardAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    Card& size(wui::CardSize value) & { node_->setSize(value); return self(); }

    Card&& size(wui::CardSize value) && { node_->setSize(value); return std::move(self()); }
    Card& orientation(wui::CardOrientation value) & { node_->setOrientation(value); return self(); }

    Card&& orientation(wui::CardOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Card& selected(bool value = true) & { node_->setSelected(value); return self(); }

    Card&& selected(bool value = true) && { node_->setSelected(value); return std::move(self()); }
    Card& selectable(bool value = true) & { node_->selectable(value); return self(); }

    Card&& selectable(bool value = true) && { node_->selectable(value); return std::move(self()); }
    Card& onSelectionChange(wui::CardNode::ChangeHandler value) & { node_->onSelectionChange(std::move(value)); return self(); }

    Card&& onSelectionChange(wui::CardNode::ChangeHandler value) && { node_->onSelectionChange(std::move(value)); return std::move(self()); }
};

class CardHeader : public ContainerBuilderBase<CardHeader, wui::CardHeaderNode> {
public:
    CardHeader(std::string title = {}, std::string description = {}) : ContainerBuilderBase(std::move(title), std::move(description)) {}
    CardHeader& media(std::unique_ptr<wui::Node> value) & { node_->media(std::move(value)); return self(); }

    CardHeader&& media(std::unique_ptr<wui::Node> value) && { node_->media(std::move(value)); return std::move(self()); }
    CardHeader& action(std::unique_ptr<wui::Node> value) & { node_->action(std::move(value)); return self(); }

    CardHeader&& action(std::unique_ptr<wui::Node> value) && { node_->action(std::move(value)); return std::move(self()); }
};

class CardPreview : public ContainerBuilderBase<CardPreview, wui::CardPreviewNode> {
public:
    CardPreview() : ContainerBuilderBase() {}
    CardPreview& height(float value) & { node_->setHeight(value); return self(); }

    CardPreview&& height(float value) && { node_->setHeight(value); return std::move(self()); }
};

class CardFooter : public ContainerBuilderBase<CardFooter, wui::CardFooterNode> {
public:
    CardFooter() : ContainerBuilderBase() {}
};

class Label : public BuilderBase<Label, wui::LabelNode> {
public:
    explicit Label(std::string text = {}) : BuilderBase(std::move(text)) {}
    Label& size(wui::LabelSize value) & { node_->setSize(value); return self(); }

    Label&& size(wui::LabelSize value) && { node_->setSize(value); return std::move(self()); }
    Label& required(bool value = true) & { node_->setRequired(value); return self(); }

    Label&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Label& forControl(wui::TextFieldNode* control) & { node_->setForControl(control); return self(); }

    Label&& forControl(wui::TextFieldNode* control) && { node_->setForControl(control); return std::move(self()); }
};

class Field : public BuilderBase<Field, wui::FieldNode> {
public:
    explicit Field(std::string label = {}) : BuilderBase(std::move(label)) {}
    Field& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    Field&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Field& hint(std::string value) & { node_->setHint(std::move(value)); return self(); }

    Field&& hint(std::string value) && { node_->setHint(std::move(value)); return std::move(self()); }
    Field& validationMessage(std::string value) & { node_->setValidationMessage(std::move(value)); return self(); }

    Field&& validationMessage(std::string value) && { node_->setValidationMessage(std::move(value)); return std::move(self()); }
    Field& validationState(wui::FieldValidationState value) & { node_->setValidationState(value); return self(); }

    Field&& validationState(wui::FieldValidationState value) && { node_->setValidationState(value); return std::move(self()); }
    Field& required(bool value = true) & { node_->setRequired(value); return self(); }

    Field&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Field& orientation(wui::FieldOrientation value) & { node_->setOrientation(value); return self(); }

    Field&& orientation(wui::FieldOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Field& enabled(bool value) & { node_->setEnabled(value); return self(); }

    Field&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
    template <class Child>
    Field& control(Child&& value) & { node_->setControl(asNode(std::forward<Child>(value))); return self(); }

    template <class Child>
    Field&& control(Child&& value) && { node_->setControl(asNode(std::forward<Child>(value))); return std::move(self()); }
};

class MessageBar : public BuilderBase<MessageBar, wui::MessageBarNode> {
public:
    explicit MessageBar(std::string body = {}) : BuilderBase(std::move(body)) {}
    MessageBar& title(std::string value) & { node_->setTitle(std::move(value)); return self(); }

    MessageBar&& title(std::string value) && { node_->setTitle(std::move(value)); return std::move(self()); }
    MessageBar& body(std::string value) & { node_->setBody(std::move(value)); return self(); }

    MessageBar&& body(std::string value) && { node_->setBody(std::move(value)); return std::move(self()); }
    MessageBar& intent(wui::MessageBarIntent value) & { node_->setIntent(value); return self(); }

    MessageBar&& intent(wui::MessageBarIntent value) && { node_->setIntent(value); return std::move(self()); }
    MessageBar& multiline(bool value = true) & { node_->setMultiline(value); return self(); }

    MessageBar&& multiline(bool value = true) && { node_->setMultiline(value); return std::move(self()); }
    MessageBar& action(wui::MessageBarAction value) & { node_->addAction(std::move(value)); return self(); }

    MessageBar&& action(wui::MessageBarAction value) && { node_->addAction(std::move(value)); return std::move(self()); }
    MessageBar& dismissible(bool value = true) & { node_->setDismissible(value); return self(); }

    MessageBar&& dismissible(bool value = true) && { node_->setDismissible(value); return std::move(self()); }
    MessageBar& onDismiss(wui::MessageBarNode::DismissHandler value) & { node_->onDismiss(std::move(value)); return self(); }

    MessageBar&& onDismiss(wui::MessageBarNode::DismissHandler value) && { node_->onDismiss(std::move(value)); return std::move(self()); }
};

class Button : public BuilderBase<Button, wui::ButtonNode> {
public:
    explicit Button(std::string label = {})
        : BuilderBase(std::move(label))
    {
    }

    Button& label(std::string label) &
    {
        node_->setLabel(std::move(label));
        return self();
    }

    Button&& label(std::string label) &&
    {
        node_->setLabel(std::move(label));
        return std::move(self());
    }

    Button& onClick(std::function<void()> handler) &
    {
        node_->onClick(std::move(handler));
        return self();
    }

    Button&& onClick(std::function<void()> handler) &&
    {
        node_->onClick(std::move(handler));
        return std::move(self());
    }

    Button& appearance(wui::ButtonAppearance value) &
    {
        node_->setAppearance(value);
        return self();
    }

    Button&& appearance(wui::ButtonAppearance value) &&
    {
        node_->setAppearance(value);
        return std::move(self());
    }

    Button& size(wui::ButtonSize value) &
    {
        node_->setSize(value);
        return self();
    }

    Button&& size(wui::ButtonSize value) &&
    {
        node_->setSize(value);
        return std::move(self());
    }

    Button& shape(wui::ButtonShape value) &
    {
        node_->setShape(value);
        return self();
    }

    Button&& shape(wui::ButtonShape value) &&
    {
        node_->setShape(value);
        return std::move(self());
    }

    Button& icon(wui::IconName value) &
    {
        node_->setIcon(value);
        return self();
    }

    Button&& icon(wui::IconName value) &&
    {
        node_->setIcon(value);
        return std::move(self());
    }

    Button& iconStyle(wui::IconStyle value) &
    {
        node_->setIconStyle(value);
        return self();
    }

    Button&& iconStyle(wui::IconStyle value) &&
    {
        node_->setIconStyle(value);
        return std::move(self());
    }

    Button& iconPosition(wui::ButtonIconPosition value) &
    {
        node_->setIconPosition(value);
        return self();
    }

    Button&& iconPosition(wui::ButtonIconPosition value) &&
    {
        node_->setIconPosition(value);
        return std::move(self());
    }

    Button& iconOnly(bool value = true) &
    {
        node_->setIconOnly(value);
        return self();
    }

    Button&& iconOnly(bool value = true) &&
    {
        node_->setIconOnly(value);
        return std::move(self());
    }
};

class Checkbox : public BuilderBase<Checkbox, wui::CheckboxNode> {
public:
    explicit Checkbox(std::string label = {}, bool checked = false) : BuilderBase(std::move(label), checked) {}
    Checkbox& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    Checkbox&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Checkbox& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Checkbox&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Checkbox& checked(bool value) & { node_->setChecked(value); return self(); }

    Checkbox&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    Checkbox& mixed(bool value = true) & { node_->setMixed(value); return self(); }

    Checkbox&& mixed(bool value = true) && { node_->setMixed(value); return std::move(self()); }
    Checkbox& checkState(wui::CheckboxState value) & { node_->setCheckState(value); return self(); }

    Checkbox&& checkState(wui::CheckboxState value) && { node_->setCheckState(value); return std::move(self()); }
    Checkbox& bind(wui::State<bool>& state) & { node_->bind(state); return self(); }

    Checkbox&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Checkbox& onChange(std::function<void(bool)> handler) & { node_->onChange(std::move(handler)); return self(); }

    Checkbox&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Checkbox& onStateChange(std::function<void(wui::CheckboxState)> handler) & { node_->onStateChange(std::move(handler)); return self(); }

    Checkbox&& onStateChange(std::function<void(wui::CheckboxState)> handler) && { node_->onStateChange(std::move(handler)); return std::move(self()); }
    Checkbox& size(wui::CheckboxSize value) & { node_->setSize(value); return self(); }

    Checkbox&& size(wui::CheckboxSize value) && { node_->setSize(value); return std::move(self()); }
    Checkbox& shape(wui::CheckboxShape value) & { node_->setShape(value); return self(); }

    Checkbox&& shape(wui::CheckboxShape value) && { node_->setShape(value); return std::move(self()); }
    Checkbox& labelPosition(wui::CheckboxLabelPosition value) & { node_->setLabelPosition(value); return self(); }

    Checkbox&& labelPosition(wui::CheckboxLabelPosition value) && { node_->setLabelPosition(value); return std::move(self()); }
    Checkbox& required(bool value = true) & { node_->setRequired(value); return self(); }

    Checkbox&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Checkbox& enabled(bool value) & { node_->setEnabled(value); return self(); }

    Checkbox&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class ToggleButton : public BuilderBase<ToggleButton, wui::ToggleButtonNode> {
public:
    explicit ToggleButton(std::string label = {}, bool checked = false) : BuilderBase(std::move(label), checked) {}
    ToggleButton& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    ToggleButton&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    ToggleButton& checked(bool value) & { node_->setChecked(value); return self(); }

    ToggleButton&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    ToggleButton& bind(wui::State<bool>& state) & { node_->bind(state); return self(); }

    ToggleButton&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    ToggleButton& onChange(std::function<void(bool)> handler) & { node_->onChange(std::move(handler)); return self(); }

    ToggleButton&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    ToggleButton& size(wui::ButtonSize value) & { node_->setSize(value); return self(); }

    ToggleButton&& size(wui::ButtonSize value) && { node_->setSize(value); return std::move(self()); }
    ToggleButton& shape(wui::ButtonShape value) & { node_->setShape(value); return self(); }

    ToggleButton&& shape(wui::ButtonShape value) && { node_->setShape(value); return std::move(self()); }
    ToggleButton& appearance(wui::ButtonAppearance value) & { node_->setAppearance(value); return self(); }

    ToggleButton&& appearance(wui::ButtonAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    ToggleButton& icon(wui::IconName value) & { node_->setIcon(value); return self(); }

    ToggleButton&& icon(wui::IconName value) && { node_->setIcon(value); return std::move(self()); }
    ToggleButton& iconStyle(wui::IconStyle value) & { node_->setIconStyle(value); return self(); }

    ToggleButton&& iconStyle(wui::IconStyle value) && { node_->setIconStyle(value); return std::move(self()); }
    ToggleButton& iconPosition(wui::ButtonIconPosition value) & { node_->setIconPosition(value); return self(); }

    ToggleButton&& iconPosition(wui::ButtonIconPosition value) && { node_->setIconPosition(value); return std::move(self()); }
    ToggleButton& iconOnly(bool value = true) & { node_->setIconOnly(value); return self(); }

    ToggleButton&& iconOnly(bool value = true) && { node_->setIconOnly(value); return std::move(self()); }
};

class CompoundButton : public BuilderBase<CompoundButton, wui::CompoundButtonNode> {
public:
    CompoundButton(std::string label = {}, std::string secondaryContent = {})
        : BuilderBase(std::move(label), std::move(secondaryContent)) {}
    CompoundButton& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    CompoundButton&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    CompoundButton& secondaryContent(std::string value) & { node_->setSecondaryContent(std::move(value)); return self(); }

    CompoundButton&& secondaryContent(std::string value) && { node_->setSecondaryContent(std::move(value)); return std::move(self()); }
    CompoundButton& onClick(std::function<void()> handler) & { node_->onClick(std::move(handler)); return self(); }

    CompoundButton&& onClick(std::function<void()> handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
    CompoundButton& appearance(wui::ButtonAppearance value) & { node_->setAppearance(value); return self(); }

    CompoundButton&& appearance(wui::ButtonAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    CompoundButton& size(wui::ButtonSize value) & { node_->setSize(value); return self(); }

    CompoundButton&& size(wui::ButtonSize value) && { node_->setSize(value); return std::move(self()); }
    CompoundButton& shape(wui::ButtonShape value) & { node_->setShape(value); return self(); }

    CompoundButton&& shape(wui::ButtonShape value) && { node_->setShape(value); return std::move(self()); }
};

class Radio : public BuilderBase<Radio, wui::RadioNode> {
public:
    explicit Radio(std::string label = {}, bool selected = false) : BuilderBase(std::move(label), selected) {}
    Radio& selected(bool value) & { node_->setSelected(value); return self(); }

    Radio&& selected(bool value) && { node_->setSelected(value); return std::move(self()); }
    Radio& bind(wui::State<bool>& state) & { node_->bind(state); return self(); }

    Radio&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Radio& onChange(std::function<void(bool)> handler) & { node_->onChange(std::move(handler)); return self(); }

    Radio&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
};

class RadioGroup : public ContainerBuilderBase<RadioGroup, wui::RadioGroupNode> {
public:
    RadioGroup() : ContainerBuilderBase() {}
    RadioGroup& option(std::string value, std::string label, bool enabled = true) &
    {
        node_->addOption(std::move(value), std::move(label), enabled);
        return self();
    }

    RadioGroup&& option(std::string value, std::string label, bool enabled = true) &&
    {
        node_->addOption(std::move(value), std::move(label), enabled);
        return std::move(self());
    }
    RadioGroup& name(std::string value) & { node_->setName(std::move(value)); return self(); }

    RadioGroup&& name(std::string value) && { node_->setName(std::move(value)); return std::move(self()); }
    RadioGroup& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    RadioGroup&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    RadioGroup& value(std::string value) & { node_->setValue(std::move(value)); return self(); }

    RadioGroup&& value(std::string value) && { node_->setValue(std::move(value)); return std::move(self()); }
    RadioGroup& bind(wui::State<std::string>& state) & { node_->bind(state); return self(); }

    RadioGroup&& bind(wui::State<std::string>& state) && { node_->bind(state); return std::move(self()); }
    RadioGroup& onChange(std::function<void(const std::string&)> handler) & { node_->onChange(std::move(handler)); return self(); }

    RadioGroup&& onChange(std::function<void(const std::string&)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    RadioGroup& layout(wui::RadioGroupLayout value) & { node_->setGroupLayout(value); return self(); }

    RadioGroup&& layout(wui::RadioGroupLayout value) && { node_->setGroupLayout(value); return std::move(self()); }
    RadioGroup& required(bool value = true) & { node_->setRequired(value); return self(); }

    RadioGroup&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    RadioGroup& enabled(bool value) & { node_->setEnabled(value); return self(); }

    RadioGroup&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class Switch : public BuilderBase<Switch, wui::SwitchNode> {
public:
    explicit Switch(std::string label = {}, bool on = false) : BuilderBase(std::move(label), on) {}
    Switch& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    Switch&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Switch& on(bool value) & { node_->setOn(value); return self(); }

    Switch&& on(bool value) && { node_->setOn(value); return std::move(self()); }
    Switch& bind(wui::State<bool>& state) & { node_->bind(state); return self(); }

    Switch&& bind(wui::State<bool>& state) && { node_->bind(state); return std::move(self()); }
    Switch& onChange(std::function<void(bool)> handler) & { node_->onChange(std::move(handler)); return self(); }

    Switch&& onChange(std::function<void(bool)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Switch& size(wui::SwitchSize value) & { node_->setSize(value); return self(); }

    Switch&& size(wui::SwitchSize value) && { node_->setSize(value); return std::move(self()); }
    Switch& labelPosition(wui::SwitchLabelPosition value) & { node_->setLabelPosition(value); return self(); }

    Switch&& labelPosition(wui::SwitchLabelPosition value) && { node_->setLabelPosition(value); return std::move(self()); }
    Switch& required(bool value = true) & { node_->setRequired(value); return self(); }

    Switch&& required(bool value = true) && { node_->setRequired(value); return std::move(self()); }
    Switch& enabled(bool value) & { node_->setEnabled(value); return self(); }

    Switch&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class Slider : public BuilderBase<Slider, wui::SliderNode> {
public:
    Slider(float minimum = 0.0f, float maximum = 100.0f, float value = 0.0f) : BuilderBase(minimum, maximum, value) {}
    Slider& value(float value) & { node_->setValue(value); return self(); }

    Slider&& value(float value) && { node_->setValue(value); return std::move(self()); }
    Slider& step(float value) & { node_->setStep(value); return self(); }

    Slider&& step(float value) && { node_->setStep(value); return std::move(self()); }
    Slider& bind(wui::State<float>& state) & { node_->bind(state); return self(); }

    Slider&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
    Slider& onChange(std::function<void(float)> handler) & { node_->onChange(std::move(handler)); return self(); }

    Slider&& onChange(std::function<void(float)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Slider& size(wui::SliderSize value) & { node_->setSize(value); return self(); }

    Slider&& size(wui::SliderSize value) && { node_->setSize(value); return std::move(self()); }
    Slider& orientation(wui::SliderOrientation value) & { node_->setOrientation(value); return self(); }

    Slider&& orientation(wui::SliderOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Slider& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Slider&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Slider& enabled(bool value) & { node_->setEnabled(value); return self(); }

    Slider&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class ProgressBar : public BuilderBase<ProgressBar, wui::ProgressBarNode> {
public:
    ProgressBar(float minimum = 0.0f, float maximum = 1.0f,
                std::optional<float> value = std::nullopt)
        : BuilderBase(minimum, maximum, value) {}
    ProgressBar& value(float value) & { node_->setValue(value); return self(); }

    ProgressBar&& value(float value) && { node_->setValue(value); return std::move(self()); }
    ProgressBar& bind(wui::State<float>& state) & { node_->bind(state); return self(); }

    ProgressBar&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
    ProgressBar& indeterminate(bool value = true) & { node_->setIndeterminate(value); return self(); }

    ProgressBar&& indeterminate(bool value = true) && { node_->setIndeterminate(value); return std::move(self()); }
    ProgressBar& color(wui::ProgressBarColor value) & { node_->setColor(value); return self(); }

    ProgressBar&& color(wui::ProgressBarColor value) && { node_->setColor(value); return std::move(self()); }
    ProgressBar& shape(wui::ProgressBarShape value) & { node_->setShape(value); return self(); }

    ProgressBar&& shape(wui::ProgressBarShape value) && { node_->setShape(value); return std::move(self()); }
    ProgressBar& thickness(wui::ProgressBarThickness value) & { node_->setThickness(value); return self(); }

    ProgressBar&& thickness(wui::ProgressBarThickness value) && { node_->setThickness(value); return std::move(self()); }
    ProgressBar& motionEnabled(bool value) & { node_->setMotionEnabled(value); return self(); }

    ProgressBar&& motionEnabled(bool value) && { node_->setMotionEnabled(value); return std::move(self()); }
    ProgressBar& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    ProgressBar&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Toast : public BuilderBase<Toast, wui::ToastNode> {
public:
    Toast(std::string title = {}, std::string body = {}) : BuilderBase(std::move(title), std::move(body)) {}
    Toast& title(std::string value) & { node_->setTitle(std::move(value)); return self(); }

    Toast&& title(std::string value) && { node_->setTitle(std::move(value)); return std::move(self()); }
    Toast& body(std::string value) & { node_->setBody(std::move(value)); return self(); }

    Toast&& body(std::string value) && { node_->setBody(std::move(value)); return std::move(self()); }
    Toast& intent(wui::ToastIntent value) & { node_->setIntent(value); return self(); }

    Toast&& intent(wui::ToastIntent value) && { node_->setIntent(value); return std::move(self()); }
    Toast& position(wui::ToastPosition value) & { node_->setPosition(value); return self(); }

    Toast&& position(wui::ToastPosition value) && { node_->setPosition(value); return std::move(self()); }
    Toast& action(std::string label, std::function<void()> handler) & { node_->setAction(std::move(label), std::move(handler)); return self(); }

    Toast&& action(std::string label, std::function<void()> handler) && { node_->setAction(std::move(label), std::move(handler)); return std::move(self()); }
    Toast& timeout(std::chrono::milliseconds value) & { node_->setTimeout(value); return self(); }

    Toast&& timeout(std::chrono::milliseconds value) && { node_->setTimeout(value); return std::move(self()); }
};

class Spinner : public BuilderBase<Spinner, wui::SpinnerNode> {
public:
    explicit Spinner(std::string label = {}) : BuilderBase(std::move(label)) {}
    Spinner& label(std::string value) & { node_->setLabel(std::move(value)); return self(); }

    Spinner&& label(std::string value) && { node_->setLabel(std::move(value)); return std::move(self()); }
    Spinner& size(wui::SpinnerSize value) & { node_->setSize(value); return self(); }

    Spinner&& size(wui::SpinnerSize value) && { node_->setSize(value); return std::move(self()); }
    Spinner& labelPosition(wui::SpinnerLabelPosition value) & { node_->setLabelPosition(value); return self(); }

    Spinner&& labelPosition(wui::SpinnerLabelPosition value) && { node_->setLabelPosition(value); return std::move(self()); }
    Spinner& motionEnabled(bool value) & { node_->setMotionEnabled(value); return self(); }

    Spinner&& motionEnabled(bool value) && { node_->setMotionEnabled(value); return std::move(self()); }
};

class Divider : public BuilderBase<Divider, wui::DividerNode> {
public:
    explicit Divider(wui::DividerOrientation orientation = wui::DividerOrientation::Horizontal) : BuilderBase(orientation) {}
    Divider& thickness(float value) & { node_->setThickness(value); return self(); }

    Divider&& thickness(float value) && { node_->setThickness(value); return std::move(self()); }
    Divider& content(std::string value) & { node_->setContent(std::move(value)); return self(); }

    Divider&& content(std::string value) && { node_->setContent(std::move(value)); return std::move(self()); }
    Divider& appearance(wui::DividerAppearance value) & { node_->setAppearance(value); return self(); }

    Divider&& appearance(wui::DividerAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    Divider& contentAlignment(wui::DividerContentAlignment value) & { node_->setContentAlignment(value); return self(); }

    Divider&& contentAlignment(wui::DividerContentAlignment value) && { node_->setContentAlignment(value); return std::move(self()); }
    Divider& inset(bool value = true) & { node_->setInset(value); return self(); }

    Divider&& inset(bool value = true) && { node_->setInset(value); return std::move(self()); }
};

class Badge : public BuilderBase<Badge, wui::BadgeNode> {
public:
    explicit Badge(std::string text = {}) : BuilderBase(std::move(text)) {}
    Badge& text(std::string value) & { node_->setText(std::move(value)); return self(); }

    Badge&& text(std::string value) && { node_->setText(std::move(value)); return std::move(self()); }
    Badge& appearance(wui::BadgeAppearance value) & { node_->setAppearance(value); return self(); }

    Badge&& appearance(wui::BadgeAppearance value) && { node_->setAppearance(value); return std::move(self()); }
    Badge& color(wui::BadgeColor value) & { node_->setColor(value); return self(); }

    Badge&& color(wui::BadgeColor value) && { node_->setColor(value); return std::move(self()); }
    Badge& size(wui::BadgeSize value) & { node_->setSize(value); return self(); }

    Badge&& size(wui::BadgeSize value) && { node_->setSize(value); return std::move(self()); }
    Badge& shape(wui::BadgeShape value) & { node_->setShape(value); return self(); }

    Badge&& shape(wui::BadgeShape value) && { node_->setShape(value); return std::move(self()); }
    Badge& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Badge&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class CounterBadge : public BuilderBase<CounterBadge, wui::CounterBadgeNode> {
public:
    explicit CounterBadge(std::uint64_t count = 0) : BuilderBase(count) {}
    CounterBadge& count(std::uint64_t value) & { node_->setCount(value); return self(); }

    CounterBadge&& count(std::uint64_t value) && { node_->setCount(value); return std::move(self()); }
    CounterBadge& max(std::uint64_t value) & { node_->setMax(value); return self(); }

    CounterBadge&& max(std::uint64_t value) && { node_->setMax(value); return std::move(self()); }
    CounterBadge& showZero(bool value = true) & { node_->setShowZero(value); return self(); }

    CounterBadge&& showZero(bool value = true) && { node_->setShowZero(value); return std::move(self()); }
    CounterBadge& size(wui::BadgeSize value) & { node_->setSize(value); return self(); }

    CounterBadge&& size(wui::BadgeSize value) && { node_->setSize(value); return std::move(self()); }
    CounterBadge& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    CounterBadge&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class PresenceBadge : public BuilderBase<PresenceBadge, wui::PresenceBadgeNode> {
public:
    explicit PresenceBadge(wui::PresenceStatus status = wui::PresenceStatus::Available) : BuilderBase(status) {}
    PresenceBadge& status(wui::PresenceStatus value) & { node_->setStatus(value); return self(); }

    PresenceBadge&& status(wui::PresenceStatus value) && { node_->setStatus(value); return std::move(self()); }
    PresenceBadge& position(wui::PresenceBadgePosition value) & { node_->setPosition(value); return self(); }

    PresenceBadge&& position(wui::PresenceBadgePosition value) && { node_->setPosition(value); return std::move(self()); }
    PresenceBadge& avatarSize(float value) & { node_->setAvatarSize(value); return self(); }

    PresenceBadge&& avatarSize(float value) && { node_->setAvatarSize(value); return std::move(self()); }
    PresenceBadge& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    PresenceBadge&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Avatar : public BuilderBase<Avatar, wui::AvatarNode> {
public:
    explicit Avatar(std::string name = {}, wui::AvatarSize size = wui::AvatarSize::Size32)
        : BuilderBase(std::move(name), size) {}
    Avatar& initials(std::string value) & { node_->setInitials(std::move(value)); return self(); }

    Avatar&& initials(std::string value) && { node_->setInitials(std::move(value)); return std::move(self()); }
    Avatar& image(wui::ImageSource source) & { node_->setImage(std::move(source)); return self(); }

    Avatar&& image(wui::ImageSource source) && { node_->setImage(std::move(source)); return std::move(self()); }
    Avatar& size(wui::AvatarSize value) & { node_->setSize(value); return self(); }

    Avatar&& size(wui::AvatarSize value) && { node_->setSize(value); return std::move(self()); }
    Avatar& shape(wui::AvatarShape value) & { node_->setShape(value); return self(); }

    Avatar&& shape(wui::AvatarShape value) && { node_->setShape(value); return std::move(self()); }
    Avatar& color(wui::AvatarColor value) & { node_->setColor(value); return self(); }

    Avatar&& color(wui::AvatarColor value) && { node_->setColor(value); return std::move(self()); }
    Avatar& active(bool value = true) & { node_->setActive(value); return self(); }

    Avatar&& active(bool value = true) && { node_->setActive(value); return std::move(self()); }
    Avatar& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Avatar&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class AvatarGroup : public ContainerBuilderBase<AvatarGroup, wui::AvatarGroupNode> {
public:
    AvatarGroup() : ContainerBuilderBase() {}
    AvatarGroup& avatar(std::string name, wui::AvatarSize size = wui::AvatarSize::Size32) &
    { node_->addAvatar(std::move(name), size); return self(); }

    AvatarGroup&& avatar(std::string name, wui::AvatarSize size = wui::AvatarSize::Size32) &&
    { node_->addAvatar(std::move(name), size); return std::move(self()); }
    AvatarGroup& maxVisible(std::size_t value) & { node_->setMaxVisible(value); return self(); }

    AvatarGroup&& maxVisible(std::size_t value) && { node_->setMaxVisible(value); return std::move(self()); }
    AvatarGroup& layout(wui::AvatarGroupLayout value) & { node_->setGroupLayout(value); return self(); }

    AvatarGroup&& layout(wui::AvatarGroupLayout value) && { node_->setGroupLayout(value); return std::move(self()); }
    AvatarGroup& size(wui::AvatarSize value) & { node_->setSize(value); return self(); }

    AvatarGroup&& size(wui::AvatarSize value) && { node_->setSize(value); return std::move(self()); }
    AvatarGroup& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    AvatarGroup&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Persona : public BuilderBase<Persona, wui::PersonaNode> {
public:
    explicit Persona(std::string name = {}, wui::PersonaSize size = wui::PersonaSize::Medium)
        : BuilderBase(std::move(name), size) {}
    Persona& primaryText(std::string value) & { node_->setPrimaryText(std::move(value)); return self(); }

    Persona&& primaryText(std::string value) && { node_->setPrimaryText(std::move(value)); return std::move(self()); }
    Persona& secondaryText(std::string value) & { node_->setSecondaryText(std::move(value)); return self(); }

    Persona&& secondaryText(std::string value) && { node_->setSecondaryText(std::move(value)); return std::move(self()); }
    Persona& tertiaryText(std::string value) & { node_->setTertiaryText(std::move(value)); return self(); }

    Persona&& tertiaryText(std::string value) && { node_->setTertiaryText(std::move(value)); return std::move(self()); }
    Persona& quaternaryText(std::string value) & { node_->setQuaternaryText(std::move(value)); return self(); }

    Persona&& quaternaryText(std::string value) && { node_->setQuaternaryText(std::move(value)); return std::move(self()); }
    Persona& size(wui::PersonaSize value) & { node_->setSize(value); return self(); }

    Persona&& size(wui::PersonaSize value) && { node_->setSize(value); return std::move(self()); }
    Persona& avatarColor(wui::AvatarColor value) & { node_->setAvatarColor(value); return self(); }

    Persona&& avatarColor(wui::AvatarColor value) && { node_->setAvatarColor(value); return std::move(self()); }
    Persona& avatarShape(wui::AvatarShape value) & { node_->setAvatarShape(value); return self(); }

    Persona&& avatarShape(wui::AvatarShape value) && { node_->setAvatarShape(value); return std::move(self()); }
    Persona& avatarImage(wui::ImageSource value) & { node_->setAvatarImage(std::move(value)); return self(); }

    Persona&& avatarImage(wui::ImageSource value) && { node_->setAvatarImage(std::move(value)); return std::move(self()); }
    Persona& presence(wui::PresenceStatus value) & { node_->setPresence(value); return self(); }

    Persona&& presence(wui::PresenceStatus value) && { node_->setPresence(value); return std::move(self()); }
    Persona& presenceOnly(bool value = true) & { node_->setPresenceOnly(value); return self(); }

    Persona&& presenceOnly(bool value = true) && { node_->setPresenceOnly(value); return std::move(self()); }
    Persona& textPosition(wui::PersonaTextPosition value) & { node_->setTextPosition(value); return self(); }

    Persona&& textPosition(wui::PersonaTextPosition value) && { node_->setTextPosition(value); return std::move(self()); }
    Persona& textAlignment(wui::PersonaTextAlignment value) & { node_->setTextAlignment(value); return self(); }

    Persona&& textAlignment(wui::PersonaTextAlignment value) && { node_->setTextAlignment(value); return std::move(self()); }
    Persona& onClick(wui::PersonaNode::ClickHandler handler) & { node_->onClick(std::move(handler)); return self(); }

    Persona&& onClick(wui::PersonaNode::ClickHandler handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
    Persona& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Persona&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Calendar : public BuilderBase<Calendar, wui::CalendarNode> {
public:
    Calendar() : BuilderBase() {}
    Calendar& displayedMonth(wui::CivilDate value) & { node_->setDisplayedMonth(value); return self(); }

    Calendar&& displayedMonth(wui::CivilDate value) && { node_->setDisplayedMonth(value); return std::move(self()); }
    Calendar& selectedDate(std::optional<wui::CivilDate> value) & { node_->setSelectedDate(value); return self(); }

    Calendar&& selectedDate(std::optional<wui::CivilDate> value) && { node_->setSelectedDate(value); return std::move(self()); }
    Calendar& selectionMode(wui::CalendarSelectionMode value) & { node_->setSelectionMode(value); return self(); }

    Calendar&& selectionMode(wui::CalendarSelectionMode value) && { node_->setSelectionMode(value); return std::move(self()); }
    Calendar& minimumDate(std::optional<wui::CivilDate> value) & { node_->minimumDate(value); return self(); }

    Calendar&& minimumDate(std::optional<wui::CivilDate> value) && { node_->minimumDate(value); return std::move(self()); }
    Calendar& maximumDate(std::optional<wui::CivilDate> value) & { node_->maximumDate(value); return self(); }

    Calendar&& maximumDate(std::optional<wui::CivilDate> value) && { node_->maximumDate(value); return std::move(self()); }
};

class DatePicker : public BuilderBase<DatePicker, wui::DatePickerNode> {
public:
    explicit DatePicker(std::string placeholder = "Select a date") : BuilderBase(std::move(placeholder)) {}
    DatePicker& value(std::optional<wui::CivilDate> value) & { node_->setValue(value); return self(); }

    DatePicker&& value(std::optional<wui::CivilDate> value) && { node_->setValue(value); return std::move(self()); }
    DatePicker& text(std::string value) & { node_->text(std::move(value)); return self(); }

    DatePicker&& text(std::string value) && { node_->text(std::move(value)); return std::move(self()); }
    DatePicker& overlayHost(wui::OverlayHost& host) & { node_->bindOverlayHost(host); return self(); }

    DatePicker&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
};

class TimePicker : public BuilderBase<TimePicker, wui::TimePickerNode> {
public:
    explicit TimePicker(std::string placeholder = "Select a time") : BuilderBase(std::move(placeholder)) {}
    TimePicker& value(std::optional<wui::CivilTime> value) & { node_->setValue(value); return self(); }

    TimePicker&& value(std::optional<wui::CivilTime> value) && { node_->setValue(value); return std::move(self()); }
    TimePicker& text(std::string value) & { node_->text(std::move(value)); return self(); }

    TimePicker&& text(std::string value) && { node_->text(std::move(value)); return std::move(self()); }
    TimePicker& minuteStep(int value) & { node_->minuteStep(value); return self(); }

    TimePicker&& minuteStep(int value) && { node_->minuteStep(value); return std::move(self()); }
};

class Table : public BuilderBase<Table, wui::TableNode> {
public:
    explicit Table(std::vector<wui::TableColumn> columns = {}) : BuilderBase(std::move(columns)) {}
    Table& rows(std::vector<wui::TableRow> value) & { node_->setRows(std::move(value)); return self(); }

    Table&& rows(std::vector<wui::TableRow> value) && { node_->setRows(std::move(value)); return std::move(self()); }
    Table&& rowProvider(std::size_t count, wui::TableNode::RowProvider provider, wui::TableNode::RowEnabledProvider enabled = {}) { node_->setRowProvider(count, std::move(provider), std::move(enabled)); return std::move(self()); }
    Table& maxVisibleRows(std::size_t value) & { node_->maxVisibleRows(value); return self(); }

    Table&& maxVisibleRows(std::size_t value) && { node_->maxVisibleRows(value); return std::move(self()); }
    Table& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Table&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class DataGrid : public BuilderBase<DataGrid, wui::DataGridNode> {
public:
    DataGrid() : BuilderBase() {}
    DataGrid& columns(std::vector<wui::TableColumn> value) & { node_->setColumns(std::move(value)); return self(); }

    DataGrid&& columns(std::vector<wui::TableColumn> value) && { node_->setColumns(std::move(value)); return std::move(self()); }
    DataGrid& rows(std::vector<wui::TableRow> value) & { node_->setRows(std::move(value)); return self(); }

    DataGrid&& rows(std::vector<wui::TableRow> value) && { node_->setRows(std::move(value)); return std::move(self()); }
    DataGrid&& rowProvider(std::size_t count, wui::TableNode::RowProvider provider, wui::TableNode::RowEnabledProvider enabled = {}) { node_->setRowProvider(count, std::move(provider), std::move(enabled)); return std::move(self()); }
    DataGrid& selectionMode(wui::DataGridSelectionMode value) & { node_->selectionMode(value); return self(); }

    DataGrid&& selectionMode(wui::DataGridSelectionMode value) && { node_->selectionMode(value); return std::move(self()); }
    DataGrid& selectedRows(std::vector<std::size_t> value) & { node_->selectedRows(std::move(value)); return self(); }

    DataGrid&& selectedRows(std::vector<std::size_t> value) && { node_->selectedRows(std::move(value)); return std::move(self()); }
    DataGrid& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    DataGrid&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Tree : public BuilderBase<Tree, wui::TreeNode> {
public:
    Tree() : BuilderBase() {}
    Tree& item(std::string id, std::string label) & { node_->addItem(std::move(id), std::move(label)); return self(); }

    Tree&& item(std::string id, std::string label) && { node_->addItem(std::move(id), std::move(label)); return std::move(self()); }
    Tree& maxVisibleItems(std::size_t value) & { node_->setMaxVisibleItems(value); return self(); }

    Tree&& maxVisibleItems(std::size_t value) && { node_->setMaxVisibleItems(value); return std::move(self()); }
    Tree& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Tree&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class AccordionItem : public BuilderBase<AccordionItem, wui::AccordionItemNode> {
public:
    AccordionItem(std::string header = {}, std::string body = {}) : BuilderBase(std::move(header), std::move(body)) {}
    AccordionItem& expanded(bool value = true) & { node_->setExpanded(value); return self(); }

    AccordionItem&& expanded(bool value = true) && { node_->setExpanded(value); return std::move(self()); }
    template <class Content>
    AccordionItem& content(Content&& value) & { node_->setContent(asNode(std::forward<Content>(value))); return self(); }

    template <class Content>
    AccordionItem&& content(Content&& value) && { node_->setContent(asNode(std::forward<Content>(value))); return std::move(self()); }
};

class Accordion : public ContainerBuilderBase<Accordion, wui::AccordionNode> {
public:
    Accordion() : ContainerBuilderBase() {}
    Accordion&& item(std::string header, std::string body = {}) { node_->addItem(std::move(header), std::move(body)); return std::move(self()); }
    Accordion& expandMode(wui::AccordionExpandMode value) & { node_->setExpandMode(value); return self(); }

    Accordion&& expandMode(wui::AccordionExpandMode value) && { node_->setExpandMode(value); return std::move(self()); }
    Accordion& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Accordion&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Drawer : public BuilderBase<Drawer, wui::DrawerNode> {
public:
    Drawer(std::string title = {}, std::string subtitle = {}) : BuilderBase(std::move(title), std::move(subtitle)) {}
    Drawer& type(wui::DrawerType value) & { node_->type(value); return self(); }

    Drawer&& type(wui::DrawerType value) && { node_->type(value); return std::move(self()); }
    Drawer& position(wui::DrawerPosition value) & { node_->position(value); return self(); }

    Drawer&& position(wui::DrawerPosition value) && { node_->position(value); return std::move(self()); }
    Drawer& size(wui::DrawerSize value) & { node_->size(value); return self(); }

    Drawer&& size(wui::DrawerSize value) && { node_->size(value); return std::move(self()); }
    Drawer& modal(bool value = true) & { node_->modal(value); return self(); }

    Drawer&& modal(bool value = true) && { node_->modal(value); return std::move(self()); }
    Drawer& dismissOnOutsidePress(bool value = true) & { node_->dismissOnOutsidePress(value); return self(); }

    Drawer&& dismissOnOutsidePress(bool value = true) && { node_->dismissOnOutsidePress(value); return std::move(self()); }
    Drawer& onDismiss(wui::DrawerNode::DismissHandler handler) & { node_->onDismiss(std::move(handler)); return self(); }

    Drawer&& onDismiss(wui::DrawerNode::DismissHandler handler) && { node_->onDismiss(std::move(handler)); return std::move(self()); }
    template <class Content>
    Drawer& content(Content&& value) & { node_->content(asNode(std::forward<Content>(value))); return self(); }

    template <class Content>
    Drawer&& content(Content&& value) && { node_->content(asNode(std::forward<Content>(value))); return std::move(self()); }
};

class Popover : public BuilderBase<Popover, wui::PopoverNode> {
public:
    Popover(std::string title = {}, std::string body = {}) : BuilderBase(std::move(title), std::move(body)) {}
    Popover& appearance(wui::PopoverAppearance value) & { node_->appearance(value); return self(); }

    Popover&& appearance(wui::PopoverAppearance value) && { node_->appearance(value); return std::move(self()); }
    Popover& arrow(bool value = true) & { node_->showArrow(value); return self(); }

    Popover&& arrow(bool value = true) && { node_->showArrow(value); return std::move(self()); }
};

class PopoverButton : public BuilderBase<PopoverButton, wui::PopoverButtonNode> {
public:
    explicit PopoverButton(std::string label = {}) : BuilderBase(std::move(label)) {}
    PopoverButton& overlayHost(wui::OverlayHost& host) & { node_->bindOverlayHost(host); return self(); }

    PopoverButton&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
    PopoverButton&& popover(std::string title, std::string body = {})
    { node_->popover(std::move(title), std::move(body)); return std::move(self()); }
};

class TeachingPopover : public BuilderBase<TeachingPopover, wui::TeachingPopoverNode> {
public:
    TeachingPopover(std::string title = {}, std::string body = {}) : BuilderBase(std::move(title), std::move(body)) {}
    TeachingPopover&& primaryAction(std::string label, wui::TeachingPopoverNode::ActionHandler handler = {})
    { node_->primaryAction(std::move(label), std::move(handler)); return std::move(self()); }
    TeachingPopover&& secondaryAction(std::string label, wui::TeachingPopoverNode::ActionHandler handler = {})
    { node_->secondaryAction(std::move(label), std::move(handler)); return std::move(self()); }
    TeachingPopover& step(std::string value) & { node_->stepText(std::move(value)); return self(); }

    TeachingPopover&& step(std::string value) && { node_->stepText(std::move(value)); return std::move(self()); }
};

class Toolbar : public ContainerBuilderBase<Toolbar, wui::ToolbarNode> {
public:
    Toolbar() : ContainerBuilderBase() {}
    Toolbar& item(std::string label, wui::ToolbarItemAppearance appearance = wui::ToolbarItemAppearance::Subtle) &
    { node_->addItem(std::move(label), appearance); return self(); }

    Toolbar&& item(std::string label, wui::ToolbarItemAppearance appearance = wui::ToolbarItemAppearance::Subtle) &&
    { node_->addItem(std::move(label), appearance); return std::move(self()); }
    Toolbar& orientation(wui::ToolbarOrientation value) & { node_->setOrientation(value); return self(); }

    Toolbar&& orientation(wui::ToolbarOrientation value) && { node_->setOrientation(value); return std::move(self()); }
    Toolbar& onOverflow(wui::ToolbarNode::OverflowHandler handler) & { node_->onOverflow(std::move(handler)); return self(); }

    Toolbar&& onOverflow(wui::ToolbarNode::OverflowHandler handler) && { node_->onOverflow(std::move(handler)); return std::move(self()); }
    Toolbar& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Toolbar&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class TabList : public ContainerBuilderBase<TabList, wui::TabListNode> {
public:
    TabList() : ContainerBuilderBase() {}
    TabList& tab(std::string value, std::string label, bool enabled = true) &
    { node_->addTab(std::move(value), std::move(label), enabled); return self(); }

    TabList&& tab(std::string value, std::string label, bool enabled = true) &&
    { node_->addTab(std::move(value), std::move(label), enabled); return std::move(self()); }
    TabList& value(std::string value) & { node_->setValue(std::move(value)); return self(); }

    TabList&& value(std::string value) && { node_->setValue(std::move(value)); return std::move(self()); }
    TabList& onChange(std::function<void(const std::string&)> handler) &
    { node_->onChange(std::move(handler)); return self(); }

    TabList&& onChange(std::function<void(const std::string&)> handler) &&
    { node_->onChange(std::move(handler)); return std::move(self()); }
    TabList& activationMode(wui::TabListNode::ActivationMode value) &
    { node_->setActivationMode(value); return self(); }

    TabList&& activationMode(wui::TabListNode::ActivationMode value) &&
    { node_->setActivationMode(value); return std::move(self()); }
};

class TabPanel : public ContainerBuilderBase<TabPanel, wui::TabPanelNode> {
public:
    explicit TabPanel(std::string value = {}) : ContainerBuilderBase(std::move(value)) {}
    TabPanel& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    TabPanel&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    TabPanel& tabList(wui::TabListNode& value) & { node_->setTabList(&value); return self(); }

    TabPanel&& tabList(wui::TabListNode& value) && { node_->setTabList(&value); return std::move(self()); }
    TabPanel& active(bool value = true) & { node_->setActive(value); return self(); }

    TabPanel&& active(bool value = true) && { node_->setActive(value); return std::move(self()); }
};

class Link : public BuilderBase<Link, wui::LinkNode> {
public:
    explicit Link(std::string label = {}) : BuilderBase(std::move(label)) {}
    Link& href(std::string value) & { node_->setHref(std::move(value)); return self(); }

    Link&& href(std::string value) && { node_->setHref(std::move(value)); return std::move(self()); }
    Link& onClick(std::function<void()> handler) & { node_->onInvoke(std::move(handler)); return self(); }

    Link&& onClick(std::function<void()> handler) && { node_->onInvoke(std::move(handler)); return std::move(self()); }
};

class Breadcrumb : public ContainerBuilderBase<Breadcrumb, wui::BreadcrumbNode> {
public:
    Breadcrumb() : ContainerBuilderBase() {}
    Breadcrumb& item(std::string label, bool current = false) &
    { node_->addItem(std::move(label), current); return self(); }

    Breadcrumb&& item(std::string label, bool current = false) &&
    { node_->addItem(std::move(label), current); return std::move(self()); }
    Breadcrumb& maxVisible(std::size_t value) & { node_->setMaxVisible(value); return self(); }

    Breadcrumb&& maxVisible(std::size_t value) && { node_->setMaxVisible(value); return std::move(self()); }
};

class ListBox : public BuilderBase<ListBox, wui::ListBoxNode> {
public:
    explicit ListBox(std::vector<wui::Option> options = {}) : BuilderBase(std::move(options)) {}
    ListBox& option(wui::Option value) & { node_->addOption(std::move(value)); return self(); }

    ListBox&& option(wui::Option value) && { node_->addOption(std::move(value)); return std::move(self()); }
    ListBox& selectedIndex(int value) & { node_->setSelectedIndex(value); return self(); }

    ListBox&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    ListBox& multiple(bool value = true) &
    { node_->setSelectionMode(value ? wui::ListBoxSelectionMode::Multiple : wui::ListBoxSelectionMode::Single); return self(); }

    ListBox&& multiple(bool value = true) &&
    { node_->setSelectionMode(value ? wui::ListBoxSelectionMode::Multiple : wui::ListBoxSelectionMode::Single); return std::move(self()); }
    ListBox& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    ListBox&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Combobox : public BuilderBase<Combobox, wui::ComboboxNode> {
public:
    explicit Combobox(std::string placeholder = {}) : BuilderBase(std::move(placeholder)) {}
    Combobox& option(wui::Option value) & { node_->addOption(std::move(value)); return self(); }

    Combobox&& option(wui::Option value) && { node_->addOption(std::move(value)); return std::move(self()); }
    Combobox& selectedIndex(int value) & { node_->setSelectedIndex(value); return self(); }

    Combobox&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    Combobox& selectedIndices(std::vector<int> value) & { node_->setSelectedIndices(std::move(value)); return self(); }

    Combobox&& selectedIndices(std::vector<int> value) && { node_->setSelectedIndices(std::move(value)); return std::move(self()); }
    Combobox& multiselect(bool value = true) & { node_->setMultiselect(value); return self(); }

    Combobox&& multiselect(bool value = true) && { node_->setMultiselect(value); return std::move(self()); }
    Combobox& overlayHost(wui::OverlayHost& host) & { node_->bindOverlayHost(host); return self(); }

    Combobox&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
    Combobox& onSelectionChanged(wui::ComboboxNode::SelectionHandler handler) &
    { node_->onSelectionChanged(std::move(handler)); return self(); }

    Combobox&& onSelectionChanged(wui::ComboboxNode::SelectionHandler handler) &&
    { node_->onSelectionChanged(std::move(handler)); return std::move(self()); }
    Combobox& onChange(wui::TextFieldNode::ChangeHandler handler) & { node_->onChange(std::move(handler)); return self(); }

    Combobox&& onChange(wui::TextFieldNode::ChangeHandler handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
};

class Dropdown : public BuilderBase<Dropdown, wui::DropdownNode> {
public:
    explicit Dropdown(std::string placeholder = "Select an option") : BuilderBase(std::move(placeholder)) {}
    Dropdown& option(wui::Option value) & { node_->addOption(std::move(value)); return self(); }

    Dropdown&& option(wui::Option value) && { node_->addOption(std::move(value)); return std::move(self()); }
    Dropdown& selectedIndex(int value) & { node_->setSelectedIndex(value); return self(); }

    Dropdown&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    Dropdown& selectedIndices(std::vector<int> value) & { node_->setSelectedIndices(std::move(value)); return self(); }

    Dropdown&& selectedIndices(std::vector<int> value) && { node_->setSelectedIndices(std::move(value)); return std::move(self()); }
    Dropdown& multiselect(bool value = true) & { node_->setMultiselect(value); return self(); }

    Dropdown&& multiselect(bool value = true) && { node_->setMultiselect(value); return std::move(self()); }
    Dropdown& overlayHost(wui::OverlayHost& host) & { node_->bindOverlayHost(host); return self(); }

    Dropdown&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
    Dropdown& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Dropdown&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Dropdown& onSelectionChanged(wui::DropdownNode::SelectionHandler handler) &
    { node_->onSelectionChanged(std::move(handler)); return self(); }

    Dropdown&& onSelectionChanged(wui::DropdownNode::SelectionHandler handler) &&
    { node_->onSelectionChanged(std::move(handler)); return std::move(self()); }
};

class Rating : public BuilderBase<Rating, wui::RatingNode> {
public:
    explicit Rating(float value = 0.0f, int maximum = 5) : BuilderBase(value, maximum) {}
    Rating& value(float value) & { node_->setValue(value); return self(); }

    Rating&& value(float value) && { node_->setValue(value); return std::move(self()); }
    Rating& maximum(int value) & { node_->setMaximum(value); return self(); }

    Rating&& maximum(int value) && { node_->setMaximum(value); return std::move(self()); }
    Rating& step(float value) & { node_->setStep(value); return self(); }

    Rating&& step(float value) && { node_->setStep(value); return std::move(self()); }
    Rating& color(wui::RatingColor value) & { node_->setColor(value); return self(); }

    Rating&& color(wui::RatingColor value) && { node_->setColor(value); return std::move(self()); }
    Rating& size(wui::RatingSize value) & { node_->setSize(value); return self(); }

    Rating&& size(wui::RatingSize value) && { node_->setSize(value); return std::move(self()); }
    Rating& shape(wui::RatingShape value) & { node_->setShape(value); return self(); }

    Rating&& shape(wui::RatingShape value) && { node_->setShape(value); return std::move(self()); }
    Rating& readOnly(bool value = true) & { node_->setReadOnly(value); return self(); }

    Rating&& readOnly(bool value = true) && { node_->setReadOnly(value); return std::move(self()); }
    Rating& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Rating&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Rating& itemLabel(wui::RatingNode::ItemLabelHandler handler) & { node_->setItemLabel(std::move(handler)); return self(); }

    Rating&& itemLabel(wui::RatingNode::ItemLabelHandler handler) && { node_->setItemLabel(std::move(handler)); return std::move(self()); }
    Rating& bind(wui::State<float>& state) & { node_->bind(state); return self(); }

    Rating&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
    Rating& onChange(std::function<void(float)> handler) & { node_->onChange(std::move(handler)); return self(); }

    Rating&& onChange(std::function<void(float)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Rating& enabled(bool value) & { node_->setEnabled(value); return self(); }

    Rating&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class RatingDisplay : public BuilderBase<RatingDisplay, wui::RatingDisplayNode> {
public:
    explicit RatingDisplay(std::optional<float> value = std::optional<float>{0.0f}, int maximum = 5)
        : BuilderBase(value, maximum) {}
    RatingDisplay& value(float value) & { node_->setValue(value); return self(); }

    RatingDisplay&& value(float value) && { node_->setValue(value); return std::move(self()); }
    RatingDisplay& maximum(int value) & { node_->setMaximum(value); return self(); }

    RatingDisplay&& maximum(int value) && { node_->setMaximum(value); return std::move(self()); }
    RatingDisplay& count(std::uint64_t value) & { node_->setCount(value); return self(); }

    RatingDisplay&& count(std::uint64_t value) && { node_->setCount(value); return std::move(self()); }
    RatingDisplay& compact(bool value = true) & { node_->setCompact(value); return self(); }

    RatingDisplay&& compact(bool value = true) && { node_->setCompact(value); return std::move(self()); }
    RatingDisplay& color(wui::RatingColor value) & { node_->setColor(value); return self(); }

    RatingDisplay&& color(wui::RatingColor value) && { node_->setColor(value); return std::move(self()); }
    RatingDisplay& size(wui::RatingSize value) & { node_->setSize(value); return self(); }

    RatingDisplay&& size(wui::RatingSize value) && { node_->setSize(value); return std::move(self()); }
    RatingDisplay& shape(wui::RatingShape value) & { node_->setShape(value); return self(); }

    RatingDisplay&& shape(wui::RatingShape value) && { node_->setShape(value); return std::move(self()); }
    RatingDisplay& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    RatingDisplay&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class ListView : public BuilderBase<ListView, wui::ListViewNode> {
public:
    explicit ListView(std::vector<wui::ListViewNode::Item> items = {}, int selectedIndex = -1)
        : BuilderBase(std::move(items), selectedIndex) {}
    ListView&& itemProvider(std::size_t count, wui::ListViewNode::ItemProvider provider, wui::ListViewNode::SelectableProvider selectable = {}) { node_->setItemProvider(count, std::move(provider), std::move(selectable)); return std::move(self()); }
    ListView& selectedIndex(int value) & { node_->setSelectedIndex(value); return self(); }

    ListView&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    ListView& bind(wui::State<int>& state) & { node_->bind(state); return self(); }

    ListView&& bind(wui::State<int>& state) && { node_->bind(state); return std::move(self()); }
    ListView& onSelectionChanged(std::function<void(int)> handler) & { node_->onSelectionChanged(std::move(handler)); return self(); }

    ListView&& onSelectionChanged(std::function<void(int)> handler) && { node_->onSelectionChanged(std::move(handler)); return std::move(self()); }
};

class IconButton : public BuilderBase<IconButton, wui::IconButtonNode> {
public:
    explicit IconButton(std::string icon = {}, std::string accessibleLabel = {}) : BuilderBase(std::move(icon), std::move(accessibleLabel)) {}
    explicit IconButton(wui::IconName icon, std::string accessibleLabel = {}) : BuilderBase(icon, std::move(accessibleLabel)) {}
    IconButton& icon(wui::IconName value) & { node_->setIcon(value); return self(); }

    IconButton&& icon(wui::IconName value) && { node_->setIcon(value); return std::move(self()); }
    IconButton& iconStyle(wui::IconStyle value) & { node_->setIconStyle(value); return self(); }

    IconButton&& iconStyle(wui::IconStyle value) && { node_->setIconStyle(value); return std::move(self()); }
    IconButton& checked(bool value) & { node_->setChecked(value); return self(); }

    IconButton&& checked(bool value) && { node_->setChecked(value); return std::move(self()); }
    IconButton& onClick(std::function<void()> handler) & { node_->onClick(std::move(handler)); return self(); }

    IconButton&& onClick(std::function<void()> handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
};

class MenuButton : public BuilderBase<MenuButton, wui::MenuButtonNode> {
public:
    explicit MenuButton(std::string label = {}) : BuilderBase(std::move(label)) {}
    MenuButton& item(wui::MenuItem value) & { node_->addItem(std::move(value)); return self(); }

    MenuButton&& item(wui::MenuItem value) && { node_->addItem(std::move(value)); return std::move(self()); }
    MenuButton& overlayHost(wui::OverlayHost& value) & { node_->bindOverlayHost(value); return self(); }

    MenuButton&& overlayHost(wui::OverlayHost& value) && { node_->bindOverlayHost(value); return std::move(self()); }
};

class SplitButton : public BuilderBase<SplitButton, wui::SplitButtonNode> {
public:
    explicit SplitButton(std::string label = {}) : BuilderBase(std::move(label)) {}
    SplitButton& onClick(std::function<void()> handler) & { node_->onClick(std::move(handler)); return self(); }

    SplitButton&& onClick(std::function<void()> handler) && { node_->onClick(std::move(handler)); return std::move(self()); }
    SplitButton& item(wui::MenuItem value) & { node_->addItem(std::move(value)); return self(); }

    SplitButton&& item(wui::MenuItem value) && { node_->addItem(std::move(value)); return std::move(self()); }
    SplitButton& overlayHost(wui::OverlayHost& value) & { node_->bindOverlayHost(value); return self(); }

    SplitButton&& overlayHost(wui::OverlayHost& value) && { node_->bindOverlayHost(value); return std::move(self()); }
};

class SearchField : public BuilderBase<SearchField, wui::SearchFieldNode> {
public:
    explicit SearchField(std::string placeholder = "Search") : BuilderBase(std::move(placeholder)) {}
    SearchField& query(std::string value) & { node_->query(std::move(value)); return self(); }

    SearchField&& query(std::string value) && { node_->query(std::move(value)); return std::move(self()); }
    SearchField& onChange(wui::TextFieldNode::ChangeHandler handler) & { node_->onQueryChange(std::move(handler)); return self(); }

    SearchField&& onChange(wui::TextFieldNode::ChangeHandler handler) && { node_->onQueryChange(std::move(handler)); return std::move(self()); }
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

class ScrollView : public ContainerBuilderBase<ScrollView, wui::ScrollViewNode> {
public:
    ScrollView() : ContainerBuilderBase() {}

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

class Dialog : public BuilderBase<Dialog, wui::DialogNode> {
public:
    Dialog() : BuilderBase() {}

    Dialog& maxWidth(float width) &
    {
        node_->setMaxWidth(width);
        return self();
    }

    Dialog&& maxWidth(float width) &&
    {
        node_->setMaxWidth(width);
        return std::move(self());
    }

    Dialog& dismissOnBackdrop(bool enabled = true) &
    {
        node_->setBackdropDismissEnabled(enabled);
        return self();
    }

    Dialog&& dismissOnBackdrop(bool enabled = true) &&
    {
        node_->setBackdropDismissEnabled(enabled);
        return std::move(self());
    }

    Dialog& onDismiss(std::function<void()> handler) &
    {
        node_->onDismiss(std::move(handler));
        return self();
    }

    Dialog&& onDismiss(std::function<void()> handler) &&
    {
        node_->onDismiss(std::move(handler));
        return std::move(self());
    }

    template <class Content>
    Dialog& content(Content&& value) &
    {
        node_->content(asNode(std::forward<Content>(value)));
        return self();
    }

    template <class Content>
    Dialog&& content(Content&& value) &&
    {
        node_->content(asNode(std::forward<Content>(value)));
        return std::move(self());
    }

    [[nodiscard]] std::unique_ptr<wui::DialogNode> build() &&
    {
        return node_.take();
    }

    // Dialogs are shown through UiWindow::showDialog(), which intentionally
    // accepts the concrete modal type so it can manage focus restoration.
};

// Structural control: mount `then(...)` only while `state` is true.
class If : public BuilderBase<If, wui::IfNode> {
public:
    explicit If(wui::State<bool>& state)
        : BuilderBase()
        , state_(state)
    {
    }

    // `factory` returns a builder (or unique_ptr<Node>) for the mounted subtree.
    template <class Factory>
    If& then(Factory factory) &
    {
        wui::IfNode* raw = node_.get();
        raw->setFactory([factory = std::move(factory)]() mutable -> std::unique_ptr<wui::Node> {
            return asNode(factory());
        });
        raw->setVisible(state_.get());
        wui::State<bool> state = state_;
        struct Subscription { std::size_t id{0}; bool active{false}; };
        auto alive = std::make_shared<bool>(true);
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, state, alive, subscription] {
            raw->setVisible(state.get());
            if (subscription->active) {
                return;
            }
            subscription->id = state.subscribe([raw, state, weakAlive = std::weak_ptr<bool>(alive)](const bool&) {
                wui::scheduleStructuralUpdate(raw, [raw, state, weakAlive] {
                    const auto guard = weakAlive.lock();
                    if (guard && *guard) {
                        raw->setVisible(state.get());
                    }
                });
            });
            subscription->active = true;
        };
        auto disconnect = [state, subscription] {
            if (subscription->active) {
                state.unsubscribe(subscription->id);
                subscription->active = false;
            }
        };
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown([disconnect, alive] { *alive = false; disconnect(); });
        return self();
    }

    template <class Factory>
    If&& then(Factory factory) &&
    {
        wui::IfNode* raw = node_.get();
        raw->setFactory([factory = std::move(factory)]() mutable -> std::unique_ptr<wui::Node> {
            return asNode(factory());
        });
        raw->setVisible(state_.get());
        wui::State<bool> state = state_;
        struct Subscription { std::size_t id{0}; bool active{false}; };
        auto alive = std::make_shared<bool>(true);
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, state, alive, subscription] {
            raw->setVisible(state.get());
            if (subscription->active) {
                return;
            }
            subscription->id = state.subscribe([raw, state, weakAlive = std::weak_ptr<bool>(alive)](const bool&) {
                wui::scheduleStructuralUpdate(raw, [raw, state, weakAlive] {
                    const auto guard = weakAlive.lock();
                    if (guard && *guard) {
                        raw->setVisible(state.get());
                    }
                });
            });
            subscription->active = true;
        };
        auto disconnect = [state, subscription] {
            if (subscription->active) {
                state.unsubscribe(subscription->id);
                subscription->active = false;
            }
        };
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown([disconnect, alive] { *alive = false; disconnect(); });
        return std::move(self());
    }

private:
    wui::State<bool> state_;
};

// Structural control: (re)generate a list of children from `items`.
// Layout direction is configurable (vertical by default, horizontal via
// `.direction(ForEachDirection::Horizontal)`).
template <class T>
class ForEach : public BuilderBase<ForEach<T>, wui::ForEachNode> {
public:
    template <class ItemBuilder>
    ForEach(wui::State<std::vector<T>>& items, ItemBuilder itemBuilder)
        : BuilderBase<ForEach<T>, wui::ForEachNode>()
    {
        wui::ForEachNode* raw = this->node_.get();
        wui::State<std::vector<T>> state = items;
        auto rebuild = [raw, state, itemBuilder]() {
            raw->clearChildren();
            for (const auto& item : state.get()) {
                raw->appendChild(asNode(itemBuilder(item)));
            }
        };
        rebuild();
        struct Subscription { std::size_t id{0}; bool active{false}; };
        auto alive = std::make_shared<bool>(true);
        auto subscription = std::make_shared<Subscription>();
        auto connect = [raw, state, rebuild, alive, subscription] {
            rebuild();
            if (subscription->active) {
                return;
            }
            subscription->id = state.subscribe([raw, rebuild, weakAlive = std::weak_ptr<bool>(alive)](const std::vector<T>&) {
                wui::scheduleStructuralUpdate(raw, [rebuild, weakAlive] {
                    const auto guard = weakAlive.lock();
                    if (guard && *guard) {
                        rebuild();
                    }
                });
            });
            subscription->active = true;
        };
        auto disconnect = [state, subscription] {
            if (subscription->active) {
                state.unsubscribe(subscription->id);
                subscription->active = false;
            }
        };
        connect();
        raw->addAttachCallback(connect);
        raw->addDetachCallback(disconnect);
        raw->addTeardown([disconnect, alive] { *alive = false; disconnect(); });
    }

    ForEach<T>& direction(ForEachDirection dir) &
    {
        this->node_->setDirection(dir);
        return this->self();
    }

    ForEach<T>&& direction(ForEachDirection dir) &&
    {
        this->node_->setDirection(dir);
        return std::move(this->self());
    }

    ForEach<T>& gap(float gap) &
    {
        this->node_->setGap(gap);
        return this->self();
    }

    ForEach<T>&& gap(float gap) &&
    {
        this->node_->setGap(gap);
        return std::move(this->self());
    }

    ForEach<T>& padding(float all) &
    {
        this->node_->setPadding(InsetsF{all, all, all, all});
        return this->self();
    }

    ForEach<T>&& padding(float all) &&
    {
        this->node_->setPadding(InsetsF{all, all, all, all});
        return std::move(this->self());
    }

    ForEach<T>& padding(InsetsF insets) &
    {
        this->node_->setPadding(insets);
        return this->self();
    }

    ForEach<T>&& padding(InsetsF insets) &&
    {
        this->node_->setPadding(insets);
        return std::move(this->self());
    }

    ForEach<T>& align(Alignment align) &
    {
        this->node_->setAlign(align);
        return this->self();
    }

    ForEach<T>&& align(Alignment align) &&
    {
        this->node_->setAlign(align);
        return std::move(this->self());
    }
};

// A keyed alternative to ForEach for interactive collections. Unchanged keys
// retain their Nodes (and therefore focus, callbacks, and transient control
// state); only inserted, removed, or value-changed entries are materialised.
// Keys must be stable and unique within the supplied State.
template <class T>
class KeyedForEach : public BuilderBase<KeyedForEach<T>, wui::ForEachNode> {
public:
    template <class KeyProvider, class ItemBuilder>
    KeyedForEach(wui::State<std::vector<T>>& items, KeyProvider keyProvider, ItemBuilder itemBuilder)
        : BuilderBase<KeyedForEach<T>, wui::ForEachNode>()
    {
        using NodeFactory = std::function<std::unique_ptr<Node>(const T&)>;
        using KeyFactory = std::function<std::string(const T&)>;

        struct Entry {
            std::string key;
            T value;
        };
        struct Reconciler {
            explicit Reconciler(wui::State<std::vector<T>> source)
                : state(std::move(source))
            {
            }

            wui::ForEachNode* raw{nullptr};
            wui::State<std::vector<T>> state;
            KeyFactory keyFor;
            NodeFactory build;
            std::vector<Entry> entries;
            bool reconciling{false};
            bool pending{false};

            void reconcile()
            {
                if (reconciling) {
                    pending = true;
                    return;
                }
                reconciling = true;
                do {
                    pending = false;
                    reconcileOnce();
                } while (pending);
                reconciling = false;
            }

            void reconcileOnce()
            {
                std::vector<Entry> desired;
                desired.reserve(state.get().size());
                for (const T& item : state.get()) {
                    std::string key = keyFor(item);
                    if (key.empty()) key = std::to_string(desired.size());
                    const auto duplicate = std::find_if(desired.begin(), desired.end(), [&key](const Entry& entry) {
                        return entry.key == key;
                    });
                    if (duplicate != desired.end()) key += "#" + std::to_string(desired.size());
                    desired.push_back({std::move(key), item});
                }

                // Destroy only removed or changed rows. A changed value keeps
                // its key but receives a fresh row so static Text/Button
                // properties stay truthful after an edit.
                for (std::size_t index = entries.size(); index > 0; --index) {
                    const Entry& previous = entries[index - 1];
                    const auto next = std::find_if(desired.begin(), desired.end(), [&previous](const Entry& entry) {
                        return entry.key == previous.key;
                    });
                    if (next == desired.end() || next->value != previous.value) {
                        (void)raw->removeChild(index - 1);
                        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index - 1));
                    }
                }

                // Reorder retained rows without detach/reattach, then build
                // only new rows. This makes appending, deleting and filtering
                // proportional to the changed items instead of the list size.
                for (std::size_t index = 0; index < desired.size(); ++index) {
                    const Entry& next = desired[index];
                    const auto existing = std::find_if(entries.begin(), entries.end(), [&next](const Entry& entry) {
                        return entry.key == next.key;
                    });
                    if (existing == entries.end()) {
                        raw->insertChild(index, build(next.value));
                        entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(index), next);
                        continue;
                    }
                    const std::size_t current = static_cast<std::size_t>(existing - entries.begin());
                    if (current != index) {
                        raw->moveChild(current, index);
                        Entry retained = std::move(entries[current]);
                        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(current));
                        entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(index), std::move(retained));
                    }
                }
            }
        };

        auto reconciler = std::make_shared<Reconciler>(items);
        reconciler->raw = this->node_.get();
        reconciler->keyFor = KeyFactory(std::move(keyProvider));
        reconciler->build = [itemBuilder = std::move(itemBuilder)](const T& item) mutable {
            return asNode(itemBuilder(item));
        };
        reconciler->reconcile();

        struct Subscription { std::size_t id{0}; bool active{false}; };
        auto alive = std::make_shared<bool>(true);
        auto subscription = std::make_shared<Subscription>();
        auto connect = [reconciler, alive, subscription] {
            reconciler->reconcile();
            if (subscription->active) return;
            subscription->id = reconciler->state.subscribe([reconciler, weakAlive = std::weak_ptr<bool>(alive)](const std::vector<T>&) {
                wui::scheduleStructuralUpdate(reconciler->raw, [reconciler, weakAlive] {
                    const auto guard = weakAlive.lock();
                    if (guard && *guard) reconciler->reconcile();
                });
            });
            subscription->active = true;
        };
        auto disconnect = [reconciler, subscription] {
            if (!subscription->active) return;
            reconciler->state.unsubscribe(subscription->id);
            subscription->active = false;
        };
        connect();
        this->node_->addAttachCallback(connect);
        this->node_->addDetachCallback(disconnect);
        this->node_->addTeardown([disconnect, alive] { *alive = false; disconnect(); });
    }

    KeyedForEach<T>& direction(ForEachDirection dir) & { this->node_->setDirection(dir); return this->self(); }

    KeyedForEach<T>&& direction(ForEachDirection dir) && { this->node_->setDirection(dir); return std::move(this->self()); }
    KeyedForEach<T>& gap(float gap) & { this->node_->setGap(gap); return this->self(); }

    KeyedForEach<T>&& gap(float gap) && { this->node_->setGap(gap); return std::move(this->self()); }
    KeyedForEach<T>& padding(float all) & { this->node_->setPadding(InsetsF{all, all, all, all}); return this->self(); }

    KeyedForEach<T>&& padding(float all) && { this->node_->setPadding(InsetsF{all, all, all, all}); return std::move(this->self()); }
    KeyedForEach<T>& padding(InsetsF insets) & { this->node_->setPadding(insets); return this->self(); }

    KeyedForEach<T>&& padding(InsetsF insets) && { this->node_->setPadding(insets); return std::move(this->self()); }
    KeyedForEach<T>& align(Alignment align) & { this->node_->setAlign(align); return this->self(); }

    KeyedForEach<T>&& align(Alignment align) && { this->node_->setAlign(align); return std::move(this->self()); }
};

} // namespace wui
