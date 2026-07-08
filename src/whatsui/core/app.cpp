#include "wui/app.h"

#include <stdexcept>

namespace wui {

UiWindow::UiWindow(std::unique_ptr<PlatformWindow> platformWindow)
    : platformWindow_(std::move(platformWindow))
{
    if (!platformWindow_) {
        throw std::invalid_argument("platformWindow must not be null");
    }
}

WindowId UiWindow::id() const noexcept
{
    return platformWindow_ != nullptr ? platformWindow_->id() : 0;
}

PlatformWindow& UiWindow::platformWindow() noexcept
{
    return *platformWindow_;
}

const PlatformWindow& UiWindow::platformWindow() const noexcept
{
    return *platformWindow_;
}

UiRoot& UiWindow::uiRoot() noexcept
{
    return uiRoot_;
}

const UiRoot& UiWindow::uiRoot() const noexcept
{
    return uiRoot_;
}

FocusManager& UiWindow::focusManager() noexcept
{
    return focusManager_;
}

const FocusManager& UiWindow::focusManager() const noexcept
{
    return focusManager_;
}

InputRouter& UiWindow::inputRouter() noexcept
{
    return inputRouter_;
}

const InputRouter& UiWindow::inputRouter() const noexcept
{
    return inputRouter_;
}

Navigator& UiWindow::navigator() noexcept
{
    return navigator_;
}

const Navigator& UiWindow::navigator() const noexcept
{
    return navigator_;
}

OverlayHost& UiWindow::overlayHost() noexcept
{
    return overlayHost_;
}

const OverlayHost& UiWindow::overlayHost() const noexcept
{
    return overlayHost_;
}

void UiWindow::setRoot(std::unique_ptr<Node> root) noexcept
{
    uiRoot_.setContent(std::move(root));
    inputRouter_.setRoot(uiRoot_.content());
}

Node* UiWindow::root() const noexcept
{
    return uiRoot_.content();
}

UiApp::UiApp(std::unique_ptr<PlatformHost> host) noexcept
    : host_(std::move(host))
{
}

PlatformHost* UiApp::host() const noexcept
{
    return host_.get();
}

UiWindow& UiApp::attachWindow(std::unique_ptr<PlatformWindow> platformWindow)
{
    auto window = std::make_unique<UiWindow>(std::move(platformWindow));
    windows_.push_back(std::move(window));
    return *windows_.back();
}

UiWindow& UiApp::openWindow(std::string title, SizeF logicalSize)
{
    if (!host_) {
        throw std::runtime_error("UiApp::openWindow requires a PlatformHost");
    }
    return attachWindow(host_->createWindow(std::move(title), logicalSize));
}

UiWindow* UiApp::findWindow(WindowId id) noexcept
{
    for (const auto& window : windows_) {
        if (window->id() == id) {
            return window.get();
        }
    }
    return nullptr;
}

const std::vector<std::unique_ptr<UiWindow>>& UiApp::windows() const noexcept
{
    return windows_;
}

} // namespace wui
