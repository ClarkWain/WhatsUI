#include <memory>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "wui/wui.h"

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::fputs(message, stderr);
        std::fputc('\n', stderr);
        throw std::runtime_error(message);
    }
}

class FakeSurface final : public wui::RenderSurface {
public:
    [[nodiscard]] wui::CanvasBackend backend() const noexcept override { return wui::CanvasBackend::Software; }
    [[nodiscard]] wui::SizeF framebufferSize() const noexcept override { return {640.0f, 480.0f}; }
    void beginFrame() override {}
    void endFrame() override {}
    void resize(wui::SizeF) override {}
};

class FakeClipboard final : public wui::Clipboard {
public:
    void setText(std::string_view text) override { text_ = text; }
    [[nodiscard]] std::string getText() const override { return text_; }
    [[nodiscard]] bool hasText() const override { return !text_.empty(); }
private:
    std::string text_;
};

class FakeCursor final : public wui::CursorService {
public:
    void setCursor(wui::CursorIcon) override {}
};

class FakeTextInput final : public wui::TextInputSession {
public:
    void activate() override { ++activations; }
    void deactivate() override { ++deactivations; }
    void setCaretRect(const wui::RectF& rect) override { caretRect = rect; ++caretUpdates; }
    void setSurroundingText(std::string_view text, std::size_t start, std::size_t end) override
    {
        surroundingText = text;
        selectionStart = start;
        selectionEnd = end;
        ++surroundingUpdates;
    }
    int activations{0};
    int deactivations{0};
    int caretUpdates{0};
    int surroundingUpdates{0};
    wui::RectF caretRect{};
    std::string surroundingText;
    std::size_t selectionStart{0};
    std::size_t selectionEnd{0};
};

class FakeWindow final : public wui::PlatformWindow {
public:
    explicit FakeWindow(wui::WindowId id) : id_(id) {}
    [[nodiscard]] wui::WindowId id() const noexcept override { return id_; }
    [[nodiscard]] wui::WindowMetrics metrics() const noexcept override { return {{320.0f, 180.0f}, {640.0f, 360.0f}, 2.0f}; }
    void show() override { open_ = true; }
    void close() override { open_ = false; }
    [[nodiscard]] bool isOpen() const noexcept override { return open_; }
    [[nodiscard]] bool isFocused() const noexcept override { return true; }
    void setTitle(std::string_view) override {}
    void requestRedraw() override { ++redraws; }
    [[nodiscard]] wui::RenderSurface& surface() override { return surface_; }
    [[nodiscard]] wui::Clipboard& clipboard() override { return clipboard_; }
    [[nodiscard]] wui::CursorService& cursor() override { return cursor_; }
    [[nodiscard]] wui::TextInputSession& textInput() override { return textInput_; }

    [[nodiscard]] FakeTextInput& fakeTextInput() noexcept { return textInput_; }

    int redraws{0};
private:
    wui::WindowId id_;
    bool open_{true};
    FakeSurface surface_;
    FakeClipboard clipboard_;
    FakeCursor cursor_;
    FakeTextInput textInput_;
};

class FakeHost final : public wui::PlatformHost {
public:
    [[nodiscard]] std::unique_ptr<wui::PlatformWindow> createWindow(std::string, wui::SizeF) override
    {
        return std::make_unique<FakeWindow>(nextId_++);
    }
    [[nodiscard]] int run() override { return 0; }
    void quit(int exitCode = 0) override { exitCode_ = exitCode; }
private:
    wui::WindowId nextId_{1};
    int exitCode_{0};
};

wui::PointerEvent pointer(wui::PointerAction action)
{
    return {1, wui::PointerType::Mouse, action,
            (action == wui::PointerAction::Down || action == wui::PointerAction::Up) ? wui::MouseButton::Left : wui::MouseButton::None,
            {12.0f, 12.0f}, 0};
}

class CaptureProbe final : public wui::Node {
public:
    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({100.0f, 64.0f});
    }

    void paint(wui::PaintContext&) override { clearDirty(wui::DirtyFlag::Paint); }

    bool onPointerEvent(const wui::PointerEvent& event) override
    {
        switch (event.action) {
        case wui::PointerAction::Down: ++downs; break;
        case wui::PointerAction::Move: ++moves; break;
        case wui::PointerAction::Up: ++ups; break;
        case wui::PointerAction::Cancel: ++cancels; break;
        default: break;
        }
        return event.action != wui::PointerAction::Enter && event.action != wui::PointerAction::Leave;
    }

    int downs{0};
    int moves{0};
    int ups{0};
    int cancels{0};
};

class CaptureHost final : public wui::ContainerNode {
public:
    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({320.0f, 180.0f});
    }
};

