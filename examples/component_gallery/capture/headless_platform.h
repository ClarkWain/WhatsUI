#pragma once

#include <memory>

#include "wui/platform.h"

namespace whatsui::gallery::capture {

[[nodiscard]] std::unique_ptr<wui::PlatformHost> createHeadlessPlatformHost(
    float scaleFactor);

} // namespace whatsui::gallery::capture
