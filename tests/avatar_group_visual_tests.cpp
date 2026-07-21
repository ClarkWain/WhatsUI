#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/avatar.h"
#include "wui/badge.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
constexpr int kLogicalWidth = 620;
constexpr int kLogicalHeight = 230;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void savePpm(const std::string& path, const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot save avatar visual capture");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t index = 0; index + 3 < pixels.size(); index += 4) {
        output.put(static_cast<char>(pixels[index]));
        output.put(static_cast<char>(pixels[index + 1]));
        output.put(static_cast<char>(pixels[index + 2]));
    }
}

bool containsColor(const std::vector<unsigned char>& pixels, wui::Color color)
{
    for (std::size_t index = 0; index + 3 < pixels.size(); index += 4) {
        if (pixels[index] == color.r && pixels[index + 1] == color.g && pixels[index + 2] == color.b && pixels[index + 3] == color.a) return true;
    }
    return false;
}

void render(const std::string& output, float scale)
{
    scale = std::max(1.0f, scale);
    const int width = static_cast<int>(std::lround(kLogicalWidth * scale));
    const int height = static_cast<int>(std::lround(kLogicalHeight * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(), "Avatar software canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
    wui::setTextMeasurer(&measurer);
    try {
        wui::PaintContext paint(*canvas, scale);
        canvas->beginFrame();
        const auto& current = wui::theme();
        paint.fillRect({0, 0, static_cast<float>(kLogicalWidth), static_cast<float>(kLogicalHeight)}, current.colors.neutralBackground2.rest);
        paint.drawText("Fluent Avatar", 28, 35, 22, current.colors.neutralForeground1, 600);
        paint.drawText("Icon fallback, initials, activity, presence, and group overflow", 28, 59, 13, current.colors.neutralForeground2, 400);

        // The first three examples mirror the Figma Avatar component: default
        // Person icon, a status overlay aligned to the visual edge, then an
        // Initials avatar with an activity ring that overflows without
        // changing its 64-DIP layout footprint.
        wui::Avatar fallback({}, wui::AvatarSize::Size64);
        fallback.layout({28, 82, 64, 64});
        fallback.paint(paint);
        paint.drawText("Icon", 28, 170, 13, current.colors.neutralForeground1, 600);

        wui::Avatar online({}, wui::AvatarSize::Size64);
        online.layout({130, 82, 64, 64});
        online.paint(paint);
        wui::PresenceBadge presence(wui::PresenceStatus::Available);
        presence.setAvatarSize(64);
        presence.layout(presence.boundsForAvatar(online.bounds()));
        presence.paint(paint);
        paint.drawText("Online", 130, 170, 13, current.colors.neutralForeground1, 600);

        wui::Avatar initials("Morgan Brown", wui::AvatarSize::Size64);
        initials.setColor(wui::AvatarColor::Neutral);
        initials.setActive(true);
        initials.layout({232, 82, 64, 64});
        initials.paint(paint);
        paint.drawText("Activity", 232, 170, 13, current.colors.neutralForeground1, 600);

        wui::AvatarGroup stack;
        stack.setSize(wui::AvatarSize::Size40);
        stack.setMaxVisible(3);
        stack.addAvatar("Ada Lovelace").setColor(wui::AvatarColor::Brand);
        stack.addAvatar("Grace Hopper").setColor(wui::AvatarColor::Purple);
        stack.addAvatar("Margaret Hamilton").setColor(wui::AvatarColor::Green);
        stack.addAvatar("Katherine Johnson").setColor(wui::AvatarColor::Marigold);
        stack.layout({336, 94, 150, 40});
        stack.paint(paint);
        paint.drawText("Stack", 336, 170, 13, current.colors.neutralForeground1, 600);

        wui::AvatarGroup spread;
        spread.setGroupLayout(wui::AvatarGroupLayout::Spread);
        spread.setSize(wui::AvatarSize::Size32);
        spread.setMaxVisible(3);
        spread.addAvatar("Linus Torvalds").setColor(wui::AvatarColor::Teal);
        spread.addAvatar("Annie Easley").setColor(wui::AvatarColor::Plum);
        spread.addAvatar("Alan Turing").setColor(wui::AvatarColor::DarkGreen);
        spread.layout({500, 98, 100, 32});
        spread.paint(paint);
        paint.drawText("Spread", 500, 170, 13, current.colors.neutralForeground1, 600);

        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() == static_cast<std::size_t>(width * height * 4), "Avatar visual output must be a complete RGBA frame");
        expect(containsColor(pixels, current.colors.brandBackground.rest) && containsColor(pixels, current.colors.statusSuccess),
               "Avatar visual must contain both avatar color and presence status");
        savePpm(output, pixels, width, height);
    } catch (...) {
        wui::setTextMeasurer(nullptr);
        throw;
    }
    wui::setTextMeasurer(nullptr);
}
} // namespace

int main(int argc, char** argv)
{
    try {
        render(argc > 1 ? argv[1] : "avatar_group_review.ppm", argc > 2 ? std::stof(argv[2]) : 1.0f);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Avatar visual test failure: " << error.what() << '\n';
        return 1;
    }
}
