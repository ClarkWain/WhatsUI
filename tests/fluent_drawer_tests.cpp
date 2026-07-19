#include <iostream>
#include <memory>
#include <stdexcept>

#include "wui/accessibility.h"
#include "wui/app.h"
#include "wui/drawer.h"
#include "wui/runtime.h"
#include "wui/theme.h"
#include "wui/widgets.h"

namespace {
void expect(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

class TestSurface final : public wui::RenderSurface {
public:
    [[nodiscard]] wui::CanvasBackend backend() const noexcept override { return wui::CanvasBackend::Software; }
    [[nodiscard]] wui::SizeF framebufferSize() const noexcept override { return {640, 480}; }
    void beginFrame() override {} void endFrame() override {} void resize(wui::SizeF) override {}
};
class TestClipboard final : public wui::Clipboard {
public:
    void setText(std::string_view value) override { value_ = value; }
    [[nodiscard]] std::string getText() const override { return value_; }
    [[nodiscard]] bool hasText() const override { return !value_.empty(); }
private: std::string value_;
};
class TestCursor final : public wui::CursorService { public: void setCursor(wui::CursorIcon) override {} };
class TestTextInput final : public wui::TextInputSession {
public:
    void activate() override {} void deactivate() override {} void setCaretRect(const wui::RectF&) override {}
    void setSurroundingText(std::string_view, std::size_t, std::size_t) override {}
};
class TestWindow final : public wui::PlatformWindow {
public:
    [[nodiscard]] wui::WindowId id() const noexcept override { return 1; }
    [[nodiscard]] wui::WindowMetrics metrics() const noexcept override { return {{640, 480}, {640, 480}, 1}; }
    void show() override {} void close() override {} [[nodiscard]] bool isOpen() const noexcept override { return true; }
    [[nodiscard]] bool isFocused() const noexcept override { return true; } void setTitle(std::string_view) override {} void requestRedraw() override {}
    [[nodiscard]] wui::RenderSurface& surface() override { return surface_; }
    [[nodiscard]] wui::Clipboard& clipboard() override { return clipboard_; }
    [[nodiscard]] wui::CursorService& cursor() override { return cursor_; }
    [[nodiscard]] wui::TextInputSession& textInput() override { return text_; }
private: TestSurface surface_; TestClipboard clipboard_; TestCursor cursor_; TestTextInput text_;
};

void testOverlayGeometryAndModalDismissal()
{
    int dismissed = 0;
    wui::Drawer drawer("Account settings", "Manage your profile");
    drawer.size(wui::DrawerSize::Small).position(wui::DrawerPosition::End).onDismiss([&] { ++dismissed; });
    drawer.layout({0, 0, 1000, 700});
    expect(drawer.panelBounds().x == 680 && drawer.panelBounds().width == 320 && drawer.trapsFocus(),
           "End overlay Drawer must reserve Fluent Small width and trap focus when modal");
    expect(drawer.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {20, 20}}) && dismissed == 1,
           "Modal overlay Drawer must consume and dismiss a backdrop press by default");
    expect(drawer.onKeyEvent({0, wui::KeyAction::Down, 27}) && dismissed == 2,
           "Drawer Escape must dismiss without relying on a platform-specific dialog path");
}

void testInlineAndNonModalPolicies()
{
    wui::Drawer drawer("Details");
    drawer.type(wui::DrawerType::Inline).position(wui::DrawerPosition::Start).width(288).modal(false);
    drawer.layout({12, 20, 288, 500});
    expect(drawer.panelBounds().x == 12 && drawer.panelBounds().width == 288 && !drawer.trapsFocus(),
           "Inline Drawer must use its allocated layout rect and never create a modal focus boundary");
    expect(!drawer.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {400, 40}}),
           "Inline Drawer must not intercept input outside its own layout bounds");
}

void testBodyScrollAndActions()
{
    int primary = 0, secondary = 0, dismissed = 0;
    auto content = std::make_unique<wui::Spacer>(wui::SizeF{240, 2000});
    wui::Drawer drawer("Long content");
    drawer.content(std::move(content)).primaryAction("Save", [&] { ++primary; }).secondaryAction("Cancel", [&] { ++secondary; })
        .onDismiss([&] { ++dismissed; });
    drawer.layout({0, 0, 800, 600});
    expect(drawer.maxContentScrollOffset() > 1000, "Drawer body must expose a clipped scrollable viewport for long content");
    drawer.setContentScrollOffset(99999);
    expect(drawer.contentScrollOffset() == drawer.maxContentScrollOffset(), "Drawer scroll offset must clamp at content end");
    const auto secondaryBounds = wui::RectF{drawer.panelBounds().x + drawer.panelBounds().width - 24 - 80 - 12 - 80,
                                            drawer.panelBounds().y + drawer.panelBounds().height - 24 - wui::theme().controls.height, 80, wui::theme().controls.height};
    // Invoke through the public action area rather than depending on rendering.
    drawer.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left,
                           {secondaryBounds.x + 8, secondaryBounds.y + 8}});
    expect(secondary == 1 && dismissed == 1 && primary == 0, "Secondary action must invoke then dismiss the Drawer");
}

