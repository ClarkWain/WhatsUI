// Deterministic scale probes for release/CI diagnostics.  These are not
// performance gates: elapsed values are emitted for a dashboard to compare
// across like-for-like machines, while the executable fails only on semantic
// or bounded-resource invariant regressions.

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "wui/wui.h"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class BenchmarkSurface final : public wui::RenderSurface {
public:
    [[nodiscard]] wui::CanvasBackend backend() const noexcept override { return wui::CanvasBackend::Software; }
    [[nodiscard]] wui::SizeF framebufferSize() const noexcept override { return {1280.0f, 720.0f}; }
    void beginFrame() override {}
    void endFrame() override {}
    void resize(wui::SizeF) override {}
};

class BenchmarkClipboard final : public wui::Clipboard {
public:
    void setText(std::string_view text) override { text_ = text; }
    [[nodiscard]] std::string getText() const override { return text_; }
    [[nodiscard]] bool hasText() const override { return !text_.empty(); }
private:
    std::string text_;
};

class BenchmarkCursor final : public wui::CursorService {
public:
    void setCursor(wui::CursorIcon) override {}
};

class BenchmarkTextInput final : public wui::TextInputSession {
public:
    void activate() override {}
    void deactivate() override {}
    void setCaretRect(const wui::RectF&) override {}
    void setSurroundingText(std::string_view, std::size_t, std::size_t) override {}
};

class BenchmarkWindow final : public wui::PlatformWindow {
public:
    [[nodiscard]] wui::WindowId id() const noexcept override { return 404; }
    [[nodiscard]] wui::WindowMetrics metrics() const noexcept override
    {
        return {{640.0f, 360.0f}, {1280.0f, 720.0f}, 2.0f};
    }
    void show() override {}
    void close() override {}
    [[nodiscard]] bool isOpen() const noexcept override { return true; }
    [[nodiscard]] bool isFocused() const noexcept override { return true; }
    void setTitle(std::string_view) override {}
    void requestRedraw() override {}
    [[nodiscard]] wui::RenderSurface& surface() override { return surface_; }
    [[nodiscard]] wui::Clipboard& clipboard() override { return clipboard_; }
    [[nodiscard]] wui::CursorService& cursor() override { return cursor_; }
    [[nodiscard]] wui::TextInputSession& textInput() override { return textInput_; }
private:
    BenchmarkSurface surface_;
    BenchmarkClipboard clipboard_;
    BenchmarkCursor cursor_;
    BenchmarkTextInput textInput_;
};

[[nodiscard]] wui::FrameStats renderOneFrame(std::unique_ptr<wui::Node> root)
{
    wui::UiWindow window(std::make_unique<BenchmarkWindow>());
    window.setRoot(std::move(root));
    wui::PaintContext context(2.0f);
    window.update();
    window.layout();
    window.prepare(context);
    window.paint(context);
    return window.frameStats();
}

void requireValidFrame(const wui::FrameStats& stats, std::size_t expectedMinimumNodes)
{
    require(stats.frameNumber == 1, "A benchmark frame must have exactly one completed frame number");
    require(stats.page.nodes >= expectedMinimumNodes, "Benchmark page node count is unexpectedly small");
    require(std::isfinite(stats.updateMilliseconds) && std::isfinite(stats.layoutMilliseconds) &&
                std::isfinite(stats.prepareMilliseconds) && std::isfinite(stats.paintMilliseconds) &&
                stats.updateMilliseconds >= 0.0 && stats.layoutMilliseconds >= 0.0 &&
                stats.prepareMilliseconds >= 0.0 && stats.paintMilliseconds >= 0.0,
            "Frame metrics must be finite non-negative values");
}

void emitFrame(std::string_view benchmark, const wui::FrameStats& stats)
{
    std::cout << std::fixed << std::setprecision(3)
              << "{\"benchmark\":\"" << benchmark << "\",\"page_nodes\":" << stats.page.nodes
              << ",\"overlay_nodes\":" << stats.overlays.nodes
              << ",\"update_ms\":" << stats.updateMilliseconds
              << ",\"layout_ms\":" << stats.layoutMilliseconds
              << ",\"prepare_ms\":" << stats.prepareMilliseconds
              << ",\"paint_ms\":" << stats.paintMilliseconds << "}\n";
}

