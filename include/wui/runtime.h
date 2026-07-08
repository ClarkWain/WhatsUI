#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "wui/node.h"

namespace wui {

enum class PageRetention {
    KeepAlive,
    DisposeOnHide,
};

class UiRoot {
public:
    void setContent(std::unique_ptr<Node> content) noexcept;
    [[nodiscard]] Node* content() const noexcept;

    void layout(const RectF& bounds);
    void paint(PaintContext& context);

    [[nodiscard]] const RectF& bounds() const noexcept;

private:
    std::unique_ptr<Node> content_;
    RectF bounds_{};
};

struct PageEntry {
    std::string key;
    PageRetention retention{PageRetention::KeepAlive};
    std::unique_ptr<Node> content;
};

class Navigator {
public:
    void setRoot(std::string key, std::unique_ptr<Node> page, PageRetention retention = PageRetention::KeepAlive);
    void push(std::string key, std::unique_ptr<Node> page, PageRetention retention = PageRetention::KeepAlive);
    void replace(std::string key, std::unique_ptr<Node> page, PageRetention retention = PageRetention::KeepAlive);
    [[nodiscard]] std::unique_ptr<Node> pop();
    void popToRoot();
    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool canPop() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] Node* current() const noexcept;
    [[nodiscard]] const std::string* currentKey() const noexcept;
    [[nodiscard]] const PageEntry* currentEntry() const noexcept;
    [[nodiscard]] const std::vector<PageEntry>& pages() const noexcept;

private:
    std::vector<PageEntry> stack_;
};

using OverlayId = std::size_t;

struct OverlayEntry {
    OverlayId id{0};
    std::unique_ptr<Node> content;
};

class OverlayHost {
public:
    [[nodiscard]] OverlayId show(std::unique_ptr<Node> overlay);
    [[nodiscard]] std::unique_ptr<Node> dismiss(OverlayId id);
    [[nodiscard]] std::unique_ptr<Node> dismissTop();
    void clear() noexcept;
    void paint(PaintContext& context);

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const OverlayEntry* top() const noexcept;
    [[nodiscard]] const std::vector<OverlayEntry>& overlays() const noexcept;

private:
    OverlayId nextId_{1};
    std::vector<OverlayEntry> overlays_;
};

} // namespace wui
