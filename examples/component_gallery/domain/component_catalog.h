#pragma once

#include <string_view>
#include <vector>

#include "domain/component_descriptor.h"

namespace whatsui::gallery {

class ComponentCatalog {
public:
    ComponentCatalog();
    explicit ComponentCatalog(std::vector<ComponentDescriptor> components);

    [[nodiscard]] const std::vector<ComponentDescriptor>& components() const noexcept;
    [[nodiscard]] const ComponentDescriptor* find(std::string_view id) const noexcept;

private:
    std::vector<ComponentDescriptor> components_;
};

} // namespace whatsui::gallery
