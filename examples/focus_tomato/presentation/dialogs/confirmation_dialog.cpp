#include "confirmation_dialog.h"

#include "../focus_style.h"
#include "wui/app.h"
#include "wui/scheduler.h"
#include "wui/declarative.h"

#include <utility>

namespace whatsui::focus_tomato::presentation {
namespace {

class ConfirmationDialogView {
public:
    ConfirmationDialogView(
        wui::UiWindow& window,
        ConfirmationDialogSpec spec,
        std::function<void()> onConfirm)
        : window_(&window)
        , spec_(std::move(spec))
        , onConfirm_(std::move(onConfirm))
    {
    }

    auto body()
    {
        using namespace wui;
        auto confirm = [window = window_,
                        onConfirm = std::move(onConfirm_)]() mutable {
            (void)window->dismissTopDialog();
            scheduleStructuralUpdate(
                window,
                [onConfirm = std::move(onConfirm)]() mutable {
                    if (onConfirm) onConfirm();
                });
        };
        auto cancel = Button("取消")
            .automationId("focus.confirm.cancel")
            .appearance(ButtonAppearance::Outline)
            .onClick([window = window_] {
                (void)window->dismissTopDialog();
            });
        initialFocus_ = cancel.node();

        return Dialog()
            .maxWidth(420.0f)
            .dismissOnBackdrop()
            .content(
                Box()
                    .width(360.0f)
                    .background(style::surface)
                    .radius(16.0f)
                    .padding({22.0f, 20.0f, 22.0f, 20.0f})
                    .children(
                        Column()
                            .gap(14.0f)
                            .align(wui::Alignment::Stretch)
                            .children(
                                Text(spec_.title)
                                    .style(style::text(20.0f, 700, 29.0f))
                                    .color(style::textPrimary),
                                Text(spec_.message)
                                    .style(style::text(12.0f, 400, 18.0f))
                                    .color(style::textSecondary),
                                Row()
                                    .align(wui::Alignment::Center)
                                    .gap(8.0f)
                                    .children(
                                        Spacer().flex(1.0f),
                                        std::move(cancel),
                                        Button(spec_.confirmLabel)
                                            .automationId(
                                                "focus.confirm.accept")
                                            .appearance(
                                                ButtonAppearance::Primary)
                                            .onClick(std::move(confirm))
                                    )
                            )
                    )
            );
    }

    [[nodiscard]] wui::Node* initialFocus() const noexcept
    {
        return initialFocus_;
    }

private:
    wui::UiWindow* window_;
    ConfirmationDialogSpec spec_;
    std::function<void()> onConfirm_;
    wui::Node* initialFocus_{nullptr};
};

} // namespace

void showConfirmationDialog(
    wui::UiWindow& window,
    ConfirmationDialogSpec spec,
    std::function<void()> onConfirm)
{
    ConfirmationDialogView dialog(
        window, std::move(spec), std::move(onConfirm));
    (void)window.showDialog(dialog);
    window.focusManager().setFocused(dialog.initialFocus());
}

} // namespace whatsui::focus_tomato::presentation
