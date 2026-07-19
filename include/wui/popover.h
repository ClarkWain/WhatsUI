#pragma once

// Fluent Popover and TeachingPopover surfaces.  Popover is deliberately a
// real Popup so applications can use an arbitrary anchor/trigger, while
// PopoverButton covers the common command-button trigger without requiring a
// separate event router.

#include <functional>
#include <memory>
#include <string>

#include "wui/basic_controls.h"
#include "wui/overlays.h"

namespace wui {

class OverlayHost;

enum class PopoverAppearance {
    Surface,
    Inverted,
    Brand,
};

class Popover : public Popup {
public:
    Popover(std::string title = {}, std::string body = {});

    Popover& title(std::string value);
    Popover& body(std::string value);
    Popover& appearance(PopoverAppearance value) noexcept;
    Popover& showArrow(bool value = true) noexcept;
    Popover& accessibleLabel(std::string value);

    [[nodiscard]] const std::string& title() const noexcept;
    [[nodiscard]] const std::string& body() const noexcept;
    [[nodiscard]] PopoverAppearance appearance() const noexcept;
    [[nodiscard]] bool hasArrow() const noexcept;
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    [[nodiscard]] RectF contentBounds() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;

protected:
    [[nodiscard]] virtual float headerHeight() const noexcept;
    [[nodiscard]] virtual float footerHeight() const noexcept;
    [[nodiscard]] virtual float bodyBottom() const noexcept;
    void paintArrow(PaintContext& context, const RectF& panel, Color color) const;
    [[nodiscard]] Color backgroundColor() const noexcept;
    [[nodiscard]] Color foregroundColor() const noexcept;

private:
    std::string title_;
    std::string body_;
    std::string accessibleLabel_;
    PopoverAppearance appearance_{PopoverAppearance::Surface};
    // Fluent v9's `withArrow` is opt-in. The default surface is deliberately
    // clean, avoiding a visually disconnected callout on ordinary menus.
    bool arrow_{false};
    RectF contentBounds_{};
};

// Owns only the trigger state; OverlayHost continues to own the transient
// Popover tree.  Factories make every open cycle a fresh node tree, avoiding
// stale focus or state after close/reopen.
class PopoverButton : public Button {
public:
    using PopoverFactory = std::function<std::unique_ptr<Popover>()>;

    explicit PopoverButton(std::string label = {});

    PopoverButton& bindOverlayHost(OverlayHost& host) noexcept;
    PopoverButton& popoverFactory(PopoverFactory factory);
    PopoverButton& popover(std::string title, std::string body = {});
    [[nodiscard]] bool isOpen() const noexcept;

    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

    void openPopover();
    void closePopover();

private:
    OverlayHost* overlayHost_{nullptr};
    PopoverFactory factory_;
    std::size_t overlayId_{0};
    bool open_{false};
};

enum class TeachingPopoverFocusPolicy {
    // Teach an anchored workflow without moving keyboard focus. Escape and
    // outside press still dismiss it, while Tab stays in the surrounding page.
    NonModal,
    // Default guided-step policy. The trigger focuses the dialog surface and
    // Tab stays within it until a visible action or dismissal closes it.
    TrapFocus,
};

class TeachingPopover : public Popover {
public:
    using ActionHandler = std::function<void()>;

    TeachingPopover(std::string title = {}, std::string body = {});

    TeachingPopover& primaryAction(std::string label, ActionHandler handler = {});
    TeachingPopover& secondaryAction(std::string label, ActionHandler handler = {});
    TeachingPopover& dismissLabel(std::string label);
    TeachingPopover& stepText(std::string value);
    TeachingPopover& focusPolicy(TeachingPopoverFocusPolicy value) noexcept;
    TeachingPopover& onDismiss(DismissHandler handler);

    [[nodiscard]] const std::string& primaryActionLabel() const noexcept;
    [[nodiscard]] const std::string& secondaryActionLabel() const noexcept;
    [[nodiscard]] const std::string& dismissLabel() const noexcept;
    [[nodiscard]] const std::string& stepText() const noexcept;
    [[nodiscard]] TeachingPopoverFocusPolicy focusPolicy() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    void dismiss() override;

private:
    [[nodiscard]] float headerHeight() const noexcept override;
    [[nodiscard]] float footerHeight() const noexcept override;
    [[nodiscard]] RectF primaryBounds() const noexcept;
    [[nodiscard]] RectF secondaryBounds() const noexcept;
    [[nodiscard]] RectF dismissBounds() const noexcept;
    void invoke(ActionHandler& handler);

    std::string primaryLabel_;
    std::string secondaryLabel_;
    std::string dismissLabel_{"Dismiss"};
    std::string stepText_;
    TeachingPopoverFocusPolicy focusPolicy_{TeachingPopoverFocusPolicy::TrapFocus};
    ActionHandler primaryHandler_;
    ActionHandler secondaryHandler_;
    DismissHandler dismissHandler_;
};

} // namespace wui
