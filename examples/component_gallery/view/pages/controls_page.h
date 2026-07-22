#pragma once

#include <functional>
#include <memory>

#include "wui/node.h"

namespace whatsui::gallery::view::pages {

[[nodiscard]] std::unique_ptr<wui::Node> buildControlsPage(
    std::function<void()> onOpenButtonDetail = {});

} // namespace whatsui::gallery::view::pages
