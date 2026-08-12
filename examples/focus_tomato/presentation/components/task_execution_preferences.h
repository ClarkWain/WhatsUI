#pragma once

#include <optional>
#include <string>

#include "../../domain/focus_data.h"
#include "wui/declarative/layout.h"
#include "wui/state.h"

namespace whatsui::focus_tomato::presentation {

struct TaskExecutionDraft {
    explicit TaskExecutionDraft(
        const TaskExecutionPreferences& preferences = {});

    wui::State<std::string> focusMinutes;
    wui::State<std::string> soundChoice;
};

[[nodiscard]] std::optional<TaskExecutionPreferences>
parseTaskExecutionPreferences(
    const TaskExecutionDraft& draft,
    std::string& errorMessage);

[[nodiscard]] std::string soundscapeLabel(const std::string& soundscapeId);
[[nodiscard]] std::string taskExecutionSummary(
    const TaskRecord& task,
    const FocusSettings& settings);

[[nodiscard]] wui::Column buildTaskExecutionPreferenceFields(
    const FocusSettings& settings,
    TaskExecutionDraft& draft,
    std::string automationPrefix);

} // namespace whatsui::focus_tomato::presentation
