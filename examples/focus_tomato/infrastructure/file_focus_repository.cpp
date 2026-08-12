#include "file_focus_repository.h"

#include <charconv>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "../application/focus_data_migrator.h"

namespace whatsui::focus_tomato {
namespace {

constexpr const char* kHeaderPrefix = "WhatsUIFocusTomatoStore\t";

std::string encodeField(const std::string& value)
{
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (const unsigned char byte : value) {
        if (byte == '%' || byte == '\t' || byte == '\r' || byte == '\n') {
            encoded.push_back('%');
            encoded.push_back(digits[(byte >> 4) & 0x0F]);
            encoded.push_back(digits[byte & 0x0F]);
        } else {
            encoded.push_back(static_cast<char>(byte));
        }
    }
    return encoded;
}

int hexDigit(char value) noexcept
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

bool decodeField(const std::string& value, std::string& decoded)
{
    decoded.clear();
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            decoded.push_back(value[index]);
            continue;
        }
        if (index + 2 >= value.size()) return false;
        const int high = hexDigit(value[index + 1]);
        const int low = hexDigit(value[index + 2]);
        if (high < 0 || low < 0) return false;
        decoded.push_back(static_cast<char>((high << 4) | low));
        index += 2;
    }
    return true;
}

std::vector<std::string> splitFields(const std::string& line)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const auto separator = line.find('\t', start);
        if (separator == std::string::npos) {
            fields.push_back(line.substr(start));
            return fields;
        }
        fields.push_back(line.substr(start, separator - start));
        start = separator + 1;
    }
}

template <class Integer>
bool parseInteger(const std::string& value, Integer& output)
{
    if (value.empty()) return false;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), output);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool parseBool(const std::string& value, bool& output)
{
    if (value == "0") {
        output = false;
        return true;
    }
    if (value == "1") {
        output = true;
        return true;
    }
    return false;
}

std::string encodeOptionalString(const std::optional<std::string>& value)
{
    return value ? "+" + encodeField(*value) : "-";
}

bool decodeOptionalString(const std::string& value, std::optional<std::string>& output)
{
    if (value == "-") {
        output.reset();
        return true;
    }
    if (value.empty() || value.front() != '+') return false;
    std::string decoded;
    if (!decodeField(value.substr(1), decoded)) return false;
    output = std::move(decoded);
    return true;
}

std::string encodeOptionalInteger(const std::optional<std::int64_t>& value)
{
    return value ? std::to_string(*value) : "-";
}

bool decodeOptionalInteger(const std::string& value, std::optional<std::int64_t>& output)
{
    if (value == "-") {
        output.reset();
        return true;
    }
    std::int64_t decoded = 0;
    if (!parseInteger(value, decoded)) return false;
    output = decoded;
    return true;
}

const char* taskStatusName(TaskStatus value) noexcept
{
    switch (value) {
    case TaskStatus::Active: return "active";
    case TaskStatus::Done: return "done";
    case TaskStatus::Archived: return "archived";
    case TaskStatus::ArchivedDone: return "archived_done";
    }
    return "invalid";
}

bool parseTaskStatus(const std::string& value, TaskStatus& output)
{
    if (value == "active") output = TaskStatus::Active;
    else if (value == "done") output = TaskStatus::Done;
    else if (value == "archived") output = TaskStatus::Archived;
    else if (value == "archived_done") output = TaskStatus::ArchivedDone;
    else return false;
    return true;
}

const char* taskSoundPreferenceName(TaskSoundPreference value) noexcept
{
    switch (value) {
    case TaskSoundPreference::Inherit: return "inherit";
    case TaskSoundPreference::Off: return "off";
    case TaskSoundPreference::Soundscape: return "soundscape";
    }
    return "invalid";
}

bool parseTaskSoundPreference(
    const std::string& value,
    TaskSoundPreference& output)
{
    if (value == "inherit") output = TaskSoundPreference::Inherit;
    else if (value == "off") output = TaskSoundPreference::Off;
    else if (value == "soundscape") {
        output = TaskSoundPreference::Soundscape;
    } else {
        return false;
    }
    return true;
}

