#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "wui/node.h"

namespace wui {

class View;

namespace detail {

struct BuilderMarker {
};

template <class Value>
NodePtr materialize(Value&& value);

template <class Value>
struct UniqueNodePtr : std::false_type {
};

template <class NodeT>
struct UniqueNodePtr<std::unique_ptr<NodeT>>
    : std::bool_constant<std::is_base_of_v<Node, NodeT>> {
};

template <class Value>
inline constexpr bool isUniqueNodePtrV =
    UniqueNodePtr<std::decay_t<Value>>::value;

template <class Value>
inline constexpr bool isBuilderV =
    std::is_base_of_v<BuilderMarker, std::decay_t<Value>>;

template <class Value, class = void>
struct BodyResult {
    using type = void;
    static constexpr bool available = false;
};

template <class Value>
struct BodyResult<
    Value,
    std::void_t<decltype(std::declval<Value&>().body())>> {
    using type = decltype(std::declval<Value&>().body());
    static constexpr bool available = true;
};

template <class Value>
struct ViewNodeType;

template <class Value, class = void>
struct ViewNodeTypeDecayed {
    using type = void;
};

template <class Value>
struct ViewNodeTypeDecayed<
    Value,
    std::enable_if_t<isBuilderV<Value>>> {
    using type = typename Value::node_type;
};

template <class NodeT>
struct ViewNodeTypeDecayed<
    std::unique_ptr<NodeT>,
    std::enable_if_t<std::is_base_of_v<Node, NodeT>>> {
    using type = NodeT;
};

template <>
struct ViewNodeTypeDecayed<View, void> {
    using type = Node;
};

template <class Value>
struct ViewNodeTypeDecayed<
    Value,
    std::enable_if_t<
        !isBuilderV<Value>
        && !isUniqueNodePtrV<Value>
        && !std::is_same_v<Value, View>
        && BodyResult<Value>::available>> {
    using type = typename ViewNodeType<
        typename BodyResult<Value>::type>::type;
};

template <class Value>
struct ViewNodeType
    : ViewNodeTypeDecayed<std::decay_t<Value>> {
};

template <class Factory, class = void>
struct IsViewFactory : std::false_type {
};

template <class Factory>
struct IsViewFactory<
    Factory,
    std::void_t<std::invoke_result_t<std::decay_t<Factory>&>>>
    : std::bool_constant<
          !std::is_void_v<typename ViewNodeType<
              std::invoke_result_t<std::decay_t<Factory>&>>::type>> {
};

} // namespace detail

template <class Value>
inline constexpr bool isViewLikeV =
    !std::is_void_v<typename detail::ViewNodeType<Value>::type>;

// A move-only, one-shot description used only where a view must be stored
// despite having a runtime-dependent concrete type (routes, overlays, and
// cross-module factories). Ordinary component trees remain statically typed.
class View final {
public:
    using node_type = Node;

    template <
        class Value,
        std::enable_if_t<
            isViewLikeV<Value>
            && !std::is_same_v<std::decay_t<Value>, View>
            && std::is_constructible_v<
                std::decay_t<Value>,
                Value&&>,
            int> = 0>
    View(Value&& value)
        : storage_(std::make_unique<Model<std::decay_t<Value>>>(
              std::forward<Value>(value)))
    {
    }

    View(View&&) noexcept = default;
    View& operator=(View&&) noexcept = default;
    View(const View&) = delete;
    View& operator=(const View&) = delete;

    [[nodiscard]] bool empty() const noexcept
    {
        return storage_ == nullptr;
    }

private:
    class Storage {
    public:
        virtual ~Storage() = default;
        [[nodiscard]] virtual NodePtr takeNode() = 0;
    };

    template <class Value>
    class Model final : public Storage {
    public:
        template <class Source>
        explicit Model(Source&& value)
            : value_(std::forward<Source>(value))
        {
        }

        [[nodiscard]] NodePtr takeNode() override
        {
            return detail::materialize(std::move(value_));
        }

    private:
        Value value_;
    };

    [[nodiscard]] NodePtr takeNode()
    {
        if (!storage_) {
            throw std::logic_error("cannot consume an empty WhatsUI View");
        }
        auto storage = std::move(storage_);
        return storage->takeNode();
    }

    template <class Value>
    friend NodePtr detail::materialize(Value&& value);

    std::unique_ptr<Storage> storage_;
};

namespace detail {

template <class>
inline constexpr bool alwaysFalseV = false;

template <class Value>
NodePtr materialize(Value&& value)
{
    using Decayed = std::decay_t<Value>;
    if constexpr (isUniqueNodePtrV<Decayed>) {
        static_assert(
            !std::is_lvalue_reference_v<Value>,
            "WhatsUI node consumers require an rvalue unique_ptr");
        if (!value) {
            throw std::invalid_argument("cannot consume a null WhatsUI node");
        }
        return NodePtr(std::forward<Value>(value));
    } else if constexpr (isBuilderV<Decayed>) {
        static_assert(
            !std::is_lvalue_reference_v<Value>,
            "WhatsUI view consumers require an rvalue Builder; use std::move for a named Builder");
        return NodePtr(std::move(value).build());
    } else if constexpr (std::is_same_v<Decayed, View>) {
        static_assert(
            !std::is_lvalue_reference_v<Value>,
            "WhatsUI view consumers require an rvalue View");
        return std::move(value).takeNode();
    } else if constexpr (BodyResult<Decayed>::available) {
        auto body = value.body();
        static_assert(
            isViewLikeV<decltype(body)>,
            "A WhatsUI Component body() must return a ViewLike value");
        return materialize(std::move(body));
    } else {
        static_assert(
            alwaysFalseV<Decayed>,
            "WhatsUI expected a Builder, Component, View, or unique_ptr<Node>");
    }
}

template <class RequiredNodeT, class Value>
std::unique_ptr<RequiredNodeT> materializeAs(Value&& value)
{
    NodePtr node = materialize(std::forward<Value>(value));
    auto* required = dynamic_cast<RequiredNodeT*>(node.get());
    if (required == nullptr) {
        throw std::invalid_argument(
            "WhatsUI view produced an invalid runtime node type for this boundary");
    }
    (void)node.release();
    return std::unique_ptr<RequiredNodeT>(required);
}

} // namespace detail

} // namespace wui
