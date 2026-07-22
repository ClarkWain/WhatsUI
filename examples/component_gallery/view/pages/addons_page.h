#pragma once

#include <memory>

#include "wui/app.h"
#include "wui/node.h"

namespace whatsui::gallery::view::pages {

// Lifetime contract: window must outlive the returned Node tree and dialogs.

[[nodiscard]] std::unique_ptr<wui::Node> buildAddonsPage(wui::UiWindow& window);

} // namespace whatsui::gallery::view::pages
