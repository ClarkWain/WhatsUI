#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "wui/icons.h"
#include "wui/node.h"

namespace whatsui::gallery::view::components {

struct NavigationRailItem {
    std::string id;
    std::string label;
    wui::IconName icon{wui::IconName::Circle};
};

struct NavigationRailConfig {
    std::string productName{"WhatsUI"};
    std::string productCaption{"Component Gallery"};
    std::vector<NavigationRailItem> items;
    std::string selectedId;
    float width{232.0f};
};

using NavigationRailSelectHandler = std::function<void(const std::string& id)>;

[[nodiscard]] std::unique_ptr<wui::Node> buildNavigationRail(
    NavigationRailConfig config,
    NavigationRailSelectHandler onSelect = {});

} // namespace whatsui::gallery::view::components
