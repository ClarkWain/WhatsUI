#include "new_task_dialog.h"

#include "../focus_style.h"
#include "wui/app.h"
#include "wui/scheduler.h"
#include "wui/declarative.h"

#include <memory>
#include <utility>

namespace whatsui::focus_tomato::presentation {

void showNewTaskDialog(wui::UiWindow& window,
                       FocusViewModel& viewModel,
                       TaskCreatedCallback onCreated)
{
    using namespace wui;

    auto title = std::make_unique<wui::TextFieldNode>("例如：完成产品设计稿");
    auto* titleRaw = title.get();
    titleRaw->setAutomationId("focus.new-task.title");
    titleRaw->setFlex(1.0f);

    auto error = std::make_unique<wui::TextNode>();
    auto* errorRaw = error.get();
    errorRaw->setTextStyle(style::text(12.0f, 400, 18.0f));
    errorRaw->setColor(style::accent);

    auto submit = [&window, &viewModel, titleRaw, errorRaw,
                   onCreated = std::move(onCreated)]() mutable {
        const auto result = viewModel.addTask(titleRaw->model().text());
        if (!result.succeeded()) {
            errorRaw->setValue(result.message.empty()
                ? "任务没有保存，请检查名称后重试。"
                : result.message);
            return;
        }
        (void)window.dismissTopDialog();
        if (onCreated) {
            auto refresh = std::move(onCreated);
            wui::scheduleStructuralUpdate(
                &window,
                [refresh = std::move(refresh)]() mutable {
                    if (refresh) refresh();
                });
        }
    };
    titleRaw->onSubmit(submit);

    auto dialog = Dialog()
        .maxWidth(400.0f)
        .content(
            Box()
                .width(352.0f)
                .background(style::surface)
                .radius(16.0f)
                .padding({22.0f, 20.0f, 22.0f, 20.0f})
                .children(
                    Column()
                        .gap(14.0f)
                        .align(wui::Alignment::Stretch)
                        .children(
                            Column()
                                .gap(4.0f)
                                .children(
                                    Text("新建任务")
                                        .style(style::text(20.0f, 700, 29.0f))
                                        .color(style::textPrimary),
                                    Text("先写清这一轮要推进的具体结果。")
                                        .style(style::text(12.0f, 400, 18.0f))
                                        .color(style::textSecondary)
                                ),
                            std::move(title),
                            std::move(error),
                            Row()
                                .align(wui::Alignment::Center)
                                .gap(8.0f)
                                .children(
                                    Spacer().flex(1.0f),
                                    Button("取消")
                                        .automationId("focus.new-task.cancel")
                                        .appearance(wui::ButtonAppearance::Outline)
                                        .onClick([&window] {
                                            (void)window.dismissTopDialog();
                                        }),
                                    Button("保存任务")
                                        .automationId("focus.new-task.save")
                                        .appearance(wui::ButtonAppearance::Primary)
                                        .onClick(std::move(submit))
                                )
                        )
                )
            );
    (void)window.showDialog(std::move(dialog));
    window.focusManager().setFocused(titleRaw);
}

} // namespace whatsui::focus_tomato::presentation