const char* sessionTypeName(SessionType value) noexcept
{
    switch (value) {
    case SessionType::Focus: return "focus";
    case SessionType::ShortBreak: return "short_break";
    case SessionType::LongBreak: return "long_break";
    }
    return "invalid";
}

bool parseSessionType(const std::string& value, SessionType& output)
{
    if (value == "focus") output = SessionType::Focus;
    else if (value == "short_break") output = SessionType::ShortBreak;
    else if (value == "long_break") output = SessionType::LongBreak;
    else return false;
    return true;
}

const char* sessionStatusName(SessionStatus value) noexcept
{
    switch (value) {
    case SessionStatus::Pending: return "pending";
    case SessionStatus::Running: return "running";
    case SessionStatus::Paused: return "paused";
    case SessionStatus::CompletionPending: return "completion_pending";
    case SessionStatus::Completed: return "completed";
    case SessionStatus::Aborted: return "aborted";
    case SessionStatus::Skipped: return "skipped";
    }
    return "invalid";
}

bool parseSessionStatus(const std::string& value, SessionStatus& output)
{
    if (value == "pending") output = SessionStatus::Pending;
    else if (value == "running") output = SessionStatus::Running;
    else if (value == "paused") output = SessionStatus::Paused;
    else if (value == "completion_pending") output = SessionStatus::CompletionPending;
    else if (value == "completed") output = SessionStatus::Completed;
    else if (value == "aborted") output = SessionStatus::Aborted;
    else if (value == "skipped") output = SessionStatus::Skipped;
    else return false;
    return true;
}

const char* completionReasonName(CompletionReason value) noexcept
{
    switch (value) {
    case CompletionReason::None: return "none";
    case CompletionReason::Natural: return "natural";
    case CompletionReason::Manual: return "manual";
    case CompletionReason::Recovered: return "recovered";
    case CompletionReason::UserAborted: return "user_aborted";
    case CompletionReason::UserSkipped: return "user_skipped";
    }
    return "invalid";
}

bool parseCompletionReason(const std::string& value, CompletionReason& output)
{
    if (value == "none") output = CompletionReason::None;
    else if (value == "natural") output = CompletionReason::Natural;
    else if (value == "manual") output = CompletionReason::Manual;
    else if (value == "recovered") output = CompletionReason::Recovered;
    else if (value == "user_aborted") output = CompletionReason::UserAborted;
    else if (value == "user_skipped") output = CompletionReason::UserSkipped;
    else return false;
    return true;
}

const char* interruptionReasonName(InterruptionReason value) noexcept
{
    switch (value) {
    case InterruptionReason::UserPause: return "user_pause";
    case InterruptionReason::UserAway: return "user_away";
    case InterruptionReason::Meeting: return "meeting";
    case InterruptionReason::Emergency: return "emergency";
    case InterruptionReason::SystemLock: return "system_lock";
    case InterruptionReason::ApplicationClose: return "application_close";
    case InterruptionReason::NetworkOffline: return "network_offline";
    case InterruptionReason::Other: return "other";
    }
    return "invalid";
}

bool parseInterruptionReason(const std::string& value, InterruptionReason& output)
{
    if (value == "user_pause") output = InterruptionReason::UserPause;
    else if (value == "user_away") output = InterruptionReason::UserAway;
    else if (value == "meeting") output = InterruptionReason::Meeting;
    else if (value == "emergency") output = InterruptionReason::Emergency;
    else if (value == "system_lock") output = InterruptionReason::SystemLock;
    else if (value == "application_close") output = InterruptionReason::ApplicationClose;
    else if (value == "network_offline") output = InterruptionReason::NetworkOffline;
    else if (value == "other") output = InterruptionReason::Other;
    else return false;
    return true;
}

const char* interruptionSourceName(InterruptionSource value) noexcept
{
    switch (value) {
    case InterruptionSource::User: return "user";
    case InterruptionSource::System: return "system";
    case InterruptionSource::Application: return "application";
    }
    return "invalid";
}

