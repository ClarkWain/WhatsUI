#include "wui/runtime.h"

#include <stdexcept>
#include <utility>

namespace wui {

void UiRoot::setContent(std::unique_ptr<Node> content) noexcept
{
    if (content_ != nullptr) {
        content_->setInvalidationHandler({});
    }
    ownedContent_ = std::move(content);
    content_ = ownedContent_.get();
    wireInvalidationHandler();
}

void UiRoot::setBorrowedContent(Node* content) noexcept
{
    if (content_ != nullptr && content_ != content) {
        content_->setInvalidationHandler({});
    }
    ownedContent_.reset();
    content_ = content;
    wireInvalidationHandler();
}

void UiRoot::setOnInvalidate(std::function<void()> handler)
{
    onInvalidate_ = std::move(handler);
    wireInvalidationHandler();
}

void UiRoot::wireInvalidationHandler() noexcept
{
    if (content_ != nullptr) {
        content_->setInvalidationHandler(onInvalidate_);
    }
}

Node* UiRoot::content() const noexcept
{
    return content_;
}

void UiRoot::layout(const RectF& bounds)
{
    bounds_ = bounds;
    if (content_ != nullptr) {
        content_->layout(bounds);
    }
}

void UiRoot::paint(PaintContext& context)
{
    if (content_ != nullptr) {
        content_->paint(context);
    }
}

const RectF& UiRoot::bounds() const noexcept
{
    return bounds_;
}

void UiRoot::prepare(PaintContext& context)
{
    if (content_ != nullptr) {
        content_->prepare(context);
    }
}

void Navigator::setOnChange(ChangeHandler handler)
{
    onChange_ = std::move(handler);
    notifyChanged();
}

void Navigator::setBeforeChange(BeforeChangeHandler handler)
{
    onBeforeChange_ = std::move(handler);
}

void Navigator::notifyWillChange()
{
    if (onBeforeChange_) {
        onBeforeChange_();
    }
}

void Navigator::notifyChanged()
{
    if (onChange_) {
        onChange_(current());
    }
}

void Navigator::setRoot(std::string key, std::unique_ptr<Node> page, PageRetention retention)
{
    if (retention == PageRetention::DisposeOnHide) {
        throw std::invalid_argument("DisposeOnHide pages require a PageFactory");
    }
    clear();
    push(std::move(key), std::move(page), retention);
}

void Navigator::setRoot(std::string key, PageFactory factory, PageRetention retention)
{
    clear();
    push(std::move(key), std::move(factory), retention);
}

void Navigator::push(std::string key, std::unique_ptr<Node> page, PageRetention retention)
{
    if (!page) {
        throw std::invalid_argument("page must not be null");
    }
    if (retention == PageRetention::DisposeOnHide) {
        throw std::invalid_argument("DisposeOnHide pages require a PageFactory");
    }
    notifyWillChange();
    hideCurrent();
    stack_.push_back(PageEntry{std::move(key), retention, std::move(page), {}});
    notifyChanged();
}

void Navigator::push(std::string key, PageFactory factory, PageRetention retention)
{
    if (!factory) {
        throw std::invalid_argument("page factory must not be empty");
    }
    auto page = factory();
    if (!page) {
        throw std::runtime_error("page factory returned null");
    }
    notifyWillChange();
    hideCurrent();
    stack_.push_back(PageEntry{std::move(key), retention, std::move(page), std::move(factory)});
    notifyChanged();
}

void Navigator::replace(std::string key, std::unique_ptr<Node> page, PageRetention retention)
{
    if (!page) {
        throw std::invalid_argument("page must not be null");
    }
    if (retention == PageRetention::DisposeOnHide) {
        throw std::invalid_argument("DisposeOnHide pages require a PageFactory");
    }
    if (stack_.empty()) {
        push(std::move(key), std::move(page), retention);
        return;
    }
    notifyWillChange();
    stack_.back() = PageEntry{std::move(key), retention, std::move(page), {}};
    notifyChanged();
}

void Navigator::replace(std::string key, PageFactory factory, PageRetention retention)
{
    if (!factory) {
        throw std::invalid_argument("page factory must not be empty");
    }
    if (stack_.empty()) {
        push(std::move(key), std::move(factory), retention);
        return;
    }
    auto page = factory();
    if (!page) {
        throw std::runtime_error("page factory returned null");
    }
    notifyWillChange();
    stack_.back() = PageEntry{std::move(key), retention, std::move(page), std::move(factory)};
    notifyChanged();
}

std::unique_ptr<Node> Navigator::pop()
{
    if (!canPop()) {
        return nullptr;
    }

    notifyWillChange();
    auto page = std::move(stack_.back().content);
    stack_.pop_back();
    activateCurrent();
    notifyChanged();
    return page;
}

void Navigator::popToRoot()
{
    if (canPop()) {
        notifyWillChange();
    }
    while (canPop()) {
        stack_.pop_back();
    }
    activateCurrent();
    notifyChanged();
}

void Navigator::clear()
{
    if (!stack_.empty()) {
        notifyWillChange();
    }
    stack_.clear();
    notifyChanged();
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
    if (onChange_) {
        onChange_();
    }
    return id;
}

void OverlayHost::setOnChange(ChangeHandler handler)
{
    onChange_ = std::move(handler);
}

std::unique_ptr<Node> OverlayHost::dismiss(OverlayId id)
{
    for (auto it = overlays_.begin(); it != overlays_.end(); ++it) {
        if (it->id == id) {
            auto overlay = std::move(it->content);
            overlay->setInvalidationHandler({});
            overlays_.erase(it);
            if (onChange_) {
                onChange_();
            }
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
    overlay->setInvalidationHandler({});
    overlays_.pop_back();
    if (onChange_) {
        onChange_();
    }
    return overlay;
}

void OverlayHost::clear() noexcept
{
    if (overlays_.empty()) {
        return;
    }
    for (const auto& overlay : overlays_) {
        if (overlay.content) {
            overlay.content->setInvalidationHandler({});
        }
    }
    overlays_.clear();
    if (onChange_) {
        onChange_();
    }
}

void Navigator::hideCurrent()
{
    if (!stack_.empty() && stack_.back().retention == PageRetention::DisposeOnHide) {
        stack_.back().content.reset();
    }
}

void Navigator::activateCurrent()
{
    if (stack_.empty() || stack_.back().content) {
        return;
    }
    if (!stack_.back().factory) {
        throw std::logic_error("disposed page has no factory");
    }
    stack_.back().content = stack_.back().factory();
    if (!stack_.back().content) {
        throw std::runtime_error("page factory returned null");
    }
}

void OverlayHost::layout(const RectF& bounds)
{
    for (const auto& overlay : overlays_) {
        if (overlay.content) {
            overlay.content->layout(bounds);
        }
    }
}

void OverlayHost::prepare(PaintContext& context)
{
    for (const auto& overlay : overlays_) {
        if (overlay.content) {
            overlay.content->prepare(context);
        }
    }
}

void OverlayHost::paint(PaintContext& context)
{
    for (const auto& overlay : overlays_) {
        if (overlay.content) {
            overlay.content->paint(context);
        }
    }
}

Node* OverlayHost::hitTest(PointF point) const
{
    for (auto it = overlays_.rbegin(); it != overlays_.rend(); ++it) {
        if (it->content) {
            if (auto* hit = it->content->hitTest(point)) {
                return hit;
            }
        }
    }
    return nullptr;
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
