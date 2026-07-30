#include "application/gallery_router.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace whatsui::gallery {
namespace {

GalleryRoute routeForKey(std::string_view key) noexcept
{
    for (const auto route : {GalleryRoute::Overview, GalleryRoute::AllComponents,
                             GalleryRoute::Controls, GalleryRoute::LongText,
                             GalleryRoute::AddOns,
                             GalleryRoute::VisualQa, GalleryRoute::About,
                             GalleryRoute::ButtonDetail}) {
        if (galleryRouteKey(route) == key) return route;
    }
    return GalleryRoute::Overview;
}

} // namespace

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
    if (window_->navigator().empty()) installRoot(initialRoute);
}

void GalleryRouter::navigate(GalleryRoute route)
{
    if (route == GalleryRoute::ButtonDetail) {
        if (window_->navigator().empty()) installRoot(GalleryRoute::Overview);
        if (currentRoute() != route) pushDetail(route);
        return;
    }
    if (window_->navigator().size() == 1 && currentRoute() == route) return;
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
    if (window_ == nullptr || window_->navigator().empty()) return;
    const GalleryRoute route = currentRoute();
    window_->navigator().replace(std::string(galleryRouteKey(route)),
                                 [this, route] { return buildPage(route); },
                                 wui::PageRetention::DisposeOnHide);
}

bool GalleryRouter::canGoBack() const noexcept
{
    return window_ != nullptr && window_->navigator().canPop();
}

void GalleryRouter::goBack()
{
    if (!canGoBack()) return;
    const auto& pages = window_->navigator().pages();
    navigation_->select(routeForKey(pages[pages.size() - 2].key));
    (void)window_->navigator().pop();
}

void GalleryRouter::shutdown() noexcept
{
    if (window_ != nullptr) window_->navigator().clear();
    if (navigation_ != nullptr) navigation_->select(GalleryRoute::Overview);
}

GalleryRoute GalleryRouter::currentRoute() const noexcept
{
    return navigation_ == nullptr
        ? GalleryRoute::Overview
        : navigation_->currentRoute().get();
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
    navigation_->select(route);
    window_->navigator().clear();
    window_->navigator().setRoot(std::string(galleryRouteKey(route)),
                                 [this, route] { return buildPage(route); },
                                 wui::PageRetention::DisposeOnHide);
}

void GalleryRouter::pushDetail(GalleryRoute route)
{
    navigation_->select(route);
    window_->navigator().push(std::string(galleryRouteKey(route)),
                              [this, route] { return buildPage(route); },
                              wui::PageRetention::DisposeOnHide);
}

} // namespace whatsui::gallery