void testPointerCaptureCancelsForWindowOverlayAndDetach()
{
    wui::UiApp app(std::make_unique<FakeHost>());
    auto& window = app.openWindow("capture", {320.0f, 180.0f});

    auto probe = std::make_unique<CaptureProbe>();
    auto* probeRaw = probe.get();
    window.setRoot(std::move(probe));
    window.layout();
    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "Pointer down should start a capture gesture");
    expect(window.inputRouter().capturedPointer() == probeRaw, "Handled primary down must capture its target");

    auto outside = pointer(wui::PointerAction::Move);
    outside.position = {800.0f, 600.0f};
    expect(window.dispatchPointer(outside), "Captured move outside hit bounds must still be delivered");
    expect(probeRaw->moves == 1, "Capture target must receive outside move");

    window.onPlatformFocusChanged(false);
    expect(probeRaw->cancels == 1 && window.inputRouter().capturedPointer() == nullptr,
           "Native focus loss must cancel and release the active gesture exactly once");

    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "A new gesture should start after focus cancellation");
    const auto overlay = window.overlayHost().show(std::make_unique<wui::Button>("Overlay"));
    expect(probeRaw->cancels == 2 && window.inputRouter().capturedPointer() == nullptr,
           "Overlay mutations must cancel page capture before changing the input layer");
    (void)window.overlayHost().dismiss(overlay);

    auto host = std::make_unique<CaptureHost>();
    auto child = std::make_unique<CaptureProbe>();
    auto* hostRaw = host.get();
    auto* childRaw = child.get();
    child->layout({0.0f, 0.0f, 100.0f, 64.0f});
    host->appendChild(std::move(child));
    window.setRoot(std::move(host));
    window.layout();
    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "Attached child should begin a captured gesture");
    expect(window.inputRouter().capturedPointer() == childRaw, "Child should own the active capture");
    auto retained = hostRaw->removeChild(0);
    expect(childRaw->cancels == 1 && window.inputRouter().capturedPointer() == nullptr,
           "Detaching a captured node must synchronously cancel and release it");

    auto button = std::make_unique<wui::Button>("No drag-out activation");
    int clicks = 0;
    button->onClick([&] { ++clicks; });
    window.setRoot(std::move(button));
    window.layout();
    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "Button press should be handled");
    auto upOutside = pointer(wui::PointerAction::Up);
    upOutside.position = {800.0f, 600.0f};
    expect(window.dispatchPointer(upOutside), "Captured button up should be delivered outside bounds");
    expect(clicks == 0 && window.inputRouter().capturedPointer() == nullptr,
           "Release outside must not activate and must release the capture");
}

void testWindowRoutesTopOverlayAndRequestsRedraw()
{
    wui::UiApp app(std::make_unique<FakeHost>());
    auto& window = app.openWindow("test", {320.0f, 180.0f});
    auto& platform = static_cast<FakeWindow&>(window.platformWindow());
    expect(app.findWindow(window.id()) == &window, "UiApp should retain the window created by its host");

    int pageClicks = 0;
    auto page = std::make_unique<wui::Button>("Page");
    page->onClick([&] { ++pageClicks; });
    window.navigator().setRoot("page", std::move(page));
    expect(platform.redraws > 0, "Navigator activation should request a redraw");

    window.layout();
    expect(window.root()->bounds().width == 320.0f, "Window layout should use logical platform dimensions");

    const int redrawsBeforeStateChange = platform.redraws;
    auto* pageButton = dynamic_cast<wui::Button*>(window.root());
    expect(pageButton != nullptr, "The page root should be the configured button");
    pageButton->setLabel("Changed outside event dispatch");
    expect(platform.redraws > redrawsBeforeStateChange,
           "Retained-node invalidation should request a redraw without a new input event");

    int overlayClicks = 0;
    auto overlay = std::make_unique<wui::Button>("Overlay");
    overlay->onClick([&] { ++overlayClicks; });
    const auto overlayId = window.overlayHost().show(std::move(overlay));
    const int redrawsAfterShow = platform.redraws;
    window.layout();

    auto* overlayButton = dynamic_cast<wui::Button*>(window.overlayHost().top()->content.get());
    expect(overlayButton != nullptr, "The overlay should retain the configured button");
    overlayButton->setLabel("Overlay changed outside event dispatch");
    expect(platform.redraws > redrawsAfterShow,
           "Overlay-node invalidation should request a redraw without a new input event");

    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "Overlay pointer down should be handled");
    expect(window.dispatchPointer(pointer(wui::PointerAction::Up)), "Overlay pointer up should be handled");
    expect(overlayClicks == 1 && pageClicks == 0, "The top overlay must receive pointer input before page content");

    (void)window.overlayHost().dismiss(overlayId);
    expect(platform.redraws > redrawsAfterShow, "Overlay removal should request a redraw");
    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "Page pointer down should be handled after dismissal");
    expect(window.dispatchPointer(pointer(wui::PointerAction::Up)), "Page pointer up should be handled after dismissal");
    expect(pageClicks == 1, "Page content should receive input after the overlay is dismissed");
}