bool parseInterruptionSource(const std::string& value, InterruptionSource& output)
{
    if (value == "user") output = InterruptionSource::User;
    else if (value == "system") output = InterruptionSource::System;
    else if (value == "application") output = InterruptionSource::Application;
    else return false;
    return true;
}

bool parseInterruption(const std::vector<std::string>& fields, InterruptionEvent& event)
{
    // I <reason> <source> <occurredAtUtcMs> <detectedAtUtcMs> <note>
    return fields.size() == 6
        && parseInterruptionReason(fields[1], event.reason)
        && parseInterruptionSource(fields[2], event.source)
        && parseInteger(fields[3], event.occurredAtUtcMs)
        && parseInteger(fields[4], event.detectedAtUtcMs)
        && decodeField(fields[5], event.note);
}

bool parseTask(const std::vector<std::string>& fields, TaskRecord& task)
{
    const bool baseParsed = (fields.size() == 10 || fields.size() == 13)
        && decodeField(fields[1], task.id)
        && decodeField(fields[2], task.title)
        && parseTaskStatus(fields[3], task.status)
        && parseInteger(fields[4], task.estimatedPomodoros)
        && parseInteger(fields[5], task.completedPomodoros)
        && parseInteger(fields[6], task.sortOrder)
        && parseInteger(fields[7], task.revision)
        && parseInteger(fields[8], task.createdAtUtcMs)
        && parseInteger(fields[9], task.updatedAtUtcMs);
    if (!baseParsed || fields.size() == 10) return baseParsed;

    int focusMinutes = 0;
    if (!parseInteger(fields[10], focusMinutes)
        || !parseTaskSoundPreference(fields[11], task.execution.sound)
        || !decodeField(fields[12], task.execution.soundscapeId)) {
        return false;
    }
    if (focusMinutes == 0) task.execution.focusMinutes.reset();
    else task.execution.focusMinutes = focusMinutes;
    return true;
}

bool parseSession(const std::vector<std::string>& fields, FocusSessionRecord& session)
{
    const bool baseParsed = (fields.size() == 13 || fields.size() == 14)
        && decodeField(fields[1], session.id)
        && decodeOptionalString(fields[2], session.taskId)
        && decodeField(fields[3], session.titleSnapshot)
        && parseSessionType(fields[4], session.type)
        && parseInteger(fields[5], session.plannedDurationMs)
        && parseInteger(fields[6], session.startedAtUtcMs)
        && decodeOptionalInteger(fields[7], session.targetEndAtUtcMs)
        && parseInteger(fields[8], session.remainingMs)
        && parseSessionStatus(fields[9], session.status)
        && decodeOptionalInteger(fields[10], session.endedAtUtcMs)
        && parseCompletionReason(fields[11], session.completionReason)
        && decodeField(fields[12], session.idempotencyKey);
    if (!baseParsed || fields.size() == 13) return baseParsed;
    return decodeOptionalString(
        fields[13], session.soundscapeIdSnapshot);
}

bool parseSettingsRow(const std::vector<std::string>& fields, FocusSettings& settings)
{
    const bool baseParsed = (fields.size() == 8 || fields.size() == 9)
        && parseInteger(fields[1], settings.focusMinutes)
        && parseInteger(fields[2], settings.shortBreakMinutes)
        && parseInteger(fields[3], settings.longBreakMinutes)
        && parseInteger(fields[4], settings.longBreakEvery)
        && parseInteger(fields[5], settings.soundVolumePercent)
        && parseBool(fields[6], settings.autoStartBreak)
        && parseBool(fields[7], settings.launchAtLogin);
    if (!baseParsed || fields.size() == 8) return baseParsed;
    return decodeField(fields[8], settings.defaultSoundscapeId);
}

