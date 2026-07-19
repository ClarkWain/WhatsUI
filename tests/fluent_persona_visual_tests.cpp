#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/persona.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
constexpr int kLogicalWidth = 720;
constexpr int kLogicalHeight = 300;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void savePpm(const std::string& path, const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot save Persona visual capture");
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
    expect(canvas && canvas->initializeContext(), "Persona software canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
    wui::setTextMeasurer(&measurer);
    try {
        wui::PaintContext paint(*canvas, scale);
        canvas->beginFrame();
        const auto& current = wui::theme();
        paint.fillRect({0, 0, static_cast<float>(kLogicalWidth), static_cast<float>(kLogicalHeight)}, current.colors.neutralBackground2.rest);
        paint.drawText("Fluent Persona", 28, 36, 22, current.colors.neutralForeground1, 600);
        paint.drawText("Avatar, presence, four text levels, and safe truncation", 28, 60, 13, current.colors.neutralForeground2, 400);

        wui::Persona owner("Ada Lovelace", wui::PersonaSize::ExtraLarge);
        owner.setAvatarColor(wui::AvatarColor::Brand);
        owner.setPresence(wui::PresenceStatus::Available);
        owner.setSecondaryText("Task owner");
        owner.setTertiaryText("Research and analytics");
        owner.layout({28, 92, 260, 72});
        owner.paint(paint);

        wui::Persona compact("Grace Hopper", wui::PersonaSize::Small);
        compact.setAvatarColor(wui::AvatarColor::Cranberry);
        compact.setSecondaryText("Rear Admiral, US Navy");
        compact.setTextPosition(wui::PersonaTextPosition::Before);
        compact.layout({350, 96, 190, 40});
        compact.paint(paint);

        wui::Persona below("Katherine Johnson", wui::PersonaSize::Large);
        below.setAvatarColor(wui::AvatarColor::Purple);
        below.setSecondaryText("Flight dynamics");
        below.setTextPosition(wui::PersonaTextPosition::Below);
        below.setTextAlignment(wui::PersonaTextAlignment::Center);
        below.layout({594, 82, 96, 160});
        below.paint(paint);

        wui::Persona status({}, wui::PersonaSize::Huge);
        status.setPresence(wui::PresenceStatus::DoNotDisturb);
        status.setPresenceOnly(true);
        status.layout({350, 184, 64, 64});
        status.paint(paint);
        paint.drawText("Presence only", 330, 276, 12, current.colors.neutralForeground2, 400);

        wui::Persona clipped("A very long display name that stays inside its column", wui::PersonaSize::Medium);
        clipped.setSecondaryText("Long secondary information is also safely ellipsized");
        clipped.setAvatarColor(wui::AvatarColor::Teal);
        clipped.layout({28, 206, 250, 40});
        clipped.paint(paint);

        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() == static_cast<std::size_t>(width * height * 4), "Persona visual output must be a complete RGBA frame");
        expect(containsColor(pixels, current.colors.brandBackground.rest) && containsColor(pixels, current.colors.statusDanger),
               "Persona visual must contain Avatar color and presence status");
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
        render(argc > 1 ? argv[1] : "fluent_persona_review.ppm", argc > 2 ? std::stof(argv[2]) : 1.0f);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WhatsUI Fluent Persona visual test failure: " << error.what() << '\n';
        return 1;
    }
}
