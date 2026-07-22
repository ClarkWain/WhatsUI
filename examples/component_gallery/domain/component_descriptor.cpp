#include "domain/component_descriptor.h"

namespace whatsui::gallery {

bool operator==(const ComponentDescriptor& left, const ComponentDescriptor& right) noexcept
{
    return left.id == right.id && left.name == right.name &&
           left.summary == right.summary && left.category == right.category &&
           left.icon == right.icon && left.maturity == right.maturity &&
           left.featured == right.featured && left.keywords == right.keywords;
}

bool operator!=(const ComponentDescriptor& left, const ComponentDescriptor& right) noexcept
{
    return !(left == right);
}

std::string_view componentCategoryName(ComponentCategory category) noexcept
{
    switch (category) {
    case ComponentCategory::All: return "All";
    case ComponentCategory::Controls: return "Controls";
    case ComponentCategory::Inputs: return "Inputs";
    case ComponentCategory::Feedback: return "Feedback";
    case ComponentCategory::Navigation: return "Navigation";
    case ComponentCategory::DataDisplay: return "Data display";
    case ComponentCategory::Layout: return "Layout";
    case ComponentCategory::Identity: return "Identity";
    case ComponentCategory::DateTime: return "Date and time";
    case ComponentCategory::Overlays: return "Overlays";
    }
    return "All";
}

} // namespace whatsui::gallery
