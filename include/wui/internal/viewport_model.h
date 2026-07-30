#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace wui::internal {

class ViewportModel {
public:
    using Index = std::size_t;

    struct Range {
        Index first{0};
        Index last{0};

        [[nodiscard]] Index size() const noexcept { return last - first; }
        [[nodiscard]] bool empty() const noexcept { return first == last; }
    };

    void setItemCount(Index count) noexcept
    {
        itemCount_ = count;
        setScrollOffset(scrollOffset_);
    }

    [[nodiscard]] Index itemCount() const noexcept { return itemCount_; }

    void setItemExtent(float extent) noexcept
    {
        itemExtent_ = std::isfinite(extent) ? std::max(1.0f, extent) : 1.0f;
        setScrollOffset(scrollOffset_);
    }

    [[nodiscard]] float itemExtent() const noexcept { return itemExtent_; }

    void setViewportExtent(float extent) noexcept
    {
        viewportExtent_ = std::isfinite(extent) ? std::max(0.0f, extent) : 0.0f;
        setScrollOffset(scrollOffset_);
    }

    [[nodiscard]] float viewportExtent() const noexcept { return viewportExtent_; }

    void setOverscanItems(Index count) noexcept { overscanItems_ = count; }
    [[nodiscard]] Index overscanItems() const noexcept { return overscanItems_; }

    void setScrollOffset(float offset) noexcept
    {
        scrollOffset_ = std::isfinite(offset) ? std::clamp(offset, 0.0f, maxScrollOffset()) : 0.0f;
    }

    [[nodiscard]] float scrollOffset() const noexcept { return scrollOffset_; }

    [[nodiscard]] float maxScrollOffset() const noexcept
    {
        return std::max(0.0f, static_cast<float>(itemCount_) * itemExtent_ - viewportExtent_);
    }

    [[nodiscard]] Range visibleRange() const noexcept
    {
        if (itemCount_ == 0 || viewportExtent_ <= 0.0f || itemExtent_ <= 0.0f) return {};
        const auto first = static_cast<Index>(std::min<float>(static_cast<float>(itemCount_), std::floor(scrollOffset_ / itemExtent_)));
        const auto last = static_cast<Index>(std::min<float>(static_cast<float>(itemCount_), std::ceil((scrollOffset_ + viewportExtent_) / itemExtent_)));
        return {first, std::max(first, last)};
    }

    [[nodiscard]] Range overscanRange() const noexcept
    {
        const Range visible = visibleRange();
        if (visible.empty()) return visible;
        const Index first = visible.first > overscanItems_ ? visible.first - overscanItems_ : 0;
        const Index last = std::min(itemCount_, visible.last + overscanItems_);
        return {first, last};
    }

    void scrollToIndex(Index index) noexcept
    {
        if (index >= itemCount_ || viewportExtent_ <= 0.0f) return;
        const float top = static_cast<float>(index) * itemExtent_;
        const float bottom = top + itemExtent_;
        if (top < scrollOffset_) setScrollOffset(top);
        else if (bottom > scrollOffset_ + viewportExtent_) setScrollOffset(bottom - viewportExtent_);
    }

private:
    Index itemCount_{0};
    float itemExtent_{1.0f};
    float viewportExtent_{0.0f};
    Index overscanItems_{0};
    float scrollOffset_{0.0f};
};

} // namespace wui::internal