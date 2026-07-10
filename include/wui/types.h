#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace wui {

struct SizeF {
    float width{0.0f};
    float height{0.0f};
};

struct PointF {
    float x{0.0f};
    float y{0.0f};
};

struct RectF {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};

    [[nodiscard]] bool contains(PointF point) const noexcept
    {
        return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height;
    }
};

struct InsetsF {
    float left{0.0f};
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};

    [[nodiscard]] float horizontal() const noexcept
    {
        return left + right;
    }

    [[nodiscard]] float vertical() const noexcept
    {
        return top + bottom;
    }
};

struct Color {
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
    std::uint8_t a{255};
};

struct Constraints {
    float minWidth{0.0f};
    float maxWidth{std::numeric_limits<float>::infinity()};
    float minHeight{0.0f};
    float maxHeight{std::numeric_limits<float>::infinity()};

    [[nodiscard]] SizeF clamp(SizeF size) const noexcept
    {
        const float safeMinWidth = std::max(0.0f, minWidth);
        const float safeMinHeight = std::max(0.0f, minHeight);
        const float safeMaxWidth = std::max(safeMinWidth, maxWidth);
        const float safeMaxHeight = std::max(safeMinHeight, maxHeight);
        return {
            std::clamp(std::max(0.0f, size.width), safeMinWidth, safeMaxWidth),
            std::clamp(std::max(0.0f, size.height), safeMinHeight, safeMaxHeight),
        };
    }

    [[nodiscard]] Constraints deflate(InsetsF insets) const noexcept
    {
        const float horizontal = std::max(0.0f, insets.horizontal());
        const float vertical = std::max(0.0f, insets.vertical());
        return {
            std::max(0.0f, minWidth - horizontal),
            std::max(0.0f, maxWidth - horizontal),
            std::max(0.0f, minHeight - vertical),
            std::max(0.0f, maxHeight - vertical),
        };
    }
};

enum class DirtyFlag : std::uint32_t {
    None = 0,
    Style = 1u << 0,
    Layout = 1u << 1,
    Paint = 1u << 2,
    Compositing = 1u << 3,
};

using DirtyFlags = std::uint32_t;

constexpr DirtyFlags toMask(DirtyFlag flag) noexcept
{
    return static_cast<DirtyFlags>(flag);
}

enum class Alignment {
    Start,
    Center,
    End,
    Stretch,
    Baseline,
};

enum class CanvasBackend {
    Unknown,
    Software,
    OpenGL,
    OpenGLES,
    Vulkan,
};

using WindowId = std::uint64_t;

} // namespace wui
