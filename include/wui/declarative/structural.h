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

#include "wui/scheduler.h"
#include "wui/state.h"
#include "wui/structural.h"
#include "wui/widgets.h"
#include "wui/declarative/builder_base.h"

namespace wui {

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
        applyThen(std::move(factory));
        return self();
    }

    template <class Factory>
    If&& then(Factory factory) &&
    {
        applyThen(std::move(factory));
        return std::move(self());
    }

private:
    template <class Factory>
    void applyThen(Factory factory)
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
    }

    wui::State<bool> state_;
};

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

template <class T>
class KeyedForEach : public BuilderBase<KeyedForEach<T>, wui::ForEachNode> {
public:
    using ItemUpdater = std::function<void(Node&, const T&)>;

    template <class KeyProvider, class ItemBuilder>
    KeyedForEach(
        wui::State<std::vector<T>>& items,
        KeyProvider keyProvider,
        ItemBuilder itemBuilder,
        ItemUpdater itemUpdater = {})
        : BuilderBase<KeyedForEach<T>, wui::ForEachNode>()
    {
        using NodeFactory = std::function<std::unique_ptr<Node>(const T&)>;
        using KeyFactory = std::function<NodeKey(const T&)>;

        struct Entry {
            NodeKey key;
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
            ItemUpdater update;
            std::vector<Entry> entries;
            bool reconciling{false};
            bool pending{false};
            bool hasValidSnapshot{false};

            void reconcile()
            {
                if (reconciling) {
                    pending = true;
                    return;
                }
                reconciling = true;
                try {
                    do {
                        pending = false;
                        reconcileOnce();
                    } while (pending);
                    reconciling = false;
                } catch (...) {
                    reconciling = false;
                    throw;
                }
            }

            void reconcileOnce()
            {
                std::vector<Entry> desired;
                desired.reserve(state.get().size());
                std::unordered_set<std::string> keys;
                for (const T& item : state.get()) {
                    NodeKey key = keyFor(item);
                    if (key.empty()) {
                        rejectInvalidSnapshot(
                            "KeyedForEach key must not be empty");
                        return;
                    }
                    if (!keys.insert(key.value()).second) {
                        rejectInvalidSnapshot(
                            "KeyedForEach keys must be unique: " + key.value());
                        return;
                    }
                    desired.push_back({std::move(key), item});
                }

                hasValidSnapshot = true;
                std::unordered_map<std::string, std::size_t> desiredIndex;
                desiredIndex.reserve(desired.size());
                for (std::size_t index = 0; index < desired.size(); ++index) {
                    desiredIndex.emplace(desired[index].key.value(), index);
                }

                // Destroy only removed or changed rows. A changed value keeps
                // its key but receives a fresh row so static Text/Button
                // properties stay truthful after an edit.
                for (std::size_t index = entries.size(); index > 0; --index) {
                    const Entry& previous = entries[index - 1];
                    const auto nextIndex = desiredIndex.find(previous.key.value());
                    if (nextIndex == desiredIndex.end()) {
                        (void)raw->removeChild(index - 1);
                        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index - 1));
                    } else if (desired[nextIndex->second].value != previous.value) {
                        if (update) {
                            update(
                                *raw->children()[index - 1],
                                desired[nextIndex->second].value);
                            entries[index - 1].value =
                                desired[nextIndex->second].value;
                        } else {
                            (void)raw->removeChild(index - 1);
                            entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index - 1));
                        }
                    }
                }

                std::unordered_map<std::string, std::size_t> currentIndex;
                auto rebuildCurrentIndex = [&] {
                    currentIndex.clear();
                    currentIndex.reserve(entries.size());
                    for (std::size_t index = 0; index < entries.size(); ++index) {
                        currentIndex.emplace(entries[index].key.value(), index);
                    }
                };
                rebuildCurrentIndex();

                // Reorder retained rows without detach/reattach, then build
                // only new rows. This makes appending, deleting and filtering
                // proportional to the changed items instead of the list size.
                for (std::size_t index = 0; index < desired.size(); ++index) {
                    const Entry& next = desired[index];
                    const auto existing = currentIndex.find(next.key.value());
                    if (existing == currentIndex.end()) {
                        raw->insertChild(index, build(next.value));
                        entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(index), next);
                        rebuildCurrentIndex();
                        continue;
                    }
                    const std::size_t current = existing->second;
                    if (current != index) {
                        raw->moveChild(current, index);
                        Entry retained = std::move(entries[current]);
                        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(current));
                        entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(index), std::move(retained));
                        rebuildCurrentIndex();
                    }
                }
            }

            void rejectInvalidSnapshot(std::string message)
            {
                if (!hasValidSnapshot) {
                    throw std::invalid_argument(message);
                }
                raw->uiContext().reportDiagnostic({
                    UiDiagnosticCode::InvalidNodeKey,
                    std::move(message),
                    {},
                });
            }
        };

        auto reconciler = std::make_shared<Reconciler>(items);
        reconciler->raw = this->node_.get();
        reconciler->keyFor = KeyFactory(std::move(keyProvider));
        reconciler->build = [itemBuilder = std::move(itemBuilder)](const T& item) mutable {
            return asNode(itemBuilder(item));
        };
        reconciler->update = std::move(itemUpdater);
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