void testOverlayHostFocusLifecycle()
{
    wui::FocusManager focus;
    wui::OverlayHost host;
    host.bindFocusManager(focus);
    wui::Button trigger("Open drawer");
    focus.setFocused(&trigger);

    auto drawer = std::make_unique<wui::Drawer>("Settings");
    wui::Drawer* raw = drawer.get();
    const auto id = host.show(std::move(drawer));
    expect(id != 0 && host.focused() == raw,
           "OverlayHost must move focus into a modal Drawer as it is shown");
    expect(raw->onKeyEvent({0, wui::KeyAction::Down, 27}) && host.empty() && host.focused() == &trigger,
           "Drawer Escape must remove its hosted overlay and restore trigger focus safely");

    auto nonModal = std::make_unique<wui::Drawer>("Preview");
    nonModal->modal(false);
    (void)host.show(std::move(nonModal));
    expect(host.focused() == &trigger,
           "Non-modal Drawer must not steal keyboard focus from the invoking control");
    (void)host.dismissTop();

    auto disposable = std::make_unique<wui::Drawer>("Disposable");
    (void)host.show(std::move(disposable));
    expect(host.focused() != &trigger, "Modal Drawer must own focus before host-wide clear");
    host.clear();
    expect(host.focused() == &trigger, "OverlayHost::clear must not leave focus pointing into a removed modal Drawer");
}

void testAccessibilityDialogSemantics()
{
    auto root = std::make_unique<wui::Container>();
    auto drawer = std::make_unique<wui::Drawer>("Notifications", "Manage alert delivery");
    drawer->type(wui::DrawerType::Inline);
    root->appendChild(std::move(drawer));
    root->layout({0, 0, 360, 400});
    const auto snapshot = wui::snapshotAccessibilityTree(*root);
    bool found = false;
    for (const auto& entry : snapshot) {
        if (entry.properties.role == wui::AccessibilityRole::Dialog && entry.properties.label == "Notifications") {
            found = entry.properties.description == "Manage alert delivery" && entry.properties.actions.focus;
        }
    }
    expect(found, "Drawer must project an accessible Dialog with a name, description and focus action");
}

void testUiWindowModalIsolation()
{
    wui::UiWindow window(std::make_unique<TestWindow>());
    auto page = std::make_unique<wui::Container>();
    auto trigger = std::make_unique<wui::Button>("Page action");
    wui::Button* triggerRaw = trigger.get(); page->appendChild(std::move(trigger)); window.setRoot(std::move(page));
    window.focusManager().setFocused(triggerRaw);
    auto drawer = std::make_unique<wui::Drawer>("Secure settings");
    auto drawerChild = std::make_unique<wui::Button>("Apply changes");
    wui::Button* drawerChildRaw = drawerChild.get();
    drawer->content(std::move(drawerChild));
    wui::Drawer* drawerRaw = drawer.get(); (void)window.overlayHost().show(std::move(drawer));
    expect(window.focusManager().focused() == drawerRaw, "UiWindow must receive Drawer focus after OverlayHost adoption");
    const auto snapshot = window.accessibilitySnapshot();
    bool drawerVisible = false, pageVisible = false;
    for (const auto& entry : snapshot) { drawerVisible = drawerVisible || entry.properties.label == "Secure settings"; pageVisible = pageVisible || entry.properties.label == "Page action"; }
    expect(drawerVisible && !pageVisible, "Modal Drawer must replace the page in UiWindow accessibility navigation");
    window.focusManager().setFocused(drawerChildRaw);
    expect(window.dispatchKey({0, wui::KeyAction::Down, 27}) && window.overlayHost().empty() && window.focusManager().focused() == triggerRaw,
           "UiWindow Escape must dismiss a modal Drawer and restore page focus even from a focused drawer child");
}
} // namespace

int main()
{
    try {
        testOverlayGeometryAndModalDismissal();
        testInlineAndNonModalPolicies();
        testBodyScrollAndActions();
        testOverlayHostFocusLifecycle();
        testAccessibilityDialogSemantics();
        testUiWindowModalIsolation();
        std::cout << "Fluent drawer tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fluent drawer test failure: " << error.what() << '\n';
        return 1;
    }
}
