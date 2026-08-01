#include "page_header.h"

#include <utility>

#include "wui/theme.h"
#include "wui/declarative.h"

namespace whatsui::gallery::view::components {
namespace {

std::unique_ptr<wui::Node> buildTextBlock(PageHeaderConfig config)
{
    using namespace wui;
    const auto& colors = wui::theme().colors;
    auto text = std::make_unique<wui::ColumnNode>();
    text->setGap(6.0f);
    text->setAlign(wui::Alignment::Stretch);

    if (!config.eyebrow.empty()) {
        text->appendChild(Text(std::move(config.eyebrow))
            .size(11.0f)
            .weight(600)
            .color(colors.accent)
            .build());
    }
    text->appendChild(Text(std::move(config.title))
        .role(wui::TextRole::Heading)
        .size(28.0f)
        .lineHeight(36.0f)
        .weight(600)
        .color(colors.text)
        .build());
    if (!config.description.empty()) {
        text->appendChild(Text(std::move(config.description))
            .size(13.0f)
            .lineHeight(20.0f)
            .wrap()
            .color(colors.textMuted)
            .build());
    }
    return text;
}

std::unique_ptr<wui::Node> buildActions(std::vector<PageHeaderAction> actions)
{
    using namespace wui;
    auto row = std::make_unique<wui::RowNode>();
    row->setGap(8.0f);
    row->setAlign(wui::Alignment::Center);

    for (auto& action : actions) {
        auto button = Button(std::move(action.label)).appearance(action.appearance);
        if (action.icon) {
            button = std::move(button).icon(*action.icon);
        }
        if (action.onInvoke) {
            button = std::move(button).onClick(std::move(action.onInvoke));
        }
        row->appendChild(std::move(button).build());
    }
    return row;
}

} // namespace

std::unique_ptr<wui::Node> buildPageHeader(PageHeaderConfig config)
{
    using namespace wui;
    auto actions = std::move(config.actions);

    return Row()
        .gap(24.0f)
        .align(wui::Alignment::Center)
        .children(
            buildTextBlock(std::move(config)),
            Spacer().flex(1.0f),
            buildActions(std::move(actions)))
        .build();
}

} // namespace whatsui::gallery::view::components
