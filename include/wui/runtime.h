#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "wui/view.h"

namespace wui {

class FocusManager;

enum class PageRetention {
    KeepAlive,
    DisposeOnHide,
};

class UiRoot {
public:
    explicit UiRoot(UiContext context = {});
    ~UiRoot();

    void setOnInvalidate(std::function<void()> handler);
    void setContent(std::unique_ptr<Node> content);
    template <
        class Content,
        std::enable_if_t<isViewLikeV<Content>, int> = 0>
    void setContent(Content&& content)
    {
        setContent(detail::materialize(std::forward<Content>(content)));
    }
    void setBorrowedContent(Node* content);
    [[nodiscard]] Node* content() const noexcept;

    void layout(const RectF& bounds);
    void prepare(PaintContext& context);
    void paint(PaintContext& context);

    [[nodiscard]] const RectF& bounds() const noexcept;

private:
    void requireOwnerThread() const;
    void wireInvalidationHandler() noexcept;

    std::unique_ptr<Node> ownedContent_;
    Node* content_{nullptr};
    RectF bounds_{};
    std::function<void()> onInvalidate_;
    UiContext context_;
    std::thread::id ownerThread_;
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

    explicit Navigator(UiContext context = {});

    void setOnChange(ChangeHandler handler);
    void setBeforeChange(BeforeChangeHandler handler);
    void setRoot(std::string key, std::unique_ptr<Node> page, PageRetention retention = PageRetention::KeepAlive);
    void setRoot(std::string key, PageFactory factory, PageRetention retention);
    template <
        class Page,
        std::enable_if_t<isViewLikeV<Page>, int> = 0>
    void setRoot(
        std::string key,
        Page&& page,
        PageRetention retention = PageRetention::KeepAlive)
    {
        setRoot(
            std::move(key),
            detail::materialize(std::forward<Page>(page)),
            retention);
    }
    template <
        class Factory,
        std::enable_if_t<
            !isViewLikeV<Factory>
            && detail::IsViewFactory<Factory>::value,
            int> = 0>
    void setRoot(
        std::string key,
        Factory&& factory,
        PageRetention retention)
    {
        setRoot(
            std::move(key),
            eraseFactory(std::forward<Factory>(factory)),
            retention);
    }
    void push(std::string key, std::unique_ptr<Node> page, PageRetention retention = PageRetention::KeepAlive);
    void push(std::string key, PageFactory factory, PageRetention retention);
    template <
        class Page,
        std::enable_if_t<isViewLikeV<Page>, int> = 0>
    void push(
        std::string key,
        Page&& page,
        PageRetention retention = PageRetention::KeepAlive)
    {
        push(
            std::move(key),
            detail::materialize(std::forward<Page>(page)),
            retention);
    }
    template <
        class Factory,
        std::enable_if_t<
            !isViewLikeV<Factory>
            && detail::IsViewFactory<Factory>::value,
            int> = 0>
    void push(
        std::string key,
        Factory&& factory,
        PageRetention retention)
    {
        push(
            std::move(key),
            eraseFactory(std::forward<Factory>(factory)),
            retention);
    }
    void replace(std::string key, std::unique_ptr<Node> page, PageRetention retention = PageRetention::KeepAlive);
    void replace(std::string key, PageFactory factory, PageRetention retention);
    template <
        class Page,
        std::enable_if_t<isViewLikeV<Page>, int> = 0>
    void replace(
        std::string key,
        Page&& page,
        PageRetention retention = PageRetention::KeepAlive)
    {
        replace(
            std::move(key),
            detail::materialize(std::forward<Page>(page)),
            retention);
    }
    template <
        class Factory,
        std::enable_if_t<
            !isViewLikeV<Factory>
            && detail::IsViewFactory<Factory>::value,
            int> = 0>
    void replace(
        std::string key,
        Factory&& factory,
        PageRetention retention)
    {
        replace(
            std::move(key),
            eraseFactory(std::forward<Factory>(factory)),
            retention);
    }
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
    template <class Factory>
    static PageFactory eraseFactory(Factory&& factory)
    {
        static_assert(
            std::is_copy_constructible_v<std::decay_t<Factory>>,
            "Navigator factories must be copy-constructible");
        return [factory = std::forward<Factory>(factory)]() mutable {
            return detail::materialize(std::invoke(factory));
        };
    }

    void requireOwnerThread() const;
    void notifyWillChange();
    void hideCurrent();
    void activateCurrent();
    void notifyChanged();

    std::vector<PageEntry> stack_;
    ChangeHandler onChange_;
    BeforeChangeHandler onBeforeChange_;
    UiContext context_;
    std::thread::id ownerThread_;
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

    explicit OverlayHost(UiContext context = {});

    ~OverlayHost();

    // UiWindow binds its focus manager once. Standalone/headless hosts may
    // omit this and retain their previous ownership-only behavior.
    void bindFocusManager(FocusManager& focusManager) noexcept;
    void focus(Node* node) noexcept;
    [[nodiscard]] Node* focused() const noexcept;
    void setOnChange(ChangeHandler handler);
    [[nodiscard]] OverlayId show(std::unique_ptr<Node> overlay);
    template <
        class Overlay,
        std::enable_if_t<isViewLikeV<Overlay>, int> = 0>
    [[nodiscard]] OverlayId show(Overlay&& overlay)
    {
        return show(detail::materialize(std::forward<Overlay>(overlay)));
    }
    [[nodiscard]] std::unique_ptr<Node> dismiss(OverlayId id);
    [[nodiscard]] std::unique_ptr<Node> dismissTop();
    void clear() noexcept;

    // Deferred-dismissal scope. `dismiss(id)` and `dismissTop()` invoked
    // while depth > 0 queue the id for later removal and return nullptr,
    // matching UiWindow::dismissDialog's contract. When the outermost scope
    // unwinds, the queued ids are dismissed in reverse-order-of-request so
    // that a nested overlay's cleanup cannot outlive its host.
    // Rationale: InputRouter snapshots the pointer/keyboard path before
    // dispatching. If a widget's handler dismisses its own overlay (Combobox,
    // Dropdown, DatePicker, Popup outside-press), immediate destruction turns
    // every parent frame in that snapshot into a dangling pointer and the
    // subsequent Bubble phase crashes deep inside stdlib.
    void beginDeferredDismissals() noexcept;
    void endDeferredDismissals();
    [[nodiscard]] bool isDeferringDismissals() const noexcept;

    void layout(const RectF& bounds);
    void prepare(PaintContext& context);
    void paint(PaintContext& context);
    [[nodiscard]] Node* hitTest(PointF point) const;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const OverlayEntry* top() const noexcept;
    [[nodiscard]] const std::vector<OverlayEntry>& overlays() const noexcept;

private:
    void requireOwnerThread() const;
    OverlayId nextId_{1};
    std::vector<OverlayEntry> overlays_;
    ChangeHandler onChange_;
    FocusManager* focusManager_{nullptr};
    UiContext context_;
    std::thread::id ownerThread_;
    std::size_t deferralDepth_{0};
    std::vector<OverlayId> deferredDismissals_;
};

} // namespace wui
