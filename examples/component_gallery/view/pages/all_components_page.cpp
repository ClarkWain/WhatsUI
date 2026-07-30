#include "all_components_page.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

#include "view/components/component_card.h"
#include "view/components/page_header.h"
#include "view/components/responsive_choice_group.h"
#include "wui/events.h"
#include "wui/theme.h"
#include "wui/ui.h"

using namespace wui::ui;

namespace whatsui::gallery::view::pages {
namespace {

wui::IconName iconFor(ComponentIcon icon)
{
    switch (icon) {
    case ComponentIcon::Input: return wui::IconName::Edit;
    case ComponentIcon::Feedback: return wui::IconName::Info;
    case ComponentIcon::Navigation: return wui::IconName::ChevronRight;
    case ComponentIcon::Data: return wui::IconName::TaskList;
    case ComponentIcon::Layout: return wui::IconName::Square;
    case ComponentIcon::Person: return wui::IconName::Circle;
    case ComponentIcon::Calendar: return wui::IconName::Calendar;
    case ComponentIcon::Overlay: return wui::IconName::MoreHorizontal;
    case ComponentIcon::Component: return wui::IconName::CheckmarkCircle;
    }
    return wui::IconName::Square;
}

std::unique_ptr<wui::Node> buildDescriptorPreview(const ComponentDescriptor& descriptor)
{
    if (descriptor.id == "textarea") {
        return TextArea("Write a short note...").rows(2).intoNode();
    }

    if (descriptor.id == "table") {
        return Table({{"name", "Name", 92.0f}, {"status", "Status", 92.0f}})
            .rows({{"one", {"Button", "Stable"}}, {"two", {"Avatar", "Stable"}}})
            .maxVisibleRows(2)
            .accessibleLabel("Component status preview")
            .intoNode();
    }

    if (descriptor.id == "avatar") {
        return Row().gap(8.0f).align(wui::Alignment::Center).children(
            Avatar("Ada Lovelace", wui::AvatarSize::Size40).initials("AL"),
            Text("Ada Lovelace").size(12.0f)).intoNode();
    }

    if (descriptor.id == "progress-bar") {
        return Column().gap(8.0f).align(wui::Alignment::Stretch).children(
            Text("Uploading · 68%").size(11.0f),
            ProgressBar(0.0f, 100.0f, 68.0f).accessibleLabel("Upload progress"))
            .intoNode();
    }

    return Row()
        .gap(10.0f)
        .align(wui::Alignment::Center)
        .children(
            Icon(iconFor(descriptor.icon)).size(wui::IconSize::Size20),
            Text(descriptor.name).size(12.0f).weight(600)
        )
        .intoNode();
}

std::unique_ptr<wui::Node> buildComponent(
    const ComponentDescriptor& descriptor,
    const OpenComponentHandler& onOpen)
{
    view::components::ComponentCardConfig config;
    config.title = descriptor.name;
    config.description = descriptor.summary;
    config.category = std::string(componentCategoryName(descriptor.category));
    config.icon = iconFor(descriptor.icon);
    if (onOpen && descriptor.id == "button") {
        config.onOpen = [onOpen, id = descriptor.id] { onOpen(id); };
    }
    return view::components::buildComponentCard(
        std::move(config), buildDescriptorPreview(descriptor));
}

class ComponentResultsList final : public wui::ContainerNode {
public:
    ComponentResultsList(GalleryViewModel& viewModel, OpenComponentHandler onOpen)
        : viewModel_(viewModel)
        , onOpen_(std::move(onOpen))
    {
        auto* components = &viewModel_.visibleComponents();
        const auto subscription = components->subscribe([this](const std::vector<ComponentDescriptor>& value) {
            (void)value;
            scrollOffset_ = 0.0f;
            firstMounted_ = invalidIndex();
            markDirty(wui::DirtyFlag::Layout);
        });
        addTeardown([components, subscription] { components->unsubscribe(subscription); });
    }

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        const float visibleRows = std::min<float>(kMaxVisibleRows, static_cast<float>(itemCount()));
        return constraints.clamp({320.0f, visibleRows * kRowExtent});
    }

    void layout(const wui::RectF& bounds) override
    {
        setBounds(bounds);
        clampScroll();
        reconcile();
        const auto& components = viewModel_.visibleComponents().get();
        const std::size_t count = children().size();
        for (std::size_t slot = 0; slot < count; ++slot) {
            const std::size_t index = firstMounted_ + slot;
            if (index >= components.size()) break;
            children()[slot]->layout({bounds.x,
                                      bounds.y + static_cast<float>(index) * kRowExtent - scrollOffset_,
                                      bounds.width,
                                      kRowExtent - kRowGap});
        }
        clearLayoutDirtyRecursively();
    }

    void paint(wui::PaintContext& context) override
    {
        wui::ContainerNode::paint(context);
        clearDirty(wui::DirtyFlag::Paint);
    }

    [[nodiscard]] wui::Node* hitTest(wui::PointF point) override
    {
        if (!bounds().contains(point)) return nullptr;
        for (auto it = children().rbegin(); it != children().rend(); ++it) {
            if (wui::Node* hit = (*it)->hitTest(point)) return hit;
        }
        return this;
    }

