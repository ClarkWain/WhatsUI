#include "application/gallery_router.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace whatsui::gallery {

GalleryRouter::GalleryRouter(wui::UiWindow& window, NavigationViewModel& navigation,
                             PageFactory pageFactory)
    : window_(&window)
    , navigation_(&navigation)
    , pageFactory_(std::move(pageFactory))
{
    if (!pageFactory_) {
        throw std::invalid_argument("GalleryRouter requires a page factory");
    }
}

GalleryRouter::~GalleryRouter()
{
    shutdown();
}

void GalleryRouter::start(GalleryRoute initialRoute)
{
    if (routeStack_.empty()) installRoot(initialRoute);
}

void GalleryRouter::navigate(GalleryRoute route)
{
    if (route == GalleryRoute::ButtonDetail) {
        if (routeStack_.empty()) installRoot(GalleryRoute::Overview);
        if (currentRoute() != route) pushDetail(route);
        return;
    }
    if (routeStack_.size() == 1 && currentRoute() == route) return;
    installRoot(route);
}

void GalleryRouter::openComponent(const ComponentDescriptor& component)
{
    if (component.id == "button") {
        navigate(GalleryRoute::ButtonDetail);
    }
}

void GalleryRouter::refresh()
{
    if (routeStack_.empty() || window_ == nullptr) return;
    const GalleryRoute route = currentRoute();
    window_->navigator().replace(std::string(galleryRouteKey(route)),
                                 [this, route] { return buildPage(route); },
                                 wui::PageRetention::DisposeOnHide);
}

bool GalleryRouter::canGoBack() const noexcept
{
    return routeStack_.size() > 1 && window_ != nullptr && window_->navigator().canPop();
}

void GalleryRouter::goBack()
{
    if (!canGoBack()) return;
    (void)window_->navigator().pop();
    routeStack_.pop_back();
    navigation_->select(routeStack_.back());
}

void GalleryRouter::shutdown() noexcept
{
    if (window_ != nullptr) window_->navigator().clear();
    routeStack_.clear();
}

GalleryRoute GalleryRouter::currentRoute() const noexcept
{
    return routeStack_.empty() ? GalleryRoute::Overview : routeStack_.back();
}

wui::UiWindow& GalleryRouter::window() noexcept
{
    return *window_;
}

std::unique_ptr<wui::Node> GalleryRouter::buildPage(GalleryRoute route)
{
    auto page = pageFactory_(route, *this);
    if (!page) throw std::runtime_error("Gallery page factory returned null");
    return page;
}

void GalleryRouter::installRoot(GalleryRoute route)
{
    window_->navigator().clear();
    window_->navigator().setRoot(std::string(galleryRouteKey(route)),
                                 [this, route] { return buildPage(route); },
                                 wui::PageRetention::DisposeOnHide);
    routeStack_.assign(1, route);
    navigation_->select(route);
}

void GalleryRouter::pushDetail(GalleryRoute route)
{
    window_->navigator().push(std::string(galleryRouteKey(route)),
                              [this, route] { return buildPage(route); },
                              wui::PageRetention::DisposeOnHide);
    routeStack_.push_back(route);
    navigation_->select(route);
}

} // namespace whatsui::gallery
