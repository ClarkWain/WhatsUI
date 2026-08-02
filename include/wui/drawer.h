#pragma once

// Fluent DrawerNode. Drawers are surfaces attached to a window edge: inline
// drawers participate in their parent's layout, while overlay drawers own a
// full-window input boundary and optionally a modal scrim.

#include <functional>
#include <memory>
#include <string>

#include "wui/widgets.h"

namespace wui {

enum class DrawerType { Inline, Overlay };
enum class DrawerPosition { Start, End, Bottom };
enum class DrawerSize { Small, Medium, Large, Full };

class DrawerNode : public ContainerNode {
public:
    using DismissHandler = std::function<void()>;
    using ActionHandler = std::function<void()>;

    DrawerNode(std::string title = {}, std::string subtitle = {});

    DrawerNode& content(std::unique_ptr<Node> value);
    DrawerNode& title(std::string value);
    DrawerNode& subtitle(std::string value);
    DrawerNode& primaryAction(std::string label, ActionHandler handler = {});
    DrawerNode& secondaryAction(std::string label, ActionHandler handler = {});
    DrawerNode& type(DrawerType value) noexcept;
    DrawerNode& position(DrawerPosition value) noexcept;
    DrawerNode& size(DrawerSize value) noexcept;
    DrawerNode& width(float logicalPixels) noexcept;
    DrawerNode& modal(bool value = true) noexcept;
    DrawerNode& dismissOnOutsidePress(bool value = true) noexcept;
    DrawerNode& closeOnEscape(bool value = true) noexcept;
    DrawerNode& onDismiss(DismissHandler handler);

    [[nodiscard]] const std::string& title() const noexcept;
    [[nodiscard]] const std::string& subtitle() const noexcept;
    [[nodiscard]] DrawerType type() const noexcept;
    [[nodiscard]] DrawerPosition position() const noexcept;
    [[nodiscard]] DrawerSize size() const noexcept;
    [[nodiscard]] bool isModal() const noexcept;
    [[nodiscard]] bool dismissesOnOutsidePress() const noexcept;
    [[nodiscard]] bool closesOnEscape() const noexcept;
    // Modal overlay drawers form a keyboard boundary. UiWindow/OverlayHost
    // should focus the DrawerNode when it is shown and restore the trigger focus
    // after dismissing it.
    [[nodiscard]] bool trapsFocus() const noexcept;
    [[nodiscard]] const RectF& panelBounds() const noexcept;
    [[nodiscard]] const RectF& contentBounds() const noexcept;
    [[nodiscard]] float contentScrollOffset() const noexcept;
    [[nodiscard]] float maxContentScrollOffset() const noexcept;
    void setContentScrollOffset(float value) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    void dismiss();

private:
    friend class OverlayHost;
    [[nodiscard]] float desiredExtent(const RectF& host) const noexcept;
    [[nodiscard]] RectF resolvePanel(const RectF& host) const noexcept;
    [[nodiscard]] RectF closeBounds() const noexcept;
    [[nodiscard]] RectF primaryBounds() const noexcept;
    [[nodiscard]] RectF secondaryBounds() const noexcept;
    [[nodiscard]] float headerHeight() const noexcept;
    [[nodiscard]] float footerHeight() const noexcept;
    void clampScrollOffset() noexcept;
    void invoke(ActionHandler& action);
    // OverlayHost installs this ownership callback when it adopts a modal
    // DrawerNode. It is intentionally independent from the author's onDismiss
    // handler, so application callbacks cannot accidentally bypass removal.
    void setOverlayDismissHandler(DismissHandler handler) noexcept;

    std::string title_;
    std::string subtitle_;
    std::string primaryLabel_;
    std::string secondaryLabel_;
    ActionHandler primaryHandler_;
    ActionHandler secondaryHandler_;
    DismissHandler dismissHandler_;
    DismissHandler overlayDismissHandler_;
    DrawerType type_{DrawerType::Overlay};
    DrawerPosition position_{DrawerPosition::End};
    DrawerSize size_{DrawerSize::Medium};
    float explicitExtent_{0.0f};
    bool modal_{true};
    bool dismissOnOutsidePress_{true};
    bool closeOnEscape_{true};
    RectF panelBounds_{};
    RectF contentBounds_{};
    SizeF contentSize_{};
    float contentScrollOffset_{0.0f};
};

} // namespace wui
