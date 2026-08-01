#pragma once

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "wui/declarative.h"

namespace whatsui::gallery::view::components {

// Preserve the familiar desktop Row while allowing a page to opt into a
// readable vertical stack at the Gallery compact breakpoint.  This belongs in
// the demo layer rather than changing the framework's general-purpose Row.
class ResponsiveRow final : public wui::ContainerNode {
public:
    ResponsiveRow& gap(float value) noexcept
    {
        gap_ = std::max(0.0f, value);
        markDirty(wui::DirtyFlag::Layout);
        return *this;
    }

    ResponsiveRow& align(wui::Alignment value) noexcept
    {
        alignment_ = value;
        markDirty(wui::DirtyFlag::Layout);
        return *this;
    }

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        const bool compact = isCompact(constraints.maxWidth);
        float width = 0.0f;
        float height = 0.0f;
        bool hasChild = false;
        bool hasFlex = false;
        for (const auto& child : children()) {
            const float availableWidth = compact
                ? constraints.maxWidth
                : std::max(0.0f, constraints.maxWidth - width - (hasChild ? gap_ : 0.0f));
            const auto size = child->measureWithConstraints(
                {0.0f, availableWidth, 0.0f, constraints.maxHeight});
            if (compact) {
                width = std::max(width, size.width);
                height += size.height + (hasChild ? gap_ : 0.0f);
            } else {
                width += size.width + (hasChild ? gap_ : 0.0f);
                height = std::max(height, size.height);
            }
            hasFlex = hasFlex || (!compact && child->flex() > 0.0f);
            hasChild = true;
        }
        if (hasFlex) width = constraints.maxWidth;
        return constraints.clamp({width, height});
    }

    void layout(const wui::RectF& bounds) override
    {
        wui::Node::layout(bounds);
        const bool compact = isCompact(bounds.width);
        if (compact) {
            float cursor = bounds.y;
            for (const auto& child : children()) {
                const auto size = child->measureWithConstraints(
                    {0.0f, bounds.width, 0.0f, std::max(0.0f, bounds.y + bounds.height - cursor)});
                child->layout({bounds.x, cursor, bounds.width, size.height});
                cursor += size.height + gap_;
            }
        } else {
            std::vector<wui::SizeF> sizes(children().size());
            std::vector<bool> active(children().size(), false);
            float fixedWidth = 0.0f;
            float totalFlex = 0.0f;
            std::size_t activeCount = 0;
            for (std::size_t index = 0; index < children().size(); ++index) {
                const auto& child = children()[index];
                if (child->flex() > 0.0f) {
                    totalFlex += child->flex();
                    active[index] = true;
                } else {
                    sizes[index] = child->measureWithConstraints({0.0f, bounds.width, 0.0f, bounds.height});
                    active[index] = sizes[index].width > 0.0f || sizes[index].height > 0.0f;
                    if (active[index]) fixedWidth += sizes[index].width;
                }
                if (active[index]) ++activeCount;
            }
            if (activeCount > 1) fixedWidth += gap_ * static_cast<float>(activeCount - 1);
            float cursor = bounds.x;
            bool hasChild = false;
            const float remaining = std::max(0.0f, bounds.width - fixedWidth);
            for (std::size_t index = 0; index < children().size(); ++index) {
                const auto& child = children()[index];
                if (active[index] && hasChild) cursor += gap_;
                auto size = sizes[index];
                if (child->flex() > 0.0f) {
                    const float allocated = totalFlex > 0.0f
                        ? remaining * (child->flex() / totalFlex) : 0.0f;
                    size = child->measureWithConstraints({0.0f, allocated, 0.0f, bounds.height});
                    size.width = allocated;
                }
                float y = bounds.y;
                if (alignment_ == wui::Alignment::Center) {
                    y += (bounds.height - size.height) * 0.5f;
                } else if (alignment_ == wui::Alignment::End) {
                    y += bounds.height - size.height;
                } else if (alignment_ == wui::Alignment::Stretch) {
                    size.height = bounds.height;
                }
                child->layout({cursor, y, size.width, size.height});
                if (active[index]) {
                    cursor += size.width;
                    hasChild = true;
                }
            }
        }
        clearLayoutDirtyRecursively();
    }

private:
    static constexpr float kCompactBreakpoint{720.0f};

