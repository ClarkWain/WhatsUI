#include "view_model/button_detail_view_model.h"

namespace whatsui::gallery {

wui::State<ButtonAppearanceSample>& ButtonDetailViewModel::appearance() noexcept { return appearance_; }
const wui::State<ButtonAppearanceSample>& ButtonDetailViewModel::appearance() const noexcept { return appearance_; }
wui::State<ButtonSizeSample>& ButtonDetailViewModel::size() noexcept { return size_; }
const wui::State<ButtonSizeSample>& ButtonDetailViewModel::size() const noexcept { return size_; }
wui::State<bool>& ButtonDetailViewModel::iconVisible() noexcept { return iconVisible_; }
const wui::State<bool>& ButtonDetailViewModel::iconVisible() const noexcept { return iconVisible_; }
wui::State<bool>& ButtonDetailViewModel::enabled() noexcept { return enabled_; }
const wui::State<bool>& ButtonDetailViewModel::enabled() const noexcept { return enabled_; }

void ButtonDetailViewModel::selectAppearance(ButtonAppearanceSample appearance) { appearance_.set(appearance); }
void ButtonDetailViewModel::selectSize(ButtonSizeSample size) { size_.set(size); }
void ButtonDetailViewModel::setIconVisible(bool visible) { iconVisible_.set(visible); }
void ButtonDetailViewModel::setEnabled(bool enabled) { enabled_.set(enabled); }

} // namespace whatsui::gallery
