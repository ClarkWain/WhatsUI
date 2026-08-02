#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "wui/view.h"

namespace wui {

namespace detail {

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

    Self& automationId(std::string id) &
    {
        node_->setAutomationId(std::move(id));
        return self();
    }

    Self&& automationId(std::string id) &&
    {
        node_->setAutomationId(std::move(id));
        return std::move(self());
    }

    Self& debugName(std::string name) &
    {
        node_->setDebugName(std::move(name));
        return self();
    }

    Self&& debugName(std::string name) &&
    {
        node_->setDebugName(std::move(name));
        return std::move(self());
    }

    [[nodiscard]] bool empty() const noexcept { return node_.empty(); }

    [[nodiscard]] NodeT* node() & noexcept { return node_.raw(); }
    [[nodiscard]] const NodeT* node() const & noexcept { return node_.raw(); }
    NodeT* node() && = delete;
    const NodeT* node() const && = delete;

    [[nodiscard]] std::unique_ptr<NodeT> build() && { return node_.take(); }

protected:
    [[nodiscard]] Self& self() & noexcept { return static_cast<Self&>(*this); }
    [[nodiscard]] Self&& self() && noexcept { return std::move(static_cast<Self&>(*this)); }

    detail::BuilderNodeOwner<NodeT> node_;
};

namespace detail {

template <class AllowedNodeT, class Value>
inline constexpr bool isTypedNodeLike =
    std::is_base_of_v<
        AllowedNodeT,
        typename ViewNodeType<std::decay_t<Value>>::type>
    || std::is_same_v<
        Node,
        typename ViewNodeType<std::decay_t<Value>>::type>;

template <class AllowedNodeT, class... Values>
struct AreTypedNodeLike
    : std::conjunction<
          std::bool_constant<isTypedNodeLike<AllowedNodeT, Values>>...> {
};

} // namespace detail

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
        (nodes.push_back(detail::materialize(
             std::forward<Children>(items))), ...);
        this->node_->appendChildren(std::move(nodes));
    }
};

template <class Self, class NodeT>
class SingleContentBuilderBase : public BuilderBase<Self, NodeT> {
public:
    using BuilderBase<Self, NodeT>::BuilderBase;

    template <class Content>
    Self& content(Content&& value) &
    {
        setContent(std::forward<Content>(value));
        return this->self();
    }

    template <class Content>
    Self&& content(Content&& value) &&
    {
        setContent(std::forward<Content>(value));
        return std::move(this->self());
    }

private:
    template <class Content>
    void setContent(Content&& value)
    {
        (void)this->node_.get();
        this->node_->content(detail::materialize(
            std::forward<Content>(value)));
    }
};

template <class Self, class NodeT>
class AccessibleBuilderMixin {
public:
    Self& accessibleLabel(std::string value) &
    {
        mixinSelf().node()->setAccessibleLabel(std::move(value));
        return mixinSelf();
    }

    Self&& accessibleLabel(std::string value) &&
    {
        mixinSelf().node()->setAccessibleLabel(std::move(value));
        return std::move(mixinSelf());
    }

private:
    [[nodiscard]] Self& mixinSelf() noexcept
    {
        return static_cast<Self&>(*this);
    }
};

template <class Self, class NodeT, class AllowedNodeT>
class TypedChildrenBuilderBase : public BuilderBase<Self, NodeT> {
public:
    using BuilderBase<Self, NodeT>::BuilderBase;

    template <
        class... Children,
        std::enable_if_t<
            detail::AreTypedNodeLike<AllowedNodeT, Children...>::value,
            int> = 0>
    Self& children(Children&&... items) &
    {
        appendChildren(std::forward<Children>(items)...);
        return this->self();
    }

    template <
        class... Children,
        std::enable_if_t<
            detail::AreTypedNodeLike<AllowedNodeT, Children...>::value,
            int> = 0>
    Self&& children(Children&&... items) &&
    {
        appendChildren(std::forward<Children>(items)...);
        return std::move(this->self());
    }

private:
    template <class... Children>
    void appendChildren(Children&&... items)
    {
        (void)this->node_.get();
        std::vector<NodePtr> nodes;
        nodes.reserve(sizeof...(Children));
        (nodes.push_back(detail::materialize(
             std::forward<Children>(items))), ...);
        this->node_->appendChildren(std::move(nodes));
    }
};

} // namespace wui