void testWindowCoordinatesTextInputSession()
{
    wui::UiApp app(std::make_unique<FakeHost>());
    auto& window = app.openWindow("input", {320.0f, 180.0f});
    auto& platform = static_cast<FakeWindow&>(window.platformWindow());
    auto input = std::make_unique<wui::TextInput>("Type here");
    auto* inputPtr = input.get();
    window.setRoot(std::move(input));
    window.layout();

    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "Text input should accept focus pointer");
    auto& session = platform.fakeTextInput();
    expect(session.activations == 1, "Focusing a TextInput should activate its platform session");
    expect(session.caretUpdates == 1 && session.surroundingUpdates == 1,
           "Focusing a TextInput should immediately synchronize caret and surrounding text");
    expect(session.caretRect.x == inputPtr->bounds().x + wui::theme().controls.horizontalPadding,
           "Caret rect should be expressed in logical window coordinates");

    expect(window.dispatchComposition({window.id(), "ni", wui::CompositionInputEvent::Phase::Start}),
           "Composition start should route to the focused TextInput");
    expect(session.surroundingText == "ni" && session.selectionEnd == 2,
           "Composition updates should synchronize transient text and selection");
    expect(window.dispatchComposition({window.id(), "", wui::CompositionInputEvent::Phase::End}),
           "Composition end should route to the focused TextInput");
    expect(inputPtr->model().composition().empty(), "Only explicit composition end should clear pre-edit state");

    window.setRoot(std::make_unique<wui::Button>("Not text"));
    expect(session.deactivations == 1, "Replacing a focused text root should deactivate the session");
}

void testWindowSuspendsAndRestoresTextInputOnPlatformFocusChange()
{
    wui::UiApp app(std::make_unique<FakeHost>());
    auto& window = app.openWindow("focus", {320.0f, 180.0f});
    auto& platform = static_cast<FakeWindow&>(window.platformWindow());
    auto input = std::make_unique<wui::TextInput>("Type here");
    auto* inputPtr = input.get();
    window.setRoot(std::move(input));
    window.layout();
    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "Text input should accept focus before deactivation");

    const int redrawsBefore = platform.redraws;
    window.onPlatformFocusChanged(false);
    expect(platform.fakeTextInput().deactivations == 1,
           "Native focus loss must deactivate the platform text-input session");
    expect(window.focusManager().focused() == inputPtr,
           "Native focus loss should retain logical focus for restoration");
    expect(platform.redraws > redrawsBefore, "Native focus changes should request a redraw");

    window.onPlatformFocusChanged(true);
    expect(platform.fakeTextInput().activations == 2,
           "Native focus gain must reactivate the logically focused TextInput");
}

void testWindowRoutesClipboardShortcutsToFocusedTextInput()
{
    wui::UiApp app(std::make_unique<FakeHost>());
    auto& window = app.openWindow("clipboard", {320.0f, 180.0f});
    auto input = std::make_unique<wui::TextInput>();
    input->text("alpha");
    auto* inputPtr = input.get();
    window.setRoot(std::move(input));
    window.layout();
    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "Text input should accept focus before clipboard shortcuts");

    inputPtr->controller().setSelection({1, 4});
    const auto ctrlC = wui::KeyEvent{window.id(), wui::KeyAction::Down, 67, wui::KeyModifierControl, false};
    const auto ctrlX = wui::KeyEvent{window.id(), wui::KeyAction::Down, 88, wui::KeyModifierControl, false};
    const auto ctrlV = wui::KeyEvent{window.id(), wui::KeyAction::Down, 86, wui::KeyModifierControl, false};
    expect(window.dispatchKey(ctrlC), "UiWindow should route Ctrl+C to the focused TextInput");
    expect(window.platformWindow().clipboard().getText() == "lph", "Ctrl+C should copy the selected text through the platform clipboard");
    expect(window.dispatchKey(ctrlX), "UiWindow should route Ctrl+X to the focused TextInput");
    expect(inputPtr->controller().text() == "aa", "Ctrl+X should remove the selected text");
    window.platformWindow().clipboard().setText("LPH");
    expect(window.dispatchKey(ctrlV), "UiWindow should route Ctrl+V to the focused TextInput");
    expect(inputPtr->controller().text() == "aLPHa", "Ctrl+V should insert the platform clipboard content at the caret");
}

