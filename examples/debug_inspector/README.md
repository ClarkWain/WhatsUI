# Debug Inspector structure

The example keeps read-only inspection separate from presentation and hosts:

- `inspector_diagnostics.*` owns the deterministic sample inspection model.
- `sample_tree.*` owns the deterministic preview component.
- `inspector_components.*` owns the page's private presentation components.
- `inspector_page.*` is the small top-level `InspectorPage` composition.
- `software_capture.*` owns the headless WhatsCanvas rendering boundary.
- `main.cpp` and `interactive_main.cpp` are minimal host-specific entry points.

The example UI uses declarative builders and `body()` components. Concrete
`Node` access is limited to `inspector_diagnostics.cpp`, where the public
Inspector API intentionally consumes a materialized retained tree.

Core control-style projection is isolated in
`src/whatsui/core/ui_inspector_style.cpp`; tree traversal and summary logic
remain in `src/whatsui/core/ui_inspector.cpp`.
