#pragma once

#include <filesystem>

#include "wui/widgets.h"

namespace whatsui::focus_tomato::presentation {

struct FocusAssets {
    wui::ImageSource brandTomato;
    wui::ImageSource mascotFocus;
    wui::ImageSource mascotBreak;
    wui::ImageSource mascotComplete;
    wui::ImageSource iconPlay;
    wui::ImageSource iconPause;
    wui::ImageSource iconReset;
    wui::ImageSource iconSkip;
};

[[nodiscard]] FocusAssets loadFocusAssets(const std::filesystem::path& directory);

} // namespace whatsui::focus_tomato::presentation
