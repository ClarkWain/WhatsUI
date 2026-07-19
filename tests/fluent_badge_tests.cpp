#include <iostream>
#include <stdexcept>

#include "wui/accessibility.h"
#include "wui/badge.h"
#include "wui/widgets.h"

namespace {
void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void testBadgeVariantsAndSizing()
{
    wui::Badge badge("New");
    const auto medium = badge.measure({0, 1000, 0, 1000});
    badge.setSize(wui::BadgeSize::ExtraLarge);
    const auto large = badge.measure({0, 1000, 0, 1000});
    expect(large.height > medium.height && large.width > medium.width,
           "Badge sizes must use Fluent's increasing density scale");
    badge.setAppearance(wui::BadgeAppearance::Outline);
    badge.setColor(wui::BadgeColor::Success);
    expect(badge.appearance() == wui::BadgeAppearance::Outline &&
               badge.color() == wui::BadgeColor::Success,
           "Badge must retain independent appearance and semantic color");
    badge.setAccessibleLabel("New activity");
    expect(badge.generatedAccessibleLabel() == "New activity",
           "Badge must allow an explicit compact-content accessible label");
}

void testCounterOverflowAndAccessibility()
{
    wui::CounterBadge counter(0);
    expect(counter.text().empty() && counter.measure({0, 100, 0, 100}).width == 0.0f,
           "CounterBadge must hide an empty zero count by default");
    counter.setShowZero(true);
    expect(counter.text() == "0", "CounterBadge must render zero when requested");
    counter.setMax(9);
    counter.setCount(12);
    expect(counter.text() == "9+", "CounterBadge must clamp visual text to max plus");
    expect(counter.generatedAccessibleLabel() == "12 notifications",
           "CounterBadge accessibility must retain the real untruncated count");
    counter.setCount(1);
    expect(counter.generatedAccessibleLabel() == "1 notification",
           "CounterBadge must pluralize its generated label");
}

void testPresenceGeometryAndSemantics()
{
    wui::PresenceBadge presence(wui::PresenceStatus::DoNotDisturb);
    presence.setAvatarSize(64.0f);
    const auto extent = presence.measure({0, 100, 0, 100});
    expect(extent.width == 16.0f && extent.height == 16.0f,
           "PresenceBadge must cap overlay size at Fluent's large-avatar status extent");
    const wui::RectF avatar{10, 20, 64, 64};
    const auto bottomRight = presence.boundsForAvatar(avatar);
    presence.setPosition(wui::PresenceBadgePosition::TopLeft);
    const auto topLeft = presence.boundsForAvatar(avatar);
    expect(bottomRight.x > avatar.x && bottomRight.y > avatar.y &&
               topLeft.x < avatar.x + 1.0f && topLeft.y < avatar.y + 1.0f,
           "PresenceBadge must expose deterministic Avatar overlay positions");
    expect(presence.generatedAccessibleLabel() == "Do not disturb",
           "PresenceBadge must expose status text rather than a color-only meaning");
}

void testAccessibilitySnapshot()
{
    auto root = std::make_unique<wui::Container>();
    auto badge = std::make_unique<wui::Badge>("Beta");
    badge->setAccessibleLabel("Beta feature");
    root->appendChild(std::move(badge));
    auto counter = std::make_unique<wui::CounterBadge>(101);
    root->appendChild(std::move(counter));
    auto presence = std::make_unique<wui::PresenceBadge>(wui::PresenceStatus::Away);
    root->appendChild(std::move(presence));
    root->layout({0, 0, 240, 40});
    const auto snapshot = wui::snapshotAccessibilityTree(*root);
    expect(snapshot.size() == 3 && snapshot[0].properties.label == "Beta feature" &&
               snapshot[1].properties.label == "101 notifications" &&
               snapshot[2].properties.label == "Away",
           "Badge family must project non-interactive descriptive semantics");
}
}

int main()
{
    try {
        testBadgeVariantsAndSizing();
        testCounterOverflowAndAccessibility();
        testPresenceGeometryAndSemantics();
        testAccessibilitySnapshot();
        std::cout << "Fluent Badge tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fluent Badge test failure: " << error.what() << '\n';
        return 1;
    }
}
