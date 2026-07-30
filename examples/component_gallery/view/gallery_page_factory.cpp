#include "view/gallery_page_factory.h"

#include <memory>
#include <string>

#include "application/gallery_router.h"
#include "application/platform_url_opener.h"
#include "view/app_shell_view.h"
#include "view/pages/about_page.h"
#include "view/pages/addons_page.h"
#include "view/pages/all_components_page.h"
#include "view/pages/button_detail_page.h"
#include "view/pages/controls_page.h"
#include "view/pages/long_text_page.h"
#include "view/pages/overview_page.h"
#include "view/pages/visual_qa_page.h"
#include "view_model/gallery_view_models.h"
#include "wui/theme.h"
#include "wui/theme_extensions.h"

namespace whatsui::gallery::view {
namespace {

std::unique_ptr<wui::Node> buildPageContent(GalleryRoute route,
                                            GalleryViewModels& viewModels,
                                            GalleryRouter& router)
{
    using namespace pages;
    viewModels.visualQa().setActualScaleFactor(
        router.window().platformWindow().metrics().scaleFactor);
    switch (route) {
    case GalleryRoute::Overview:
        return buildOverviewPage(viewModels.catalog(), viewModels.gallery(),
                                 [&router](GalleryRoute target) { router.navigate(target); });
    case GalleryRoute::AllComponents:
        return buildAllComponentsPage(
            viewModels.gallery(),
            [&router, &viewModels](const std::string& id) {
                if (const auto* component = viewModels.catalog().find(id)) {
                    router.openComponent(*component);
                }
            });
    case GalleryRoute::Controls:
        return buildControlsPage([&router] { router.navigate(GalleryRoute::ButtonDetail); });
    case GalleryRoute::LongText:
        return buildLongTextPage();
    case GalleryRoute::AddOns:
        return buildAddonsPage(
            router.window(),
            viewModels.themeStudio(),
            [&router, &viewModels](wui::Theme theme, bool dark) {
                viewModels.visualQa().selectTheme(
                    dark ? ThemePreview::Dark : ThemePreview::Light);
                wui::setTheme(theme);
                router.refresh();
            });
    case GalleryRoute::VisualQa:
        return buildVisualQaPage(
            viewModels.visualQa(), router.window(), [&router](ThemePreview theme) {
                wui::setTheme(theme == ThemePreview::Dark
                    ? wui::fluentDarkTheme()
                    : wui::Theme{});
                router.refresh();
            });
    case GalleryRoute::About:
        return buildAboutPage([](const std::string& url) {
            (void)openExternalUrl(url);
        });
    case GalleryRoute::ButtonDetail:
        return buildButtonDetailPage(viewModels.buttonDetail(), [&router] {
            if (router.canGoBack()) router.goBack();
            else router.navigate(GalleryRoute::AllComponents);
        });
    }
    return buildOverviewPage(viewModels.catalog(), viewModels.gallery(), {});
}

} // namespace

std::unique_ptr<wui::Node> buildGalleryPage(GalleryRoute route,
                                            GalleryViewModels& viewModels,
                                            GalleryRouter& router)
{
    return buildAppShell(router.window(), viewModels.navigation().currentRoute().get(),
                         buildPageContent(route, viewModels, router),
                         [&router](GalleryRoute target) { router.navigate(target); });
}

} // namespace whatsui::gallery::view
