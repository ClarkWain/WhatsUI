#include "wui/app.h"
#include "wui/scheduler.h"
#include "wui/text_input.h"
#include "wui/widgets.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace wui {
namespace {

NodeTreeStats collectTreeStats(const Node* node)
{
    NodeTreeStats stats{};
    if (node == nullptr) {
        return stats;
    }
    stats.nodes = 1;
    const bool styleDirty = node->isDirty(DirtyFlag::Style);
    const bool layoutDirty = node->isDirty(DirtyFlag::Layout);
    const bool paintDirty = node->isDirty(DirtyFlag::Paint);
    const bool compositingDirty = node->isDirty(DirtyFlag::Compositing);
    stats.dirtyNodes = (styleDirty || layoutDirty || paintDirty || compositingDirty) ? 1u : 0u;
    stats.styleDirty = styleDirty ? 1u : 0u;
    stats.layoutDirty = layoutDirty ? 1u : 0u;
    stats.paintDirty = paintDirty ? 1u : 0u;
    stats.compositingDirty = compositingDirty ? 1u : 0u;
    stats.textNodes = dynamic_cast<const Text*>(node) != nullptr ? 1u : 0u;
    for (const auto& child : node->children()) {
        const auto nested = collectTreeStats(child.get());
        stats.nodes += nested.nodes;
        stats.dirtyNodes += nested.dirtyNodes;
        stats.styleDirty += nested.styleDirty;
        stats.layoutDirty += nested.layoutDirty;
        stats.paintDirty += nested.paintDirty;
        stats.compositingDirty += nested.compositingDirty;
        stats.textNodes += nested.textNodes;
    }
    return stats;
}

NodeTreeStats collectOverlayStats(const OverlayHost& host)
{
    NodeTreeStats stats{};
    for (const auto& overlay : host.overlays()) {
        const auto nested = collectTreeStats(overlay.content.get());
        stats.nodes += nested.nodes;
        stats.dirtyNodes += nested.dirtyNodes;
        stats.styleDirty += nested.styleDirty;
        stats.layoutDirty += nested.layoutDirty;
        stats.paintDirty += nested.paintDirty;
        stats.compositingDirty += nested.compositingDirty;
        stats.textNodes += nested.textNodes;
    }
    return stats;
}

template <class Work>
double measureMilliseconds(Work&& work)
{
    const auto started = std::chrono::steady_clock::now();
    work();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    return std::chrono::duration<double, std::milli>(elapsed).count();
}

} // namespace

class UiWindow::EventDispatchScope {
public:
    explicit EventDispatchScope(UiWindow& window) noexcept
        : window_(window)
    {
        window_.beginEventDispatch();
    }

    ~EventDispatchScope()
    {
        window_.endEventDispatch();
    }

private:
    UiWindow& window_;
};

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

UiWindow::~UiWindow()
{
    // Break every non-owning tree relationship before C++ destroys members in
    // reverse declaration order. In particular, UiRoot can borrow a page
    // owned by Navigator, which would otherwise become dangling first.
    navigator_.setOnChange({});
    navigator_.setBeforeChange({});
    overlayHost_.setOnChange({});
    deactivateTextInputSession();
    focusManager_.clear();
    inputRouter_.setRoot(nullptr);
    uiRoot_.setBorrowedContent(nullptr);
    overlayHost_.clear();
    dialogs_.clear();
    navigator_.clear();
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
    raw->setWindowDismissHandler([this, id] { requestDialogDismissal(id); });
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
    if (eventDispatchDepth_ != 0) {
        requestDialogDismissal(id);
        return nullptr;
    }
    return dismissDialogImmediately(id);
}