void testModalDialogBlocksPointerClosesOnEscapeAndRestoresFocus()
{
    wui::UiApp app(std::make_unique<FakeHost>());
    auto& window = app.openWindow("dialog", {320.0f, 180.0f});
    int pageClicks = 0;
    auto page = std::make_unique<wui::Button>("Page");
    auto* pageButton = page.get();
    page->onClick([&] { ++pageClicks; });
    window.setRoot(std::move(page));
    window.layout();
    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "Page should accept focus before showing a dialog");
    expect(window.focusManager().focused() == pageButton, "The page control should be focused before opening a dialog");

    int dialogKeyboardActivations = 0;
    auto dialog = std::make_unique<wui::Dialog>();
    auto confirm = std::make_unique<wui::Button>("Confirm");
    auto* confirmButton = confirm.get();
    confirm->onClick([&] { ++dialogKeyboardActivations; });
    dialog->content(std::move(confirm));
    const auto id = window.showDialog(std::move(dialog));
    window.layout();
    expect(window.hasDialog(), "showDialog should make a modal active");
    expect(window.focusManager().focused() == nullptr, "Opening a modal should isolate prior focus");

    // Tab and activation must target the modal subtree, never the obscured
    // page. This is the keyboard counterpart to the backdrop pointer test.
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 9, 0, false}),
           "Tab should enter the dialog's focus order");
    expect(window.focusManager().focused() == confirmButton,
           "The modal's first action should receive keyboard focus");
    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 13, 0, false}),
           "Enter should activate the focused dialog action");
    expect(dialogKeyboardActivations == 1 && pageClicks == 0,
           "Keyboard activation must not leak through a modal to the page");

    expect(window.dispatchPointer(pointer(wui::PointerAction::Down)), "The dialog backdrop should consume pointer down");
    expect(window.dispatchPointer(pointer(wui::PointerAction::Up)), "The dialog backdrop should consume pointer up");
    expect(pageClicks == 0, "A modal backdrop must block pointer input to the page");

    expect(window.dispatchKey({window.id(), wui::KeyAction::Down, 27, 0, false}), "Escape should be consumed by an active dialog");
    expect(!window.hasDialog(), "Escape should close the top dialog");
    expect(window.overlayHost().top() == nullptr, "Escape should remove the dialog overlay");
    expect(window.focusManager().focused() == pageButton, "Closing a dialog should restore its previous focus");
    expect(window.dismissDialog(id) == nullptr, "Dismissing an already closed dialog should be a safe no-op");
}

void testDeclarativeDialogBuilderProducesConcreteModal()
{
    auto dialog = wui::ui::Dialog().maxWidth(300.0f).dismissOnBackdrop().content(
        wui::ui::Text("Dialog content"))
                      .intoDialog();
    expect(dialog->maxWidth() == 300.0f, "Dialog builder should configure max width");
    expect(dialog->backdropDismissEnabled(), "Dialog builder should configure backdrop dismissal");
    expect(dialog->children().size() == 1, "Dialog builder should transfer its content subtree");
}

void testAppReleasesClosedWindows()
{
    wui::UiApp app(std::make_unique<FakeHost>());
    auto& first = app.openWindow("first", {320.0f, 180.0f});
    const auto closedId = first.id();
    auto& second = app.openWindow("second", {320.0f, 180.0f});
    const auto openId = second.id();

    first.platformWindow().close();
    expect(app.removeClosedWindows() == 1, "Closing a platform window should release its UiApp entry");
    expect(app.findWindow(closedId) == nullptr, "Closed window must no longer be discoverable");
    expect(app.findWindow(openId) == &second, "Open windows must remain managed after closed peers are removed");
    expect(app.removeClosedWindows() == 0, "Collecting an already-clean app should be a no-op");
}

} // namespace

int main()
{
    try {
        testWindowRoutesTopOverlayAndRequestsRedraw();
        testPointerCaptureCancelsForWindowOverlayAndDetach();
        testWindowCoordinatesTextInputSession();
        testWindowSuspendsAndRestoresTextInputOnPlatformFocusChange();
        testWindowRoutesClipboardShortcutsToFocusedTextInput();
        testModalDialogBlocksPointerClosesOnEscapeAndRestoresFocus();
        testDeclarativeDialogBuilderProducesConcreteModal();
        testAppReleasesClosedWindows();
        return 0;
    } catch (const std::exception& error) {
        std::fputs(error.what(), stderr);
        std::fputc('\n', stderr);
        return 1;
    } catch (...) {
        std::fputs("Unknown window-test failure\n", stderr);
        return 1;
    }
}