bool parseSnapshot(const std::vector<std::string>& fields, TimerSnapshot& snapshot)
{
    return fields.size() == 7
        && parseInteger(fields[1], snapshot.schemaVersion)
        && decodeField(fields[2], snapshot.sessionId)
        && parseSessionStatus(fields[3], snapshot.status)
        && parseInteger(fields[4], snapshot.savedAtUtcMs)
        && decodeOptionalInteger(fields[5], snapshot.targetEndAtUtcMs)
        && parseInteger(fields[6], snapshot.remainingMs);
}

bool parseStore(const std::filesystem::path& path,
                FocusData& data,
                std::string& message)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        message = "Could not open the focus store.";
        return false;
    }
    std::string line;
    if (!std::getline(input, line)) {
        message = "Focus store header or version is unsupported.";
        return false;
    }
    const std::string_view prefix(kHeaderPrefix);
    if (line.size() <= prefix.size()
        || line.compare(0, prefix.size(), prefix) != 0) {
        message = "Focus store header or version is unsupported.";
        return false;
    }
    int headerVersion = 0;
    const std::string versionText = line.substr(prefix.size());
    if (!parseInteger(versionText, headerVersion)
        || headerVersion < 1 || headerVersion > kCurrentSchemaVersion) {
        message = "Focus store header or version is unsupported.";
        return false;
    }

    data = FocusData{};
    data.schemaVersion = headerVersion;
    bool sawSettings = false;
    bool sawActive = false;
    bool sawSnapshot = false;
    std::size_t lineNumber = 1;
    FocusSessionRecord* currentSession = nullptr;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.empty()) continue;
        const auto fields = splitFields(line);
        if (fields.empty()) continue;

        bool parsed = false;
        if (fields[0] == "S" && !sawSettings) {
            parsed = parseSettingsRow(fields, data.settings);
            sawSettings = parsed;
        } else if (fields[0] == "T") {
            TaskRecord task;
            parsed = parseTask(fields, task);
            if (parsed) data.tasks.push_back(std::move(task));
        } else if (fields[0] == "F") {
            FocusSessionRecord session;
            parsed = parseSession(fields, session);
            if (parsed) {
                data.sessions.push_back(std::move(session));
                currentSession = &data.sessions.back();
            }
        } else if (fields[0] == "I") {
            if (headerVersion < 2 || currentSession == nullptr) {
                parsed = false;
            } else {
                InterruptionEvent event;
                parsed = parseInterruption(fields, event);
                if (parsed) currentSession->interruptions.push_back(std::move(event));
            }
        } else if (fields[0] == "A" && fields.size() == 2 && !sawActive) {
            parsed = decodeOptionalString(fields[1], data.activeSessionId);
            sawActive = parsed;
        } else if (fields[0] == "R" && !sawSnapshot) {
            TimerSnapshot snapshot;
            parsed = parseSnapshot(fields, snapshot);
            if (parsed) {
                data.timerSnapshot = std::move(snapshot);
                sawSnapshot = true;
            }
        }
        if (!parsed) {
            message = "Malformed or duplicate record at line "
                + std::to_string(lineNumber) + '.';
            return false;
        }
    }
    if (!input.eof() || !sawSettings || !sawActive) {
        message = "Focus store ended before required records were read.";
        return false;
    }
    return true;
}

