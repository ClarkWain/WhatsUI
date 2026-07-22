#include "addons_page.h"

#include <algorithm>
#include <string>
#include <utility>

#include "view/components/component_card.h"
#include "view/components/page_header.h"
#include "wui/theme_extensions.h"
#include "wui/ui.h"
#include "wui/ui_inspector.h"

namespace whatsui::gallery::view::pages {
namespace {

void applyTheme(wui::UiWindow& window, wui::Theme next);

void dismissTopDialog(wui::UiWindow& window)
{
    (void)window.dismissTopDialog();
}

std::unique_ptr<wui::Dialog> buildCommandPaletteDialog(wui::UiWindow& window)
{
    using namespace wui::ui;
    return Dialog()
        .maxWidth(560.0f)
        .dismissOnBackdrop()
        .content(
            Box()
                .width(520.0f)
                .padding(20.0f)
                .children(
                    Column()
                        .gap(12.0f)
                        .align(wui::Alignment::Stretch)
                        .children(
                            Text("Sample command palette").size(18.0f).weight(600),
                            Text("Fixed page-local actions; filtering and command registration are outside this demo.")
                                .size(11.0f).wrap().color(wui::theme().colors.textMuted),
                            Button("Apply light demo preset")
                                .appearance(wui::ButtonAppearance::Subtle)
                                .icon(wui::IconName::Circle)
                                .onClick([&window] {
                                    applyTheme(window, wui::Theme{});
                                    dismissTopDialog(window);
                                }),
                            Button("Apply dark demo preset")
                                .appearance(wui::ButtonAppearance::Subtle)
                                .icon(wui::IconName::Square)
                                .onClick([&window] {
                                    applyTheme(window, wui::fluentDarkTheme());
                                    dismissTopDialog(window);
                                }),
                            Button("Close")
                                .appearance(wui::ButtonAppearance::Secondary)
                                .onClick([&window] { dismissTopDialog(window); }))))
        .intoDialog();
}

std::unique_ptr<wui::Dialog> buildInspectorDialog(wui::UiWindow& window)
{
    using namespace wui::ui;
    auto entries = std::make_unique<wui::Column>();
    entries->setGap(6.0f);
    entries->setAlign(wui::Alignment::Stretch);

    if (const auto* root = window.root()) {
        const auto snapshot = wui::inspectUiTree(*root);
        const std::size_t visible = std::min<std::size_t>(snapshot.size(), 8);
        for (std::size_t index = 0; index < visible; ++index) {
            const auto& entry = snapshot[index];
            entries->appendChild(Row()
                .gap(8.0f)
                .children(
                    Badge(std::to_string(entry.depth))
                        .appearance(wui::BadgeAppearance::Tint),
                    Text(entry.type).size(11.0f).ellipsis(),
                    Spacer().flex(1.0f),
                    Text(std::to_string(entry.childCount) + " children")
                        .size(10.0f)
                        .color(wui::theme().colors.textMuted))
                .intoNode());
        }
    }

    return Dialog()
        .maxWidth(620.0f)
        .dismissOnBackdrop()
        .content(
            Box()
                .width(580.0f)
                .padding(20.0f)
                .children(
                    Column()
                        .gap(14.0f)
                        .align(wui::Alignment::Stretch)
                        .children(
                            Text("Live UI inspector").size(18.0f).weight(600),
                            Text("Read-only snapshot of the active WhatsUI node tree.")
                                .size(12.0f)
                                .color(wui::theme().colors.textMuted),
                            std::move(entries),
                            Button("Close")
                                .appearance(wui::ButtonAppearance::Secondary)
                                .onClick([&window] { dismissTopDialog(window); }))))
        .intoDialog();
}

void applyTheme(wui::UiWindow& window, wui::Theme next)
{
    wui::setTheme(next);
    if (auto* root = window.root()) root->markDirty(wui::DirtyFlag::Style);
}

wui::Theme violetDemoPreset()
{
    wui::Theme next;
    const wui::ColorTokens::Interaction violet{
        {113, 96, 232, 255}, {98, 82, 212, 255},
        {73, 57, 170, 255}, {87, 71, 194, 255}};
    next.colors.brandBackground = violet;
    next.colors.compoundBrandForeground1 = violet;
    next.colors.compoundBrandStroke = violet;
    next.colors.compoundBrandBackground = violet;
    next.colors.brandForeground1 = violet.rest;
    next.colors.accent = violet.rest;
    next.colors.accentHover = violet.hover;
    next.colors.accentPressed = violet.pressed;
    return next;
}

wui::Theme softRadiusDemoPreset()
{
    wui::Theme next;
    next.radius.small = 4.0f;
    next.radius.medium = 8.0f;
    next.radius.large = 10.0f;
    next.radius.xLarge = 12.0f;
    next.radius.xxLarge = 16.0f;
    next.radius.sm = 8.0f;
    next.radius.md = 10.0f;
    next.radius.lg = 12.0f;
    return next;
}

std::unique_ptr<wui::Node> buildCommandPreview()
{
    using namespace wui::ui;
    return Row()
        .gap(8.0f)
        .align(wui::Alignment::Center)
        .children(Icon(wui::IconName::Search), Text("Type a command...").size(12.0f))
        .intoNode();
}

std::unique_ptr<wui::Node> buildInspectorPreview()
{
    using namespace wui::ui;
    return Column()
        .gap(6.0f)
        .children(
            Row().gap(8.0f).children(Badge("0"), Text("UiRoot").size(11.0f)),
            Row().gap(8.0f).children(Badge("1"), Text("ScrollView").size(11.0f)))
        .intoNode();
}

std::unique_ptr<wui::Node> buildThemeStudio(wui::UiWindow& window)
{
    using namespace wui::ui;
    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            Column()
                .gap(14.0f)
                .align(wui::Alignment::Stretch)
                .children(
                    Row().gap(10.0f).align(wui::Alignment::Center).children(
                        Icon(wui::IconName::Edit).color(wui::theme().colors.accent),
                        Text("Theme Studio demo").size(16.0f).weight(600),
                        Spacer().flex(1.0f),
                        Badge("LIMITED DEMO").appearance(wui::BadgeAppearance::Tint).color(wui::BadgeColor::Warning)),
                    Text("This sample changes only light/dark, accent, and radius tokens. It does not import, export, or persist themes.")
                        .size(12.0f)
                        .lineHeight(18.0f)
                        .wrap()
                        .color(wui::theme().colors.textMuted),
                    Row().gap(8.0f).children(
                        Button("Light").onClick([&window] { applyTheme(window, wui::Theme{}); }),
                        Button("Dark").onClick([&window] { applyTheme(window, wui::fluentDarkTheme()); })),
                    Row().gap(8.0f).children(
                        Button("Blue preset").appearance(wui::ButtonAppearance::Subtle)
                            .onClick([&window] { applyTheme(window, wui::Theme{}); }),
                        Button("Violet preset").appearance(wui::ButtonAppearance::Subtle)
                            .onClick([&window] { applyTheme(window, violetDemoPreset()); })),
                    Row().gap(8.0f).children(
                        Button("Default radius preset").appearance(wui::ButtonAppearance::Subtle)
                            .onClick([&window] { applyTheme(window, wui::Theme{}); }),
                        Button("Soft radius preset").appearance(wui::ButtonAppearance::Subtle)
                            .onClick([&window] { applyTheme(window, softRadiusDemoPreset()); }))))
        .intoNode();
}

} // namespace

