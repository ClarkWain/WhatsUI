#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/feedback.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
constexpr int kWidth = 760;
constexpr int kHeight = 460;
void expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void savePpm(const std::string& path, const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream out(path, std::ios::binary); if (!out) throw std::runtime_error("cannot save feedback review");
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) { out.put(static_cast<char>(pixels[i])); out.put(static_cast<char>(pixels[i + 1])); out.put(static_cast<char>(pixels[i + 2])); }
}
bool exactPixel(const std::vector<unsigned char>& pixels, int width, float scale, int x, int y, wui::Color color)
{
    const int px = static_cast<int>(std::lround(x * scale)), py = static_cast<int>(std::lround(y * scale));
    const std::size_t at = static_cast<std::size_t>((py * width + px) * 4);
    return at + 3 < pixels.size() && pixels[at] == color.r && pixels[at + 1] == color.g && pixels[at + 2] == color.b;
}
void render(const std::string& file, float scale)
{
    scale = std::max(1.0f, scale); const int width = static_cast<int>(std::lround(kWidth * scale)), height = static_cast<int>(std::lround(kHeight * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height); expect(canvas && canvas->initializeContext(), "feedback software canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale); wui::setTextMeasurer(&measurer);
    try {
        wui::PaintContext paint(*canvas, scale); canvas->beginFrame();
        paint.fillRect({0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, wui::theme().colors.neutralBackground2.rest);
        wui::Toast success("Task completed", "The task was moved to Completed."); success.setIntent(wui::ToastIntent::Success); success.setPosition(wui::ToastPosition::TopEnd); success.setAction("Undo", [] {}); success.layout({0, 0, 740, 340}); success.paint(paint);
        wui::Toast warning("Storage almost full", "Clear completed tasks to free space."); warning.setIntent(wui::ToastIntent::Warning); warning.setPosition(wui::ToastPosition::TopStart); warning.layout({0, 140, 740, 280}); warning.paint(paint);
        wui::Spinner inlineSpinner("Syncing your tasks"); inlineSpinner.setSize(wui::SpinnerSize::Large); inlineSpinner.layout({32, 310, 230, 44}); inlineSpinner.paint(paint);
        wui::Spinner verticalSpinner("Loading"); verticalSpinner.setSize(wui::SpinnerSize::Medium); verticalSpinner.setLabelPosition(wui::SpinnerLabelPosition::Below); verticalSpinner.setMotionEnabled(false); verticalSpinner.layout({320, 292, 110, 90}); verticalSpinner.paint(paint);
        canvas->endFrame(); const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() == static_cast<std::size_t>(width * height * 4), "feedback visual must return complete RGBA frame");
        expect(paint.paintStats().boxShadowCalls >= 4, "Toast visual must use Fluent elevation layers");
        expect(exactPixel(pixels, width, scale, 8, 8, wui::theme().colors.neutralBackground2.rest), "Toast positioning must not paint outside host margin");
        // Medium/Below uses a 32-DIP ring centered at (375, 322).  Its center
        // must remain the host background: this catches regressions back to a
        // filled or mis-centered indicator at fractional DPI.
        expect(exactPixel(pixels, width, scale, 375, 322,
                          wui::theme().colors.neutralBackground2.rest),
               "Spinner must render a centered hollow ring at fractional DPI");
        savePpm(file, pixels, width, height);
    } catch (...) { wui::setTextMeasurer(nullptr); throw; }
    wui::setTextMeasurer(nullptr);
}
} // namespace
int main(int argc, char** argv)
{
    try { render(argc > 1 ? argv[1] : "fluent_feedback_review.ppm", argc > 2 ? std::stof(argv[2]) : 1.0f); return 0; }
    catch (...) { return 1; }
}