std::unique_ptr<Dialog> UiWindow::dismissDialogImmediately(OverlayId id)
{
    auto it = std::find_if(dialogs_.begin(), dialogs_.end(), [id](const DialogEntry& entry) { return entry.id == id; });
    if (it == dialogs_.end()) {
        return nullptr;
    }
    // Modal ownership is a stack. Removing a dialog beneath the active one
    // would leave the nested dialog's restoreFocus pointing into a destroyed
    // subtree, so only the top entry may be dismissed.
    if (it != std::prev(dialogs_.end())) {
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

void UiWindow::beginEventDispatch() noexcept
{
    ++eventDispatchDepth_;
}

void UiWindow::endEventDispatch() noexcept
{
    if (eventDispatchDepth_ == 0) {
        return;
    }
    --eventDispatchDepth_;
    if (eventDispatchDepth_ == 0) {
        flushDeferredDialogDismissals();
    }
}

void UiWindow::requestDialogDismissal(OverlayId id)
{
    if (eventDispatchDepth_ == 0) {
        (void)dismissDialogImmediately(id);
        return;
    }

    if (std::find(deferredDialogDismissals_.begin(), deferredDialogDismissals_.end(), id)
        == deferredDialogDismissals_.end()) {
        deferredDialogDismissals_.push_back(id);
    }
}

void UiWindow::flushDeferredDialogDismissals() noexcept
{
    // A dialog's dismiss callback may request the same dialog multiple times
    // (for example through both an author callback and its window handler).
    // Requests are coalesced above. Sort them by current modal depth so an
    // outer-first callback cannot destroy the focus target retained by an
    // inner dialog. Unknown ids sort last and become safe no-ops.
    auto pending = std::move(deferredDialogDismissals_);
    deferredDialogDismissals_.clear();
    const auto depth = [this](OverlayId id) {
        const auto it = std::find_if(dialogs_.begin(), dialogs_.end(), [id](const DialogEntry& entry) {
            return entry.id == id;
        });
        return it == dialogs_.end() ? std::size_t{0}
                                    : static_cast<std::size_t>(std::distance(dialogs_.begin(), it)) + 1;
    };
    std::stable_sort(pending.begin(), pending.end(), [&depth](OverlayId left, OverlayId right) {
        return depth(left) > depth(right);
    });
    for (const auto id : pending) {
        (void)dismissDialogImmediately(id);
    }
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

AccessibilitySnapshot UiWindow::accessibilitySnapshot() const
{
    AccessibilitySnapshot snapshot;
    AccessibilityProperties application;
    application.role = AccessibilityRole::Application;
    application.label = platformWindow_->title();
    if (application.label.empty()) {
        application.label = "WhatsUI application";
    }
    snapshot.push_back({{}, 0, std::move(application)});

    // A modal is the active accessibility subtree. Exposing the dimmed page
    // alongside it would let automation clients navigate controls that the
    // input router deliberately blocks.
    const Node* activeRoot = activeDialog() != nullptr
        ? static_cast<const Node*>(activeDialog())
        : uiRoot_.content();
    if (activeRoot == nullptr) {
        return snapshot;
    }

    auto visualSnapshot = snapshotAccessibilityTree(*activeRoot, focusManager_.focused());
    for (auto& entry : visualSnapshot) {
        entry.path.insert(entry.path.begin(), 0);
        ++entry.depth;
        snapshot.push_back(std::move(entry));
    }
    return snapshot;
}

void UiWindow::update()
{
    ++frameStats_.frameNumber;
    frameStats_.layoutMilliseconds = 0.0;
    frameStats_.prepareMilliseconds = 0.0;
    frameStats_.paintMilliseconds = 0.0;
    frameStats_.page = {};
    frameStats_.overlays = {};
    frameStats_.render = {};
    frameStats_.updateMilliseconds = measureMilliseconds([] { flushStructuralUpdates(); });
}

void UiWindow::layout()
{
    const auto metrics = platformWindow_->metrics();
    const RectF bounds{0.0f, 0.0f, metrics.logicalSize.width, metrics.logicalSize.height};
    const RectF previousBounds = uiRoot_.bounds();
    const bool boundsChanged = previousBounds.x != bounds.x || previousBounds.y != bounds.y
        || previousBounds.width != bounds.width || previousBounds.height != bounds.height;
    const bool pageNeedsLayout = boundsChanged
        || (uiRoot_.content() != nullptr && uiRoot_.content()->isDirty(DirtyFlag::Layout));
    const bool overlaysNeedLayout = boundsChanged || std::any_of(
        overlayHost_.overlays().begin(), overlayHost_.overlays().end(),
        [](const OverlayEntry& overlay) {
            return overlay.content != nullptr && overlay.content->isDirty(DirtyFlag::Layout);
        });

    if (!pageNeedsLayout && !overlaysNeedLayout) {
        frameStats_.layoutMilliseconds = 0.0;
        return;
    }
    frameStats_.layoutMilliseconds = measureMilliseconds([&] {
        if (pageNeedsLayout) uiRoot_.layout(bounds);
        if (overlaysNeedLayout) overlayHost_.layout(bounds);
    });
}

void UiWindow::paint(PaintContext& context)
{
    context.resetPaintStats();
    frameStats_.paintMilliseconds = measureMilliseconds([&] {
        uiRoot_.paint(context);
        overlayHost_.paint(context);
    });
    frameStats_.page = collectTreeStats(uiRoot_.content());
    frameStats_.overlays = collectOverlayStats(overlayHost_);
    // These counts describe the framework's paint candidates, not renderer
    // command emission. Renderer counters are captured after the host ends
    // its render-surface frame through captureCompletedRendererStats().
    frameStats_.render.paintTraversalNodes = frameStats_.page.nodes + frameStats_.overlays.nodes;
    frameStats_.render.textNodes = frameStats_.page.textNodes + frameStats_.overlays.textNodes;
    frameStats_.render.paintOperations = context.paintStats();
}

void UiWindow::captureCompletedRendererStats(PaintContext& context)
{
#ifdef WHATSUI_HAS_WHATSCANVAS
    if (auto* canvas = context.canvas()) {
        const auto canvasStats = canvas->getRenderStats();
        auto& render = frameStats_.render;
        render.commandCount = {canvasStats.commandCount, CounterAvailability::Available};
        render.drawCalls = {canvasStats.drawCallCount, CounterAvailability::Available};
        // WhatsCanvas provides public geometry-cache snapshots. Its public
        // Canvas API does not expose glyph-atlas/text cache or text draw-call
        // counters, so those remain explicitly Unavailable rather than being
        // inferred from generic render commands.
        render.tessellationCacheHits = {canvasStats.tessellationCacheHits, CounterAvailability::Available};
        render.tessellationCacheMisses = {canvasStats.tessellationCacheMisses, CounterAvailability::Available};
        render.strokeCacheHits = {canvasStats.strokeCacheHits, CounterAvailability::Available};
        render.strokeCacheMisses = {canvasStats.strokeCacheMisses, CounterAvailability::Available};
    }
#else
    (void)context;
#endif
}

void UiWindow::prepare(PaintContext& context)
{
    frameStats_.prepareMilliseconds = measureMilliseconds([&] {
        uiRoot_.prepare(context);
        overlayHost_.prepare(context);
    });
}

const FrameStats& UiWindow::frameStats() const noexcept
{
    return frameStats_;
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
    bool handled = false;
    {
        EventDispatchScope dispatchScope(*this);
        handled = inputRouter_.dispatchPointerTo(hitTest(event.position), event);
    }
    syncTextInputSession();
    return handled;
}

bool UiWindow::dispatchKey(const KeyEvent& event)
{
    bool handled = false;
    {
        EventDispatchScope dispatchScope(*this);
        if (event.action == KeyAction::Down && event.keyCode == 27) {
            if (auto* dialog = activeDialog()) {
                dialog->dismiss();
                handled = true;
            }
        }

        // Clipboard ownership is a platform concern, while selection ownership is
        // a text-control concern. Keep this small bridge at the window boundary
        // so TextInput remains usable in headless/unit-test trees and native
        // backends never need widget-specific shortcut handling.
        if (!handled && event.action == KeyAction::Down && (event.modifiers & KeyModifierControl) != 0) {
            if (auto* input = dynamic_cast<TextInput*>(focusManager_.focused())) {
                switch (event.keyCode) {
                case 67: // Ctrl+C
                    if (input->copySelection(platformWindow_->clipboard())) {
                        handled = true;
                    }
                    break;
                case 88: // Ctrl+X
                    if (input->cutSelection(platformWindow_->clipboard())) {
                        handled = true;
                    }
                    break;
                case 86: // Ctrl+V
                    if (input->paste(platformWindow_->clipboard())) {
                        handled = true;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        if (!handled) {
            handled = inputRouter_.dispatchKey(event);
        }
    }
    syncTextInputSession();
    return handled;
}

bool UiWindow::dispatchTextInput(const TextInputEvent& event)
{
    bool handled = false;
    {
        EventDispatchScope dispatchScope(*this);
        handled = inputRouter_.dispatchTextInput(event);
    }
    syncTextInputSession();
    return handled;
}

bool UiWindow::dispatchComposition(const CompositionInputEvent& event)
{
    bool handled = false;
    {
        EventDispatchScope dispatchScope(*this);
        handled = inputRouter_.dispatchComposition(event);
    }
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
