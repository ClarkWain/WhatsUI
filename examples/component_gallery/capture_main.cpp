#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "application/gallery_router.h"
#include "capture/headless_platform.h"
#include "domain/gallery_route.h"
#include "view/gallery_page_factory.h"
#include "view_model/gallery_view_models.h"
#include "wsc/Canvas.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {

struct CaptureOptions {
    std::filesystem::path output;
    whatsui::gallery::GalleryRoute route{whatsui::gallery::GalleryRoute::Overview};
    int width{1280};
    int height{800};
    float scale{1.5f};
};

whatsui::gallery::GalleryRoute parseRoute(const std::string& value)
{
    using whatsui::gallery::GalleryRoute;
    if (value == "overview") return GalleryRoute::Overview;
    if (value == "all-components") return GalleryRoute::AllComponents;
    if (value == "controls") return GalleryRoute::Controls;
    if (value == "long-text") return GalleryRoute::LongText;
    if (value == "add-ons") return GalleryRoute::AddOns;
    if (value == "visual-qa") return GalleryRoute::VisualQa;
    if (value == "about") return GalleryRoute::About;
    if (value == "button-detail") return GalleryRoute::ButtonDetail;
    throw std::invalid_argument(
        "--route expects overview|all-components|controls|long-text|add-ons|visual-qa|about|button-detail");
}

CaptureOptions parseOptions(int argc, char** argv)
{
    CaptureOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--route") {
            if (++index >= argc) {
                throw std::invalid_argument("--route expects a route name");
            }
            options.route = parseRoute(argv[index]);
        } else if (argument == "--size") {
            if (++index >= argc || std::sscanf(argv[index], "%dx%d", &options.width,
                                                &options.height) != 2 ||
                options.width < 240 || options.height < 320) {
                throw std::invalid_argument("--size expects WIDTHxHEIGHT (minimum 240x320)");
            }
        } else if (argument == "--scale") {
            if (++index >= argc || std::sscanf(argv[index], "%f", &options.scale) != 1 ||
                !std::isfinite(options.scale) || options.scale < 1.0f || options.scale > 2.0f) {
                throw std::invalid_argument("--scale expects a value from 1.0 to 2.0");
            }
        } else if (argument.rfind("--", 0) == 0) {
            throw std::invalid_argument("unknown option: " + argument);
        } else {
            options.output = argument;
        }
    }
    if (options.output.empty()) {
        options.output = "component_gallery_"
            + std::string(whatsui::gallery::galleryRouteKey(options.route)) + ".ppm";
    }
    return options;
}

void capture(const CaptureOptions& options)
{
    const int pixelWidth = static_cast<int>(std::lround(options.width * options.scale));
    const int pixelHeight = static_cast<int>(std::lround(options.height * options.scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, pixelWidth, pixelHeight);
    if (!canvas || !canvas->initializeContext()) {
        throw std::runtime_error("failed to initialize the Software canvas");
    }

    wui::WhatsCanvasTextMeasurer text(*canvas, options.scale);
    if (!text.policyStatus().regularIconFont || !text.policyStatus().filledIconFont) {
        throw std::runtime_error("bundled Fluent icon fonts are unavailable");
    }
    wui::setTextMeasurer(&text);

    wui::setTheme(wui::Theme{});
    wui::UiApp application(
        whatsui::gallery::capture::createHeadlessPlatformHost(options.scale));
    auto& window = application.openWindow(
        "WhatsUI Component Gallery Capture",
        {static_cast<float>(options.width), static_cast<float>(options.height)});
    whatsui::gallery::GalleryViewModels viewModels;
    whatsui::gallery::GalleryRouter router(
        window, viewModels.navigation(),
        [&viewModels](whatsui::gallery::GalleryRoute route,
                      whatsui::gallery::GalleryRouter& activeRouter) {
            return whatsui::gallery::view::buildGalleryPage(
                route, viewModels, activeRouter);
        });
    router.start(options.route);

    wui::PaintContext paint(*canvas, options.scale);
    for (int pass = 0; pass < 2; ++pass) {
        window.update();
        window.layout();
        window.prepare(paint);
        canvas->beginFrame();
        paint.fillRect({0.0f, 0.0f, static_cast<float>(options.width),
                        static_cast<float>(options.height)},
                       wui::theme().colors.background);
        window.paint(paint);
        canvas->endFrame();
    }
    if (!options.output.parent_path().empty()) {
        std::filesystem::create_directories(options.output.parent_path());
    }
    if (!canvas->savePixelsPPM(options.output.string())) {
        throw std::runtime_error("failed to save " + options.output.string());
    }
    wui::setTextMeasurer(nullptr);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const auto options = parseOptions(argc, argv);
        capture(options);
        std::cout << "wrote " << options.output << std::endl;
        return 0;
    } catch (const std::exception& error) {
        wui::setTextMeasurer(nullptr);
        std::cerr << "WhatsUI Component Gallery capture: " << error.what() << std::endl;
        return 1;
    }
}
