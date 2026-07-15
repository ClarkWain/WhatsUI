#pragma once

// Fluent desktop overlays and compact action controls.  These nodes are
// deliberately usable through OverlayHost directly: a host owns the overlay,
// while Popup keeps the anchored surface, dismissal and keyboard contract in
// one place.

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "wui/text_input.h"
#include "wui/widgets.h"

namespace wui {

enum class PopupPlacement {
    BelowStart,
    BelowEnd,
    AboveStart,
    AboveEnd,
};

class Popup : public ContainerNode {
public:
    using DismissHandler = std::function<void()>;

    Popup& content(std::unique_ptr<Node> content);
    Popup& anchor(RectF anchor) noexcept;
    Popup& placement(PopupPlacement placement) noexcept;
    Popup& preferredSize(SizeF size) noexcept;
    Popup& dismissOnOutsidePress(bool enabled = true) noexcept;
    Popup& onDismiss(DismissHandler handler);

    [[nodiscard]] const RectF& anchor() const noexcept;
    [[nodiscard]] const RectF& panelBounds() const noexcept;
    [[nodiscard]] PopupPlacement placement() const noexcept;
    [[nodiscard]] bool dismissOnOutsidePress() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

    virtual void dismiss();

protected:
    [[nodiscard]] RectF resolvePanelBounds(const RectF& hostBounds, SizeF desired) const noexcept;
    [[nodiscard]] const RectF& hostBounds() const noexcept;
    void paintSurface(PaintContext& context, const RectF& panel) const;

private:
    RectF anchor_{};
    RectF panelBounds_{};
    SizeF preferredSize_{280.0f, 0.0f};
    PopupPlacement placement_{PopupPlacement::BelowStart};
    bool dismissOnOutsidePress_{true};
    DismissHandler onDismiss_;
};

struct MenuItem {
    std::string label;
    std::string shortcut;
    bool enabled{true};
    std::function<void()> onInvoke;
};

// A native menu surface with deterministic keyboard navigation.  The owner
// may dismiss it via OverlayHost from onDismiss(); invoking an item runs the
// callback before dismissal so an action can safely update application state.
class Menu : public Popup {
public:
    Menu& addItem(MenuItem item);
    Menu& clearItems();
    Menu& onDismiss(DismissHandler handler);

    [[nodiscard]] const std::vector<MenuItem>& items() const noexcept;
    [[nodiscard]] int selectedIndex() const noexcept;
    void setSelectedIndex(int index) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    void dismiss() override;

private:
    void moveSelection(int delta) noexcept;
    void invokeSelection();
    [[nodiscard]] int itemAt(PointF point) const noexcept;
    [[nodiscard]] float rowHeight() const noexcept;

    std::vector<MenuItem> items_;
    int selectedIndex_{-1};
    DismissHandler onDismiss_;
};

// Tooltip owns a small anchored surface.  Pointer/focus code calls showAfter
// and advance() from its host frame clock; separating the clock avoids hidden
// threads and keeps test and deterministic-renderer behavior reproducible.
class Tooltip : public Popup {
public:
    Tooltip& text(std::string text);
    Tooltip& delay(std::chrono::milliseconds delay) noexcept;
    Tooltip& showAfter(std::chrono::milliseconds elapsed) noexcept;
    Tooltip& hide() noexcept;

    [[nodiscard]] const std::string& text() const noexcept;
    [[nodiscard]] bool isVisible() const noexcept;
    [[nodiscard]] std::chrono::milliseconds delay() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;

private:
    std::string text_;
    std::chrono::milliseconds delay_{500};
    std::chrono::milliseconds elapsed_{0};
    bool visible_{false};
};

// A compact Fluent command affordance.  icon is intentionally text-based for
// now so applications can use their own icon font or familiar glyphs without
// a separate asset pipeline; accessibleLabel remains available to platform
// accessibility bridges.
class IconButton : public ControlNode {
public:
    using ClickHandler = std::function<void()>;

    explicit IconButton(std::string icon = {}, std::string accessibleLabel = {});

    IconButton& icon(std::string value);
    IconButton& accessibleLabel(std::string value);
    // When present, the compact action is exposed as a two-state semantic
    // control while retaining IconButton visuals.
    IconButton& checked(bool value);
    IconButton& onClick(ClickHandler handler);
    void setIcon(std::string value);
    void setAccessibleLabel(std::string value);
    void setChecked(std::optional<bool> value) noexcept;

    [[nodiscard]] const std::string& icon() const noexcept;
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    [[nodiscard]] std::optional<bool> checked() const noexcept;
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    std::string icon_;
    std::string accessibleLabel_;
    std::optional<bool> checked_;
    ClickHandler onClick_;
};

// The standard query field keeps TextInput editing/IME semantics intact while
// providing the expected Windows Escape-to-clear behavior for search.
class SearchField : public TextInput {
public:
    explicit SearchField(std::string placeholder = "Search");

    SearchField& query(std::string value);
    SearchField& onQueryChange(ChangeHandler handler);
    [[nodiscard]] const std::string& query() const noexcept;
    bool onKeyEvent(const KeyEvent& event) override;
};

} // namespace wui
