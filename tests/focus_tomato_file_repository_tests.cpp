#include "file_focus_repository.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace whatsui::focus_tomato;

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
                / "whatsui_focus_tomato_file_repository_tests")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        error.clear();
        std::filesystem::create_directories(path_, error);
        if (error) throw std::runtime_error("Could not create isolated test directory.");
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

FocusData validData()
{
    FocusData data;
    data.settings.focusMinutes = 50;
    data.settings.autoStartBreak = true;
    data.settings.defaultSoundscapeId = "cafe";
    data.tasks.push_back({
        "task-%-1", "含有 Unicode\t与换行\n🍅", TaskStatus::Active,
        3, 0, 1024, 4, 1'000, 2'000,
    });
    data.tasks.push_back({
        "task-deleted-done", "已删除的已完成任务", TaskStatus::ArchivedDone,
        2, 0, 2048, 3, 1'000, 2'000,
    });
    data.tasks.front().execution = {
        40, TaskSoundPreference::Soundscape, "forest"};
    data.sessions.push_back({
        "session-1",
        std::string{"task-%-1"},
        "含有 Unicode\t与换行\n🍅",
        SessionType::Focus,
        50 * kMinuteMs,
        3'000,
        std::nullopt,
        20 * kMinuteMs,
        SessionStatus::Paused,
        std::nullopt,
        CompletionReason::None,
        "session-1",
        std::string{"forest"},
    });
    data.activeSessionId = "session-1";
    data.timerSnapshot = TimerSnapshot{
        kCurrentSchemaVersion,
        "session-1",
        SessionStatus::Paused,
        5'000,
        std::nullopt,
        20 * kMinuteMs,
    };
    return data;
}

void roundTripPreservesCompleteAggregate()
{
    TemporaryDirectory directory;
    FileFocusRepository repository(directory.path() / "nested" / "focus.store");
    expect(repository.load().status == RepositoryLoadStatus::Missing,
           "A new repository should report Missing with valid defaults");

    const FocusData expected = validData();
    expect(repository.save(expected).succeeded(), "A valid aggregate should save");
    const auto loaded = repository.load();
    expect(loaded.status == RepositoryLoadStatus::Loaded,
           "A saved aggregate should load normally");
    expect(loaded.validation.ok() && loaded.data == expected,
           "Round trip must preserve every record and pass post-read validation");
}

void legacyRowsReceiveSafeExecutionDefaults()
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "legacy.store";
    {
        std::ofstream output(path, std::ios::binary);
        output
            << "WhatsUIFocusTomatoStore\t1\n"
            << "S\t25\t5\t15\t4\t70\t0\t0\n"
            << "T\ttask-1\tLegacy\tactive\t1\t0\t1024\t1\t1000\t1000\n"
            << "A\t-\n";
    }
    FileFocusRepository repository(path);
    const auto loaded = repository.load();
    expect(loaded.status == RepositoryLoadStatus::Loaded
               && loaded.data.tasks.size() == 1,
           "Rows written before task preferences must remain readable");
    expect(!loaded.data.tasks.front().execution.focusMinutes
               && loaded.data.tasks.front().execution.sound
                    == TaskSoundPreference::Inherit
               && loaded.data.settings.defaultSoundscapeId == "rain",
           "Legacy rows must inherit the same duration and sound users previously saw");
}

void rejectedSaveLeavesLastGoodFileUntouched()
{
    TemporaryDirectory directory;
    FileFocusRepository repository(directory.path() / "focus.store");
    const FocusData expected = validData();
    expect(repository.save(expected).succeeded(), "Setup save should succeed");

    FocusData invalid = expected;
    invalid.sessions.front().idempotencyKey = "wrong";
    const auto rejected = repository.save(invalid);
    expect(rejected.status == RepositoryWriteStatus::ValidationRejected
               && rejected.validation.has(ValidationCode::InvalidIdempotencyKey),
           "Repository must independently reject invalid data with diagnostics");
    expect(repository.load().data == expected,
           "Rejected data must not replace the last good aggregate");
}

void malformedFileIsPreservedInsteadOfOverwritten()
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "focus.store";
    {
        std::ofstream output(path, std::ios::binary);
        output << "not a focus store\nprivate user bytes";
    }
    FileFocusRepository repository(path);
    const auto loaded = repository.load();
    expect(loaded.status == RepositoryLoadStatus::RejectedCorrupt,
           "Malformed input must enter an explicit corrupt-data state");
    expect(!loaded.recoveryPath.empty() && std::filesystem::exists(loaded.recoveryPath),
           "The original malformed bytes must remain available for recovery");
    expect(!std::filesystem::exists(path),
           "A corrupt source must not be mistaken for a newly initialized store");
}

void semanticallyInvalidFileReturnsValidationReport()
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "focus.store";
    {
        std::ofstream output(path, std::ios::binary);
        output
            << "WhatsUIFocusTomatoStore\t1\n"
            << "S\t25\t5\t15\t4\t70\t0\t0\n"
            << "T\ttask-1\tTitle\tactive\t1\t9\t1024\t1\t1000\t1000\n"
            << "A\t-\n";
    }
    FileFocusRepository repository(path);
    const auto loaded = repository.load();
    expect(loaded.status == RepositoryLoadStatus::RejectedCorrupt,
           "Well-formed but inconsistent records must still be quarantined");
    expect(loaded.validation.has(ValidationCode::TaskCompletionCacheMismatch),
           "Load result must expose semantic diagnostics to the recovery UI");
}

void interruptedReplacementRestoresCompleteBackup()
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "focus.store";
    FileFocusRepository repository(path);
    const FocusData expected = validData();
    expect(repository.save(expected).succeeded(), "Setup save should succeed");
    std::filesystem::rename(path, std::filesystem::path(path.string() + ".bak"));

    const auto loaded = repository.load();
    expect(loaded.status == RepositoryLoadStatus::RecoveredBackup,
           "An interrupted replace must restore its complete backup");
    expect(loaded.data == expected && std::filesystem::exists(path),
           "Recovered data must be valid and restored to the requested path");
}

} // namespace

int main()
{
    try {
        roundTripPreservesCompleteAggregate();
        legacyRowsReceiveSafeExecutionDefaults();
        rejectedSaveLeavesLastGoodFileUntouched();
        malformedFileIsPreservedInsteadOfOverwritten();
        semanticallyInvalidFileReturnsValidationReport();
        interruptedReplacementRestoresCompleteBackup();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
