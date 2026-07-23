#include "responsive_overview_hero.h"

#include <stdexcept>
#include <utility>

#include "wui/theme.h"
#include "wui/ui.h"

namespace whatsui::gallery::view::components {
namespace {

class ResponsiveOverviewHero final : public wui::ContainerNode {
public:
    ResponsiveOverviewHero(GalleryViewModel& gallery, OverviewNavigateHandler navigate)
        : gallery_(&gallery), navigate_(std::move(navigate)) {}

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        const float height = constraints.maxWidth < 720.0f ? 300.0f : 244.0f;
        return constraints.clamp({constraints.maxWidth, height});
    }

    void layout(const wui::RectF& bounds) override
    {
        wui::Node::layout(bounds);
        const bool compact = bounds.width < 720.0f;
        if (!content_ || compact != compact_) rebuild(compact);
        const auto size = content_->measureWithConstraints(
            {0.0f, bounds.width, 0.0f, 1000000.0f});
        content_->layout({bounds.x, bounds.y, bounds.width, size.height});
        setBounds({bounds.x, bounds.y, bounds.width, size.height});
        clearLayoutDirtyRecursively();
    }

    void paint(wui::PaintContext& context) override
    {
        if (content_) content_->paint(context);
        clearDirty(wui::DirtyFlag::Paint);
    }

    [[nodiscard]] wui::Node* hitTest(wui::PointF point) override
    {
        if (!bounds().contains(point)) return nullptr;
        if (content_ != nullptr) {
            if (auto* hit = content_->hitTest(point)) return hit;
        }
        return this;
    }

private:
    [[nodiscard]] std::unique_ptr<wui::Node> buildCopy(float titleSize,
                                                        float titleLineHeight,
                                                        bool verticalActions)
    {
        using namespace wui::ui;
        const auto& current = wui::theme();
        auto actions = verticalActions
            ? Column()
                  .gap(8.0f)
                  .align(wui::Alignment::Stretch)
                  .children(
                      Button("Browse all components")
                          .appearance(wui::ButtonAppearance::Primary)
                          .icon(wui::IconName::ChevronRight)
                          .iconPosition(wui::ButtonIconPosition::After)
                          .onClick([this] {
                              gallery_->clearFilters();
                              if (navigate_) navigate_(GalleryRoute::AllComponents);
                          }),
                      Button("Visual QA")
                          .appearance(wui::ButtonAppearance::Secondary)
                          .onClick([this] {
                              if (navigate_) navigate_(GalleryRoute::VisualQa);
                          }))
                  .intoNode()
            : Row()
                  .gap(8.0f)
                  .children(
                      Button("Browse all components")
                          .appearance(wui::ButtonAppearance::Primary)
                          .icon(wui::IconName::ChevronRight)
                          .iconPosition(wui::ButtonIconPosition::After)
                          .onClick([this] {
                              gallery_->clearFilters();
                              if (navigate_) navigate_(GalleryRoute::AllComponents);
                          }),
                      Button("Visual QA")
                          .appearance(wui::ButtonAppearance::Secondary)
                          .onClick([this] {
                              if (navigate_) navigate_(GalleryRoute::VisualQa);
                          }))
                  .intoNode();

        return Column()
            .gap(14.0f)
            .align(wui::Alignment::Start)
            .children(
                Badge("FLUENT 2 FOR C++")
                    .appearance(wui::BadgeAppearance::Tint)
                    .color(wui::BadgeColor::Brand),
                Text("Build polished native interfaces faster.")
                    .size(titleSize)
                    .lineHeight(titleLineHeight)
                    .weight(600)
                    .wrap()
                    .color(current.colors.text),
                Text("Explore production-ready WhatsUI components, states, tokens, and developer tooling in one live gallery.")
                    .size(14.0f)
                    .lineHeight(22.0f)
                    .wrap()
                    .color(current.colors.textMuted),
                std::move(actions))
            .intoNode();
    }

    [[nodiscard]] std::unique_ptr<wui::Node> buildContent(bool compact)
    {
        using namespace wui::ui;
        const auto& current = wui::theme();
        const float padding = compact ? 16.0f : 32.0f;
        if (compact) {
            return Box()
                .background(current.colors.surface)
                .radius(current.radius.xLarge)
                .padding(padding)
                .children(buildCopy(26.0f, 32.0f, true))
                .intoNode();
        }
        return Box()
            .background(current.colors.surface)
            .radius(current.radius.xLarge)
            .padding(padding)
            .children(
                Row()
                    .gap(28.0f)
                    .align(wui::Alignment::Center)
                    .children(
                        buildCopy(32.0f, 40.0f, false),
                        Box()
                            .width(220.0f)
                            .height(180.0f)
                            .radius(current.radius.xxLarge)
                            .background(current.colors.surfaceAlt)
                            .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                            .children(
                                Column()
                                    .gap(10.0f)
                                    .align(wui::Alignment::Center)
                                    .children(
                                        Icon(wui::IconName::TaskList)
                                            .size(wui::IconSize::Size24)
                                            .style(wui::IconStyle::Filled)
                                            .color(current.colors.accent),
                                        Text("Live components").size(13.0f).weight(600),
                                        Badge("INTERACTIVE PREVIEW")
                                            .appearance(wui::BadgeAppearance::Tint)
                                            .color(wui::BadgeColor::Brand))))
            )
            .intoNode();
    }

    void rebuild(bool compact)
    {
        clearChildren();
        compact_ = compact;
        auto content = buildContent(compact_);
        content_ = content.get();
        appendChild(std::move(content));
    }

    GalleryViewModel* gallery_;
    OverviewNavigateHandler navigate_;
    wui::Node* content_{nullptr};
    bool compact_{false};
};

} // namespace

std::unique_ptr<wui::Node> buildResponsiveOverviewHero(
    GalleryViewModel& gallery, OverviewNavigateHandler navigate)
{
    return std::make_unique<ResponsiveOverviewHero>(gallery, std::move(navigate));
}

} // namespace whatsui::gallery::view::components
