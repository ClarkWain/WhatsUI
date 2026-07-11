#pragma once

#include <cstddef>
#include <cstdint>

namespace wui {

// Lightweight frame accounting intended for tools and debug overlays. Values
// are collected by UiWindow without allocating or walking platform state; the
// node totals are sampled once after paint has completed.
struct NodeTreeStats {
    std::size_t nodes{0};
    std::size_t layoutDirty{0};
    std::size_t paintDirty{0};
};

struct FrameStats {
    std::uint64_t frameNumber{0};
    double updateMilliseconds{0.0};
    double layoutMilliseconds{0.0};
    double prepareMilliseconds{0.0};
    double paintMilliseconds{0.0};
    NodeTreeStats page{};
    NodeTreeStats overlays{};
};

} // namespace wui