    bool onPointerEvent(const wui::PointerEvent& event) override
    {
        if (event.action != wui::PointerAction::Scroll || !bounds().contains(event.position)) return false;
        scrollOffset_ -= event.scrollDelta.y;
        clampScroll();
        markDirty(wui::DirtyFlag::Layout);
        return true;
    }

    [[nodiscard]] std::size_t itemCount() const noexcept
    {
        return viewModel_.visibleComponents().get().size();
    }

    [[nodiscard]] std::size_t mountedCount() const noexcept
    {
        return children().size();
    }

private:
    static constexpr float kRowExtent = 236.0f;
    static constexpr float kRowGap = 12.0f;
    static constexpr float kMaxVisibleRows = 8.0f;

    [[nodiscard]] static constexpr std::size_t invalidIndex() noexcept
    {
        return static_cast<std::size_t>(-1);
    }

    [[nodiscard]] float maxScrollOffset() const noexcept
    {
        return std::max(0.0f, static_cast<float>(itemCount()) * kRowExtent - bounds().height);
    }

    void clampScroll() noexcept
    {
        if (!std::isfinite(scrollOffset_)) scrollOffset_ = 0.0f;
        scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
    }

    [[nodiscard]] std::size_t firstVisibleIndex() const noexcept
    {
        if (itemCount() == 0 || kRowExtent <= 0.0f) return 0;
        return std::min(itemCount(), static_cast<std::size_t>(scrollOffset_ / kRowExtent));
    }

    [[nodiscard]] std::size_t desiredMountedCount(std::size_t first) const noexcept
    {
        if (first >= itemCount()) return 0;
        const auto visible = static_cast<std::size_t>(std::ceil(bounds().height / kRowExtent)) + 1;
        return std::min<std::size_t>(itemCount() - first, std::max<std::size_t>(1, visible));
    }

    void reconcile()
    {
        const std::size_t first = firstVisibleIndex();
        const std::size_t count = desiredMountedCount(first);
        if (firstMounted_ == first && children().size() == count) return;
        clearChildren();
        firstMounted_ = first;
        const auto& components = viewModel_.visibleComponents().get();
        for (std::size_t offset = 0; offset < count; ++offset) {
            appendChild(buildComponent(components[first + offset], onOpen_));
        }
    }

    GalleryViewModel& viewModel_;
    OpenComponentHandler onOpen_;
    float scrollOffset_{0.0f};
    std::size_t firstMounted_{invalidIndex()};
};

std::unique_ptr<wui::Node> buildFilters(GalleryViewModel& viewModel)
{
    constexpr std::array<ComponentCategory, 10> categories{{
        ComponentCategory::All,
        ComponentCategory::Controls,
        ComponentCategory::Inputs,
        ComponentCategory::Feedback,
        ComponentCategory::Navigation,
        ComponentCategory::DataDisplay,
        ComponentCategory::Layout,
        ComponentCategory::Identity,
        ComponentCategory::DateTime,
        ComponentCategory::Overlays,
    }};

    std::vector<view::components::ResponsiveChoiceOption> options;
    options.reserve(categories.size());
    for (const auto category : categories) {
        options.push_back({std::string(componentCategoryName(category)),
                           std::string(componentCategoryName(category))});
    }

    return Column()
        .gap(10.0f)
        .align(wui::Alignment::Stretch)
        .children(
            std::make_unique<view::components::ResponsiveChoiceGroup>(
                std::move(options),
                [&viewModel] {
                    return std::string(componentCategoryName(viewModel.selectedCategory().get()));
                },
                [&viewModel, categories](const std::string& value) {
                    for (const auto category : categories) {
                        if (value == componentCategoryName(category)) {
                            viewModel.selectCategory(category);
                            return;
                        }
                    }
                },
                "Component category",
                5)
        )
        .intoNode();
}

std::unique_ptr<wui::Node> buildResults(GalleryViewModel& viewModel, OpenComponentHandler onOpen)
{
    return Column()
        .gap(12.0f)
        .align(wui::Alignment::Stretch)
        .children(
            Text().bind(viewModel.resultCount(), [](std::size_t count) {
                return std::to_string(count) + (count == 1 ? " component" : " components");
            }).size(12.0f).color(wui::theme().colors.textMuted),
            std::make_unique<ComponentResultsList>(viewModel, std::move(onOpen))
        )
        .intoNode();
}

} // namespace

std::unique_ptr<wui::Node> buildAllComponentsPage(
    GalleryViewModel& viewModel,
    OpenComponentHandler onOpenComponent)
{
    return ScrollView()
        .children(
            Column()
            .gap(20.0f)
            .padding({32.0f, 32.0f, 40.0f, 32.0f})
            .align(wui::Alignment::Stretch)
            .children(
                view::components::buildPageHeader({"CATALOG", "All components", "Filter the complete Fluent component set by category.", {}}),
                buildFilters(viewModel),
                buildResults(viewModel, std::move(onOpenComponent))
            )
        )
        .intoNode();
}

} // namespace whatsui::gallery::view::pages
