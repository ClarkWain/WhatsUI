#pragma once

#include <memory>

#include "wui/node.h"

namespace whatsui::gallery::view::pages {

// A single-Text-node 10,000-line multilingual document stress demo.
[[nodiscard]] std::unique_ptr<wui::Node> buildLongTextPage();

} // namespace whatsui::gallery::view::pages
