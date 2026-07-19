#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/basic_controls.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
constexpr int logicalWidth = 960;
constexpr int logicalHeight = 760;

void savePpm(const std::string& path, const std::vector<unsigned char>& rgba, int width, int height)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create Fluent range controls review image");
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        out.put(static_cast<char>(rgba[i]));
        out.put(static_cast<char>(rgba[i + 1]));
        out.put(static_cast<char>(rgba[i + 2]));
    }
}

bool pixelIs(const std::vector<unsigned char>& rgba, int width, float scale,
             int logicalX, int logicalY, wui::Color color)
{
    const int x = static_cast<int>(std::lround(static_cast<float>(logicalX) * scale));
    const int y = static_cast<int>(std::lround(static_cast<float>(logicalY) * scale));
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    return offset + 3 < rgba.size() && rgba[offset] == color.r && rgba[offset + 1] == color.g
        && rgba[offset + 2] == color.b && rgba[offset + 3] == color.a;
}

void draw(wui::Node& node, wui::PaintContext& paint, wui::RectF bounds)
{
    node.layout(bounds);
    node.prepare(paint);
    node.paint(paint);
}

void section(wui::PaintContext& paint, const std::string& text, float y)
{
    paint.drawText(text, 28, y, 12, wui::theme().colors.neutralForeground3, 600);
}
} // namespace

