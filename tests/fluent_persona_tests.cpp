#include <cmath>
#include <iostream>
#include <stdexcept>

#include "wui/accessibility.h"
#include "wui/persona.h"
#include "wui/ui.h"

namespace {
void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

wui::PointerEvent primaryPointer(wui::PointerAction action, wui::PointF position)
{
    wui::PointerEvent event;
    event.pointerType = wui::PointerType::Mouse;
    event.action = action;
    event.button = wui::MouseButton::Left;
    event.position = position;
    return event;
}

void testFluentSizingAndTextContract()
{
    wui::Persona persona("Ada Lovelace", wui::PersonaSize::Medium);
    expect(persona.avatarSize() == wui::AvatarSize::Size32,
           "Fluent medium Persona must map to a 32 DIP Avatar");
    persona.setSize(wui::PersonaSize::Huge);
    expect(persona.avatarSize() == wui::AvatarSize::Size56,
           "Fluent huge Persona must map to a 56 DIP Avatar");
    persona.setSize(wui::PersonaSize::Small);
    persona.setPrimaryText("Ada");
    persona.setSecondaryText("Mathematician");
    persona.setTertiaryText("Analytical Engine");
    persona.setQuaternaryText("London");
    const wui::SizeF preferred = persona.measure({});
    expect(preferred.width > 28.0f && preferred.height >= 28.0f,
           "Persona must reserve an Avatar and a readable text block");
    persona.setTextPosition(wui::PersonaTextPosition::Below);
    const wui::SizeF below = persona.measure({});
    expect(below.height > preferred.height,
           "Below text position must stack text below the visual identity");
    expect(persona.generatedAccessibleLabel() == "Ada, Mathematician, Analytical Engine, London",
           "Persona accessible name must retain all supplied text lines");
}

void testPresenceAndInteractivePolicy()
{
    wui::Persona persona("Grace Hopper");
    expect(!persona.isInteractive(), "Persona must remain passive without an explicit activation handler");
    expect(!persona.onPointerEvent(primaryPointer(wui::PointerAction::Down, {2, 2})),
           "Passive Persona must not consume pointer input");
    persona.setPresence(wui::PresenceStatus::Available);
    expect(persona.generatedAccessibleLabel().find("Available") != std::string::npos,
           "Persona accessible label must include its presence state");
    persona.setPresenceOnly(true);
    expect(persona.measure({}).width > 0.0f,
           "Presence-only Persona must retain a visible, measurable status indicator");

    int invoked = 0;
    persona.setPresenceOnly(false);
    persona.onClick([&] { ++invoked; });
    persona.layout({0, 0, 180, 40});
    expect(persona.onPointerEvent(primaryPointer(wui::PointerAction::Down, {8, 8})),
           "Interactive Persona must accept primary press");
    expect(persona.onPointerEvent(primaryPointer(wui::PointerAction::Up, {8, 8})) && invoked == 1,
           "Interactive Persona must invoke exactly once on a completed primary click");
    expect(persona.accessibilityActions().invoke,
           "Interactive Persona must advertise Invoke for assistive technology");
}

void testAccessibilitySnapshot()
{
    wui::Persona persona("Katherine Johnson");
    persona.setSecondaryText("Flight dynamics");
    const auto passive = wui::snapshotAccessibilityTree(persona);
    expect(!passive.empty() && passive.front().properties.role == wui::AccessibilityRole::Group &&
               passive.front().properties.label == "Katherine Johnson, Flight dynamics",
           "Passive Persona must expose one named grouping semantic");
    persona.onClick([] {});
    const auto interactive = wui::snapshotAccessibilityTree(persona);
    expect(!interactive.empty() && interactive.front().properties.role == wui::AccessibilityRole::Button &&
               interactive.front().properties.actions.invoke,
           "Interactive Persona must expose named button semantics");
}

void testDeclarativeBuilderCoverage()
{
    auto built = std::move(wui::ui::Persona("Annie Easley")
                               .avatarColor(wui::AvatarColor::Teal)
                               .avatarShape(wui::AvatarShape::Square)
                               .avatarImage(wui::ImageSource({255, 255, 255, 255}, 1, 1))
                               .presence(wui::PresenceStatus::Away)
                               .presenceOnly()
                               .textPosition(wui::PersonaTextPosition::Below)
                               .textAlignment(wui::PersonaTextAlignment::Center)
                               .secondaryText("Engineer"))
                     .intoNode();
    const auto* persona = dynamic_cast<const wui::Persona*>(built.get());
    expect(persona != nullptr && persona->isPresenceOnly() && persona->avatarColor() == wui::AvatarColor::Teal,
           "Persona declarative builder must expose identity, presence and layout configuration");
}
} // namespace

int main()
{
    try {
        testFluentSizingAndTextContract();
        testPresenceAndInteractivePolicy();
        testAccessibilitySnapshot();
        testDeclarativeBuilderCoverage();
        std::cout << "WhatsUI Fluent Persona tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WhatsUI Fluent Persona tests failed: " << error.what() << '\n';
        return 1;
    }
}