bool writeStore(const std::filesystem::path& path,
                const FocusData& data,
                std::string& message)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        message = "Could not create the temporary focus store.";
        return false;
    }
    output << kHeaderPrefix << kCurrentSchemaVersion << '\n'
           << "S\t" << data.settings.focusMinutes
           << '\t' << data.settings.shortBreakMinutes
           << '\t' << data.settings.longBreakMinutes
           << '\t' << data.settings.longBreakEvery
           << '\t' << data.settings.soundVolumePercent
           << '\t' << (data.settings.autoStartBreak ? 1 : 0)
           << '\t' << (data.settings.launchAtLogin ? 1 : 0)
           << '\t' << encodeField(data.settings.defaultSoundscapeId) << '\n';
    for (const auto& task : data.tasks) {
        output << "T\t" << encodeField(task.id)
               << '\t' << encodeField(task.title)
               << '\t' << taskStatusName(task.status)
               << '\t' << task.estimatedPomodoros
               << '\t' << task.completedPomodoros
               << '\t' << task.sortOrder
               << '\t' << task.revision
               << '\t' << task.createdAtUtcMs
               << '\t' << task.updatedAtUtcMs
               << '\t' << task.execution.focusMinutes.value_or(0)
               << '\t' << taskSoundPreferenceName(task.execution.sound)
               << '\t' << encodeField(task.execution.soundscapeId) << '\n';
    }
    for (const auto& session : data.sessions) {
        output << "F\t" << encodeField(session.id)
               << '\t' << encodeOptionalString(session.taskId)
               << '\t' << encodeField(session.titleSnapshot)
               << '\t' << sessionTypeName(session.type)
               << '\t' << session.plannedDurationMs
               << '\t' << session.startedAtUtcMs
               << '\t' << encodeOptionalInteger(session.targetEndAtUtcMs)
               << '\t' << session.remainingMs
               << '\t' << sessionStatusName(session.status)
               << '\t' << encodeOptionalInteger(session.endedAtUtcMs)
               << '\t' << completionReasonName(session.completionReason)
               << '\t' << encodeField(session.idempotencyKey)
               << '\t' << encodeOptionalString(
                    session.soundscapeIdSnapshot) << '\n';
        for (const auto& event : session.interruptions) {
            output << "I\t" << interruptionReasonName(event.reason)
                   << '\t' << interruptionSourceName(event.source)
                   << '\t' << event.occurredAtUtcMs
                   << '\t' << event.detectedAtUtcMs
                   << '\t' << encodeField(event.note) << '\n';
        }
    }
    output << "A\t" << encodeOptionalString(data.activeSessionId) << '\n';
    if (data.timerSnapshot) {
        const auto& snapshot = *data.timerSnapshot;
        output << "R\t" << snapshot.schemaVersion
               << '\t' << encodeField(snapshot.sessionId)
               << '\t' << sessionStatusName(snapshot.status)
               << '\t' << snapshot.savedAtUtcMs
               << '\t' << encodeOptionalInteger(snapshot.targetEndAtUtcMs)
               << '\t' << snapshot.remainingMs << '\n';
    }
    output.flush();
    if (!output) {
        message = "Could not finish writing the temporary focus store.";
        return false;
    }
    output.close();
    if (!output) {
        message = "Could not close the temporary focus store.";
        return false;
    }
    return true;
}

} // namespace

FileFocusRepository::FileFocusRepository(std::filesystem::path filePath)
    : filePath_(std::move(filePath))
{
}

const std::filesystem::path& FileFocusRepository::filePath() const noexcept
{
    return filePath_;
}

RepositoryLoadResult FileFocusRepository::load() const
{
    RepositoryLoadResult result;
    std::error_code error;
    bool recoveredBackup = false;
    if (!std::filesystem::exists(filePath_, error)) {
        if (error) {
            result.status = RepositoryLoadStatus::IoError;
            result.message = "Could not inspect the focus store: " + error.message();
            return result;
        }
        const auto backup = backupPath();
        if (!std::filesystem::exists(backup, error)) {
            result.status = error ? RepositoryLoadStatus::IoError
                                  : RepositoryLoadStatus::Missing;
            if (error) result.message = "Could not inspect the focus backup: " + error.message();
            return result;
        }
        std::filesystem::rename(backup, filePath_, error);
        if (error) {
            result.status = RepositoryLoadStatus::IoError;
            result.message = "Could not restore the focus backup: " + error.message();
            return result;
        }
        recoveredBackup = true;
    }

    if (!parseStore(filePath_, result.data, result.message)) {
        result.recoveryPath = nextCorruptPath();
        std::filesystem::rename(filePath_, result.recoveryPath, error);
        if (error) {
            result.status = RepositoryLoadStatus::IoError;
            result.message += " The original file could not be preserved: " + error.message();
            result.recoveryPath.clear();
            return result;
        }
        result.status = RepositoryLoadStatus::RejectedCorrupt;
        return result;
    }

    if (result.data.schemaVersion != kCurrentSchemaVersion) {
        const auto migrator = makeDefaultMigrator();
        const int originalVersion = result.data.schemaVersion;
        auto migration = migrator.migrateToCurrent(std::move(result.data));
        if (auto* err = std::get_if<MigrationError>(&migration)) {
            result.data = FocusData{};
            result.status = RepositoryLoadStatus::MigrationFailed;
            result.message =
                "Focus store schema " + std::to_string(originalVersion)
                + " could not be migrated to "
                + std::to_string(kCurrentSchemaVersion) + ": " + err->message;
            return result;
        }
        result.data = std::move(std::get<FocusData>(migration));
    }

    result.validation = validateFocusData(result.data);
    if (!result.validation.ok()) {
        result.message = "Focus store records violate domain invariants.";
        result.recoveryPath = nextCorruptPath();
        std::filesystem::rename(filePath_, result.recoveryPath, error);
        if (error) {
            result.status = RepositoryLoadStatus::IoError;
            result.message += " The original file could not be preserved: " + error.message();
            result.recoveryPath.clear();
            return result;
        }
        result.status = RepositoryLoadStatus::RejectedCorrupt;
        result.data = FocusData{};
        return result;
    }

    result.status = recoveredBackup ? RepositoryLoadStatus::RecoveredBackup
                                    : RepositoryLoadStatus::Loaded;
    return result;
}

