#include <memory>
#include <stdexcept>

#include "wui/wui.h"

#ifdef WHATSUI_HAS_WHATSCANVAS
#include "wsc/Canvas.h"
#endif

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class TestSurface final : public wui::RenderSurface {
public:
    [[nodiscard]] wui::CanvasBackend backend() const noexcept override { return wui::CanvasBackend::Software; }
    [[nodiscard]] wui::SizeF framebufferSize() const noexcept override { return {640.0f, 360.0f}; }
    void beginFrame() override {}
    void endFrame() override {}
    void resize(wui::SizeF) override {}
};

class TestClipboard final : public wui::Clipboard {
public:
    void setText(std::string_view text) override { value_ = text; }
    [[nodiscard]] std::string getText() const override { return value_; }
    [[nodiscard]] bool hasText() const override { return !value_.empty(); }
private:
    std::string value_;
};

class TestCursor final : public wui::CursorService { public: void setCursor(wui::CursorIcon) override {} };
class TestTextInput final : public wui::TextInputSession {
public:
    void activate() override {}
    void deactivate() override {}
    void setCaretRect(const wui::RectF&) override {}
    void setSurroundingText(std::string_view, std::size_t, std::size_t) override {}
};

class TestWindow final : public wui::PlatformWindow {
public:
    [[nodiscard]] wui::WindowId id() const noexcept override { return 91; }
    [[nodiscard]] wui::WindowMetrics metrics() const noexcept override { return {{320.0f, 180.0f}, {640.0f, 360.0f}, 2.0f}; }
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
    TestSurface surface_;
    TestClipboard clipboard_;
    TestCursor cursor_;
    TestTextInput textInput_;
};

void testFrameStatsCoverPipelineAndTree()
{
    wui::UiWindow window(std::make_unique<TestWindow>());
    auto root = std::make_unique<wui::Column>();
    auto text = std::make_unique<wui::Text>("Frame stats");
    auto* const textRaw = text.get();
    root->appendChild(std::move(text));
    window.setRoot(std::move(root));

    wui::PaintContext context(2.0f);
    window.update();
    window.layout();
    window.prepare(context);
    window.paint(context);

    const auto& stats = window.frameStats();
    expect(stats.frameNumber == 1, "update must begin a numbered diagnostics frame");
    expect(stats.page.nodes == 2, "page diagnostics must include root and descendant nodes");
    expect(stats.overlays.nodes == 0, "an empty overlay host must report no overlay nodes");
    expect(stats.page.textNodes == 1 && stats.render.textNodes == 1 && stats.render.paintTraversalNodes == 2,
           "backend-neutral diagnostics must report text and paint traversal candidates");
    expect(stats.render.paintOperations.commandCount == 1 &&
               stats.render.paintOperations.textDrawCalls == 1 &&
               stats.render.paintOperations.fillRectCalls == 0 &&
               stats.render.paintOperations.fillRoundRectCalls == 0 &&
               stats.render.paintOperations.clipRectCalls == 0,
           "PaintContext must snapshot real framework text commands independently of backend draw calls");
    expect(!stats.render.drawCalls.isAvailable() && !stats.render.textDrawCalls.isAvailable() &&
               !stats.render.textCacheHits.isAvailable() && !stats.render.textCacheMisses.isAvailable(),
           "renderer command and cache counters must be explicitly unavailable without backend instrumentation");
    expect(stats.updateMilliseconds >= 0.0 && stats.layoutMilliseconds >= 0.0 &&
               stats.prepareMilliseconds >= 0.0 && stats.paintMilliseconds >= 0.0,
           "each frame phase must report a non-negative duration");

    textRaw->markDirty(wui::DirtyFlag::Style);
    window.update();
    window.layout();
    window.prepare(context);
    window.paint(context);
    const auto& dirtyStats = window.frameStats();
    expect(dirtyStats.frameNumber == 2 && dirtyStats.page.dirtyNodes == 2 && dirtyStats.page.styleDirty == 2,
           "dirty diagnostics must count both the changed node and propagated ancestor exactly once");
    expect(dirtyStats.page.layoutDirty == 0 && dirtyStats.page.paintDirty == 0 &&
               dirtyStats.page.compositingDirty == 0,
           "per-flag diagnostics must distinguish style dirtiness from cleared layout and paint work");
}

void testPaintContextOperationResetAndSnapshot()
{
    wui::PaintContext context;
    context.fillRect({0.0f, 0.0f, 10.0f, 10.0f}, {1, 2, 3, 255});
    context.fillRoundRect({1.0f, 1.0f, 8.0f, 8.0f}, 2.0f, {4, 5, 6, 255});
    context.drawText("stats", 2.0f, 8.0f, 12.0f, {7, 8, 9, 255});
    context.clipRect({0.0f, 0.0f, 4.0f, 4.0f});
    const auto snapshot = context.paintStats();
    expect(snapshot.commandCount == 4 && snapshot.fillRectCalls == 1 &&
               snapshot.fillRoundRectCalls == 1 && snapshot.textDrawCalls == 1 &&
               snapshot.clipRectCalls == 1,
           "PaintContext must count every primitive, text and clip operation in a headless frame");
    context.resetPaintStats();
    const auto reset = context.paintStats();
    expect(reset.commandCount == 0 && reset.fillRectCalls == 0 && reset.fillRoundRectCalls == 0 &&
               reset.textDrawCalls == 0 && reset.clipRectCalls == 0,
           "Resetting PaintContext stats must start an independent frame sample");
}

#ifdef WHATSUI_HAS_WHATSCANVAS
void testWhatsCanvasCompletedFrameCounters()
{
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, 320, 180);
    expect(canvas != nullptr && canvas->initializeContext(),
           "WhatsCanvas Software must initialize for completed-frame diagnostics");

    wui::UiWindow window(std::make_unique<TestWindow>());
    auto root = std::make_unique<wui::Container>();
    root->setBackground({20, 30, 40, 255});
    window.setRoot(std::move(root));

    wui::PaintContext context(*canvas, 1.0f);
    canvas->beginFrame();
    window.update();
    window.layout();
    window.prepare(context);
    window.paint(context);
    canvas->endFrame();
    window.captureCompletedRendererStats(context);

    const auto& stats = window.frameStats().render;
    expect(stats.commandCount.isAvailable() && stats.drawCalls.isAvailable(),
           "Completed WhatsCanvas frames must expose real command and draw-call counters");
    expect(stats.commandCount.value >= 1 && stats.drawCalls.value >= 1,
           "A painted opaque container must produce completed renderer work");
    expect(stats.tessellationCacheHits.isAvailable() && stats.tessellationCacheMisses.isAvailable() &&
               stats.strokeCacheHits.isAvailable() && stats.strokeCacheMisses.isAvailable(),
           "WhatsCanvas public geometry-cache snapshots must be reported as available");
    expect(!stats.textDrawCalls.isAvailable() && !stats.textCacheHits.isAvailable() &&
               !stats.textCacheMisses.isAvailable(),
           "Unavailable WhatsCanvas text-cache metrics must not be fabricated");
    canvas->finalizeContext();
}
#endif

} // namespace

int main()
{
    testFrameStatsCoverPipelineAndTree();
    testPaintContextOperationResetAndSnapshot();
#ifdef WHATSUI_HAS_WHATSCANVAS
    testWhatsCanvasCompletedFrameCounters();
#endif
    return 0;
}
