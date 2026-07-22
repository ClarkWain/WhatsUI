#include "view_model/gallery_view_model.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>

namespace whatsui::gallery {
namespace {

std::string normalized(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        if (!std::isspace(character)) {
            result.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    return result;
}

bool contains(const ComponentDescriptor& item, const std::string& query)
{
    if (query.empty()) return true;
    if (normalized(item.name).find(query) != std::string::npos ||
        normalized(item.id).find(query) != std::string::npos ||
        normalized(item.summary).find(query) != std::string::npos) {
        return true;
    }
    return std::any_of(item.keywords.begin(), item.keywords.end(),
                       [&query](const std::string& keyword) {
                           return normalized(keyword).find(query) != std::string::npos;
                       });
}

} // namespace

GalleryViewModel::GalleryViewModel(const ComponentCatalog& catalog)
    : catalog_(catalog)
    , visibleComponents_(catalog.components())
    , resultCount_([this] { return visibleComponents_.get().size(); }, visibleComponents_)
{
}

wui::State<std::string>& GalleryViewModel::searchQuery() noexcept { return searchQuery_; }
const wui::State<std::string>& GalleryViewModel::searchQuery() const noexcept { return searchQuery_; }
wui::State<ComponentCategory>& GalleryViewModel::selectedCategory() noexcept { return selectedCategory_; }
const wui::State<ComponentCategory>& GalleryViewModel::selectedCategory() const noexcept { return selectedCategory_; }
wui::State<std::vector<ComponentDescriptor>>& GalleryViewModel::visibleComponents() noexcept { return visibleComponents_; }
const wui::State<std::vector<ComponentDescriptor>>& GalleryViewModel::visibleComponents() const noexcept { return visibleComponents_; }
wui::Computed<std::size_t>& GalleryViewModel::resultCount() noexcept { return resultCount_; }
const wui::Computed<std::size_t>& GalleryViewModel::resultCount() const noexcept { return resultCount_; }

void GalleryViewModel::setSearchQuery(std::string query)
{
    if (searchQuery_.set(std::move(query))) refresh();
}

void GalleryViewModel::selectCategory(ComponentCategory category)
{
    if (selectedCategory_.set(category)) refresh();
}

void GalleryViewModel::clearFilters()
{
    const bool queryChanged = searchQuery_.set({});
    const bool categoryChanged = selectedCategory_.set(ComponentCategory::All);
    if (queryChanged || categoryChanged) refresh();
}

void GalleryViewModel::refresh()
{
    const std::string query = normalized(searchQuery_.get());
    const ComponentCategory category = selectedCategory_.get();
    std::vector<ComponentDescriptor> result;
    result.reserve(catalog_.components().size());
    for (const auto& item : catalog_.components()) {
        if (category != ComponentCategory::All && item.category != category) continue;
        if (contains(item, query)) result.push_back(item);
    }
    visibleComponents_.set(std::move(result));
}

} // namespace whatsui::gallery
