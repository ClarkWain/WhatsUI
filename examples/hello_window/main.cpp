// Hello Window - minimal WhatsUI interactive window using GLFW backend.

#include <string>

#include "wui/wui.h"
#include "wui/glfw_platform.h"

using namespace wui;

auto helloView()
{
    State<int> count{0};
    return Column()
        .padding(24)
        .gap(16)
        .children(
            Text()
                .size(24)
                .weight(600)
                .color({255, 255, 255, 255})
                .bind(count, [](const int& value) {
                    return "Clicked: " + std::to_string(value) + " times";
                }),
            Row()
                .gap(12)
                .children(
                    Button("Click me!")
                        .appearance(ButtonAppearance::Primary)
                        .onClick([count] {
                            count.set(count.get() + 1);
                        }),
                    Button("Reset")
                        .onClick([count] {
                            count.set(0);
                        })
                )
        );
}

int main()
{
    return runGlfwApp(
        "WhatsUI - Hello Window",
        {400.0f, 300.0f},
        helloView());
}