std::unique_ptr<wui::Node> buildAddonsPage(wui::UiWindow& window)
{
    using namespace wui::ui;
    view::components::ComponentCardConfig commands{
        "Command Palette", "Search and invoke common gallery actions.", "Developer tool",
        wui::IconName::Search, "Open palette", 112.0f,
        [&window] { (void)window.showDialog(buildCommandPaletteDialog(window)); }};
    view::components::ComponentCardConfig inspector{
        "UI Inspector", "Inspect the active retained node tree without mutating it.", "Diagnostics",
        wui::IconName::TaskList, "Inspect tree", 112.0f,
        [&window] { (void)window.showDialog(buildInspectorDialog(window)); }};

    return ScrollView()
        .children(
            Column()
                .gap(20.0f)
                .padding({32.0f, 32.0f, 40.0f, 32.0f})
                .align(wui::Alignment::Stretch)
                .children(
                    view::components::buildPageHeader({"TOOLS", "Add-ons", "Focused utilities built on public WhatsUI dialog, inspector, and theme APIs.", {}}),
                    Row().gap(12.0f).align(wui::Alignment::Stretch).children(
                        view::components::buildComponentCard(std::move(commands), buildCommandPreview()),
                        view::components::buildComponentCard(std::move(inspector), buildInspectorPreview())),
                    buildThemeStudio(window)))
        .intoNode();
}

} // namespace whatsui::gallery::view::pages
