#include "wui/runtime.h"

#include <stdexcept>
#include <utility>

namespace wui {

void UiRoot::setContent(std::unique_ptr<Node> content) noexcept
{
    content_ = std::move(content);
}

Node* UiRoot::content() const noexcept
{
    return content_.get();
}

void UiRoot::layout(const RectF& bounds)
{
    bounds_ = bounds;
    if (content_) {
        content_->layout(bounds);
    }
}

void UiRoot::paint(PaintContext& context)
{
    if (content_) {
        content_->paint(context);
    }
}

const RectF& UiRoot::bounds() const noexcept
{
    return bounds_;
}

void Navigator::setRoot(std::string key, std::unique_ptr<Node> page, PageRetention retention)
{
    clear();
    push(std::move(key), std::move(page), retention);
}

void Navigator::push(std::string key, std::unique_ptr<Node> page, PageRetention retention)
{
    if (!page) {
        throw std::invalid_argument("page must not be null");
    }
    stack_.push_back(PageEntry{std::move(key), retention, std::move(page)});
}

void Navigator::replace(std::string key, std::unique_ptr<Node> page, PageRetention retention)
{
    if (!page) {
        throw std::invalid_argument("page must not be null");
    }
    if (stack_.empty()) {
        push(std::move(key), std::move(page), retention);
        return;
    }
    stack_.back() = PageEntry{std::move(key), retention, std::move(page)};
}

std::unique_ptr<Node> Navigator::pop()
{
    if (!canPop()) {
        return nullptr;
    }

    auto page = std::move(stack_.back().content);
    stack_.pop_back();
    return page;
}

void Navigator::popToRoot()
{
    while (canPop()) {
        stack_.pop_back();
    }
}

void Navigator::clear() noexcept
{
    stack_.clear();
}

bool Navigator::empty() const noexcept
{
    return stack_.empty();
}

bool Navigator::canPop() const noexcept
{
    return stack_.size() > 1;
}

std::size_t Navigator::size() const noexcept
{
    return stack_.size();
}

Node* Navigator::current() const noexcept
{
    if (stack_.empty()) {
        return nullptr;
    }
    return stack_.back().content.get();
}

const std::string* Navigator::currentKey() const noexcept
{
    const auto* entry = currentEntry();
    return entry != nullptr ? &entry->key : nullptr;
}

const PageEntry* Navigator::currentEntry() const noexcept
{
    return stack_.empty() ? nullptr : &stack_.back();
}

const std::vector<PageEntry>& Navigator::pages() const noexcept
{
    return stack_;
}

OverlayId OverlayHost::show(std::unique_ptr<Node> overlay)
{
    if (!overlay) {
        throw std::invalid_argument("overlay must not be null");
    }

    const auto id = nextId_++;
    overlays_.push_back(OverlayEntry{id, std::move(overlay)});
    return id;
}

std::unique_ptr<Node> OverlayHost::dismiss(OverlayId id)
{
    for (auto it = overlays_.begin(); it != overlays_.end(); ++it) {
        if (it->id == id) {
            auto overlay = std::move(it->content);
            overlays_.erase(it);
            return overlay;
        }
    }
    return nullptr;
}

std::unique_ptr<Node> OverlayHost::dismissTop()
{
    if (overlays_.empty()) {
        return nullptr;
    }

    auto overlay = std::move(overlays_.back().content);
    overlays_.pop_back();
    return overlay;
}

void OverlayHost::clear() noexcept
{
    overlays_.clear();
}

void OverlayHost::paint(PaintContext& context)
{
    for (const auto& overlay : overlays_) {
        if (overlay.content) {
            overlay.content->paint(context);
        }
    }
}

bool OverlayHost::empty() const noexcept
{
    return overlays_.empty();
}

std::size_t OverlayHost::size() const noexcept
{
    return overlays_.size();
}

const OverlayEntry* OverlayHost::top() const noexcept
{
    return overlays_.empty() ? nullptr : &overlays_.back();
}

const std::vector<OverlayEntry>& OverlayHost::overlays() const noexcept
{
    return overlays_;
}

} // namespace wui
