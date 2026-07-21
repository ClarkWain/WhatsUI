// Fluent Card contract tests.  The model checks exercise dynamic slots and
// selection semantics; the Software capture makes the Card composition and
// state matrix reviewable without a native desktop.

#include <fstream>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/accessibility.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"
#include "wui/widgets.h"

namespace {

constexpr int kWidth = 760;
constexpr int kHeight = 376;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class FixedNode final : public wui::Node {
public:
    FixedNode(wui::SizeF size, wui::Color color = {0, 0, 0, 0})
        : size_(size), color_(color) {}

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp(size_);
    }

    void paint(wui::PaintContext& context) override
    {
        if (color_.a != 0) context.fillRect(bounds(), color_);
        clearDirty(wui::DirtyFlag::Paint);
    }

private:
    wui::SizeF size_;
    wui::Color color_;
};

void savePpm(const std::string& path, const std::vector<unsigned char>& rgba,
             int width, int height)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create Fluent Card visual capture");
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t index = 0; index + 3 < rgba.size(); index += 4) {
        out.put(static_cast<char>(rgba[index]));
        out.put(static_cast<char>(rgba[index + 1]));
        out.put(static_cast<char>(rgba[index + 2]));
    }
}

bool containsColor(const std::vector<unsigned char>& rgba, wui::Color color)
{
    for (std::size_t index = 0; index + 3 < rgba.size(); index += 4) {
        if (rgba[index] == color.r && rgba[index + 1] == color.g &&
            rgba[index + 2] == color.b && rgba[index + 3] == color.a) {
            return true;
        }
    }
    return false;
}

wui::Color pixelAt(const std::vector<unsigned char>& rgba, int width,
                   float scale, float logicalX, float logicalY)
{
    const int x = static_cast<int>(std::lround(logicalX * scale));
    const int y = static_cast<int>(std::lround(logicalY * scale));
    const std::size_t offset =
        static_cast<std::size_t>((y * width + x) * 4);
    if (offset + 3 >= rgba.size()) return {};
    return {rgba[offset], rgba[offset + 1], rgba[offset + 2],
            rgba[offset + 3]};
}

int luminance(wui::Color color) noexcept
{
    return static_cast<int>(color.r) + static_cast<int>(color.g) +
        static_cast<int>(color.b);
}

wui::PointerEvent pointer(wui::PointerAction action, wui::PointF position,
                          wui::MouseButton button = wui::MouseButton::None)
{
    wui::PointerEvent event;
    event.action = action;
    event.position = position;
    event.button = button;
    return event;
}

void testHeaderSlotsAreStableAndReplaceable()
{
    wui::CardHeader header("Task plan", "Three actions due today");
    auto firstAction = std::make_unique<FixedNode>(wui::SizeF{32.0f, 24.0f});
    wui::Node* const firstActionRaw = firstAction.get();
    header.action(std::move(firstAction));
    auto media = std::make_unique<FixedNode>(wui::SizeF{24.0f, 24.0f});
    wui::Node* const mediaRaw = media.get();
    // Configure action first to verify that media() does not encode call order
    // into child storage or layout.
    header.media(std::move(media));
    expect(header.media() == mediaRaw && header.action() == firstActionRaw &&
               header.children().size() == 2 && header.children()[0].get() == mediaRaw &&
               header.children()[1].get() == firstActionRaw,
           "CardHeader must maintain media/action slots independent of configuration order");

    auto replacement = std::make_unique<FixedNode>(wui::SizeF{40.0f, 24.0f});
    wui::Node* const replacementRaw = replacement.get();
    header.setAction(std::move(replacement));
    expect(header.media() == mediaRaw && header.action() == replacementRaw &&
               header.children()[0].get() == mediaRaw && header.children()[1].get() == replacementRaw,
           "CardHeader action replacement must preserve the media slot and visual order");

    header.setMedia(nullptr);
    expect(header.media() == nullptr && header.action() == replacementRaw && header.children().size() == 1,
           "CardHeader must support clearing media without losing a dynamic trailing action");
    header.layout({0.0f, 0.0f, 240.0f, 36.0f});
    expect(header.action()->bounds().x >= 200.0f,
           "CardHeader trailing action must remain right aligned after media replacement");

    wui::CardHeader narrow(
        "A long title that must never enter the trailing action",
        "Metadata is ellipsized inside the text column");
    narrow.media(
        std::make_unique<FixedNode>(wui::SizeF{32.0f, 32.0f}));
    narrow.action(
        std::make_unique<FixedNode>(wui::SizeF{32.0f, 32.0f}));
    narrow.layout({0.0f, 0.0f, 144.0f, 36.0f});
    expect(narrow.media()->bounds().x + narrow.media()->bounds().width +
                   wui::theme().spacing.horizontal.m <=
               narrow.action()->bounds().x -
                   wui::theme().spacing.horizontal.m,
           "A narrow CardHeader must preserve distinct media, text and trailing-action columns");

    wui::CardFooter footer;
    footer.child(
        std::make_unique<FixedNode>(wui::SizeF{44.0f, 20.0f}));
    footer.child(
        std::make_unique<FixedNode>(wui::SizeF{32.0f, 20.0f}));
    expect(std::abs(footer.measure({}).width - 88.0f) < 0.001f,
           "CardFooter measure must equal child widths plus its 12 DIP action gap");
    footer.layout({0.0f, 0.0f, 200.0f, 32.0f});
    expect(std::abs(footer.children()[1]->bounds().x - 56.0f) < 0.001f,
           "CardFooter layout must use the same 12 DIP gap as measurement");
}

