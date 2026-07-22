#pragma once

#include "wui/state.h"

namespace whatsui::gallery {

enum class DpiProfile { System, Dpi100, Dpi125, Dpi150, Dpi200 };
enum class ThemePreview { Light, Dark };
enum class InteractionPreview { Rest, Hovered, Pressed, Focused, Disabled };

class VisualQaViewModel {
public:
    [[nodiscard]] wui::State<float>& actualScaleFactor() noexcept;
    [[nodiscard]] const wui::State<float>& actualScaleFactor() const noexcept;
    [[nodiscard]] wui::State<DpiProfile>& selectedDpi() noexcept;
    [[nodiscard]] const wui::State<DpiProfile>& selectedDpi() const noexcept;
    [[nodiscard]] wui::State<ThemePreview>& selectedTheme() noexcept;
    [[nodiscard]] const wui::State<ThemePreview>& selectedTheme() const noexcept;
    [[nodiscard]] wui::State<InteractionPreview>& selectedInteraction() noexcept;
    [[nodiscard]] const wui::State<InteractionPreview>& selectedInteraction() const noexcept;

    void setActualScaleFactor(float scaleFactor);
    void selectDpi(DpiProfile profile);
    void selectTheme(ThemePreview theme);
    void selectInteraction(InteractionPreview interaction);

private:
    wui::State<float> actualScaleFactor_{1.0f};
    wui::State<DpiProfile> selectedDpi_{DpiProfile::System};
    wui::State<ThemePreview> selectedTheme_{ThemePreview::Light};
    wui::State<InteractionPreview> selectedInteraction_{InteractionPreview::Rest};
};

} // namespace whatsui::gallery
