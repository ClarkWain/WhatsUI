// Hello Window - minimal WhatsUI interactive window using GLFW backend.

#include <exception>
#include <iostream>

#include "wui/wui.h"
#include "wui/glfw_platform.h"

int main()
{
    try {
        using namespace wui::ui;

        wui::State<int> clickCount{0};

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

        return wui::runGlfwApp("WhatsUI - Hello Window", {800.0f, 600.0f}, std::move(root));
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "FATAL: unknown exception" << std::endl;
        return 1;
    }
}
