#include "capture/headless_platform.h"

#include <cmath>
#include <string>
#include <utility>

namespace whatsui::gallery::capture {
namespace {

class HeadlessSurface final : public wui::RenderSurface {
public:
    explicit HeadlessSurface(wui::SizeF size)
        : size_(size)
    {
    }

    [[nodiscard]] wui::CanvasBackend backend() const noexcept override
    {
        return wui::CanvasBackend::Software;
    }
    [[nodiscard]] wui::SizeF framebufferSize() const noexcept override { return size_; }
    void beginFrame() override {}
    void endFrame() override {}
    void resize(wui::SizeF size) override { size_ = size; }

private:
    wui::SizeF size_;
};

class HeadlessClipboard final : public wui::Clipboard {
public:
    void setText(std::string_view text) override { text_ = text; }
    [[nodiscard]] std::string getText() const override { return text_; }
    [[nodiscard]] bool hasText() const override { return !text_.empty(); }

private:
    std::string text_;
};

class HeadlessCursor final : public wui::CursorService {
public:
    void setCursor(wui::CursorIcon) override {}
};

class HeadlessTextInput final : public wui::TextInputSession {
public:
    void activate() override {}
    void deactivate() override {}
    void setCaretRect(const wui::RectF&) override {}
    void setSurroundingText(std::string_view, std::size_t, std::size_t) override {}
};

class HeadlessWindow final : public wui::PlatformWindow {
public:
    HeadlessWindow(wui::WindowId id, std::string title, wui::SizeF logicalSize,
                   float scaleFactor)
        : id_(id)
        , title_(std::move(title))
        , metrics_{logicalSize,
                   {logicalSize.width * scaleFactor, logicalSize.height * scaleFactor},
                   scaleFactor}
        , surface_(metrics_.framebufferSize)
    {
    }

    [[nodiscard]] wui::WindowId id() const noexcept override { return id_; }
    [[nodiscard]] wui::WindowMetrics metrics() const noexcept override { return metrics_; }
    void show() override { open_ = true; }
    void close() override { open_ = false; }
    [[nodiscard]] bool isOpen() const noexcept override { return open_; }
    [[nodiscard]] bool isFocused() const noexcept override { return true; }
    void setTitle(std::string_view title) override { title_ = title; }
    [[nodiscard]] std::string title() const override { return title_; }
    void requestRedraw() override {}
    [[nodiscard]] wui::RenderSurface& surface() override { return surface_; }
    [[nodiscard]] wui::Clipboard& clipboard() override { return clipboard_; }
    [[nodiscard]] wui::CursorService& cursor() override { return cursor_; }
    [[nodiscard]] wui::TextInputSession& textInput() override { return textInput_; }

private:
    wui::WindowId id_;
    std::string title_;
    wui::WindowMetrics metrics_;
    HeadlessSurface surface_;
    HeadlessClipboard clipboard_;
    HeadlessCursor cursor_;
    HeadlessTextInput textInput_;
    bool open_{true};
};

class HeadlessHost final : public wui::PlatformHost {
public:
    explicit HeadlessHost(float scaleFactor)
        : scaleFactor_(scaleFactor)
    {
    }

    [[nodiscard]] std::unique_ptr<wui::PlatformWindow> createWindow(
        std::string title, wui::SizeF logicalSize) override
    {
        return std::make_unique<HeadlessWindow>(nextWindowId_++, std::move(title),
                                                logicalSize, scaleFactor_);
    }
    [[nodiscard]] int run() override { return exitCode_; }
    void quit(int exitCode) override { exitCode_ = exitCode; }

private:
    float scaleFactor_;
    wui::WindowId nextWindowId_{1};
    int exitCode_{0};
};

} // namespace

std::unique_ptr<wui::PlatformHost> createHeadlessPlatformHost(float scaleFactor)
{
    const float safeScale = std::isfinite(scaleFactor) && scaleFactor > 0.0f
        ? scaleFactor
        : 1.0f;
    return std::make_unique<HeadlessHost>(safeScale);
}

} // namespace whatsui::gallery::capture
