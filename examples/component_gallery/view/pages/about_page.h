#pragma once

#include <functional>
#include <memory>
#include <string>

#include "wui/node.h"

namespace whatsui::gallery::view::pages {

using OpenLinkHandler = std::function<void(const std::string& href)>;

[[nodiscard]] std::unique_ptr<wui::Node> buildAboutPage(
    OpenLinkHandler openLink = {});

} // namespace whatsui::gallery::view::pages
