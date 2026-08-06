#include "migration_transaction.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

using whatsui::focus_tomato::MigrationTransaction;
using whatsui::focus_tomato::migrationBackupPath;

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

std::filesystem::path uniqueTempDir()
{
    std::mt19937_64 rng{std::random_device{}()};
    for (int attempt = 0; attempt < 8; ++attempt) {
        auto candidate = std::filesystem::temp_directory_path()
            / ("wui-migtx-" + std::to_string(rng()));
        std::error_code ec;
        if (std::filesystem::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    throw std::runtime_error("unable to create unique temp dir");
}

class ScopedTempDir {
public:
    ScopedTempDir() : path_(uniqueTempDir()) {}
    ~ScopedTempDir() noexcept
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    ScopedTempDir(const ScopedTempDir&) = delete;
    ScopedTempDir& operator=(const ScopedTempDir&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void writeTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    if (!out) throw std::runtime_error("write file failed: " + path.string());
}

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("read file failed: " + path.string());
    std::string content(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    return content;
}

void testBackupPathHelper()
{
    const auto path = migrationBackupPath("focus_data.jsonl", 1);
    expect(path.filename() == "focus_data.jsonl.pre-migration-v1.bak",
           "backup filename must follow the ADR-010 convention");
}

void testMissingTargetReportsNotFound()
{
    ScopedTempDir dir;
    MigrationTransaction tx(dir.path() / "focus_data.jsonl", 1);
    std::error_code ec;
    expect(!tx.prepareBackup(ec), "missing target must not report success");
    expect(ec == std::errc::no_such_file_or_directory,
           "missing target must map to no_such_file_or_directory");
}

void testBackupCreatedForExistingTarget()
{
    ScopedTempDir dir;
    const auto target = dir.path() / "focus_data.jsonl";
    writeTextFile(target, "v1-payload");

    MigrationTransaction tx(target, 1);
    std::error_code ec;
    expect(tx.prepareBackup(ec), "existing target must be backed up");
    expect(!ec, "no error expected on successful backup");
    expect(std::filesystem::exists(tx.backupPath()),
           ".bak sidecar must exist");
    expect(readTextFile(tx.backupPath()) == "v1-payload",
           ".bak contents must match target");
    expect(tx.backupPath()
            == migrationBackupPath(target, 1),
           "backup path must use the version-tagged filename");
}

void testRepeatedPrepareIsIdempotent()
{
    ScopedTempDir dir;
    const auto target = dir.path() / "focus_data.jsonl";
    writeTextFile(target, "v1-payload");

    MigrationTransaction tx(target, 1);
    std::error_code ec;
    expect(tx.prepareBackup(ec), "first prepareBackup must succeed");
    expect(!ec, "no error on first prepareBackup");
    expect(tx.prepareBackup(ec),
           "second prepareBackup must succeed when .bak matches target");
    expect(!ec, "no error on second prepareBackup");
    expect(readTextFile(tx.backupPath()) == "v1-payload",
           ".bak must remain untouched between prepareBackup calls");
}

void testStalePreexistingBackupIsRejected()
{
    ScopedTempDir dir;
    const auto target = dir.path() / "focus_data.jsonl";
    writeTextFile(target, "v2-payload");
    // Simulate a previous, unrelated migration attempt leaving stale bytes.
    writeTextFile(migrationBackupPath(target, 1), "old-unrelated-bytes");

    MigrationTransaction tx(target, 1);
    std::error_code ec;
    expect(!tx.prepareBackup(ec),
           "stale .bak that disagrees with target must be rejected");
    expect(ec == std::errc::file_exists,
           "stale .bak must surface as file_exists to prompt user handling");
    expect(readTextFile(migrationBackupPath(target, 1))
            == "old-unrelated-bytes",
           "stale .bak must not be overwritten by prepareBackup");
}

void testRollbackKeepsBackup()
{
    ScopedTempDir dir;
    const auto target = dir.path() / "focus_data.jsonl";
    writeTextFile(target, "v1-payload");

    MigrationTransaction tx(target, 1);
    std::error_code ec;
    expect(tx.prepareBackup(ec), "prepareBackup must succeed");
    expect(!ec, "no error expected");
    tx.rollback();
    expect(std::filesystem::exists(tx.backupPath()),
           ".bak must survive an explicit rollback");
    expect(!tx.committed(),
           "rollback must clear the committed flag");
}

void testCommitDoesNotDeleteBackup()
{
    ScopedTempDir dir;
    const auto target = dir.path() / "focus_data.jsonl";
    writeTextFile(target, "v1-payload");

    {
        MigrationTransaction tx(target, 1);
        std::error_code ec;
        expect(tx.prepareBackup(ec), "prepareBackup must succeed");
        expect(!ec, "no error expected");
        tx.commit();
        expect(tx.committed(), "commit must flip the committed flag");
    }
    expect(std::filesystem::exists(migrationBackupPath(target, 1)),
           ".bak must survive commit + destruction (never auto-cleaned)");
}

void testEmptyTargetProducesEmptyBackup()
{
    ScopedTempDir dir;
    const auto target = dir.path() / "focus_data.jsonl";
    writeTextFile(target, "");

    MigrationTransaction tx(target, 1);
    std::error_code ec;
    expect(tx.prepareBackup(ec), "empty target must still be backed up");
    expect(!ec, "no error on empty target backup");
    expect(std::filesystem::exists(tx.backupPath()),
           "empty target backup file must exist");
    expect(std::filesystem::file_size(tx.backupPath()) == 0,
           "empty target backup must have size zero");
}

} // namespace

int main()
{
    try {
        testBackupPathHelper();
        testMissingTargetReportsNotFound();
        testBackupCreatedForExistingTarget();
        testRepeatedPrepareIsIdempotent();
        testStalePreexistingBackupIsRejected();
        testRollbackKeepsBackup();
        testCommitDoesNotDeleteBackup();
        testEmptyTargetProducesEmptyBackup();
    } catch (const std::exception& e) {
        std::cerr << "focus_tomato_migration_transaction_tests: "
                  << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
