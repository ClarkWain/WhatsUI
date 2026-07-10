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
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/ui.h"
#include "wui/whatscanvas_text.h"

namespace {

std::vector<unsigned char> makeStatusIcon(bool done)
{
    constexpr int size = 28;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(size * size * 4), 0);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = static_cast<float>(x) - 13.5f;
            const float dy = static_cast<float>(y) - 13.5f;
            const bool circle = dx * dx + dy * dy <= 12.0f * 12.0f;
            const bool check = done && ((x >= 7 && x <= 12 && y - x >= 4 && y - x <= 7)
                                      || (x >= 11 && x <= 21 && x + y >= 27 && x + y <= 31));
            const std::size_t offset = static_cast<std::size_t>((y * size + x) * 4);
            const bool border = dx * dx + dy * dy >= 9.5f * 9.5f;
            const wui::Color color = !circle ? wui::Color{255, 255, 255, 255}
                : check ? wui::Color{255, 255, 255, 255}
                : done ? wui::Color{99, 102, 241, 255}
                : border ? wui::Color{148, 163, 184, 255}
                : wui::Color{248, 250, 252, 255};
            pixels[offset] = color.r;
            pixels[offset + 1] = color.g;
            pixels[offset + 2] = color.b;
            pixels[offset + 3] = color.a;
        }
    }
    return pixels;
}

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
                                       wui::State<bool>& isEmpty,
                                       std::function<void(int)>,
                                       std::function<void(int)>)
{
    using namespace wui::ui;
    auto summary = [](const std::vector<Todo>& items) {
        int done = 0;
        for (const auto& item : items) {
            if (item.done) {
                ++done;
            }
        }
        return std::to_string(done) + " of " + std::to_string(items.size()) + " done";
    };

    const wui::Color canvas{241, 245, 249, 255};
    const wui::Color ink{15, 23, 42, 255};
    const wui::Color muted{100, 116, 139, 255};
    const wui::Color violet{79, 70, 229, 255};
    const wui::Color violetSoft{238, 242, 255, 255};
    const wui::Color white{255, 255, 255, 255};

    return Box()
        .background(canvas)
        .children(Column()
        .padding(wui::InsetsF{36.0f, 30.0f, 36.0f, 30.0f})
        .gap(18.0f)
        .align(wui::Alignment::Stretch)
        .children(
            Row().align(wui::Alignment::Center).children(
                Column().gap(5.0f).children(
                    Text("Today").size(30.0f).lineHeight(36.0f).color(ink),
                    Text("A calm place to finish what matters.").size(14.0f).lineHeight(20.0f).color(muted)),
                Spacer().flex(1.0f),
                Box().background(violetSoft).radius(18.0f).padding({14.0f, 8.0f, 14.0f, 8.0f})
                    .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                    .children(Text().bind(todos, summary).size(13.0f).lineHeight(18.0f).color(violet))),
            Box().background(violet).radius(16.0f).padding({20.0f, 16.0f, 20.0f, 16.0f})
                .children(Row().align(wui::Alignment::Center).children(
                    Column().gap(4.0f).children(
                        Text("FOCUS FOR TODAY").size(11.0f).lineHeight(16.0f).color({199, 210, 254, 255}),
                        Text("Small steps, visible progress").size(18.0f).lineHeight(24.0f).color(white)),
                    Spacer().flex(1.0f),
                    Text("\xE2\x9C\xA6").size(28.0f).lineHeight(34.0f).color(white))),
            Text("TASKS").size(11.0f).lineHeight(16.0f).color(muted),
            If(isEmpty).then([muted] {
                return Box().background({255, 255, 255, 255}).radius(14.0f)
                    .height(94.0f)
                    .padding({20.0f, 24.0f, 20.0f, 24.0f})
                    .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                    .children(Column().gap(6.0f).align(wui::Alignment::Center).children(
                        Text("Your day is clear").size(16.0f).lineHeight(22.0f).color({51, 65, 85, 255}),
                        Text("Add a task when you are ready to focus.").size(12.0f).lineHeight(18.0f).color(muted)));
            }),
            ForEach<Todo>(todos, [](const Todo& item) {
                return Box()
                    .background({255, 255, 255, 255})
                    .radius(14.0f)
                    .height(64.0f)
                    .padding(wui::InsetsF{16.0f, 13.0f, 16.0f, 13.0f})
                    .children(
                        Row()
                            .align(wui::Alignment::Center)
                            .gap(13.0f)
                            .children(
                                Image(makeStatusIcon(item.done), 28, 28),
                                Column().gap(3.0f).children(
                                    Text(item.text).size(15.0f).lineHeight(20.0f).color(item.done
                                        ? wui::Color{148, 163, 184, 255}
                                        : wui::Color{30, 41, 59, 255}),
                                    Text(item.done ? "Completed" : "In progress").size(11.0f).lineHeight(16.0f)
                                        .color(item.done ? wui::Color{99, 102, 241, 255}
                                                         : wui::Color{148, 163, 184, 255})),
                                Spacer().flex(1.0f),
                                Box().background(item.done ? wui::Color{238, 242, 255, 255}
                                                           : wui::Color{248, 250, 252, 255})
                                    .radius(12.0f).padding({10.0f, 6.0f, 10.0f, 6.0f})
                                    .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                                    .children(Text(item.done ? "DONE" : "OPEN").size(10.0f).lineHeight(14.0f)
                                        .color(item.done ? wui::Color{79, 70, 229, 255}
                                                         : wui::Color{100, 116, 139, 255}))));
            }).gap(10.0f).align(wui::Alignment::Stretch),
            Spacer().flex(1.0f),
            Row().align(wui::Alignment::Center).children(
                Text("WHATSUI / WHATSCANVAS").size(10.0f).lineHeight(14.0f).color({148, 163, 184, 255}),
                Spacer().flex(1.0f),
                Text("DETERMINISTIC SOFTWARE RENDER").size(10.0f).lineHeight(14.0f).color({148, 163, 184, 255}))));
}

} // namespace

