#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "wui/thread_check.h"
#include "wui/ui_context.h"

namespace wui {

enum class StatePostResult {
    Scheduled,
    Coalesced,
    ContextStopped,
};

namespace detail {

template <typename T>
struct StateObserverSlot {
    using Callback = std::function<void(const T&)>;

    explicit StateObserverSlot(Callback observer)
        : callback(std::move(observer))
    {
    }

    std::atomic_bool active{true};
    Callback callback;
};

template <typename T>
class StateCore {
public:
    using Callback = typename StateObserverSlot<T>::Callback;
    using SubscriptionId = std::size_t;
    using ObserverSlot = StateObserverSlot<T>;

    StateCore() = default;

    explicit StateCore(T initialValue)
        : value(std::move(initialValue))
    {
    }

    StateCore(UiContext owner, T initialValue)
        : ui(std::move(owner))
        , value(std::move(initialValue))
    {
        if (!ui.isValid()) {
            throw std::invalid_argument(
                "State requires a valid UiContext");
        }
    }

    void requireCurrentThread() const
    {
        if (ui.isValid()) {
            ui.requireCurrentThread();
            return;
        }
        WUI_ASSERT_UI_THREAD();
    }

    [[nodiscard]] SubscriptionId addObserver(Callback callback)
    {
        requireCurrentThread();
        if (!callback) {
            throw std::invalid_argument(
                "State observer callback must not be empty");
        }
        const auto id = nextObserverId++;
        observers.emplace(
            id, std::make_shared<ObserverSlot>(std::move(callback)));
        return id;
    }

    [[nodiscard]] std::pair<SubscriptionId, std::shared_ptr<ObserverSlot>>
    addOwnedObserver(Callback callback)
    {
        requireCurrentThread();
        if (!callback) {
            throw std::invalid_argument(
                "State observer callback must not be empty");
        }
        const auto id = nextObserverId++;
        auto slot = std::make_shared<ObserverSlot>(std::move(callback));
        observers.emplace(id, slot);
        return {id, std::move(slot)};
    }

    void removeObserver(SubscriptionId id)
    {
        requireCurrentThread();
        const auto it = observers.find(id);
        if (it != observers.end()) {
            it->second->active = false;
            observers.erase(it);
        }
    }

    [[nodiscard]] std::uint64_t nextMutationVersion() noexcept
    {
        return nextVersion.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    bool apply(std::uint64_t version, T nextValue)
    {
        requireCurrentThread();
        if (version <= appliedVersion) {
            return false;
        }
        if (value == nextValue) {
            appliedVersion = version;
            return false;
        }

        value = std::move(nextValue);
        appliedVersion = version;
        pendingNotifications.push_back(value);
        if (notifying) {
            return true;
        }

        notifying = true;
        std::exception_ptr firstFailure;
        try {
            while (!pendingNotifications.empty()) {
                T snapshot = std::move(pendingNotifications.front());
                pendingNotifications.pop_front();
                if (auto failure = notify(snapshot);
                    failure && !firstFailure) {
                    firstFailure = std::move(failure);
                }
            }
        } catch (...) {
            notifying = false;
            throw;
        }
        notifying = false;
        if (firstFailure) {
            std::rethrow_exception(firstFailure);
        }
        return true;
    }

    std::exception_ptr notify(const T& snapshot)
    {
        std::vector<SubscriptionId> ids;
        ids.reserve(observers.size());
        for (const auto& entry : observers) {
            ids.push_back(entry.first);
        }

        std::exception_ptr firstFailure;
        for (const auto id : ids) {
            const auto it = observers.find(id);
            if (it == observers.end()) {
                continue;
            }
            const auto slot = it->second;
            if (!slot->active.load(std::memory_order_acquire)) {
                continue;
            }
            try {
                slot->callback(snapshot);
            } catch (...) {
                if (!firstFailure) {
                    firstFailure = std::current_exception();
                }
            }
        }
        return firstFailure;
    }

    UiContext ui;
    T value{};
    SubscriptionId nextObserverId{1};
    std::map<SubscriptionId, std::shared_ptr<ObserverSlot>> observers;
    bool notifying{false};
    std::deque<T> pendingNotifications;
    std::atomic<std::uint64_t> nextVersion{0};
    std::uint64_t appliedVersion{0};

    struct VersionedValue {
        std::uint64_t version;
        T value;
    };

    std::mutex postMutex;
    std::optional<VersionedValue> pendingPost;
    bool dispatchScheduled{false};
};

} // namespace detail

template <typename T>
class State {
public:
    using Callback = std::function<void(const T&)>;
    using SubscriptionId = std::size_t;

    State()
        : core_(std::make_shared<detail::StateCore<T>>())
    {
    }

    explicit State(T value)
        : core_(std::make_shared<detail::StateCore<T>>(
              std::move(value)))
    {
    }

    State(UiContext context, T value)
        : core_(std::make_shared<detail::StateCore<T>>(
              std::move(context), std::move(value)))
    {
    }

    ~State() = default;

    // State is a lightweight shared handle. Copies refer to the same reactive
    // identity, allowing bindings to retain the core without retaining the
    // owning view model object itself.
    State(const State&) noexcept = default;
    State& operator=(const State&) noexcept = default;
    State(State&&) noexcept = default;
    State& operator=(State&&) noexcept = default;

