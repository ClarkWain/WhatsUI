// Renders a WhatsUI tree with the real WhatsCanvas software backend and writes
// the result to a PPM image. This exercises the full path:
//   declarative tree -> layout (real text metrics) -> paint via WhatsCanvas.
//
// Only built when WHATSUI_WITH_WHATSCANVAS=ON.

#include <iostream>

#include "wsc/Canvas.h"

#include "wui/declarative.h"
#include "wui/paint_context.h"
#include "wui/whatscanvas_text.h"

using namespace wui;
using namespace wsc;

int main()
{
    constexpr int width = 320;
    constexpr int height = 180;

    auto canvas = Canvas::create(Canvas::Backend::Software, width, height);
    if (!canvas || !canvas->initializeContext()) {
        std::cerr << "failed to create software canvas" << std::endl;
        return 1;
    }

    // Real, shaped text metrics for layout.
    WhatsCanvasTextMeasurer measurer(*canvas);
    setTextMeasurer(&measurer);
    
    State<int> count{3};

    auto rootNode = Column().padding(16).gap(12).children(
            Text("WhatsUI on WhatsCanvas").color({255, 255, 255, 255}).size(20).weight(600),
            Text().bind(count, [](const int& c) { return "Items: " + std::to_string(c); }),
            Row().gap(8).children(Button("Cancel"),Button("Confirm"))
        )
        .build();

    rootNode->layout({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});

    PaintContext paint(*canvas);
    rootNode->prepare(paint);
    canvas->beginFrame();
    rootNode->paint(paint);
    canvas->endFrame();

    std::string outputPath = "whatsui_render.ppm";
    if (!canvas->savePixelsPPM(outputPath)) {
        std::cerr << "failed to save image" << std::endl;
        return 1;
    }

    std::cout << "wrote " << outputPath << " (" << width << "x" << height << ")" << std::endl;
    return 0;
}
