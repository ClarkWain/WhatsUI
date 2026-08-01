#include "focus_assets.h"

#include <stdexcept>
#include <string>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace whatsui::focus_tomato::presentation {
namespace {

wui::ImageSource loadRgba(const std::filesystem::path& path)
{
    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    unsigned char* decoded = stbi_load(path.string().c_str(), &width, &height,
                                       &sourceChannels, STBI_rgb_alpha);
    if (decoded == nullptr || width <= 0 || height <= 0) {
        throw std::runtime_error("Could not decode FocusTomato asset: "
                                 + path.string());
    }
    const std::size_t byteCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    std::vector<unsigned char> pixels(decoded, decoded + byteCount);
    stbi_image_free(decoded);
    return wui::ImageSource(std::move(pixels), width, height);
}

} // namespace

FocusAssets loadFocusAssets(const std::filesystem::path& directory)
{
    return {
        loadRgba(directory / "brand-tomato.png"),
        loadRgba(directory / "mascot-focus.png"),
        loadRgba(directory / "mascot-break.png"),
        loadRgba(directory / "mascot-complete.png"),
        loadRgba(directory / "icon-play.png"),
        loadRgba(directory / "icon-pause.png"),
        loadRgba(directory / "icon-reset.png"),
        loadRgba(directory / "icon-skip.png"),
    };
}

} // namespace whatsui::focus_tomato::presentation
