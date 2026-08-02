#include "software_capture.h"

#include "inspector_page.h"

#include "wsc/Canvas.h"
#include "wui/paint_context.h"
#include "wui/runtime.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace whatsui::debug_inspector {
namespace {

class TextMeasurerScope {
public:
    TextMeasurerScope(wsc::Canvas& canvas, float scale)
        : measurer_(canvas, scale)
    {
        wui::setTextMeasurer(&measurer_);
    }

    ~TextMeasurerScope()
    {
        wui::setTextMeasurer(nullptr);
    }

    TextMeasurerScope(const TextMeasurerScope&) = delete;
    TextMeasurerScope& operator=(const TextMeasurerScope&) = delete;

private:
    wui::WhatsCanvasTextMeasurer measurer_;
};

void render(wui::UiRoot& root, wsc::Canvas& canvas, float scale)
{
    root.layout({0.0f,
                 0.0f,
                 static_cast<float>(canvas.getWidth()) / scale,
                 static_cast<float>(canvas.getHeight()) / scale});
    wui::PaintContext paint(canvas, scale);
    root.prepare(paint);
    for (int pass = 0; pass < 2; ++pass) {
        canvas.beginFrame();
        paint.fillRect(root.bounds(), wui::theme().colors.background);
        root.paint(paint);
        canvas.endFrame();
    }
}

} // namespace

void writeInspectorReference(
    InspectorDiagnostics diagnostics,
    const std::filesystem::path& outputDirectory)
{
    std::filesystem::create_directories(outputDirectory);

    constexpr float scale = 2.0f;
    auto canvas = wsc::Canvas::create(
        wsc::Canvas::Backend::Software, 2400, 1520);
    if (!canvas || !canvas->initializeContext()) {
        throw std::runtime_error(
            "failed to create Debug Inspector software canvas");
    }

    TextMeasurerScope text(*canvas, scale);
    wui::UiRoot root;
    root.setContent(InspectorPage(std::move(diagnostics)));
    render(root, *canvas, scale);

    const auto path = outputDirectory / "ui_inspector_reference.ppm";
    if (!canvas->savePixelsPPM(path.string())) {
        throw std::runtime_error("failed to save " + path.string());
    }
    std::cout << "wrote " << path << std::endl;
}

} // namespace whatsui::debug_inspector
