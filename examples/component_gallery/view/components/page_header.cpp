#include "page_header.h"

#include <utility>

#include "wui/theme.h"
#include "wui/ui.h"

namespace whatsui::gallery::view::components {
namespace {

std::unique_ptr<wui::Node> buildTextBlock(PageHeaderConfig config)
{
    using namespace wui::ui;
    const auto& colors = wui::theme().colors;
    auto text = std::make_unique<wui::Column>();
    text->setGap(6.0f);
    text->setAlign(wui::Alignment::Stretch);

    if (!config.eyebrow.empty()) {
        text->appendChild(Text(std::move(config.eyebrow))
            .size(11.0f)
            .weight(600)
            .color(colors.accent)
            .intoNode());
    }
    text->appendChild(Text(std::move(config.title))
        .role(wui::TextRole::Heading)
        .size(28.0f)
        .lineHeight(36.0f)
        .weight(600)
        .color(colors.text)
        .intoNode());
    if (!config.description.empty()) {
        text->appendChild(Text(std::move(config.description))
            .size(13.0f)
            .lineHeight(20.0f)
            .wrap()
            .color(colors.textMuted)
            .intoNode());
    }
    return text;
}

std::unique_ptr<wui::Node> buildActions(std::vector<PageHeaderAction> actions)
{
    using namespace wui::ui;
    auto row = std::make_unique<wui::Row>();
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
        row->appendChild(std::move(button).intoNode());
    }
    return row;
}

} // namespace

std::unique_ptr<wui::Node> buildPageHeader(PageHeaderConfig config)
{
    using namespace wui::ui;
    auto actions = std::move(config.actions);

    return Row()
        .gap(24.0f)
        .align(wui::Alignment::Center)
        .children(
            buildTextBlock(std::move(config)),
            Spacer().flex(1.0f),
            buildActions(std::move(actions)))
        .intoNode();
}

} // namespace whatsui::gallery::view::components