RepositoryWriteResult FileFocusRepository::save(const FocusData& data)
{
    ValidationReport report = validateFocusData(data);
    if (!report.ok()) {
        return {RepositoryWriteStatus::ValidationRejected, std::move(report),
                "Focus data failed repository validation."};
    }

    std::error_code error;
    const auto parent = filePath_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return {RepositoryWriteStatus::IoError, {},
                    "Could not create the focus data directory: " + error.message()};
        }
    }

    const auto temporary = temporaryPath();
    const auto backup = backupPath();
    std::filesystem::remove(temporary, error);
    error.clear();
    std::string message;
    if (!writeStore(temporary, data, message)) {
        return {RepositoryWriteStatus::IoError, {}, std::move(message)};
    }

    const bool hadOriginal = std::filesystem::exists(filePath_, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return {RepositoryWriteStatus::IoError, {},
                "Could not inspect the current focus store: " + error.message()};
    }
    if (hadOriginal) {
        std::filesystem::remove(backup, error);
        error.clear();
        std::filesystem::rename(filePath_, backup, error);
        if (error) {
            std::filesystem::remove(temporary, error);
            return {RepositoryWriteStatus::IoError, {},
                    "Could not stage the current focus store for replacement: "
                        + error.message()};
        }
    }

    std::filesystem::rename(temporary, filePath_, error);
    if (error) {
        const std::string replacementError = error.message();
        error.clear();
        if (hadOriginal) std::filesystem::rename(backup, filePath_, error);
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        return {RepositoryWriteStatus::IoError, {},
                "Could not publish the new focus store: " + replacementError};
    }
    if (hadOriginal) {
        std::filesystem::remove(backup, error);
    }
    return {};
}

std::filesystem::path FileFocusRepository::temporaryPath() const
{
    return std::filesystem::path(filePath_.string() + ".tmp");
}

std::filesystem::path FileFocusRepository::backupPath() const
{
    return std::filesystem::path(filePath_.string() + ".bak");
}

std::filesystem::path FileFocusRepository::nextCorruptPath() const
{
    std::filesystem::path candidate(filePath_.string() + ".corrupt");
    std::error_code error;
    if (!std::filesystem::exists(candidate, error)) return candidate;
    for (int suffix = 1; suffix < std::numeric_limits<int>::max(); ++suffix) {
        candidate = std::filesystem::path(
            filePath_.string() + ".corrupt." + std::to_string(suffix));
        error.clear();
        if (!std::filesystem::exists(candidate, error)) return candidate;
    }
    return std::filesystem::path(filePath_.string() + ".corrupt.last");
}

} // namespace whatsui::focus_tomato
