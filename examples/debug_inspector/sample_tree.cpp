#include "sample_tree.h"

#include "wui/theme.h"

namespace whatsui::debug_inspector {

wui::Box SampleTree::body()
{
    using namespace wui;
    const auto& current = theme();

    return Box()
        .background(current.colors.surface)
        .radius(current.radius.lg)
        .padding(16.0f)
        .children(
            Column()
                .gap(12.0f)
                .align(Alignment::Stretch)
                .children(
                    Row()
                        .align(Alignment::Center)
                        .children(
                            Column()
                                .gap(2.0f)
                                .flex(1.0f)
                                .children(
                                    Text("Sample work item")
                                        .size(15.0f)
                                        .lineHeight(21.0f)
                                        .color(current.colors.text),
                                    Text("A retained tree rendered beside its snapshot")
                                        .size(11.0f)
                                        .lineHeight(16.0f)
                                        .color(current.colors.textMuted)
                                ),
                            Text("LIVE")
                                .size(10.0f)
                                .lineHeight(14.0f)
                                .color(current.colors.accent)
                        ),
                    Checkbox("Mark the layout pass complete", false),
                    Row()
                        .align(Alignment::Center)
                        .gap(8.0f)
                        .children(
                            Button("Inspect node")
                                .appearance(ButtonAppearance::Primary),
                            Button("Reset")
                                .appearance(ButtonAppearance::Outline)
                        )
                )
        );
}

} // namespace whatsui::debug_inspector
