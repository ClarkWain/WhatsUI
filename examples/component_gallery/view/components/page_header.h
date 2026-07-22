#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "wui/icons.h"
#include "wui/node.h"
#include "wui/widgets.h"

namespace whatsui::gallery::view::components {

struct PageHeaderAction {
    std::string label;
    std::optional<wui::IconName> icon;
    wui::ButtonAppearance appearance{wui::ButtonAppearance::Secondary};
    std::function<void()> onInvoke;
};

struct PageHeaderConfig {
    std::string eyebrow;
    std::string title;
    std::string description;
    std::vector<PageHeaderAction> actions;
};

[[nodiscard]] std::unique_ptr<wui::Node> buildPageHeader(PageHeaderConfig config);

} // namespace whatsui::gallery::view::components
