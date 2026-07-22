#pragma once

#include <functional>
#include <memory>
#include <string>

#include "application/gallery_router.h"
#include "view_model/gallery_view_models.h"
#include "wui/app.h"

namespace whatsui::gallery {

class GalleryApplication {
public:
    using PageFactory = std::function<std::unique_ptr<wui::Node>(
        GalleryRoute, GalleryViewModels&, GalleryRouter&)>;

    GalleryApplication(std::unique_ptr<wui::PlatformHost> host, PageFactory pageFactory,
                       std::string title = "WhatsUI Component Gallery",
                       wui::SizeF initialSize = {1280.0f, 800.0f});
    ~GalleryApplication() = default;

    GalleryApplication(const GalleryApplication&) = delete;
    GalleryApplication& operator=(const GalleryApplication&) = delete;

    void start(GalleryRoute initialRoute = GalleryRoute::Overview);
    int run();
    void close();

    [[nodiscard]] GalleryViewModels& viewModels() noexcept;
    [[nodiscard]] GalleryRouter& router() noexcept;
    [[nodiscard]] wui::UiWindow& window() noexcept;
    [[nodiscard]] wui::UiApp& app() noexcept;

private:
    GalleryViewModels viewModels_;
    wui::UiApp app_;
    wui::UiWindow* window_{nullptr};
    std::unique_ptr<GalleryRouter> router_;
    bool started_{false};
};

} // namespace whatsui::gallery
