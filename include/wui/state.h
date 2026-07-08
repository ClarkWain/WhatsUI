#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>

namespace wui {

template <typename T>
class State {
public:
    using Callback = std::function<void(const T&)>;
    using SubscriptionId = std::size_t;

    State() = default;

    explicit State(T value)
        : value_(std::move(value))
    {
    }

    [[nodiscard]] const T& get() const noexcept
    {
        return value_;
    }

    bool set(T value)
    {
        if (value_ == value) {
            return false;
        }
        value_ = std::move(value);
        notify();
        return true;
    }

    [[nodiscard]] SubscriptionId subscribe(Callback callback)
    {
        const auto id = nextId_++;
        observers_.emplace(id, std::move(callback));
        return id;
    }

    void unsubscribe(SubscriptionId id)
    {
        observers_.erase(id);
    }

private:
    void notify()
    {
        for (const auto& [id, callback] : observers_) {
            (void)id;
            callback(value_);
        }
    }

    T value_{};
    SubscriptionId nextId_{1};
    std::unordered_map<SubscriptionId, Callback> observers_;
};

template <typename T>
class Binding {
public:
    using Getter = std::function<const T&()>;
    using Setter = std::function<void(T)>;

    explicit Binding(State<T>& state)
        : getter_([&state]() -> const T& { return state.get(); })
        , setter_([&state](T value) { state.set(std::move(value)); })
    {
    }

    Binding(Getter getter, Setter setter)
        : getter_(std::move(getter))
        , setter_(std::move(setter))
    {
    }

    [[nodiscard]] const T& get() const
    {
        return getter_();
    }

    void set(T value) const
    {
        setter_(std::move(value));
    }

private:
    Getter getter_;
    Setter setter_;
};

} // namespace wui
