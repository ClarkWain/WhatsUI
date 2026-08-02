#pragma once

#include "wui/ui_inspector.h"

#include <optional>

namespace wui::detail {

[[nodiscard]] std::optional<UiInspectorEntry::ResolvedStyle>
resolveInspectorStyle(const Node& node);

} // namespace wui::detail
