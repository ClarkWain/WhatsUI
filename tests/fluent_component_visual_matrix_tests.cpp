#include <fstream>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/basic_controls.h"
#include "wui/overlays.h"
#include "wui/paint_context.h"
#include "wui/text_input.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"
#include "wui/widgets.h"

namespace {
constexpr int logicalWidth = 960;
constexpr int logicalHeight = 1380;

void savePpm(const std::string& path, const std::vector<unsigned char>& rgba, int width, int height)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create Fluent visual matrix");
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        out.put(static_cast<char>(rgba[i]));
        out.put(static_cast<char>(rgba[i + 1]));
        out.put(static_cast<char>(rgba[i + 2]));
    }
}

void label(wui::PaintContext& paint, const std::string& text, float x, float y)
{
    paint.drawText(text, x, y + 16.0f, 12.0f, wui::theme().colors.neutralForeground3, 600);
}

void draw(wui::Node& node, wui::PaintContext& paint, wui::RectF bounds)
{
    node.layout(bounds);
    node.prepare(paint);
    node.paint(paint);
}

bool pixelIs(const std::vector<unsigned char>& rgba, int width, float scale,
             int logicalX, int logicalY, wui::Color color)
{
    const int x = static_cast<int>(std::lround(static_cast<float>(logicalX) * scale));
    const int y = static_cast<int>(std::lround(static_cast<float>(logicalY) * scale));
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    return offset + 3 < rgba.size() && rgba[offset] == color.r
        && rgba[offset + 1] == color.g && rgba[offset + 2] == color.b
        && rgba[offset + 3] == color.a;
}

// The visual matrix deliberately runs with the Software backend at both 100%
// and 150%.  Looking for dark glyph pixels in a control's content box gives us
// a renderer-independent check that its actual ink (not merely its nominal
// baseline) is vertically centred.  This catches the otherwise subtle case
// where a BOTTOM-anchored renderer receives a line-height-based y coordinate.
bool darkInkIsVerticallyCentered(const std::vector<unsigned char>& rgba, int width,
                                 float scale, wui::RectF inkRegion,
                                 float expectedCenterY, float tolerance,
                                 int maxChannel = 105)
{
    const int height = static_cast<int>(rgba.size() / 4U / static_cast<std::size_t>(width));
    const int left = std::clamp(static_cast<int>(std::lround(inkRegion.x * scale)), 0, width);
    const int top = std::clamp(static_cast<int>(std::lround(inkRegion.y * scale)), 0, height);
    const int right = std::clamp(static_cast<int>(std::lround((inkRegion.x + inkRegion.width) * scale)), 0, width);
    const int bottom = std::clamp(static_cast<int>(std::lround((inkRegion.y + inkRegion.height) * scale)), 0, height);
    int inkTop = height;
    int inkBottom = -1;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const auto offset = static_cast<std::size_t>((y * width + x) * 4);
            if (rgba[offset] <= maxChannel && rgba[offset + 1] <= maxChannel
                && rgba[offset + 2] <= maxChannel) {
                inkTop = std::min(inkTop, y);
                inkBottom = std::max(inkBottom, y);
            }
        }
    }
    if (inkBottom < inkTop) return false;
    const float inkCenter = (static_cast<float>(inkTop) + static_cast<float>(inkBottom)) * 0.5f / scale;
    return std::abs(inkCenter - expectedCenterY) <= tolerance;
}
}

