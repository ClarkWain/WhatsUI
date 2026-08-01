#include "addons_page.h"

#include <algorithm>
#include <string>
#include <utility>

#include "view/components/component_card.h"
#include "view/components/page_header.h"
#include "view/components/responsive_layouts.h"
#include "wui/theme_extensions.h"
#include "wui/declarative.h"
#include "wui/ui_inspector.h"

using namespace wui;

namespace whatsui::gallery::view::pages {
namespace {

void dismissTopDialog(wui::UiWindow& window)
{
    (void)window.dismissTopDialog();
}

std::unique_ptr<wui::DialogNode> buildCommandPaletteDialog(
    wui::UiWindow& window,
    ApplyGalleryThemeHandler applyTheme
)
{
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
                        .onClick([&window, applyTheme] {
                            if (applyTheme) applyTheme(wui::Theme{}, false);
                            dismissTopDialog(window);
                        }),
                    Button("Apply dark demo preset")
                        .appearance(wui::ButtonAppearance::Subtle)
                        .icon(wui::IconName::Square)
                        .onClick([&window, applyTheme] {
                            if (applyTheme) applyTheme(wui::fluentDarkTheme(), true);
                            dismissTopDialog(window);
                        }),
                    Button("Close")
                        .appearance(wui::ButtonAppearance::Secondary)
                        .onClick([&window] { dismissTopDialog(window); })
                )
            )
        )
        .build();
}

std::unique_ptr<wui::DialogNode> buildInspectorDialog(wui::UiWindow& window)
{
    auto entries = std::make_unique<wui::ColumnNode>();
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
                        .color(wui::theme().colors.textMuted)
                )
                .build());
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
                        .onClick([&window] { dismissTopDialog(window); })
                )
            )
        )
        .build();
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
    return Row()
        .gap(8.0f)
        .align(wui::Alignment::Center)
        .children(Icon(wui::IconName::Search), Text("Type a command...").size(12.0f))
        .build();
}

std::unique_ptr<wui::Node> buildInspectorPreview()
{
    return Column()
        .gap(6.0f)
        .children(
            Row().gap(8.0f).children(Badge("0"), Text("UiRoot").size(11.0f)),
            Row().gap(8.0f).children(Badge("1"), Text("ScrollView").size(11.0f))
        )
        .build();
}

// Reactive-style preset button: the caller decides whether the button is the
// currently active option (Primary appearance) or a candidate (Subtle). One
// click delegates to the shared apply lambda which mutates the ThemeStudio
// view model, rebuilds the composed theme, and triggers a router.refresh().
template <class Handler>
[[nodiscard]] std::unique_ptr<wui::Node> presetButton(std::string label,
                                                     bool active,
                                                     Handler onClick)
{
    return Button(std::move(label))
        .appearance(active ? wui::ButtonAppearance::Primary
                           : wui::ButtonAppearance::Subtle)
        .onClick(std::move(onClick))
        .build();
}

