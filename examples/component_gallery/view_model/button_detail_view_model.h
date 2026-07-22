#pragma once

#include "wui/state.h"

namespace whatsui::gallery {

enum class ButtonAppearanceSample { Primary, Secondary, Subtle, Transparent, Outline };
enum class ButtonSizeSample { Small, Medium, Large };

class ButtonDetailViewModel {
public:
    [[nodiscard]] wui::State<ButtonAppearanceSample>& appearance() noexcept;
    [[nodiscard]] const wui::State<ButtonAppearanceSample>& appearance() const noexcept;
    [[nodiscard]] wui::State<ButtonSizeSample>& size() noexcept;
    [[nodiscard]] const wui::State<ButtonSizeSample>& size() const noexcept;
    [[nodiscard]] wui::State<bool>& iconVisible() noexcept;
    [[nodiscard]] const wui::State<bool>& iconVisible() const noexcept;
    [[nodiscard]] wui::State<bool>& enabled() noexcept;
    [[nodiscard]] const wui::State<bool>& enabled() const noexcept;

    void selectAppearance(ButtonAppearanceSample appearance);
    void selectSize(ButtonSizeSample size);
    void setIconVisible(bool visible);
    void setEnabled(bool enabled);

private:
    wui::State<ButtonAppearanceSample> appearance_{ButtonAppearanceSample::Primary};
    wui::State<ButtonSizeSample> size_{ButtonSizeSample::Medium};
    wui::State<bool> iconVisible_{false};
    wui::State<bool> enabled_{true};
};

} // namespace whatsui::gallery
