#pragma once

#include <memory>
#include <string>

#include "wui/node.h"

namespace whatsui::gallery::view::components {

struct PreviewSurfaceConfig {
    std::string title{"Preview"};
    std::string caption;
    std::string statusLabel{"Interactive"};
    float minHeight{220.0f};
    bool showStatus{true};
};

// Places a real WhatsUI subtree on a reusable Fluent preview canvas.
[[nodiscard]] std::unique_ptr<wui::Node> buildPreviewSurface(
    PreviewSurfaceConfig config,
    std::unique_ptr<wui::Node> content);

} // namespace whatsui::gallery::view::components
