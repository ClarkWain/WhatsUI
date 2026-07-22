#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "domain/component_catalog.h"
#include "view/app_shell_view.h"
#include "view/pages/overview_page.h"
#include "view_model/gallery_view_model.h"
#include "wsc/Canvas.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {

struct CaptureOptions {
    std::filesystem::path output{"component_gallery_overview.ppm"};
    int width{1280};
    int height{800};
    float scale{1.5f};
};

CaptureOptions parseOptions(int argc, char** argv)
{
    CaptureOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--size") {
            if (++index >= argc || std::sscanf(argv[index], "%dx%d", &options.width,
                                                &options.height) != 2 ||
                options.width < 960 || options.height < 640) {
                throw std::invalid_argument("--size expects WIDTHxHEIGHT (minimum 960x640)");
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

    whatsui::gallery::ComponentCatalog catalog;
    whatsui::gallery::GalleryViewModel gallery(catalog);
    auto content = whatsui::gallery::view::pages::buildOverviewPage(catalog, gallery, {});
    auto root = whatsui::gallery::view::buildAppShell(
        whatsui::gallery::GalleryRoute::Overview, std::move(content));
    root->layout({0.0f, 0.0f, static_cast<float>(options.width),
                  static_cast<float>(options.height)});
    wui::PaintContext paint(*canvas, options.scale);
    root->prepare(paint);
    for (int pass = 0; pass < 2; ++pass) {
        canvas->beginFrame();
        paint.fillRect({0.0f, 0.0f, static_cast<float>(options.width),
                        static_cast<float>(options.height)},
                       wui::theme().colors.background);
        root->paint(paint);
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
