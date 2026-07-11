# Core controls

`Checkbox` is a focusable two-state control. Construct it with a label and an
optional initial value, bind it to `State<bool>`, or author it declaratively:

```cpp
State<bool> enabled{false};
auto control = ui::Checkbox("Enable notifications").bind(enabled);
```

Pointer activation toggles on a left-button down/up sequence. When focused,
Space (`32`) and Enter (`13`) toggle it. Disabled controls ignore both input
paths and render with muted colors. `onChange` receives the newly requested
value after the state is updated.

## TextInput callbacks

`TextInput` offers `onChange`, `onSubmit`, and `onCancel` for application
editing flows. `onChange` runs after committed text, cut/paste, undo/redo, or
composition updates. `onSubmit` consumes Enter when supplied; `onCancel`
consumes Escape when supplied.

```cpp
auto search = ui::TextField("Search commands")
    .onChange([&](const std::string& query) { filter(query); })
    .onSubmit([&] { executeFirstResult(); })
    .onCancel([&] { closePalette(); });
```

An active `Dialog` owns Escape first so its focus restoration contract remains
authoritative; this is why the command palette reference uses a dialog while
the input callback stays useful for inline command surfaces.
