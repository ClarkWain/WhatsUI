# Fluent Field and MessageBar

## Field

`Field` composes one retained input/control with a Fluent label, optional hint
and validation message. Use `required(true)` only where an input is actually
required. For `TextInput`/`TextArea`, Field assigns the visible label as the
accessible name and maps `Error` validation to the input's invalid visual
state. The Field group exposes its label, required state and current hint or
validation message to accessibility clients.

The default orientation is vertical. `Horizontal` reserves a label column and
places hint/validation text below the control column. `enabled(false)` also
disables the supplied `ControlNode`.

```cpp
using namespace wui::ui;
auto name = Field("Project name")
    .required()
    .hint("Visible to your team")
    .validationState(wui::FieldValidationState::Error)
    .validationMessage("A project name is required")
    .control(TextField("Required"));
```

## MessageBar

`MessageBar` is persistent inline feedback, not a transient notification.
It supports `Info`, `Success`, `Warning`, and `Error` intent, a title/body,
optional multiline height, named inline actions, and an optional dismiss
affordance. A dismissible MessageBar responds to Escape and accessibility
Invoke; it is exposed as an alert with title/body semantics.

```cpp
auto feedback = MessageBar("The owner was updated.")
    .title("Saved")
    .intent(wui::MessageBarIntent::Success)
    .action({"Undo", undo})
    .dismissible()
    .onDismiss(removeMessage);
```

## Verification

`whatsui_fluent_field_messagebar` covers relationships, validation,
orientation, disabled propagation, actions, Escape and accessibility invoke.
`whatsui_fluent_field_messagebar_visual` and its 150% variant produce the
software review images for the normal, error/horizontal and multiline states.
