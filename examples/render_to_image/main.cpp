// Renders a WhatsUI tree with the real WhatsCanvas software backend and writes
// the result to a PPM image. This exercises the full path:
//   declarative tree -> layout (real text metrics) -> paint via WhatsCanvas.
//
// Only built when WHATSUI_WITH_WHATSCANVAS=ON.

#include <iostream>

#include "wsc/Canvas.h"

#include "wui/ui.h"
#include "wui/paint_context.h"
#include "wui/whatscanvas_text.h"

int main()
{
    constexpr int width = 320;
    constexpr int height = 180;

    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    if (!canvas || !canvas->initializeContext()) {
        std::cerr << "failed to create software canvas" << std::endl;
        return 1;
    }

    // Real, shaped text metrics for layout.
    wui::WhatsCanvasTextMeasurer measurer(*canvas);
    wui::setTextMeasurer(&measurer);

    using namespace wui::ui;
    wui::State<int> count{3};

    std::unique_ptr<wui::Node> root =
        Column()
            .padding(16)
            .gap(12)
            .children(
                Text("WhatsUI on WhatsCanvas"),
                Text().bind(count, [](const int& c) { return "Items: " + std::to_string(c); }),
                Row().gap(8).children(
                    Button("Cancel"),
                    Button("Confirm")
                )
            );

    root->layout({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});

    canvas->beginFrame();
    wui::PaintContext paint(*canvas);
    root->paint(paint);
    canvas->endFrame();

    const char* outputPath = "whatsui_render.ppm";
    if (!canvas->savePixelsPPM(outputPath)) {
        std::cerr << "failed to save image" << std::endl;
        return 1;
    }

    std::cout << "wrote " << outputPath << " (" << width << "x" << height << ")" << std::endl;
    return 0;
}