void benchmarkThousandControls()
{
    auto root = std::make_unique<wui::Column>();
    root->gap(2.0f);
    for (int index = 0; index < 1000; ++index) {
        root->appendChild(std::make_unique<wui::Button>("Control " + std::to_string(index)));
    }
    const auto stats = renderOneFrame(std::move(root));
    requireValidFrame(stats, 1001);
    emitFrame("controls_1000", stats);
}

void benchmarkHundredThousandLogicalRows()
{
    wui::VirtualList list;
    list.setKeyProvider([](wui::VirtualList::Index index) { return "row-" + std::to_string(index); });
    list.setItemBuilder([](wui::VirtualList::Index, const std::string& key) {
        return std::make_unique<wui::Text>(key);
    });
    list.setItemCount(100000);
    list.layout({0.0f, 0.0f, 320.0f, 360.0f});
    const auto initial = list.visibleRange();
    require(list.itemCount() == 100000 && !initial.empty(), "VirtualList must retain its 100k logical model");
    require(list.mountedCount() < 32, "VirtualList viewport must not mount its whole logical model");
    list.scrollToIndex(75000);
    const auto middle = list.visibleRange();
    require(middle.first <= 75000 && middle.last > 75000 && list.mountedCount() < 32,
            "VirtualList must keep large-scroll mounting bounded and target visible");
    std::cout << "{\"benchmark\":\"virtual_list_100000\",\"logical_rows\":" << list.itemCount()
              << ",\"mounted_rows\":" << list.mountedCount()
              << ",\"pooled_rows\":" << list.pooledCount()
              << ",\"visible_first\":" << middle.first
              << ",\"visible_last\":" << middle.last << "}\n";
}

void benchmarkTenThousandTextNodes()
{
    auto root = std::make_unique<wui::Column>();
    for (int index = 0; index < 10000; ++index) {
        root->appendChild(std::make_unique<wui::Text>("Text node " + std::to_string(index)));
    }
    const auto stats = renderOneFrame(std::move(root));
    requireValidFrame(stats, 10001);
    emitFrame("text_10000", stats);
}

void benchmarkMutationStorm()
{
    wui::Column root;
    constexpr std::size_t kCeiling = 64;
    constexpr int kOperations = 10000;
    for (int operation = 0; operation < kOperations; ++operation) {
        if (root.children().size() == kCeiling) {
            const auto removeIndex = static_cast<std::size_t>(operation) % root.children().size();
            (void)root.removeChild(removeIndex);
        }
        root.appendChild(std::make_unique<wui::Text>("mutation " + std::to_string(operation)));
        require(root.children().size() <= kCeiling, "Mutation storm must keep its bounded tree invariant");
    }
    root.clearChildren();
    require(root.children().empty(), "Mutation storm must release all transient children");
    std::cout << "{\"benchmark\":\"mutation_storm\",\"operations\":" << kOperations
              << ",\"final_children\":0}\n";
}

void benchmarkOverlayLikeTreeStress()
{
    wui::OverlayHost host;
    constexpr std::size_t kOverlayCount = 256;
    for (std::size_t index = 0; index < kOverlayCount; ++index) {
        auto popup = std::make_unique<wui::Popup>();
        popup->anchor({static_cast<float>((index % 12) * 24), static_cast<float>((index % 8) * 18), 20.0f, 18.0f})
            .preferredSize({160.0f, 72.0f});
        const auto id = host.show(std::move(popup));
        require(id == index + 1, "Overlay ids must remain monotonic during overlay stress");
    }
    host.layout({0.0f, 0.0f, 640.0f, 360.0f});
    wui::PaintContext context(1.0f);
    host.prepare(context);
    host.paint(context);
    require(host.size() == kOverlayCount && host.top() != nullptr,
            "Overlay stress must retain every shown overlay until dismissal");
    for (std::size_t index = 0; index < kOverlayCount; ++index) {
        require(host.dismissTop() != nullptr, "Overlay stress must dismiss each owned overlay exactly once");
    }
    require(host.empty(), "Overlay stress must fully release dismissed overlays");
    std::cout << "{\"benchmark\":\"overlay_tree_stress\",\"overlays\":" << kOverlayCount
              << ",\"final_overlays\":0}\n";
}

} // namespace

int main()
{
    try {
        benchmarkThousandControls();
        benchmarkHundredThousandLogicalRows();
        benchmarkTenThousandTextNodes();
        benchmarkMutationStorm();
        benchmarkOverlayLikeTreeStress();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "{\"benchmark\":\"failure\",\"message\":\"" << error.what() << "\"}\n";
        return EXIT_FAILURE;
    }
}
