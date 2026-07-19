#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "wui/node.h"

namespace wui {

class FocusManager;

enum class PageRetention {
    KeepAlive,
    DisposeOnHide,
};

class UiRoot {
public:
    ~UiRoot();

    void setOnInvalidate(std::function<void()> handler);
    void setContent(std::unique_ptr<Node> content) noexcept;
    void setBorrowedContent(Node* content) noexcept;
    [[nodiscard]] Node* content() const noexcept;

    void layout(const RectF& bounds);
    void prepare(PaintContext& context);
    void paint(PaintContext& context);

    [[nodiscard]] const RectF& bounds() const noexcept;

private:
    void wireInvalidationHandler() noexcept;

    std::unique_ptr<Node> ownedContent_;
    Node* content_{nullptr};
    RectF bounds_{};
    std::function<void()> onInvalidate_;
};

struct PageEntry {
    std::string key;
    PageRetention retention{PageRetention::KeepAlive};
    std::unique_ptr<Node> content;
    std::function<std::unique_ptr<Node>()> factory;
};

class Navigator {
public:
    using ChangeHandler = std::function<void(Node*)>;
    using BeforeChangeHandler = std::function<void()>;
    using PageFactory = std::function<std::unique_ptr<Node>()>;

    void setOnChange(ChangeHandler handler);
    void setBeforeChange(BeforeChangeHandler handler);
    void setRoot(std::string key, std::unique_ptr<Node> page, PageRetention retention = PageRetention::KeepAlive);
    void setRoot(std::string key, PageFactory factory, PageRetention retention);
    void push(std::string key, std::unique_ptr<Node> page, PageRetention retention = PageRetention::KeepAlive);
    void push(std::string key, PageFactory factory, PageRetention retention);
    void replace(std::string key, std::unique_ptr<Node> page, PageRetention retention = PageRetention::KeepAlive);
    void replace(std::string key, PageFactory factory, PageRetention retention);
    [[nodiscard]] std::unique_ptr<Node> pop();
    void popToRoot();
    void clear();

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool canPop() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] Node* current() const noexcept;
    [[nodiscard]] const std::string* currentKey() const noexcept;
    [[nodiscard]] const PageEntry* currentEntry() const noexcept;
    [[nodiscard]] const std::vector<PageEntry>& pages() const noexcept;

private:
    void notifyWillChange();
    void hideCurrent();
    void activateCurrent();
    void notifyChanged();

    std::vector<PageEntry> stack_;
    ChangeHandler onChange_;
    BeforeChangeHandler onBeforeChange_;
};

using OverlayId = std::size_t;

struct OverlayEntry {
    OverlayId id{0};
    std::unique_ptr<Node> content;
    // A modal overlay captures the existing keyboard owner. OverlayHost
    // restores this only after detaching the overlay, so a focused pointer can
    // never retain a child in a removed transient tree.
    Node* restoreFocus{nullptr};
};

class OverlayHost {
public:
    using ChangeHandler = std::function<void()>;

    ~OverlayHost();

    // UiWindow binds its focus manager once. Standalone/headless hosts may
    // omit this and retain their previous ownership-only behavior.
    void bindFocusManager(FocusManager& focusManager) noexcept;
    void focus(Node* node) noexcept;
    [[nodiscard]] Node* focused() const noexcept;
    void setOnChange(ChangeHandler handler);
    [[nodiscard]] OverlayId show(std::unique_ptr<Node> overlay);
    [[nodiscard]] std::unique_ptr<Node> dismiss(OverlayId id);
    [[nodiscard]] std::unique_ptr<Node> dismissTop();
    void clear() noexcept;
    void layout(const RectF& bounds);
    void prepare(PaintContext& context);
    void paint(PaintContext& context);
    [[nodiscard]] Node* hitTest(PointF point) const;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const OverlayEntry* top() const noexcept;
    [[nodiscard]] const std::vector<OverlayEntry>& overlays() const noexcept;

private:
    OverlayId nextId_{1};
    std::vector<OverlayEntry> overlays_;
    ChangeHandler onChange_;
    FocusManager* focusManager_{nullptr};
};

} // namespace wui
