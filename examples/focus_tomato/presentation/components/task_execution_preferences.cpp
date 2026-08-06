#include "task_execution_preferences.h"

#include "../focus_style.h"
#include "wui/declarative.h"

#include <array>
#include <charconv>
#include <utility>

namespace whatsui::focus_tomato::presentation {
namespace {

struct SoundscapeChoice {
    const char* id;
    const char* label;
};

constexpr std::array<SoundscapeChoice, 3> kSoundscapes{{
    {"rain", "雨声"},
    {"forest", "森林"},
    {"cafe", "咖啡馆"},
}};

std::string soundChoice(const TaskExecutionPreferences& preferences)
{
    switch (preferences.sound) {
    case TaskSoundPreference::Inherit: return "inherit";
    case TaskSoundPreference::Off: return "off";
    case TaskSoundPreference::Soundscape:
        return preferences.soundscapeId;
    }
    return "inherit";
}

bool knownSoundscape(const std::string& soundscapeId)
{
    for (const auto& item : kSoundscapes) {
        if (soundscapeId == item.id) return true;
    }
    return false;
}

} // namespace

TaskExecutionDraft::TaskExecutionDraft(
    const TaskExecutionPreferences& preferences)
    : focusMinutes(
        preferences.focusMinutes
            ? std::to_string(*preferences.focusMinutes)
            : std::string{})
    , soundChoice(::whatsui::focus_tomato::presentation::soundChoice(
        preferences))
{
}

std::optional<TaskExecutionPreferences> parseTaskExecutionPreferences(
    const TaskExecutionDraft& draft,
    std::string& errorMessage)
{
    TaskExecutionPreferences preferences;
    const std::string minutes = draft.focusMinutes.get();
    if (!minutes.empty()) {
        int value = 0;
        const auto result = std::from_chars(
            minutes.data(), minutes.data() + minutes.size(), value);
        if (result.ec != std::errc{}
            || result.ptr != minutes.data() + minutes.size()
            || value < 1 || value > 180) {
            errorMessage = "单轮专注时长请输入 1～180 的整数，或留空跟随全局。";
            return std::nullopt;
        }
        preferences.focusMinutes = value;
    }

    const std::string sound = draft.soundChoice.get();
    if (sound == "inherit") {
        preferences.sound = TaskSoundPreference::Inherit;
    } else if (sound == "off") {
        preferences.sound = TaskSoundPreference::Off;
    } else if (!sound.empty()) {
        preferences.sound = TaskSoundPreference::Soundscape;
        preferences.soundscapeId = sound;
    } else {
        errorMessage = "请选择声音偏好。";
        return std::nullopt;
    }
    errorMessage.clear();
    return preferences;
}

std::string soundscapeLabel(const std::string& soundscapeId)
{
    for (const auto& item : kSoundscapes) {
        if (soundscapeId == item.id) return item.label;
    }
    return soundscapeId.empty() ? "关闭" : "声音 · " + soundscapeId;
}

std::string taskExecutionSummary(
    const TaskRecord& task,
    const FocusSettings& settings)
{
    const int minutes = effectiveFocusMinutes(task, settings);
    const auto soundscape = effectiveSoundscapeId(task, settings);
    std::string summary = std::to_string(minutes) + " 分钟 · ";
    summary += soundscape ? soundscapeLabel(*soundscape) : "不播放声音";
    return summary;
}

wui::Column buildTaskExecutionPreferenceFields(
    const FocusSettings& settings,
    TaskExecutionDraft& draft,
    std::string automationPrefix)
{
    using namespace wui;
    auto sounds = RadioGroup()
        .name(automationPrefix + ".sound")
        .automationId(automationPrefix + ".sound")
        .accessibleLabel("专注声音")
        .layout(RadioGroupLayout::HorizontalStacked)
        .option("inherit", "全局")
        .option("off", "关闭");
    for (const auto& item : kSoundscapes) {
        sounds.option(item.id, item.label);
    }
    const std::string currentChoice = draft.soundChoice.get();
    if (currentChoice != "inherit" && currentChoice != "off"
        && !knownSoundscape(currentChoice)) {
        sounds.option(
            currentChoice,
            "当前声音（" + soundscapeLabel(currentChoice) + "）");
    }
    sounds.value(currentChoice).bind(draft.soundChoice);

    return Column()
        .gap(10.0f)
        .align(Alignment::Stretch)
        .children(
            Column()
                .gap(3.0f)
                .children(
                    Text("执行偏好")
                        .style(style::text(13.0f, 600, 19.0f))
                        .color(style::textPrimary),
                    Text("为这个任务设置默认值；已开始的计时不会被改动。")
                        .style(style::text(11.0f, 400, 16.0f))
                        .color(style::textMuted)
                ),
            Row()
                .align(Alignment::Center)
                .gap(10.0f)
                .children(
                    Text("单轮专注")
                        .style(style::text(12.0f, 500, 18.0f))
                        .color(style::textSecondary),
                    TextField(
                        "跟随全局：" +
                        std::to_string(settings.focusMinutes))
                        .text(draft.focusMinutes.get())
                        .automationId(automationPrefix + ".focus-minutes")
                        .accessibleLabel("单轮专注时长，分钟")
                        .flex(1.0f)
                        .onChange(
                            [minutes = draft.focusMinutes](
                                const std::string& value) {
                                minutes.set(value);
                            }),
                    Text("分钟")
                        .style(style::text(11.0f, 400, 16.0f))
                        .color(style::textMuted)
                ),
            Column()
                .gap(2.0f)
                .children(
                    Text("专注声音 · 全局默认：" +
                        soundscapeLabel(settings.defaultSoundscapeId))
                        .style(style::text(12.0f, 500, 18.0f))
                        .color(style::textSecondary),
                    Text("播放器尚未接入，当前只保存任务偏好。")
                        .style(style::text(10.0f, 400, 15.0f))
                        .color(style::textMuted)
                ),
            std::move(sounds)
        );
}

} // namespace whatsui::focus_tomato::presentation
