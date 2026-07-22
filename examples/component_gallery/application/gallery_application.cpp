#include "application/gallery_application.h"

#include <stdexcept>
#include <utility>

namespace whatsui::gallery {

GalleryApplication::GalleryApplication(std::unique_ptr<wui::PlatformHost> host,
                                       PageFactory pageFactory, std::string title,
                                       wui::SizeF initialSize)
    : app_(std::move(host))
{
    if (!pageFactory) {
        throw std::invalid_argument("GalleryApplication requires a page factory");
    }
    window_ = &app_.openWindow(std::move(title), initialSize);
    viewModels_.visualQa().setActualScaleFactor(window_->platformWindow().metrics().scaleFactor);
    router_ = std::make_unique<GalleryRouter>(
        *window_, viewModels_.navigation(),
        [this, pageFactory = std::move(pageFactory)](GalleryRoute route,
                                                     GalleryRouter& router) mutable {
            return pageFactory(route, viewModels_, router);
        });
}

void GalleryApplication::start(GalleryRoute initialRoute)
{
    if (started_) return;
    router_->start(initialRoute);
    window_->platformWindow().show();
    started_ = true;
}

int GalleryApplication::run()
{
    start();
    if (app_.host() == nullptr) throw std::runtime_error("GalleryApplication has no platform host");
    return app_.host()->run();
}

void GalleryApplication::close()
{
    window_->platformWindow().close();
}

GalleryViewModels& GalleryApplication::viewModels() noexcept { return viewModels_; }
GalleryRouter& GalleryApplication::router() noexcept { return *router_; }
wui::UiWindow& GalleryApplication::window() noexcept { return *window_; }
wui::UiApp& GalleryApplication::app() noexcept { return app_; }

} // namespace whatsui::gallery
