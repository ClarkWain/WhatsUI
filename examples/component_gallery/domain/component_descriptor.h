#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace whatsui::gallery {

enum class ComponentCategory {
    All,
    Controls,
    Inputs,
    Feedback,
    Navigation,
    DataDisplay,
    Layout,
    Identity,
    DateTime,
    Overlays,
};

enum class ComponentMaturity { Stable, Preview };

enum class ComponentIcon {
    Component,
    Input,
    Feedback,
    Navigation,
    Data,
    Layout,
    Person,
    Calendar,
    Overlay,
};

struct ComponentDescriptor {
    std::string id;
    std::string name;
    std::string summary;
    ComponentCategory category{ComponentCategory::Controls};
    ComponentIcon icon{ComponentIcon::Component};
    ComponentMaturity maturity{ComponentMaturity::Stable};
    bool featured{false};
    std::vector<std::string> keywords;
};

[[nodiscard]] bool operator==(const ComponentDescriptor& left,
                              const ComponentDescriptor& right) noexcept;
[[nodiscard]] bool operator!=(const ComponentDescriptor& left,
                              const ComponentDescriptor& right) noexcept;
[[nodiscard]] std::string_view componentCategoryName(ComponentCategory category) noexcept;

} // namespace whatsui::gallery