    [[nodiscard]] const T& get() const
    {
        core_->requireCurrentThread();
        return core_->value;
    }

    bool set(T value) const
    {
        const auto core = core_;
        const auto version = core->nextMutationVersion();
        return core->apply(version, std::move(value));
    }

    // Thread-safe publication to this State's owning UI context. Pending
    // values coalesce, and versioning prevents an old worker result from
    // overwriting a newer synchronous UI mutation.
    [[nodiscard]] StatePostResult post(T value) const
    {
        const auto core = core_;
        if (!core->ui.isValid()) {
            throw std::logic_error(
                "State::post requires a State bound to UiContext");
        }
        if (!core->ui.isAlive()) {
            return StatePostResult::ContextStopped;
        }

        const auto version = core->nextMutationVersion();
        bool schedule = false;
        {
            std::lock_guard<std::mutex> lock(core->postMutex);
            core->pendingPost = typename detail::StateCore<T>::VersionedValue{
                version, std::move(value)};
            if (!core->dispatchScheduled) {
                core->dispatchScheduled = true;
                schedule = true;
            }
        }
        if (!schedule) {
            return StatePostResult::Coalesced;
        }

        std::weak_ptr<detail::StateCore<T>> weakCore = core;
        const auto result = core->ui.post([weakCore] {
            const auto locked = weakCore.lock();
            if (!locked) {
                return;
            }
            std::optional<typename detail::StateCore<T>::VersionedValue>
                pending;
            {
                std::lock_guard<std::mutex> lock(locked->postMutex);
                pending = std::move(locked->pendingPost);
                locked->pendingPost.reset();
                locked->dispatchScheduled = false;
            }
            if (pending) {
                locked->apply(
                    pending->version, std::move(pending->value));
            }
        });
        if (result == DispatchResult::Stopped) {
            std::lock_guard<std::mutex> lock(core->postMutex);
            core->pendingPost.reset();
            core->dispatchScheduled = false;
            return StatePostResult::ContextStopped;
        }
        return StatePostResult::Scheduled;
    }

    [[nodiscard]] SubscriptionId subscribe(Callback callback) const
    {
        return core_->addObserver(std::move(callback));
    }

    void unsubscribe(SubscriptionId id) const
    {
        core_->removeObserver(id);
    }

private:
    std::shared_ptr<detail::StateCore<T>> core_;

    template <typename U>
    friend class StateSubscription;

    template <typename U>
    friend class Binding;
};

// Move-only RAII ownership for one State observer. The subscription keeps only
// weak access to StateCore and deactivates its observer slot immediately, so
// State and Subscription may be destroyed in either order.
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
        : core_(std::move(other.core_))
        , slot_(std::move(other.slot_))
        , id_(std::exchange(other.id_, 0))
    {
    }

    StateSubscription& operator=(StateSubscription&& other) noexcept
    {
        if (this != &other) {
            reset();
            core_ = std::move(other.core_);
            slot_ = std::move(other.slot_);
            id_ = std::exchange(other.id_, 0);
        }
        return *this;
    }

    void subscribe(State<T>& state, Callback callback)
    {
        reset();
        auto observer = state.core_->addOwnedObserver(std::move(callback));
        core_ = state.core_;
        id_ = observer.first;
        slot_ = std::move(observer.second);
    }

    void reset()
    {
        auto slot = std::move(slot_);
        const auto id = std::exchange(id_, 0);
        if (!slot) {
            core_.reset();
            return;
        }

        slot->active.store(false, std::memory_order_release);
        const auto core = core_.lock();
        core_.reset();
        if (!core) {
            return;
        }

        if (!core->ui.isValid() || core->ui.isCurrentThread()) {
            core->removeObserver(id);
            return;
        }

        std::weak_ptr<detail::StateCore<T>> weakCore = core;
        (void)core->ui.post([weakCore, id] {
            if (const auto locked = weakCore.lock()) {
                locked->removeObserver(id);
            }
        });
    }

    [[nodiscard]] bool active() const noexcept
    {
        return !core_.expired() && slot_
            && slot_->active.load(std::memory_order_acquire);
    }

private:
    std::weak_ptr<detail::StateCore<T>> core_;
    std::shared_ptr<detail::StateObserverSlot<T>> slot_;
    SubscriptionId id_{0};
};

template <typename T>
class Binding {
public:
    using Getter = std::function<const T&()>;
    using Setter = std::function<void(T)>;

    explicit Binding(State<T>& state)
        : getter_([core = state.core_]() -> const T& {
            core->requireCurrentThread();
            return core->value;
        })
        , setter_([core = state.core_](T value) {
            const auto version = core->nextMutationVersion();
            core->apply(version, std::move(value));
        })
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
// observable (get/subscribe), so it can feed TextNode().bind or another Computed.
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
        WUI_ASSERT_UI_THREAD();
        return value_;
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
    template <class Source>
    void observe(Source& source)
    {
        const auto id = source.subscribe([this](const auto&) { recompute(); });
        unsubscribers_.push_back([&source, id] { source.unsubscribe(id); });
    }

    void recompute()
    {
        WUI_ASSERT_UI_THREAD();
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