std::unique_ptr<wui::Node> buildThemeStudio(ThemeStudioViewModel& themeStudio,
                                            ApplyGalleryThemeHandler applyTheme)
{
    // Every preset click mutates the view model on one axis and asks the
    // gallery to reapply the composed theme. The lambda outlives the current
    // page tree because it captures references that live on GalleryApplication.
    auto apply = [&themeStudio, applyTheme] {
        if (!applyTheme) return;
        applyTheme(themeStudio.buildTheme(), themeStudio.isDark());
    };

    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            Column()
            .gap(14.0f)
            .align(wui::Alignment::Stretch)
            .children(
                [] {
                    auto heading = std::make_unique<view::components::ResponsiveRow>();
                    heading->gap(10.0f).align(wui::Alignment::Center);
                    heading->appendChild(Icon(wui::IconName::Edit)
                        .color(wui::theme().colors.accent).build());
                    heading->appendChild(Text("Theme Studio demo").size(16.0f).weight(600).build());
                    heading->appendChild(Spacer().flex(1.0f).build());
                    heading->appendChild(Badge("LIMITED DEMO").appearance(wui::BadgeAppearance::Tint)
                        .color(wui::BadgeColor::Warning).build());
                    return heading;
                }(),

                Text("Presets compose along three axes — mode, accent, and radius. Each button toggles its axis without resetting the others.")
                    .size(12.0f)
                    .lineHeight(18.0f)
                    .wrap()
                    .color(wui::theme().colors.textMuted),

                Text("Mode").size(11.0f).weight(600).color(wui::theme().colors.textMuted),
                view::components::buildResponsiveFlow(8.0f,
                    presetButton("Light", themeStudio.mode() == ThemeStudioMode::Light,
                                 [&themeStudio, apply] {
                                     themeStudio.setMode(ThemeStudioMode::Light);
                                     apply();
                                 }),
                    presetButton("Dark", themeStudio.mode() == ThemeStudioMode::Dark,
                                 [&themeStudio, apply] {
                                     themeStudio.setMode(ThemeStudioMode::Dark);
                                     apply();
                                 })
                ),

                Text("Accent").size(11.0f).weight(600).color(wui::theme().colors.textMuted),
                view::components::buildResponsiveFlow(8.0f,
                    presetButton("Blue", themeStudio.accent() == ThemeStudioAccent::Blue,
                                 [&themeStudio, apply] {
                                     themeStudio.setAccent(ThemeStudioAccent::Blue);
                                     apply();
                                 }),
                    presetButton("Violet", themeStudio.accent() == ThemeStudioAccent::Violet,
                                 [&themeStudio, apply] {
                                     themeStudio.setAccent(ThemeStudioAccent::Violet);
                                     apply();
                                 }),
                    presetButton("Teal", themeStudio.accent() == ThemeStudioAccent::Teal,
                                 [&themeStudio, apply] {
                                     themeStudio.setAccent(ThemeStudioAccent::Teal);
                                     apply();
                                 }),
                    presetButton("Rose", themeStudio.accent() == ThemeStudioAccent::Rose,
                                 [&themeStudio, apply] {
                                     themeStudio.setAccent(ThemeStudioAccent::Rose);
                                     apply();
                                 }),
                    presetButton("Green", themeStudio.accent() == ThemeStudioAccent::Green,
                                 [&themeStudio, apply] {
                                     themeStudio.setAccent(ThemeStudioAccent::Green);
                                     apply();
                                 }),
                    presetButton("Orange", themeStudio.accent() == ThemeStudioAccent::Orange,
                                 [&themeStudio, apply] {
                                     themeStudio.setAccent(ThemeStudioAccent::Orange);
                                     apply();
                                 })
                ),

                Text("Radius").size(11.0f).weight(600).color(wui::theme().colors.textMuted),
                view::components::buildResponsiveFlow(8.0f,
                    presetButton("Default radius preset",
                                 themeStudio.radius() == ThemeStudioRadius::Default,
                                 [&themeStudio, apply] {
                                     themeStudio.setRadius(ThemeStudioRadius::Default);
                                     apply();
                                 }),
                    presetButton("Soft radius preset",
                                 themeStudio.radius() == ThemeStudioRadius::Soft,
                                 [&themeStudio, apply] {
                                     themeStudio.setRadius(ThemeStudioRadius::Soft);
                                     apply();
                                 })
                )
            )
        )
        .build();
}

} // namespace

std::unique_ptr<wui::Node> buildAddonsPage(
    wui::UiWindow& window,
    ThemeStudioViewModel& themeStudio,
    ApplyGalleryThemeHandler applyTheme)
{
    view::components::ComponentCardConfig commands{
        "Command Palette", "Search and invoke common gallery actions.", "Developer tool",
        wui::IconName::Search, "Open palette", 112.0f,
        [&window, applyTheme] {
            (void)window.showDialog(buildCommandPaletteDialog(window, applyTheme));
        }};
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
                [] (view::components::ComponentCardConfig commands,
                    view::components::ComponentCardConfig inspector) {
                    auto cards = std::make_unique<view::components::ResponsiveRow>();
                    cards->gap(12.0f).align(wui::Alignment::Stretch);
                    cards->appendChild(view::components::buildComponentCard(
                        std::move(commands), buildCommandPreview()));
                    cards->appendChild(view::components::buildComponentCard(
                        std::move(inspector), buildInspectorPreview()));
                    return cards;
                }(std::move(commands), std::move(inspector)),
                buildThemeStudio(themeStudio, std::move(applyTheme))
            )
        )
        .build();
}

} // namespace whatsui::gallery::view::pages
