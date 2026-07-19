#include <stdexcept>
#include <string>

#include "wui/accessibility.h"
#include "wui/form_feedback.h"
#include "wui/text_input.h"

namespace {
void expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
wui::PointerEvent pointer(wui::PointerAction action, float x, float y, wui::MouseButton button = wui::MouseButton::None)
{ wui::PointerEvent e; e.action = action; e.button = button; e.position = {x, y}; return e; }
}

int main()
{
    try {
        wui::Field field("Project name");
        auto input = std::make_unique<wui::TextInput>("Required");
        auto* rawInput = input.get();
        field.setControl(std::move(input));
        field.setHint("Use a name your team will recognize");
        field.setValidationState(wui::FieldValidationState::Error);
        field.setValidationMessage("A project name is required");
        field.setRequired(true);
        field.layout({0, 0, 320, 120});
        expect(rawInput->accessibleLabel() == "Project name" && rawInput->isInvalid(),
               "Field must associate its label and error state with TextInput");
        const auto fieldTree = wui::snapshotAccessibilityTree(field);
        expect(fieldTree.size() == 2 && fieldTree[0].properties.role == wui::AccessibilityRole::Group &&
                   fieldTree[0].properties.label == "Project name" && fieldTree[0].properties.required &&
                   fieldTree[0].properties.description == "A project name is required" &&
                   fieldTree[1].properties.role == wui::AccessibilityRole::TextField &&
                   fieldTree[1].properties.label == "Project name",
               "Field must expose group and labelled control semantic relationships");
        field.setOrientation(wui::FieldOrientation::Horizontal);
        field.layout({0, 0, 320, 80});
        expect(rawInput->bounds().x > field.bounds().x,
               "Horizontal Field must reserve an inline label column");
        field.setEnabled(false);
        expect(!rawInput->isEnabled(), "Disabled Field must disable its supplied control");

        int invoked = 0, dismissed = 0;
        wui::MessageBar message("The task owner was updated.");
        message.setTitle("Saved");
        message.setIntent(wui::MessageBarIntent::Success);
        message.setMultiline(true);
        message.addAction({"Undo", [&] { ++invoked; }});
        message.setDismissible(true);
        message.onDismiss([&] { ++dismissed; });
        message.layout({0, 0, 460, 92});
        const auto barTree = wui::snapshotAccessibilityTree(message, &message);
        expect(barTree.size() == 1 && barTree[0].properties.role == wui::AccessibilityRole::Alert &&
                   barTree[0].properties.label == "Saved" &&
                   barTree[0].properties.description == "The task owner was updated." &&
                   barTree[0].properties.live && barTree[0].properties.actions.invoke && barTree[0].properties.focused,
               "MessageBar must expose named alert semantics and a dismiss action");
        expect(message.onPointerEvent(pointer(wui::PointerAction::Down, 400, 70, wui::MouseButton::Left)) &&
                   message.onPointerEvent(pointer(wui::PointerAction::Up, 400, 70, wui::MouseButton::Left)) && invoked == 1,
               "MessageBar action must invoke from its action hit target");
        expect(message.performAccessibilityAction(wui::AccessibilityActionKind::Invoke, {}) ==
                   wui::AccessibilityActionStatus::Succeeded && dismissed == 1,
               "MessageBar accessibility Invoke must dismiss a dismissible message");
        wui::KeyEvent escape; escape.action = wui::KeyAction::Down; escape.keyCode = 27;
        expect(message.onKeyEvent(escape) && dismissed == 2,
               "Dismissible MessageBar must close with Escape");
        return 0;
    } catch (...) { return 1; }
}
