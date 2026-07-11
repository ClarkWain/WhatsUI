#include "wui/app.h"
#include "wui/scheduler.h"
#include "wui/text_input.h"
#include "wui/widgets.h"

#include <algorithm>
#include <stdexcept>

namespace wui {

UiWindow::UiWindow(std::unique_ptr<PlatformWindow> platformWindow)
    : platformWindow_(std::move(platformWindow))
{
    if (!platformWindow_) {
        throw std::invalid_argument("platformWindow must not be null");
    }
    navigator_.setOnChange([this](Node* page) { syncActiveRoot(page); });
    navigator_.setBeforeChange([this] {
        deactivateTextInputSession();
        focusManager_.clear();
        inputRouter_.clearHover();
    });
    overlayHost_.setOnChange([this] { onOverlayChanged(); });
    uiRoot_.setOnInvalidate([this] { platformWindow_->requestRedraw(); });
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

OverlayId UiWindow::showDialog(std::unique_ptr<Dialog> dialog)
{
    if (!dialog) {
        throw std::invalid_argument("dialog must not be null");
    }
    Node* const previousFocus = focusManager_.focused();
    Dialog* const raw = dialog.get();
    const auto id = overlayHost_.show(std::move(dialog));
    dialogs_.push_back({id, previousFocus});
    raw->setWindowDismissHandler([this, id] { (void)dismissDialog(id); });
    // A modal must never leave a page control focused: keyboard and IME input
    // are isolated until the dialog has been closed.
    deactivateTextInputSession();
    focusManager_.clear();
    inputRouter_.clearHover();
    // Keyboard routing has to follow the modal as well as pointer routing.
    // Keeping the page as InputRouter's root made Tab/Enter activate controls
    // behind a dialog even though the backdrop correctly blocked the pointer.
    inputRouter_.setRoot(raw);
    return id;
}

std::unique_ptr<Dialog> UiWindow::dismissDialog(OverlayId id)
{
    auto it = std::find_if(dialogs_.begin(), dialogs_.end(), [id](const DialogEntry& entry) { return entry.id == id; });
    if (it == dialogs_.end()) {
        return nullptr;
    }
    Node* restoreFocus = it->restoreFocus;
    dialogs_.erase(it);
    auto overlay = overlayHost_.dismiss(id);
    auto* rawDialog = dynamic_cast<Dialog*>(overlay.get());
    if (rawDialog != nullptr) {
        (void)overlay.release();
        std::unique_ptr<Dialog> dialog(rawDialog);
        // Restoring after OverlayHost's change hook avoids retaining a focus
        // pointer into the removed modal subtree.
        // A nested dialog hands keyboard routing back to the dialog beneath it;
        // otherwise restore the active page tree before restoring prior focus.
        inputRouter_.setRoot(activeDialog() != nullptr ? static_cast<Node*>(activeDialog()) : uiRoot_.content());
        focusManager_.setFocused(restoreFocus);
        syncTextInputSession();
        return dialog;
    }
    return nullptr;
}

std::unique_ptr<Dialog> UiWindow::dismissTopDialog()
{
    return dialogs_.empty() ? nullptr : dismissDialog(dialogs_.back().id);
}

bool UiWindow::hasDialog() const noexcept
{
    return activeDialog() != nullptr;
}

void UiWindow::setRoot(std::unique_ptr<Node> root)
{
    navigator_.clear();
    deactivateTextInputSession();
    focusManager_.clear();
    uiRoot_.setContent(std::move(root));
    inputRouter_.setRoot(uiRoot_.content());
    platformWindow_->requestRedraw();
}

Node* UiWindow::root() const noexcept
{
    return uiRoot_.content();
}

void UiWindow::update()
{
    flushStructuralUpdates();
}

void UiWindow::layout()
{
    const auto metrics = platformWindow_->metrics();
    const RectF bounds{0.0f, 0.0f, metrics.logicalSize.width, metrics.logicalSize.height};
    uiRoot_.layout(bounds);
    overlayHost_.layout(bounds);
}

void UiWindow::paint(PaintContext& context)
{
    uiRoot_.paint(context);
    overlayHost_.paint(context);
}

void UiWindow::prepare(PaintContext& context)
{
    uiRoot_.prepare(context);
    overlayHost_.prepare(context);
}

Node* UiWindow::hitTest(PointF point) const
{
    if (auto* dialog = activeDialog()) {
        return dialog->hitTest(point);
    }
    if (auto* overlay = overlayHost_.hitTest(point)) {
        return overlay;
    }
    return inputRouter_.hitTest(point);
}

bool UiWindow::dispatchPointer(const PointerEvent& event)
{
    const bool handled = inputRouter_.dispatchPointerTo(hitTest(event.position), event);
    syncTextInputSession();
    return handled;
}

bool UiWindow::dispatchKey(const KeyEvent& event)
{
    if (event.action == KeyAction::Down && event.keyCode == 27) {
        if (auto* dialog = activeDialog()) {
            dialog->dismiss();
            return true;
        }
    }

    // Clipboard ownership is a platform concern, while selection ownership is
    // a text-control concern. Keep this small bridge at the window boundary
    // so TextInput remains usable in headless/unit-test trees and native
    // backends never need widget-specific shortcut handling.
    if (event.action == KeyAction::Down && (event.modifiers & KeyModifierControl) != 0) {
        if (auto* input = dynamic_cast<TextInput*>(focusManager_.focused())) {
            switch (event.keyCode) {
            case 67: // Ctrl+C
                if (input->copySelection(platformWindow_->clipboard())) {
                    syncTextInputSession();
                    return true;
                }
                break;
            case 88: // Ctrl+X
                if (input->cutSelection(platformWindow_->clipboard())) {
                    syncTextInputSession();
                    return true;
                }
                break;
            case 86: // Ctrl+V
                if (input->paste(platformWindow_->clipboard())) {
                    syncTextInputSession();
                    return true;
                }
                break;
            default:
                break;
            }
        }
    }
    const bool handled = inputRouter_.dispatchKey(event);
    syncTextInputSession();
    return handled;
}

bool UiWindow::dispatchTextInput(const TextInputEvent& event)
{
    const bool handled = inputRouter_.dispatchTextInput(event);
    syncTextInputSession();
    return handled;
}

bool UiWindow::dispatchComposition(const CompositionInputEvent& event)
{
    const bool handled = inputRouter_.dispatchComposition(event);
    syncTextInputSession();
    return handled;
}

void UiWindow::onPlatformFocusChanged(bool focused) noexcept
{
    if (!focused) {
        deactivateTextInputSession();
        inputRouter_.cancelPointerCapture();
        inputRouter_.clearHover();
    } else {
        syncTextInputSession();
    }
    platformWindow_->requestRedraw();
}

void UiWindow::syncActiveRoot(Node* navigationRoot) noexcept
{
    deactivateTextInputSession();
    focusManager_.clear();
    uiRoot_.setBorrowedContent(navigationRoot);
    inputRouter_.setRoot(uiRoot_.content());
    platformWindow_->requestRedraw();
}

void UiWindow::onOverlayChanged() noexcept
{
    // Overlay entries own their nodes. Any mutation may have destroyed the
    // node currently referenced by focus/hover routing, so invalidate those
    // non-owning pointers before the next event is delivered.
    deactivateTextInputSession();
    focusManager_.clear();
    inputRouter_.cancelPointerCapture();
    inputRouter_.clearHover();
    for (const auto& overlay : overlayHost_.overlays()) {
        if (overlay.content) {
            overlay.content->setInvalidationHandler([this] { platformWindow_->requestRedraw(); });
        }
    }
    platformWindow_->requestRedraw();
}

Dialog* UiWindow::activeDialog() const noexcept
{
    if (dialogs_.empty()) {
        return nullptr;
    }
    const auto id = dialogs_.back().id;
    for (const auto& overlay : overlayHost_.overlays()) {
        if (overlay.id == id) {
            return dynamic_cast<Dialog*>(overlay.content.get());
        }
    }
    return nullptr;
}

void UiWindow::syncTextInputSession() noexcept
{
    auto* focused = dynamic_cast<TextInput*>(focusManager_.focused());
    if (focused == activeTextInput_) {
        if (focused != nullptr) {
            focused->syncSession(platformWindow_->textInput(), focused->caretRect());
        }
        return;
    }

    deactivateTextInputSession();
    if (focused != nullptr) {
        activeTextInput_ = focused;
        auto& session = platformWindow_->textInput();
        session.activate();
        focused->syncSession(session, focused->caretRect());
    }
}

void UiWindow::deactivateTextInputSession() noexcept
{
    if (activeTextInput_ != nullptr) {
        platformWindow_->textInput().deactivate();
        activeTextInput_ = nullptr;
    }
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

std::size_t UiApp::removeClosedWindows() noexcept
{
    const auto previousSize = windows_.size();
    windows_.erase(
        std::remove_if(windows_.begin(), windows_.end(),
                       [](const std::unique_ptr<UiWindow>& window) {
                           return !window->platformWindow().isOpen();
                       }),
        windows_.end());
    return previousSize - windows_.size();
}

const std::vector<std::unique_ptr<UiWindow>>& UiApp::windows() const noexcept
{
    return windows_;
}

} // namespace wui
