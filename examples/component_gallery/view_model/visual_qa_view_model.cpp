#include "view_model/visual_qa_view_model.h"

#include <algorithm>

namespace whatsui::gallery {

wui::State<float>& VisualQaViewModel::actualScaleFactor() noexcept { return actualScaleFactor_; }
const wui::State<float>& VisualQaViewModel::actualScaleFactor() const noexcept { return actualScaleFactor_; }
wui::State<DpiProfile>& VisualQaViewModel::selectedDpi() noexcept { return selectedDpi_; }
const wui::State<DpiProfile>& VisualQaViewModel::selectedDpi() const noexcept { return selectedDpi_; }
wui::State<ThemePreview>& VisualQaViewModel::selectedTheme() noexcept { return selectedTheme_; }
const wui::State<ThemePreview>& VisualQaViewModel::selectedTheme() const noexcept { return selectedTheme_; }
wui::State<InteractionPreview>& VisualQaViewModel::selectedInteraction() noexcept { return selectedInteraction_; }
const wui::State<InteractionPreview>& VisualQaViewModel::selectedInteraction() const noexcept { return selectedInteraction_; }

void VisualQaViewModel::setActualScaleFactor(float scaleFactor)
{
    actualScaleFactor_.set(std::max(0.5f, std::min(scaleFactor, 4.0f)));
}
void VisualQaViewModel::selectDpi(DpiProfile profile) { selectedDpi_.set(profile); }
void VisualQaViewModel::selectTheme(ThemePreview theme) { selectedTheme_.set(theme); }
void VisualQaViewModel::selectInteraction(InteractionPreview interaction) { selectedInteraction_.set(interaction); }

} // namespace whatsui::gallery