int main()
{
    constexpr int width = 640;
    constexpr int height = 560;
    constexpr float scaleFactor = 2.0f;

    wui::State<std::vector<Todo>> todos;
    wui::State<bool> isEmpty{true};
    int nextId = 1;

    auto addTodo = [&todos, &isEmpty, &nextId](std::string text) {
        auto items = todos.get();
        items.push_back({nextId++, std::move(text), false});
        todos.set(items);
        isEmpty.set(items.empty());
    };
    std::function<void(int)> toggle = [&todos, &isEmpty](int id) {
        auto items = todos.get();
        for (auto& item : items) {
            if (item.id == id) {
                item.done = !item.done;
            }
        }
        todos.set(items);
        isEmpty.set(items.empty());
    };
    std::function<void(int)> remove = [&todos, &isEmpty](int id) {
        auto items = todos.get();
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [id](const Todo& item) { return item.id == id; }),
                    items.end());
        todos.set(items);
        isEmpty.set(items.empty());
    };

    auto root = buildTodoUi(todos, isEmpty, toggle, remove);

    int frame = 0;
    auto renderFrame = [&]() {
        // Isolate every visual-regression scene. Reusing one Software canvas
        // across pixel readbacks can retain backend target state between scenes.
        auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software,
                                          static_cast<int>(width * scaleFactor),
                                          static_cast<int>(height * scaleFactor));
        if (!canvas || !canvas->initializeContext()) {
            throw std::runtime_error("failed to create software canvas");
        }
        wui::WhatsCanvasTextMeasurer measurer(*canvas, scaleFactor);
        wui::setTextMeasurer(&measurer);

        wui::flushStructuralUpdates();
        root->layout({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});
        wui::PaintContext paint(*canvas, scaleFactor);
        root->prepare(paint);
        for (int pass = 0; pass < 2; ++pass) {
            canvas->beginFrame();
            paint.fillRect({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, wui::theme().colors.surface);
            root->paint(paint);
            canvas->endFrame();
        }
        const std::string path = "todo_frame_" + std::to_string(frame++) + ".ppm";
        canvas->savePixelsPPM(path);
        wui::setTextMeasurer(nullptr);
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
