// Hello Window - minimal WhatsUI interactive window using GLFW backend.

#include "wui/wui.h"
#include "wui/glfw_platform.h"

int main()
{
    using namespace wui::ui;

    // State
    wui::State<int> clickCount{0};

    // Build UI tree
    auto root = Column()
        .padding(24)
        .gap(16)
        .children(
            Text("Welcome to WhatsUI!"),
            Text().bind(clickCount, [](const int& count) {
                return "Clicked: " + std::to_string(count) + " times";
            }),
            Row().gap(12).children(
                Button("Click me!").onClick([&clickCount] {
                    clickCount.set(clickCount.get() + 1);
                }),
                Button("Reset").onClick([&clickCount] {
                    clickCount.set(0);
                })
            )
        );

    // Run the app
    return wui::runGlfwApp("WhatsUI - Hello Window", {800.0f, 600.0f}, std::move(root));
}
