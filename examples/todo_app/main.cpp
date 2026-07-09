// A small but complete TODO-list application built with WhatsUI and rendered
// with the real WhatsCanvas software backend. It exercises the full product
// path: a reactive data model, a declarative reactive UI (ForEach list with
// per-item toggle/delete), and real text-measured layout.
//
// Interactions are driven from main() (add / toggle / delete) and each state is
// rendered to a PPM frame. Driving actions from the app loop (rather than from
// inside a button's own onClick) is intentional: State::set currently notifies
// synchronously, so a self-mutating handler would rebuild and destroy the very
// button running it. Batched/deferred invalidation is the next step for live,
// self-mutating interaction.
//
// Only built when WHATSUI_WITH_WHATSCANVAS=ON.

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/ui.h"
#include "wui/whatscanvas_text.h"

namespace {

struct Todo {
    int id{0};
    std::string text;
    bool done{false};

    bool operator==(const Todo& other) const
    {
        return id == other.id && text == other.text && done == other.done;
    }
};

std::unique_ptr<wui::Node> buildTodoUi(wui::State<std::vector<Todo>>& todos,
                                       std::function<void(int)> toggle,
                                       std::function<void(int)> remove)
{
    using namespace wui::ui;

    auto summary = [](const std::vector<Todo>& items) {
        int done = 0;
        for (const auto& item : items) {
            if (item.done) {
                ++done;
            }
        }
        return std::to_string(done) + " / " + std::to_string(items.size()) + " done";
    };

    return Column()
        .padding(20)
        .gap(12)
        .align(wui::Alignment::Stretch)
        .children(
            Text("Todo List"),
            Text().bind(todos, summary),
            ForEach<Todo>(todos, [toggle, remove](const Todo& item) {
                const int id = item.id;
                return Row()
                    .align(wui::Alignment::Center)
                    .gap(8)
                    .children(
                        Button(item.done ? "[x]" : "[ ]").onClick([toggle, id] { toggle(id); }),
                        Text(item.done ? ("done: " + item.text) : item.text),
                        Spacer().flex(1),
                        Button("del").onClick([remove, id] { remove(id); }));
            }).gap(8).align(wui::Alignment::Stretch));
}

} // namespace

int main()
{
    constexpr int width = 360;
    constexpr int height = 320;

    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    if (!canvas || !canvas->initializeContext()) {
        std::cerr << "failed to create software canvas" << std::endl;
        return 1;
    }

    wui::WhatsCanvasTextMeasurer measurer(*canvas);
    wui::setTextMeasurer(&measurer);

    wui::State<std::vector<Todo>> todos;
    int nextId = 1;

    auto addTodo = [&todos, &nextId](std::string text) {
        auto items = todos.get();
        items.push_back({nextId++, std::move(text), false});
        todos.set(items);
    };
    std::function<void(int)> toggle = [&todos](int id) {
        auto items = todos.get();
        for (auto& item : items) {
            if (item.id == id) {
                item.done = !item.done;
            }
        }
        todos.set(items);
    };
    std::function<void(int)> remove = [&todos](int id) {
        auto items = todos.get();
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [id](const Todo& item) { return item.id == id; }),
                    items.end());
        todos.set(items);
    };

    auto root = buildTodoUi(todos, toggle, remove);

    int frame = 0;
    auto renderFrame = [&]() {
        wui::flushStructuralUpdates();
        root->layout({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});
        canvas->beginFrame();
        wui::PaintContext paint(*canvas);
        paint.fillRect({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, wui::theme().colors.surface);
        root->paint(paint);
        canvas->endFrame();
        const std::string path = "todo_frame_" + std::to_string(frame++) + ".ppm";
        canvas->savePixelsPPM(path);
        std::cout << "wrote " << path << std::endl;
    };

    // Scripted product walkthrough.
    renderFrame();                 // empty list
    addTodo("Buy milk");
    addTodo("Write WhatsUI docs");
    addTodo("Ship the release");
    renderFrame();                 // three items
    toggle(2);                     // mark "Write WhatsUI docs" done
    renderFrame();
    remove(1);                     // delete "Buy milk"
    renderFrame();                 // final state

    return 0;
}
