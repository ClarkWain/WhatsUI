#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "wui/icons.h"
#include "wui/node.h"

namespace whatsui::gallery::view::components {

struct ComponentCardConfig {
    std::string title;
    std::string description;
    std::string category;
    wui::IconName icon{wui::IconName::Square};
    std::string actionLabel{"View details"};
    float previewHeight{112.0f};
    std::function<void()> onOpen;
};

// `preview` remains a real WhatsUI subtree owned by the returned Card.
[[nodiscard]] std::unique_ptr<wui::Node> buildComponentCard(
    ComponentCardConfig config,
    std::unique_ptr<wui::Node> preview);

} // namespace whatsui::gallery::view::components
