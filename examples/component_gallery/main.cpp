#include <exception>
#include <iostream>

#include "application/gallery_application.h"
#include "view/gallery_page_factory.h"
#include "wui/glfw_platform.h"

int main()
{
    try {
        whatsui::gallery::GalleryApplication application(
            wui::createGlfwPlatformHost(), 
            whatsui::gallery::view::buildGalleryPage
        );
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "WhatsUI Component Gallery: " << error.what() << std::endl;
        return 1;
    }
}