    [[nodiscard]] static bool isCompact(float width) noexcept
    {
        return width < kCompactBreakpoint;
    }

    float gap_{12.0f};
    wui::Alignment alignment_{wui::Alignment::Start};
};

// A chip/button row which preserves its wide layout and wraps at natural
// boundaries in a narrow viewport.  It is intentionally reusable for all
// compact control selectors, rather than relying on clipped Rows.
class ResponsiveFlow final : public wui::ContainerNode {
public:
    ResponsiveFlow& gap(float horizontal, float vertical = 8.0f) noexcept
    {
        horizontalGap_ = std::max(0.0f, horizontal);
        verticalGap_ = std::max(0.0f, vertical);
        markDirty(wui::DirtyFlag::Layout);
        return *this;
    }

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return measureFlow(constraints, false);
    }

    void layout(const wui::RectF& bounds) override
    {
        wui::Node::layout(bounds);
        float cursorX = bounds.x;
        float cursorY = bounds.y;
        float lineHeight = 0.0f;
        bool hasOnLine = false;
        for (const auto& child : children()) {
            const auto size = child->measureWithConstraints(
                {0.0f, bounds.width, 0.0f, std::max(0.0f, bounds.y + bounds.height - cursorY)});
            const bool wrap = hasOnLine && cursorX + horizontalGap_ + size.width > bounds.x + bounds.width;
            if (wrap) {
                cursorX = bounds.x;
                cursorY += lineHeight + verticalGap_;
                lineHeight = 0.0f;
                hasOnLine = false;
            }
            if (hasOnLine) cursorX += horizontalGap_;
            child->layout({cursorX, cursorY, size.width, size.height});
            cursorX += size.width;
            lineHeight = std::max(lineHeight, size.height);
            hasOnLine = true;
        }
        clearLayoutDirtyRecursively();
    }

private:
    [[nodiscard]] wui::SizeF measureFlow(const wui::Constraints& constraints, bool) const
    {
        float cursorX = 0.0f;
        float height = 0.0f;
        float lineHeight = 0.0f;
        float maxWidth = 0.0f;
        bool hasOnLine = false;
        for (const auto& child : children()) {
            const auto size = child->measureWithConstraints(
                {0.0f, constraints.maxWidth, 0.0f, constraints.maxHeight});
            const bool wrap = hasOnLine && cursorX + horizontalGap_ + size.width > constraints.maxWidth;
            if (wrap) {
                height += lineHeight + verticalGap_;
                maxWidth = std::max(maxWidth, cursorX);
                cursorX = 0.0f;
                lineHeight = 0.0f;
                hasOnLine = false;
            }
            if (hasOnLine) cursorX += horizontalGap_;
            cursorX += size.width;
            lineHeight = std::max(lineHeight, size.height);
            hasOnLine = true;
        }
        if (hasOnLine) {
            height += lineHeight;
            maxWidth = std::max(maxWidth, cursorX);
        }
        return constraints.clamp({maxWidth, height});
    }

    float horizontalGap_{6.0f};
    float verticalGap_{8.0f};
};

template <class... Children>
[[nodiscard]] std::unique_ptr<wui::Node> buildResponsiveFlow(
    float horizontalGap,
    Children&&... children)
{
    auto flow = std::make_unique<ResponsiveFlow>();
    flow->gap(horizontalGap);
    (flow->appendChild(wui::asNode(std::forward<Children>(children))), ...);
    return flow;
}

} // namespace whatsui::gallery::view::components