void testSelectableCardStatesAndAccessibility()
{
    wui::Card card;
    card.selectable();
    card.setAppearance(wui::CardAppearance::Outline);
    card.child(std::make_unique<FixedNode>(wui::SizeF{160.0f, 72.0f}));
    card.layout({0.0f, 0.0f, 200.0f, 104.0f});
    int changes = 0;
    card.onSelectionChange([&changes](bool) { ++changes; });

    expect(card.onPointerEvent(pointer(wui::PointerAction::Enter, {4.0f, 4.0f})),
           "Selectable Card must accept hover input");
    expect((card.visualStates() & wui::toMask(wui::ControlVisualState::Hovered)) != 0,
           "Selectable Card must expose hover state for Fluent visual resolution");
    expect(card.onPointerEvent(pointer(wui::PointerAction::Down, {4.0f, 4.0f}, wui::MouseButton::Left)),
           "Selectable Card must accept pressed input");
    expect((card.visualStates() & wui::toMask(wui::ControlVisualState::Pressed)) != 0 &&
               (card.visualStates() & wui::toMask(wui::ControlVisualState::Focused)) != 0,
           "Pointer activation must set pressed and focused Card state together");
    expect(card.onPointerEvent(pointer(wui::PointerAction::Up, {4.0f, 4.0f}, wui::MouseButton::Left)) && card.isSelected(),
           "Pointer release inside a selectable Card must change selection");
    expect(card.accessibilityActions().toggle,
           "Selectable Card must expose a programmatic selection action");
    expect(card.performAccessibilityAction(wui::AccessibilityActionKind::Toggle, {}) ==
               wui::AccessibilityActionStatus::Succeeded && !card.isSelected() && changes == 2,
           "Card accessibility Toggle must reuse the selection-change callback contract");

    const auto snapshot = wui::snapshotAccessibilityTree(card, &card);
    expect(snapshot.size() == 1 && snapshot.front().properties.role == wui::AccessibilityRole::ListItem &&
               snapshot.front().properties.checked.has_value() && !*snapshot.front().properties.checked,
           "Selectable Card must project ListItem and selected state through accessibility snapshots");
    card.setEnabled(false);
    expect(!card.onPointerEvent(pointer(wui::PointerAction::Enter, {4.0f, 4.0f})) &&
               card.performAccessibilityAction(wui::AccessibilityActionKind::Toggle, {}) ==
                   wui::AccessibilityActionStatus::ElementNotEnabled,
           "Disabled Card must reject pointer and accessibility selection actions");
}

