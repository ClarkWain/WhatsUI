#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/accessibility.h"
#include "wui/avatar.h"
#include "wui/paint_context.h"
#include "wui/whatscanvas_text.h"

namespace {
void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void testAvatarSemanticsAndSizing()
{
    wui::Avatar avatar("Ada Lovelace", wui::AvatarSize::Size40);
    expect(avatar.displayedInitials() == "AL", "Avatar must derive stable two-word initials");
    expect(std::fabs(avatar.measure({}).width - 40.0f) < 0.001f,
           "Avatar must use its Fluent size as a square intrinsic size");
    avatar.setAccessibleLabel("Task owner Ada Lovelace");
    const auto snapshot = wui::snapshotAccessibilityTree(avatar);
    expect(snapshot.size() == 1 && snapshot.front().properties.role == wui::AccessibilityRole::Image &&
               snapshot.front().properties.label == "Task owner Ada Lovelace",
           "Avatar must expose an informative image semantic");

    const std::vector<unsigned char> pixels{255, 0, 0, 255, 0, 255, 0, 255,
                                            0, 0, 255, 255, 255, 255, 255, 255};
    avatar.image(wui::ImageSource(pixels, 2, 2));
    expect(avatar.hasImage(), "Avatar must accept a reusable ImageSource");
    avatar.clearImage();
    expect(!avatar.hasImage(), "Clearing an Avatar image must restore its initials fallback");
}

void testAvatarGroupOverflowAndPaint()
{
    wui::AvatarGroup group;
    group.size(wui::AvatarSize::Size32).maxVisible(2).accessibleLabel("Project members");
    group.addAvatar("Ada Lovelace");
    group.addAvatar("Grace Hopper");
    group.addAvatar("Margaret Hamilton");
    expect(group.measure({}).width > 64.0f && group.measure({}).height == 32.0f,
           "AvatarGroup overflow must reserve a visible +N indicator");
    const auto snapshot = wui::snapshotAccessibilityTree(group);
    expect(!snapshot.empty() && snapshot.front().properties.role == wui::AccessibilityRole::Group &&
               snapshot.front().properties.label == "Project members",
           "AvatarGroup must expose one named grouping semantic");

    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, 240, 80);
    expect(canvas && canvas->initializeContext(), "Avatar visual canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas);
    wui::setTextMeasurer(&measurer);
    wui::PaintContext paint(*canvas);
    canvas->beginFrame();
    group.layout({12, 18, 140, 32});
    group.prepare(paint);
    group.paint(paint);
    canvas->endFrame();
    expect(paint.paintStats().textDrawCalls >= 3,
           "AvatarGroup must paint initials and the overflow count rather than hiding excess people");
    wui::setTextMeasurer(nullptr);
}
} // namespace

int main()
{
    try {
        testAvatarSemanticsAndSizing();
        testAvatarGroupOverflowAndPaint();
        std::cout << "WhatsUI Avatar tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WhatsUI Avatar tests failed: " << error.what() << '\n';
        return 1;
    }
}
