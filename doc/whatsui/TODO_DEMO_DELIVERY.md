# Todo demo delivery

The Todo demo has two delivery targets built from the same `buildTodoUi()`
declarative tree:

- `WhatsUITodoApp` is a deterministic, headless Software-backend capture.
- `WhatsUITodoGlfw` is the interactive GLFW/OpenGL presentation of that tree.

## Capture scenes

Run the capture executable with an empty output directory:

```powershell
New-Item -ItemType Directory -Force build-wsc\todo-visual | Out-Null
.\build-wsc\examples\Debug\WhatsUITodoApp.exe build-wsc\todo-visual
```

It writes one isolated 1280×1120 PPM canvas per product state:

| File | Scene |
| --- | --- |
| `todo_0.ppm` | Empty list |
| `todo_1.ppm` | Active list |
| `todo_2.ppm` | Mixed completed/active list |
| `todo_3.ppm` | List after delete and clear-completed |

The scene script, logical size (640×560), DPR (2), data, and filenames are
fixed. Each capture creates a new Software canvas, which prevents backend
render-target state from leaking between readbacks. Baselines are intentionally
generated in CI/build output rather than committed as binary source artifacts.

## Responsive visual review

Pixel hashes do not establish that a responsive page is well composed. Generate
the review set before approving a Todo UI change:

```powershell
ctest --test-dir build-wsc -C Debug -R ^whatsui_todo_visual_review$ --output-on-failure
```

It writes 2× DPR PPM screenshots to `build-wsc\tests\todo_visual_review`:

| Breakpoint | Logical size | Physical screenshot | Intent |
| --- | ---: | ---: | --- |
| `narrow` | 360×720 | 720×1440 | Compact portrait / snapped Windows window |
| `regular` | 640×560 | 1280×1120 | Common desktop app window |
| `wide` | 1180×760 | 2360×1520 | Wide desktop window |

Each directory contains the same four product states (`todo_0.ppm` through
`todo_3.ppm`). Review the images, rather than relying only on the dimensions
validated by the CTest harness. Approve only when all of the following hold:

- No text, check control, or action is clipped at any breakpoint.
- The content column has intentional gutters and a stable visual centre; wide
  windows must not turn the task list into an unreadably long line.
- Repeated task rows share aligned leading controls, text baselines, metadata,
  and trailing actions.
- Primary action remains discoverable in compact layout; secondary actions do
  not compete with the task title.
- Fluent surfaces, typography, and accent treatment preserve contrast and a
  clear hierarchy in empty, active, completed, and cleared states.

## Interactive demo

```powershell
.\build-wsc\examples\Debug\WhatsUITodoGlfw.exe
```

The window starts in the mixed state and supports Add task, Done / Reopen,
Important, optional `YYYY-MM-DD` due dates, search/filter, atomic task editing,
Undo, Delete, and Clear completed. Editing title, importance, and due date is a
single validated operation: an invalid date or title leaves the entire task
unchanged, and one Undo restores all edited fields.

Task cards intentionally use two levels. The checkbox and title begin on the
first level with the named Important action at the trailing edge; due-date
metadata and secondary Edit/Delete actions occupy the second level. This keeps
the 360 DIP layout readable without changing the information hierarchy at
regular and wide breakpoints.

The interactive target requires a desktop OpenGL/GLFW environment; the
headless capture remains the automation-friendly validation entry point.