int main(int argc, char** argv)
{
    try {
        const std::string output = argc > 1 ? argv[1] : "fluent_component_visual_matrix.ppm";
        const float scale = argc > 2 ? std::max(1.0f, std::stof(argv[2])) : 1.0f;
        const int width = static_cast<int>(std::lround(logicalWidth * scale));
        const int height = static_cast<int>(std::lround(logicalHeight * scale));
        auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
        if (!canvas || !canvas->initializeContext()) return 2;
        wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
        wui::setTextMeasurer(&measurer);
        wui::PaintContext paint(*canvas, scale);
        canvas->beginFrame();
        paint.fillRect({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)}, wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent component state matrix", 28, 42, 24, wui::theme().colors.neutralForeground1, 600);

        label(paint, "BUTTONS", 28, 60);
        std::vector<std::unique_ptr<wui::Button>> buttons;
        for (auto appearance : {wui::ButtonAppearance::Secondary, wui::ButtonAppearance::Primary,
                                wui::ButtonAppearance::Outline, wui::ButtonAppearance::Subtle,
                                wui::ButtonAppearance::Danger}) {
            auto button = std::make_unique<wui::Button>("Action");
            button->setAppearance(appearance);
            buttons.push_back(std::move(button));
        }
        buttons[1]->setVisualState(wui::ControlVisualState::Hovered, true);
        buttons[2]->setVisualState(wui::ControlVisualState::Pressed, true);
        buttons[3]->setVisualState(wui::ControlVisualState::Focused, true);
        buttons[4]->setEnabled(false);
        for (std::size_t i = 0; i < buttons.size(); ++i) draw(*buttons[i], paint, {28.0f + i * 128.0f, 86, 110, 32});
        wui::ToggleButton toggle("Pinned", true); draw(toggle, paint, {676, 86, 110, 32});
        wui::CompoundButton compound("Create list", "Organize tasks"); draw(compound, paint, {804, 76, 136, 52});

        label(paint, "SELECTION CONTROLS", 28, 142);
        wui::Checkbox checkRest("Rest", false), checkHover("Hover", false), checkOn("Checked", true), checkOff("Disabled", false);
        checkHover.setVisualState(wui::ControlVisualState::Hovered, true); checkOff.setEnabled(false);
        draw(checkRest, paint, {28, 168, 120, 32}); draw(checkHover, paint, {158, 168, 120, 32});
        draw(checkOn, paint, {288, 168, 130, 32}); draw(checkOff, paint, {428, 168, 130, 32});
        wui::Radio radio("Selected", true); draw(radio, paint, {580, 168, 130, 32});
        wui::Switch switchOn("On", true); draw(switchOn, paint, {730, 168, 110, 32});

        label(paint, "TEXT FIELDS", 28, 218);
        wui::TextInput placeholder("Placeholder"), focused("Focused"), invalid("Invalid"), disabled("Disabled");
        focused.text("Editing text"); focused.setVisualState(wui::ControlVisualState::Focused, true);
        invalid.setInvalid(true); disabled.setEnabled(false);
        draw(placeholder, paint, {28, 244, 210, 32}); draw(focused, paint, {254, 244, 210, 32});
        draw(invalid, paint, {480, 244, 210, 32}); draw(disabled, paint, {706, 244, 210, 32});
        wui::TextArea area("Multi-line placeholder"); area.text("First line\nSecond line\nThird line\nFourth line");
        area.layout({28, 294, 438, 84}); area.setVisualState(wui::ControlVisualState::Focused, true); area.prepare(paint); area.paint(paint);
        wui::TextArea areaInvalid("Required notes"); areaInvalid.setInvalid(true); draw(areaInvalid, paint, {480, 294, 436, 84});

        label(paint, "PROGRESS AND RANGE", 28, 396);
        wui::ProgressBar empty(0.0f, 1.0f, 0.0f), progress(0.0f, 1.0f, .62f),
            full(0.0f, 1.0f, 1.0f);
        draw(empty, paint, {28, 426, 270, 12}); draw(progress, paint, {330, 426, 270, 12}); draw(full, paint, {632, 426, 284, 12});
        wui::Slider slider(0, 100, 44); draw(slider, paint, {28, 454, 438, 32});
        wui::Slider sliderDisabled(0, 100, 70); sliderDisabled.setEnabled(false); draw(sliderDisabled, paint, {480, 454, 436, 32});

        label(paint, "CARDS", 28, 510);
        wui::Card card; card.setAppearance(wui::CardAppearance::Filled); card.child(std::make_unique<wui::CardHeader>("Filled card", "Header description"));
        draw(card, paint, {28, 540, 276, 100});
        wui::Card selected; selected.setAppearance(wui::CardAppearance::Outline); selected.setSelected(true); selected.child(std::make_unique<wui::CardHeader>("Selected card", "Brand selection state"));
        draw(selected, paint, {328, 540, 276, 100});
        wui::Card disabledCard; disabledCard.setEnabled(false); disabledCard.child(std::make_unique<wui::CardHeader>("Disabled card", "Non-interactive state"));
        draw(disabledCard, paint, {628, 540, 288, 100});

        label(paint, "BUTTON SIZES AND SHAPES", 28, 662);
        wui::Button small("Small"); small.setSize(wui::ButtonSize::Small); draw(small, paint, {28, 690, 82, 24});
        wui::Button large("Large"); large.setSize(wui::ButtonSize::Large); draw(large, paint, {128, 682, 112, 40});
        wui::Button square("Square"); square.setShape(wui::ButtonShape::Square); draw(square, paint, {260, 686, 100, 32});
        wui::Button circular("+"); circular.setShape(wui::ButtonShape::Circular); draw(circular, paint, {380, 686, 32, 32});
        wui::ToggleButton toggleOff("Toggle", false); toggleOff.setVisualState(wui::ControlVisualState::Hovered, true); draw(toggleOff, paint, {438, 686, 100, 32});

        label(paint, "LABEL, SEARCH AND DIVIDER", 28, 742);
        wui::TextInput labelled("Search by title");
        wui::Label fieldLabel("Task search"); fieldLabel.setForControl(&labelled); fieldLabel.setRequired(true);
        draw(fieldLabel, paint, {28, 768, 140, 20}); draw(labelled, paint, {180, 760, 260, 32});
        wui::SearchField search("Search tasks"); search.query("Fluent"); draw(search, paint, {468, 760, 260, 32});
        wui::Divider divider; draw(divider, paint, {28, 814, 888, 1});

        label(paint, "COMPOUND BUTTON STATES", 28, 846);
        wui::CompoundButton compoundRest("Rest", "Description");
        wui::CompoundButton compoundHover("Hover", "Description");
        wui::CompoundButton compoundPressed("Pressed", "Description");
        wui::CompoundButton compoundFocused("Focused", "Description");
        wui::CompoundButton compoundDisabled("Disabled", "Description");
        compoundHover.setVisualState(wui::ControlVisualState::Hovered, true);
        compoundPressed.setVisualState(wui::ControlVisualState::Pressed, true);
        compoundFocused.setVisualState(wui::ControlVisualState::Focused, true);
        compoundDisabled.setEnabled(false);
        draw(compoundRest, paint, {28, 872, 160, 52});
        draw(compoundHover, paint, {206, 872, 160, 52});
        draw(compoundPressed, paint, {384, 872, 160, 52});
        draw(compoundFocused, paint, {562, 872, 160, 52});
        draw(compoundDisabled, paint, {740, 872, 176, 52});

        label(paint, "TOGGLE BUTTON STATES", 28, 946);
        wui::ToggleButton toggleRest("Rest", false), toggleHover("Hover", false),
            togglePressed("Pressed", false), toggleFocused("Selected", true),
            toggleDisabled("Disabled", true);
        toggleHover.setVisualState(wui::ControlVisualState::Hovered, true);
        togglePressed.setVisualState(wui::ControlVisualState::Pressed, true);
        toggleFocused.setVisualState(wui::ControlVisualState::Focused, true);
        toggleDisabled.setEnabled(false);
        draw(toggleRest, paint, {28, 972, 150, 32});
        draw(toggleHover, paint, {196, 972, 150, 32});
        draw(togglePressed, paint, {364, 972, 150, 32});
        draw(toggleFocused, paint, {532, 972, 168, 32});
        draw(toggleDisabled, paint, {718, 972, 198, 32});

        label(paint, "CHECKBOX, RADIO AND SWITCH STATES", 28, 1026);
        wui::Checkbox checkboxPressed("Pressed", false), checkboxFocused("Focused", true);
        checkboxPressed.setVisualState(wui::ControlVisualState::Pressed, true);
        checkboxFocused.setVisualState(wui::ControlVisualState::Focused, true);
        draw(checkboxPressed, paint, {28, 1052, 146, 32});
        draw(checkboxFocused, paint, {190, 1052, 146, 32});

        wui::Radio radioRest("Rest", false), radioHover("Hover", false),
            radioPressed("Pressed", false), radioFocused("Focused", true),
            radioDisabled("Disabled", true);
        radioHover.setVisualState(wui::ControlVisualState::Hovered, true);
        radioPressed.setVisualState(wui::ControlVisualState::Pressed, true);
        radioFocused.setVisualState(wui::ControlVisualState::Focused, true);
        radioDisabled.setEnabled(false);
        draw(radioRest, paint, {352, 1052, 102, 32});
        draw(radioHover, paint, {464, 1052, 102, 32});
        draw(radioPressed, paint, {576, 1052, 112, 32});
        draw(radioFocused, paint, {698, 1052, 112, 32});
        draw(radioDisabled, paint, {820, 1052, 120, 32});

        wui::Switch switchRest("Off", false), switchHover("Hover", false),
            switchPressed("Pressed", false), switchFocused("Focused", true),
            switchDisabled("Disabled", true);
        switchHover.setVisualState(wui::ControlVisualState::Hovered, true);
        switchPressed.setVisualState(wui::ControlVisualState::Pressed, true);
        switchFocused.setVisualState(wui::ControlVisualState::Focused, true);
        switchDisabled.setEnabled(false);
        draw(switchRest, paint, {28, 1096, 120, 32});
        draw(switchHover, paint, {166, 1096, 134, 32});
        draw(switchPressed, paint, {318, 1096, 142, 32});
        draw(switchFocused, paint, {478, 1096, 142, 32});
        draw(switchDisabled, paint, {638, 1096, 160, 32});

        label(paint, "SEARCH FIELD STATES AND RANGE EXTREMES", 28, 1152);
        wui::SearchField searchRest("Search"), searchFocused("Focused search"),
            searchDisabled("Disabled search");
        searchFocused.query("Task");
        searchFocused.setVisualState(wui::ControlVisualState::Focused, true);
        searchDisabled.setEnabled(false);
        draw(searchRest, paint, {28, 1178, 270, 32});
        draw(searchFocused, paint, {316, 1178, 270, 32});
        draw(searchDisabled, paint, {604, 1178, 312, 32});

        wui::Slider sliderMinimum(0, 100, 0), sliderMaximum(0, 100, 100);
        draw(sliderMinimum, paint, {28, 1230, 420, 32});
        draw(sliderMaximum, paint, {496, 1230, 420, 32});
        wui::ProgressBar progressQuarter(0, 1, .25f);
        draw(progressQuarter, paint, {28, 1282, 888, 12});
        wui::Divider verticalDivider(wui::DividerOrientation::Vertical);
        draw(verticalDivider, paint, {478, 1314, 1, 42});

        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        if (pixels.size() != static_cast<std::size_t>(width * height * 4)) return 3;
        // A Checkbox focus ring is scoped to its indicator; the transparent
        // label tail must retain the page surface instead of becoming an
        // opaque focus-inner rectangle.
        if (!pixelIs(pixels, width, scale, 330, 1068,
                     wui::theme().colors.neutralBackground2.rest)) return 4;
        // State surfaces and focus geometry must remain distinct: pressed is
        // the documented neutral pressed token, while focus retains both the
        // outer white and inner black ring outside the control's fill.
        if (!pixelIs(pixels, width, scale, 300, 94,
                     wui::theme().colors.neutralBackground1.pressed)
            || !pixelIs(pixels, width, scale, 416, 83,
                        wui::theme().colors.strokeFocusOuter)
            || !pixelIs(pixels, width, scale, 416, 84,
                        wui::theme().colors.strokeFocusInner)
            || !pixelIs(pixels, width, scale, 300, 273,
                        wui::theme().colors.brandForeground1)) return 5;
        // Test the physical ink rather than nominal text coordinates.  The
        // regions exclude borders, carets and focus rings, and include Button,
        // ToggleButton, Input, TextArea, SearchField and CompoundButton.
        if (!darkInkIsVerticallyCentered(pixels, width, scale,
                                         {42, 94, 82, 18}, 102.0f, 1.5f)
            || !darkInkIsVerticallyCentered(pixels, width, scale,
                                            {40, 978, 126, 20}, 988.0f, 1.5f)
            || !darkInkIsVerticallyCentered(pixels, width, scale,
                                            {270, 250, 184, 22}, 260.0f, 1.5f)
            || !darkInkIsVerticallyCentered(pixels, width, scale,
                                            {42, 296, 408, 18}, 305.0f, 1.5f)
            || !darkInkIsVerticallyCentered(pixels, width, scale,
                                            {480, 764, 236, 24}, 776.0f, 1.5f)
            || !darkInkIsVerticallyCentered(pixels, width, scale,
                                            {38, 880, 142, 20}, 890.0f, 1.5f)
            || !darkInkIsVerticallyCentered(pixels, width, scale,
                                            {38, 900, 142, 16}, 908.0f, 1.5f,
                                            155)) return 6;
        savePpm(output, pixels, width, height);
        wui::setTextMeasurer(nullptr);
        return 0;
    } catch (...) {
        wui::setTextMeasurer(nullptr);
        return 1;
    }
}