std::unique_ptr<wui::Card> makeCard(wui::CardAppearance appearance, bool selectable, bool selected)
{
    auto card = std::make_unique<wui::Card>();
    card->setAppearance(appearance);
    card->selectable(selectable);
    card->setSelected(selected);

    auto header = std::make_unique<wui::CardHeader>("Today", "Review the next important task");
    header->action(std::make_unique<FixedNode>(wui::SizeF{28.0f, 24.0f}, wui::theme().colors.brandBackground.rest));
    header->media(std::make_unique<FixedNode>(wui::SizeF{24.0f, 24.0f}, wui::theme().colors.statusInfo));
    card->child(std::move(header));

    auto preview = std::make_unique<wui::CardPreview>();
    preview->setHeight(44.0f);
    preview->child(std::make_unique<FixedNode>(wui::SizeF{200.0f, 44.0f}, wui::theme().colors.brandBackground.hover));
    card->child(std::move(preview));

    auto footer = std::make_unique<wui::CardFooter>();
    footer->child(std::make_unique<FixedNode>(wui::SizeF{44.0f, 20.0f}, wui::theme().colors.statusSuccess));
    footer->child(std::make_unique<FixedNode>(wui::SizeF{32.0f, 20.0f}, wui::theme().colors.statusWarning));
    card->child(std::move(footer));
    return card;
}

void testSoftwareCompositionAndWriteReviewImage(const std::string& output,
                                                float scale)
{
    scale = std::max(1.0f, scale);
    const int width = static_cast<int>(std::lround(kWidth * scale));
    const int height = static_cast<int>(std::lround(kHeight * scale));
    auto canvas =
        wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(), "Software canvas must initialize for Fluent Card visual review");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
    wui::setTextMeasurer(&measurer);
    try {
        wui::PaintContext paint(*canvas, scale);
        auto filled = makeCard(wui::CardAppearance::Filled, true, false);
        auto alternative = makeCard(wui::CardAppearance::FilledAlternative, true, false);
        auto outline = makeCard(wui::CardAppearance::Outline, true, true);
        auto subtle = makeCard(wui::CardAppearance::Subtle, true, false);
        alternative->setVisualState(wui::ControlVisualState::Hovered, true);
        outline->setVisualState(wui::ControlVisualState::Focused, true);
        subtle->setVisualState(wui::ControlVisualState::Pressed, true);

        canvas->beginFrame();
        paint.fillRect({0.0f, 0.0f, static_cast<float>(kWidth), static_cast<float>(kHeight)},
                       wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent Card state matrix", 24.0f, 34.0f, 20.0f,
                       wui::theme().colors.neutralForeground1, 600);
        const std::vector<std::pair<wui::Card*, wui::RectF>> cards{
            {filled.get(), {24.0f, 56.0f, 340.0f, 136.0f}},
            {alternative.get(), {396.0f, 56.0f, 340.0f, 136.0f}},
            {outline.get(), {24.0f, 216.0f, 340.0f, 136.0f}},
            {subtle.get(), {396.0f, 216.0f, 340.0f, 136.0f}},
        };
        for (const auto& entry : cards) {
            entry.first->layout(entry.second);
            entry.first->prepare(paint);
            entry.first->paint(paint);
        }
        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() ==
                   static_cast<std::size_t>(width * height * 4),
               "Card Software capture must return a complete RGBA frame");
        expect(containsColor(pixels,
                             wui::theme().colors.neutralStroke1Selected),
               "Selected Card must render Fluent's neutral selected stroke");
        expect(paint.paintStats().boxShadowCalls >= 4,
               "Filled Card states must issue Fluent Shadow 04/08 layers");
        const auto restShadow =
            pixelAt(pixels, width, scale, 194.0f, 198.0f);
        const auto hoverShadow =
            pixelAt(pixels, width, scale, 566.0f, 198.0f);
        expect(luminance(hoverShadow) < luminance(restShadow),
               "Hovered Filled Card Shadow 08 must extend farther than resting Shadow 04");
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
        testHeaderSlotsAreStableAndReplaceable();
        testSelectableCardStatesAndAccessibility();
        testSoftwareCompositionAndWriteReviewImage(
            argc > 1 ? argv[1] : "fluent_card_review.ppm",
            argc > 2 ? std::stof(argv[2]) : 1.0f);
        std::cout << "WhatsUI Fluent Card tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WhatsUI Fluent Card tests failed: " << error.what() << '\n';
        return 1;
    }
}
