#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/badge.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
void expect(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void savePpm(const std::string& path, const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot write Fluent Badge capture");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t index = 0; index + 3 < pixels.size(); index += 4) {
        output.put(static_cast<char>(pixels[index])); output.put(static_cast<char>(pixels[index + 1])); output.put(static_cast<char>(pixels[index + 2]));
    }
}
bool hasColor(const std::vector<unsigned char>& pixels, wui::Color color)
{
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) if (pixels[i] == color.r && pixels[i + 1] == color.g && pixels[i + 2] == color.b && pixels[i + 3] == color.a) return true;
    return false;
}
void render(const std::string& output, float scale)
{
    constexpr int logicalWidth = 560, logicalHeight = 210;
    scale = std::max(1.0f, scale);
    const int width = static_cast<int>(std::lround(logicalWidth * scale));
    const int height = static_cast<int>(std::lround(logicalHeight * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(), "Software canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale); wui::setTextMeasurer(&measurer);
    try {
        wui::PaintContext paint(*canvas, scale);
        canvas->beginFrame();
        paint.fillRect({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)}, wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent Badge", 24, 32, 20, wui::theme().colors.neutralForeground1, 600);
        wui::Badge neutral("New"); neutral.layout({24, 54, 44, 20}); neutral.paint(paint);
        wui::Badge brand("Preview"); brand.setAppearance(wui::BadgeAppearance::Filled); brand.setColor(wui::BadgeColor::Brand); brand.layout({86, 54, 64, 20}); brand.paint(paint);
        wui::Badge success("Ready"); success.setColor(wui::BadgeColor::Success); success.setAppearance(wui::BadgeAppearance::Outline); success.layout({168, 54, 58, 20}); success.paint(paint);
        wui::Badge warning("Review"); warning.setColor(wui::BadgeColor::Warning); warning.setSize(wui::BadgeSize::Large); warning.layout({244, 52, 74, 24}); warning.paint(paint);
        wui::Badge circular("Live"); circular.setColor(wui::BadgeColor::Danger); circular.setAppearance(wui::BadgeAppearance::Filled); circular.setShape(wui::BadgeShape::Circular); circular.layout({338, 54, 52, 20}); circular.paint(paint);
        paint.drawText("Counters", 24, 112, 14, wui::theme().colors.neutralForeground1, 600);
        wui::CounterBadge zero(0); zero.setShowZero(true); zero.layout({24, 126, 20, 20}); zero.paint(paint);
        wui::CounterBadge many(99); many.layout({60, 126, 28, 20}); many.paint(paint);
        wui::CounterBadge overflow(120); overflow.layout({106, 126, 34, 20}); overflow.paint(paint);
        paint.drawText("Presence", 190, 112, 14, wui::theme().colors.neutralForeground1, 600);
        const wui::RectF avatar{190, 126, 48, 48}; paint.fillRoundRect(avatar, wui::theme().radius.circular, wui::theme().colors.brandBackground.rest);
        for (auto [status, x] : std::vector<std::pair<wui::PresenceStatus, float>>{{wui::PresenceStatus::Available, 256.0f}, {wui::PresenceStatus::Away, 286.0f}, {wui::PresenceStatus::DoNotDisturb, 316.0f}, {wui::PresenceStatus::OutOfOffice, 346.0f}}) {
            wui::PresenceBadge badge(status); badge.setAvatarSize(48); badge.layout({x, 140, 14, 14}); badge.paint(paint);
        }
        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(!pixels.empty() && hasColor(pixels, wui::theme().colors.danger) && hasColor(pixels, wui::theme().colors.statusSuccess), "Badge visual matrix must contain semantic status colors");
        savePpm(output, pixels, width, height);
    } catch (...) { wui::setTextMeasurer(nullptr); throw; }
    wui::setTextMeasurer(nullptr);
}
}
int main(int argc, char** argv)
{
    try { render(argc > 1 ? argv[1] : "fluent_badge_review.ppm", argc > 2 ? std::stof(argv[2]) : 1.0f); return 0; }
    catch (const std::exception& error) { std::cerr << "Fluent Badge visual test failure: " << error.what() << '\n'; return 1; }
}
