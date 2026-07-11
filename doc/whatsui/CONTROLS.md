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
