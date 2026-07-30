#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "wui/internal/viewport_model.h"
#include "wui/node.h"

namespace wui::internal {

class KeyedRecycler {
public:
    using Index = std::size_t;
    using Key = std::string;
    using KeyProvider = std::function<Key(Index)>;
    using Builder = std::function<std::unique_ptr<Node>(Index, const Key&)>;

    struct Mounted {
        Index index{0};
        Key key;
        Node* node{nullptr};
    };

    KeyedRecycler()
        : keyProvider_([](Index index) { return std::to_string(index); })
    {
    }

    void setKeyProvider(KeyProvider provider)
    {
        keyProvider_ = provider ? std::move(provider) : KeyProvider([](Index index) { return std::to_string(index); });
    }

    void setBuilder(Builder builder)
    {
        builder_ = std::move(builder);
    }

    void clear(Node& owner)
    {
        if (reconciling_) {
            clearPending_ = true;
            reconcilePending_ = true;
            return;
        }
        mounted_.clear();
        owner.clearChildren();
        pool_.clear();
    }

    void reconcile(Node& owner, ViewportModel::Range desiredRange)
    {
        if (reconciling_) {
            pendingRange_ = desiredRange;
            reconcilePending_ = true;
            return;
        }

        reconciling_ = true;
        ViewportModel::Range currentRange = desiredRange;
        do {
            reconcilePending_ = false;
            reconcileOnce(owner, currentRange);
            if (clearPending_) {
                mounted_.clear();
                owner.clearChildren();
                pool_.clear();
                clearPending_ = false;
            }
            if (reconcilePending_) currentRange = pendingRange_;
        } while (reconcilePending_);
        reconciling_ = false;
    }

    [[nodiscard]] const std::vector<Mounted>& mounted() const noexcept { return mounted_; }
    [[nodiscard]] std::size_t mountedCount() const noexcept { return mounted_.size(); }
    [[nodiscard]] std::size_t pooledCount() const noexcept { return pool_.size(); }

    void forget(Node* node) noexcept
    {
        mounted_.erase(std::remove_if(mounted_.begin(), mounted_.end(), [node](const Mounted& mounted) {
            return mounted.node == node;
        }), mounted_.end());
    }

private:
    struct Pooled {
        Key key;
        std::unique_ptr<Node> node;
    };

    [[nodiscard]] Key keyFor(Index index) const
    {
        return keyProvider_ ? keyProvider_(index) : std::to_string(index);
    }

    void reconcileOnce(Node& owner, ViewportModel::Range desiredRange)
    {
        std::vector<std::pair<Index, Key>> desired;
        desired.reserve(desiredRange.size());
        std::unordered_set<Key> desiredKeys;
        for (Index index = desiredRange.first; index < desiredRange.last; ++index) {
            Key key = keyFor(index);
            if (!desiredKeys.insert(key).second) {
                key += "#" + std::to_string(index);
                desiredKeys.insert(key);
            }
            desired.emplace_back(index, std::move(key));
        }

        for (std::size_t index = mounted_.size(); index > 0; --index) {
            if (desiredKeys.find(mounted_[index - 1].key) == desiredKeys.end()) unmount(owner, index - 1);
            if (clearPending_ || reconcilePending_) return;
        }

        if (clearPending_ || reconcilePending_) return;

        for (const auto& [index, key] : desired) {
            const auto existing = std::find_if(mounted_.begin(), mounted_.end(), [&key](const Mounted& mounted) {
                return mounted.key == key;
            });
            if (existing != mounted_.end()) {
                existing->index = index;
                continue;
            }
            std::unique_ptr<Node> node = takePooled(key);
            if (!node && builder_) node = builder_(index, key);
            if (!node) continue;
            Node* raw = node.get();
            owner.appendChild(std::move(node));
            mounted_.push_back({index, key, raw});
        }
        trimPool(desiredRange.size());
    }

    void unmount(Node& owner, std::size_t mountedIndex)
    {
        if (mountedIndex >= mounted_.size()) return;
        Mounted mounted = std::move(mounted_[mountedIndex]);
        mounted_.erase(mounted_.begin() + static_cast<std::ptrdiff_t>(mountedIndex));

        std::size_t childPosition = owner.children().size();
        for (std::size_t index = 0; index < owner.children().size(); ++index) {
            if (owner.children()[index].get() == mounted.node) {
                childPosition = index;
                break;
            }
        }
        if (childPosition == owner.children().size()) return;

        std::unique_ptr<Node> node = owner.removeChild(childPosition);
        addToPool(std::move(mounted.key), std::move(node));
    }

    [[nodiscard]] std::unique_ptr<Node> takePooled(const Key& key)
    {
        const auto item = std::find_if(pool_.begin(), pool_.end(), [&key](const Pooled& pooled) { return pooled.key == key; });
        if (item == pool_.end()) return nullptr;
        std::unique_ptr<Node> node = std::move(item->node);
        pool_.erase(item);
        return node;
    }

    void addToPool(Key key, std::unique_ptr<Node> node)
    {
        if (!node) return;
        pool_.emplace_back();
        pool_.back().key = std::move(key);
        pool_.back().node = std::move(node);
    }

    void trimPool(Index windowSize)
    {
        const Index limit = std::max<Index>(8, windowSize * 2);
        if (pool_.size() > limit) pool_.erase(pool_.begin(), pool_.begin() + static_cast<std::ptrdiff_t>(pool_.size() - limit));
    }

    KeyProvider keyProvider_;
    Builder builder_;
    std::vector<Mounted> mounted_;
    std::vector<Pooled> pool_;
    ViewportModel::Range pendingRange_;
    bool reconciling_{false};
    bool reconcilePending_{false};
    bool clearPending_{false};
};

} // namespace wui::internal