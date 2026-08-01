// Hello Window - minimal WhatsUI interactive window using GLFW backend.

#include <exception>
#include <iostream>

#include "wui/wui.h"
#include "wui/glfw_platform.h"

using namespace wui::ui;

wui::State<int> clickCount{0};

auto buildView() {
    return Column()
        .padding(24)
        .gap(16)
        .children(
            Text()
                .size(24)
                .weight(600)
                .color({255, 255, 255, 255})
                .bind(clickCount, [](const int& count) {
                    return "Clicked: " + std::to_string(count) + " times";
                }),

            Row().gap(12).children(
                Button("Click me!")
                .appearance(wui::ButtonAppearance::Primary).onClick([] {
                    clickCount.set(clickCount.get() + 1);
                }),
                Button("Reset").onClick([] {
                    clickCount.set(0);
                })
            )
        );
}

int main()
{
    auto root = buildView();
    return wui::runGlfwApp("WhatsUI - Hello Window", {400.0f, 300.0f}, std::move(root));
}