int main(int argc, char** argv)
{
    try {
        const std::string output = argc > 1 ? argv[1] : "fluent_range_controls_review.ppm";
        const float scale = argc > 2 ? std::max(1.0f, std::stof(argv[2])) : 1.0f;
        const int width = static_cast<int>(std::lround(logicalWidth * scale));
        const int height = static_cast<int>(std::lround(logicalHeight * scale));
        auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
        if (!canvas || !canvas->initializeContext()) return 2;
        wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
        wui::setTextMeasurer(&measurer);
        wui::PaintContext paint(*canvas, scale);
        canvas->beginFrame();
        paint.fillRect({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)},
                       wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent range, progress and selection controls", 28, 44, 24,
                       wui::theme().colors.neutralForeground1, 600);

        section(paint, "DIVIDER APPEARANCES / ALIGNMENT", 78);
        wui::Divider dividerDefault; dividerDefault.content("Default");
        wui::Divider dividerSubtle; dividerSubtle.content("Subtle").appearance(wui::DividerAppearance::Subtle).contentAlignment(wui::DividerContentAlignment::Start);
        wui::Divider dividerBrand; dividerBrand.content("Brand").appearance(wui::DividerAppearance::Brand).inset(true);
        wui::Divider dividerStrong; dividerStrong.content("Strong").appearance(wui::DividerAppearance::Strong).contentAlignment(wui::DividerContentAlignment::End);
        draw(dividerDefault, paint, {28, 92, 210, 24});
        draw(dividerSubtle, paint, {258, 92, 210, 24});
        draw(dividerBrand, paint, {488, 92, 210, 24});
        draw(dividerStrong, paint, {718, 92, 214, 24});

        section(paint, "SLIDER SIZES / STATES / ORIENTATION", 146);
        wui::Slider sliderSmall(0, 100, 38); sliderSmall.setSize(wui::SliderSize::Small);
        wui::Slider sliderMedium(0, 100, 56);
        wui::Slider sliderHover(0, 100, 64); sliderHover.setVisualState(wui::ControlVisualState::Hovered, true);
        wui::Slider sliderPressed(0, 100, 72); sliderPressed.setVisualState(wui::ControlVisualState::Pressed, true);
        wui::Slider sliderFocused(0, 100, 45); sliderFocused.setVisualState(wui::ControlVisualState::Focused, true);
        wui::Slider sliderDisabled(0, 100, 70); sliderDisabled.setEnabled(false);
        draw(sliderSmall, paint, {28, 164, 270, 24});
        draw(sliderMedium, paint, {330, 160, 270, 32});
        draw(sliderHover, paint, {632, 160, 270, 32});
        draw(sliderPressed, paint, {28, 206, 270, 32});
        draw(sliderFocused, paint, {330, 206, 270, 32});
        draw(sliderDisabled, paint, {632, 206, 270, 32});
        wui::Slider vertical(0, 100, 62); vertical.setOrientation(wui::SliderOrientation::Vertical);
        draw(vertical, paint, {910, 146, 32, 112});

        section(paint, "PROGRESS COLORS / SHAPES / THICKNESS / INDETERMINATE", 288);
        wui::ProgressBar brand(0, 1, .65f), success(0, 1, .65f), warning(0, 1, .65f), error(0, 1, .65f);
        success.color(wui::ProgressBarColor::Success);
        warning.color(wui::ProgressBarColor::Warning);
        error.color(wui::ProgressBarColor::Error);
        draw(brand, paint, {28, 306, 200, 8});
        draw(success, paint, {258, 306, 200, 8});
        draw(warning, paint, {488, 306, 200, 8});
        draw(error, paint, {718, 306, 214, 8});
        wui::ProgressBar largeSquare(0, 1, .45f); largeSquare.thickness(wui::ProgressBarThickness::Large).shape(wui::ProgressBarShape::Square);
        wui::ProgressBar indeterminate; indeterminate.indeterminate(true).motionEnabled(false);
        draw(largeSquare, paint, {28, 338, 430, 8});
        draw(indeterminate, paint, {488, 338, 444, 8});

        section(paint, "SWITCH SMALL / MEDIUM / INTERACTION / LABEL POSITION", 386);
        wui::Switch switchSmall("Small", false); switchSmall.size(wui::SwitchSize::Small);
        wui::Switch switchOn("On", true);
        wui::Switch switchHover("Hover", false); switchHover.setVisualState(wui::ControlVisualState::Hovered, true);
        wui::Switch switchPressed("Pressed", true); switchPressed.setVisualState(wui::ControlVisualState::Pressed, true);
        wui::Switch switchFocused("Focused", true); switchFocused.setVisualState(wui::ControlVisualState::Focused, true);
        wui::Switch switchDisabled("Disabled", true); switchDisabled.setEnabled(false);
        draw(switchSmall, paint, {28, 404, 120, 32});
        draw(switchOn, paint, {166, 402, 110, 36});
        draw(switchHover, paint, {294, 402, 130, 36});
        draw(switchPressed, paint, {442, 402, 140, 36});
        draw(switchFocused, paint, {600, 402, 140, 36});
        draw(switchDisabled, paint, {758, 402, 150, 36});
        wui::Switch before("Before", false); before.labelPosition(wui::SwitchLabelPosition::Before);
        wui::Switch above("Required", true); above.labelPosition(wui::SwitchLabelPosition::Above).required(true);
        draw(before, paint, {28, 450, 150, 36});
        draw(above, paint, {216, 446, 150, 56});

        section(paint, "RADIO GROUP LAYOUTS / DISABLED OPTION / SELECTED STATE", 540);
        wui::RadioGroup verticalGroup;
        verticalGroup.accessibleLabel("Vertical choices").value("two");
        verticalGroup.addOption("one", "First");
        verticalGroup.addOption("two", "Second");
        verticalGroup.addOption("three", "Unavailable", false);
        draw(verticalGroup, paint, {28, 558, 220, 104});
        wui::RadioGroup horizontalGroup;
        horizontalGroup.groupLayout(wui::RadioGroupLayout::Horizontal).value("b");
        horizontalGroup.addOption("a", "Alpha");
        horizontalGroup.addOption("b", "Beta");
        horizontalGroup.addOption("c", "Gamma");
        draw(horizontalGroup, paint, {300, 558, 450, 40});
        wui::RadioGroup stackedGroup;
        stackedGroup.groupLayout(wui::RadioGroupLayout::HorizontalStacked).value("center");
        stackedGroup.addOption("left", "Left");
        stackedGroup.addOption("center", "Center");
        stackedGroup.addOption("right", "Right");
        draw(stackedGroup, paint, {300, 620, 450, 60});

        section(paint, "VERTICAL DIVIDER", 716);
        wui::Divider verticalDivider(wui::DividerOrientation::Vertical);
        verticalDivider.appearance(wui::DividerAppearance::Brand).content("OR");
        draw(verticalDivider, paint, {858, 548, 72, 150});

        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        if (pixels.size() != static_cast<std::size_t>(width * height * 4)) return 3;
        // Determinate brand progress must remain a solid semantic token at
        // both 100% and 150% DPR; this also catches accidental double scaling.
        if (!pixelIs(pixels, width, scale, 100, 309,
                     wui::theme().colors.brandBackground.rest)) return 4;
        savePpm(output, pixels, width, height);
        wui::setTextMeasurer(nullptr);
        return 0;
    } catch (...) {
        wui::setTextMeasurer(nullptr);
        return 1;
    }
}
