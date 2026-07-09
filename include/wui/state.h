#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

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

// A lightweight derived value (WHATSUI_ARCHITECTURE §11.4). Recomputes from an
// explicit list of source States when any of them changes, and is itself
// observable (get/subscribe), so it can feed Text().bind or another Computed.
// No automatic dependency tracking: you name the sources.
template <typename T>
class Computed {
public:
    using Callback = std::function<void(const T&)>;
    using SubscriptionId = std::size_t;

    template <class Compute, class... Sources>
    explicit Computed(Compute compute, Sources&... sources)
        : compute_(std::move(compute))
    {
        value_ = compute_();
        const int expand[] = {0, (observe(sources), 0)...};
        (void)expand;
    }

    ~Computed()
    {
        for (auto& unsubscribe : unsubscribers_) {
            if (unsubscribe) {
                unsubscribe();
            }
        }
    }

    Computed(const Computed&) = delete;
    Computed& operator=(const Computed&) = delete;

    [[nodiscard]] const T& get() const noexcept
    {
        return value_;
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
    template <class Source>
    void observe(Source& source)
    {
        const auto id = source.subscribe([this](const auto&) { recompute(); });
        unsubscribers_.push_back([&source, id] { source.unsubscribe(id); });
    }

    void recompute()
    {
        T next = compute_();
        if (next == value_) {
            return;
        }
        value_ = std::move(next);
        for (const auto& [id, callback] : observers_) {
            (void)id;
            callback(value_);
        }
    }

    std::function<T()> compute_;
    T value_{};
    SubscriptionId nextId_{1};
    std::unordered_map<SubscriptionId, Callback> observers_;
    std::vector<std::function<void()>> unsubscribers_;
};

} // namespace wui
