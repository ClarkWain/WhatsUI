#pragma once

#include <functional>
#include <memory>

#include "domain/component_descriptor.h"
#include "domain/gallery_route.h"
#include "view_model/navigation_view_model.h"
#include "wui/app.h"

namespace whatsui::gallery {

class GalleryRouter {
public:
    using PageFactory =
        std::function<std::unique_ptr<wui::Node>(GalleryRoute, GalleryRouter&)>;

    GalleryRouter(wui::UiWindow& window, NavigationViewModel& navigation,
                  PageFactory pageFactory);
    ~GalleryRouter();

    GalleryRouter(const GalleryRouter&) = delete;
    GalleryRouter& operator=(const GalleryRouter&) = delete;

    void start(GalleryRoute initialRoute = GalleryRoute::Overview);
    void navigate(GalleryRoute route);
    void openComponent(const ComponentDescriptor& component);
    void refresh();
    [[nodiscard]] bool canGoBack() const noexcept;
    void goBack();
    void shutdown() noexcept;

    [[nodiscard]] GalleryRoute currentRoute() const noexcept;
    [[nodiscard]] wui::UiWindow& window() noexcept;

private:
    [[nodiscard]] std::unique_ptr<wui::Node> buildPage(GalleryRoute route);
    void installRoot(GalleryRoute route);
    void pushDetail(GalleryRoute route);

    wui::UiWindow* window_{nullptr};
    NavigationViewModel* navigation_{nullptr};
    PageFactory pageFactory_;
};

} // namespace whatsui::gallery
