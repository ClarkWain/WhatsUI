#include "migration_transaction.h"

#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace whatsui::focus_tomato {

std::filesystem::path migrationBackupPath(
    const std::filesystem::path& target, int fromVersion)
{
    auto backup = target;
    backup += ".pre-migration-v";
    backup += std::to_string(fromVersion);
    backup += ".bak";
    return backup;
}

MigrationTransaction::MigrationTransaction(
    std::filesystem::path targetFile, int fromVersion)
    : target_(std::move(targetFile))
    , backup_(migrationBackupPath(target_, fromVersion))
    , fromVersion_(fromVersion)
{
}

namespace {

std::vector<char> readWholeFile(
    const std::filesystem::path& path, std::error_code& ec)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        ec.assign(errno ? errno : 1, std::generic_category());
        return {};
    }
    input.seekg(0, std::ios::end);
    if (!input) {
        ec.assign(errno ? errno : 1, std::generic_category());
        return {};
    }
    const auto size = input.tellg();
    if (size < 0) {
        ec.assign(errno ? errno : 1, std::generic_category());
        return {};
    }
    input.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<std::size_t>(size));
    if (size > 0 && !input.read(buffer.data(), size)) {
        ec.assign(errno ? errno : 1, std::generic_category());
        return {};
    }
    ec.clear();
    return buffer;
}

bool writeWholeFile(
    const std::filesystem::path& path,
    const std::vector<char>& bytes,
    std::error_code& ec)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        ec.assign(errno ? errno : 1, std::generic_category());
        return false;
    }
    if (!bytes.empty()
        && !output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        ec.assign(errno ? errno : 1, std::generic_category());
        return false;
    }
    output.flush();
    if (!output) {
        ec.assign(errno ? errno : 1, std::generic_category());
        return false;
    }
    ec.clear();
    return true;
}

} // namespace

bool MigrationTransaction::prepareBackup(std::error_code& ec) noexcept
{
    ec.clear();
    if (!std::filesystem::exists(target_, ec)) {
        if (!ec) ec = std::make_error_code(std::errc::no_such_file_or_directory);
        return false;
    }
    if (ec) return false;

    if (std::filesystem::exists(backup_, ec)) {
        if (ec) return false;
        std::error_code compareEc;
        const auto existing = readWholeFile(backup_, compareEc);
        if (compareEc) {
            ec = compareEc;
            return false;
        }
        const auto current = readWholeFile(target_, compareEc);
        if (compareEc) {
            ec = compareEc;
            return false;
        }
        if (existing == current) return true;
        ec = std::make_error_code(std::errc::file_exists);
        return false;
    }
    if (ec) return false;

    auto bytes = readWholeFile(target_, ec);
    if (ec) return false;
    return writeWholeFile(backup_, bytes, ec);
}

} // namespace whatsui::focus_tomato
