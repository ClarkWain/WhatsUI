#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <utility>
#include <vector>

#include "wui/thread_check.h"

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
        WUI_ASSERT_UI_THREAD();
        return value_;
    }

    bool set(T value)
    {
        WUI_ASSERT_UI_THREAD();
        if (value_ == value) {
            return false;
        }
        value_ = std::move(value);
        pendingNotifications_.push_back(value_);
        if (notifying_) {
            return true;
        }

        notifying_ = true;
        while (!pendingNotifications_.empty()) {
            T snapshot = std::move(pendingNotifications_.front());
            pendingNotifications_.erase(pendingNotifications_.begin());
            notify(snapshot);
        }
        notifying_ = false;
        return true;
    }

    [[nodiscard]] SubscriptionId subscribe(Callback callback)
    {
        WUI_ASSERT_UI_THREAD();
        const auto id = nextId_++;
        observers_.emplace(id, std::move(callback));
        return id;
    }

    void unsubscribe(SubscriptionId id)
    {
        WUI_ASSERT_UI_THREAD();
        observers_.erase(id);
    }

private:
    void notify(const T& value)
    {
        // Observers are allowed to unsubscribe themselves or another observer
        // from inside a callback. Iterate a stable ID snapshot and re-check
        // membership before each delivery so map mutation never invalidates
        // the active iteration.
        std::vector<SubscriptionId> ids;
        ids.reserve(observers_.size());
        for (const auto& entry : observers_) {
            ids.push_back(entry.first);
        }
        for (const auto id : ids) {
            const auto it = observers_.find(id);
            if (it != observers_.end()) {
                auto callback = it->second;
                callback(value);
            }
        }
    }

    T value_{};
    SubscriptionId nextId_{1};
    std::map<SubscriptionId, Callback> observers_;
    bool notifying_{false};
    std::vector<T> pendingNotifications_;
};

// Move-only RAII ownership for one State observer. The observed State must
// outlive this handle, matching Binding's lifetime contract. Re-subscribing
// first unregisters the previous observer, which lets widgets safely bind to
// a different State without retaining teardown work for every old source.
template <typename T>
class StateSubscription {
public:
    using Callback = typename State<T>::Callback;
    using SubscriptionId = typename State<T>::SubscriptionId;

    StateSubscription() = default;

    StateSubscription(State<T>& state, Callback callback)
    {
        subscribe(state, std::move(callback));
    }

    ~StateSubscription()
    {
        reset();
    }

    StateSubscription(const StateSubscription&) = delete;
    StateSubscription& operator=(const StateSubscription&) = delete;

    StateSubscription(StateSubscription&& other) noexcept
        : state_(std::exchange(other.state_, nullptr))
        , id_(std::exchange(other.id_, 0))
    {
    }

    StateSubscription& operator=(StateSubscription&& other) noexcept
    {
        if (this != &other) {
            reset();
            state_ = std::exchange(other.state_, nullptr);
            id_ = std::exchange(other.id_, 0);
        }
        return *this;
    }

    void subscribe(State<T>& state, Callback callback)
    {
        reset();
        const auto id = state.subscribe(std::move(callback));
        state_ = &state;
        id_ = id;
    }

    void reset()
    {
        if (state_ != nullptr) {
            state_->unsubscribe(id_);
            state_ = nullptr;
            id_ = 0;
        }
    }

    [[nodiscard]] bool active() const noexcept
    {
        return state_ != nullptr;
    }

private:
    State<T>* state_{nullptr};
    SubscriptionId id_{0};
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
        const T snapshot = value_;
        std::vector<SubscriptionId> ids;
        ids.reserve(observers_.size());
        for (const auto& entry : observers_) {
            ids.push_back(entry.first);
        }
        for (const auto id : ids) {
            const auto it = observers_.find(id);
            if (it != observers_.end()) {
                auto callback = it->second;
                callback(snapshot);
            }
        }
    }

    std::function<T()> compute_;
    T value_{};
    SubscriptionId nextId_{1};
    std::map<SubscriptionId, Callback> observers_;
    std::vector<std::function<void()>> unsubscribers_;
};

} // namespace wui
