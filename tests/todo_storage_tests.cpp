#include "todo_storage.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path() / "whatsui_todo_storage_tests")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_, error);
        if (error) throw std::runtime_error("could not create isolated test directory");
    }
    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    const std::filesystem::path& path() const noexcept { return path_; }
private:
    std::filesystem::path path_;
};

void roundTripAndMissing()
{
    TemporaryDirectory directory;
    whatsui::todo::TodoStorage storage(directory.path() / "todos.store");
    expect(storage.load().status == whatsui::todo::TodoLoadStatus::Missing, "new storage should be missing");

    const std::vector<whatsui::todo::TodoRecord> expected{
        {1, "Plan the release", false, true, std::string{"2026-07-12"}},
        {2, "含有 Unicode\tand newline\n", true, false, std::nullopt},
    };
    std::string error;
    expect(storage.save(expected, &error), "storage should write a complete store");
    const auto result = storage.load();
    expect(result.status == whatsui::todo::TodoLoadStatus::Loaded, "saved storage should load normally");
    expect(result.records == expected, "storage round trip should preserve every field");
}

void metadataReplacementRoundTrip()
{
    TemporaryDirectory directory;
    whatsui::todo::TodoStorage storage(directory.path() / "nested" / "todos.store");
    const std::vector<whatsui::todo::TodoRecord> initial{
        {11, "Initially dated", false, false, std::string{"2026-08-09"}},
        {12, "Initially important", true, true, std::nullopt},
    };
    expect(storage.save(initial), "metadata setup save should succeed");

    const std::vector<whatsui::todo::TodoRecord> updated{
        {11, "Initially dated", false, true, std::nullopt},
        {12, "Initially important", true, false, std::string{"2027-10-11"}},
    };
    std::string error = "stale error";
    expect(storage.save(updated, &error), "metadata replacement save should succeed");
    const auto loaded = storage.load();
    expect(loaded.status == whatsui::todo::TodoLoadStatus::Loaded,
           "a replaced metadata store should load normally");
    expect(loaded.records == updated,
           "replacement round trip should preserve changed important flags, added dates, and cleared dates exactly");
    expect(!std::filesystem::exists(std::filesystem::path(storage.filePath().string() + ".bak")),
           "a successful metadata replacement should not leave a stale backup");
}

void malformedStoreIsPreserved()
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "todos.store";
    { std::ofstream output(path, std::ios::binary); output << "not a todo store\n"; }
    whatsui::todo::TodoStorage storage(path);
    const auto result = storage.load();
    expect(result.status == whatsui::todo::TodoLoadStatus::RecoveredMalformed, "malformed stores should recover safely");
    expect(result.records.empty() && std::filesystem::exists(result.recoveryPath), "malformed source should remain available for recovery");
}

void backupIsRestored()
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "todos.store";
    whatsui::todo::TodoStorage storage(path);
    const std::vector<whatsui::todo::TodoRecord> expected{{7, "Recover me", false, false, std::nullopt}};
    expect(storage.save(expected), "setup save should succeed");
    std::filesystem::rename(path, std::filesystem::path(path.string() + ".bak"));
    const auto result = storage.load();
    expect(result.status == whatsui::todo::TodoLoadStatus::RecoveredBackup, "backup should recover an interrupted replacement");
    expect(result.records == expected && std::filesystem::exists(path), "recovered backup should be restored to the requested path");
}

} // namespace

int main()
{
    try {
        roundTripAndMissing();
        metadataReplacementRoundTrip();
        malformedStoreIsPreserved();
        backupIsRestored();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
