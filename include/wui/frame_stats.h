#pragma once

#include <cstddef>
#include <cstdint>

namespace wui {

// Lightweight frame accounting intended for tools and debug overlays. Values
// are collected by UiWindow without allocating or walking platform state; the
// node totals are sampled once after paint has completed.
struct NodeTreeStats {
    std::size_t nodes{0};
    // Nodes with one or more pending dirty flags. Per-flag totals below may
    // each include the same node, while this count never does.
    std::size_t dirtyNodes{0};
    std::size_t styleDirty{0};
    std::size_t layoutDirty{0};
    std::size_t paintDirty{0};
    std::size_t compositingDirty{0};
    // Tree-level diagnostics can identify Text nodes without asking a
    // renderer to rasterize or inspect glyphs.
    std::size_t textNodes{0};
};

enum class CounterAvailability {
    Available,
    Unavailable,
};

// A renderer-owned counter. WhatsUI initializes these as unavailable because
// PaintContext intentionally has no backend command-stream or glyph-cache
// inspection API. A future backend can set availability to Available only
// when it supplies a count for the same completed frame.
struct RendererCounter {
    std::size_t value{0};
    CounterAvailability availability{CounterAvailability::Unavailable};

    [[nodiscard]] bool isAvailable() const noexcept
    {
        return availability == CounterAvailability::Available;
    }
};

struct RenderStats {
    // These two are framework tree diagnostics, available on every backend.
    // paintTraversalNodes is the number of nodes eligible for the completed
    // frame's paint traversal; it is not a GPU draw-call count.
    std::size_t paintTraversalNodes{0};
    std::size_t textNodes{0};

    // Explicitly unavailable without backend instrumentation. Keeping them
    // typed rather than silently reporting zero prevents tools from treating
    // "not measured" as a successful zero-cost frame.
    RendererCounter drawCalls{};
    RendererCounter textDrawCalls{};
    RendererCounter textCacheHits{};
    RendererCounter textCacheMisses{};
};

struct FrameStats {
    std::uint64_t frameNumber{0};
    double updateMilliseconds{0.0};
    double layoutMilliseconds{0.0};
    double prepareMilliseconds{0.0};
    double paintMilliseconds{0.0};
    NodeTreeStats page{};
    NodeTreeStats overlays{};
    RenderStats render{};
};

} // namespace wui
