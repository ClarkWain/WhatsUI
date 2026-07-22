#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "domain/component_catalog.h"
#include "wui/state.h"

namespace whatsui::gallery {

class GalleryViewModel {
public:
    explicit GalleryViewModel(const ComponentCatalog& catalog);

    [[nodiscard]] wui::State<std::string>& searchQuery() noexcept;
    [[nodiscard]] const wui::State<std::string>& searchQuery() const noexcept;
    [[nodiscard]] wui::State<ComponentCategory>& selectedCategory() noexcept;
    [[nodiscard]] const wui::State<ComponentCategory>& selectedCategory() const noexcept;
    [[nodiscard]] wui::State<std::vector<ComponentDescriptor>>& visibleComponents() noexcept;
    [[nodiscard]] const wui::State<std::vector<ComponentDescriptor>>& visibleComponents() const noexcept;
    [[nodiscard]] wui::Computed<std::size_t>& resultCount() noexcept;
    [[nodiscard]] const wui::Computed<std::size_t>& resultCount() const noexcept;

    void setSearchQuery(std::string query);
    void selectCategory(ComponentCategory category);
    void clearFilters();

private:
    void refresh();

    const ComponentCatalog& catalog_;
    wui::State<std::string> searchQuery_;
    wui::State<ComponentCategory> selectedCategory_{ComponentCategory::All};
    wui::State<std::vector<ComponentDescriptor>> visibleComponents_;
    wui::Computed<std::size_t> resultCount_;
};

} // namespace whatsui::gallery
