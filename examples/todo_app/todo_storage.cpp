#include "todo_storage.h"

#include <charconv>
#include <fstream>
#include <set>
#include <system_error>

namespace whatsui::todo {
namespace {

constexpr const char* kHeader = "WhatsUITodoStore\t1";

void setError(std::string* error, std::string message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

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

bool parseInteger(const std::string& value, int& output)
{
    if (value.empty()) return false;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}

bool parseFlag(const std::string& value, bool& output)
{
    if (value == "0") { output = false; return true; }
    if (value == "1") { output = true; return true; }
    return false;
}

bool parseLine(const std::string& line, TodoRecord& record)
{
    std::string fields[5];
    std::size_t start = 0;
    for (int field = 0; field < 4; ++field) {
        const auto separator = line.find('\t', start);
        if (separator == std::string::npos) return false;
        fields[field] = line.substr(start, separator - start);
        start = separator + 1;
    }
    fields[4] = line.substr(start);

    if (!parseInteger(fields[0], record.id)
        || !parseFlag(fields[1], record.completed)
        || !parseFlag(fields[2], record.important)) {
        return false;
    }
    if (fields[3] == "-") {
        record.dueDateIso.reset();
    } else if (!decodeField(fields[3], record.dueDateIso.emplace())) {
        return false;
    }
    return decodeField(fields[4], record.title);
}

bool parseStore(const std::filesystem::path& path, std::vector<TodoRecord>& records, std::string& message)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) { message = "could not open Todo store"; return false; }
    std::string line;
    if (!std::getline(input, line) || line != kHeader) { message = "unsupported or missing Todo store header"; return false; }
    std::set<int> ids;
    records.clear();
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        TodoRecord record;
        if (!parseLine(line, record) || record.id <= 0 || record.title.empty() || !ids.insert(record.id).second) {
            message = "malformed Todo record";
            records.clear();
            return false;
        }
        records.push_back(std::move(record));
    }
    if (!input.eof()) { message = "could not finish reading Todo store"; records.clear(); return false; }
    return true;
}

} // namespace

TodoStorage::TodoStorage(std::filesystem::path filePath)
    : filePath_(std::move(filePath))
{
}

const std::filesystem::path& TodoStorage::filePath() const noexcept { return filePath_; }
std::filesystem::path TodoStorage::temporaryPath() const { return filePath_.string() + ".tmp"; }
std::filesystem::path TodoStorage::backupPath() const { return filePath_.string() + ".bak"; }

std::filesystem::path TodoStorage::nextCorruptPath() const
{
    auto candidate = std::filesystem::path(filePath_.string() + ".corrupt");
    std::error_code error;
    for (unsigned index = 1; std::filesystem::exists(candidate, error) && !error; ++index) {
        candidate = std::filesystem::path(filePath_.string() + ".corrupt." + std::to_string(index));
    }
    return candidate;
}

TodoLoadResult TodoStorage::load() const
{
    TodoLoadResult result;
    if (filePath_.empty()) {
        result.status = TodoLoadStatus::IoError;
        result.message = "Todo store path is empty";
        return result;
    }

    std::error_code error;
    const auto backup = backupPath();
    if (!std::filesystem::exists(filePath_, error) && !error && std::filesystem::exists(backup, error) && !error) {
        std::filesystem::rename(backup, filePath_, error);
        if (error) {
            result.status = TodoLoadStatus::IoError;
            result.message = "could not restore Todo backup: " + error.message();
            return result;
        }
        result.status = TodoLoadStatus::RecoveredBackup;
    }
    if (error) {
        result.status = TodoLoadStatus::IoError;
        result.message = "could not inspect Todo store: " + error.message();
        return result;
    }
    if (!std::filesystem::exists(filePath_, error) && !error) {
        result.status = TodoLoadStatus::Missing;
        return result;
    }
    if (error) {
        result.status = TodoLoadStatus::IoError;
        result.message = "could not inspect Todo store: " + error.message();
        return result;
    }

    std::string message;
    if (parseStore(filePath_, result.records, message)) {
        if (result.status != TodoLoadStatus::RecoveredBackup) result.status = TodoLoadStatus::Loaded;
        return result;
    }

    const auto corrupt = nextCorruptPath();
    std::filesystem::rename(filePath_, corrupt, error);
    if (error) {
        result.status = TodoLoadStatus::IoError;
        result.message = "Todo store is malformed and could not be preserved: " + error.message();
        return result;
    }
    result.status = TodoLoadStatus::RecoveredMalformed;
    result.recoveryPath = corrupt;
    result.message = std::move(message);
    result.records.clear();
    return result;
}

bool TodoStorage::save(const std::vector<TodoRecord>& records, std::string* error) const
{
    if (filePath_.empty()) { setError(error, "Todo store path is empty"); return false; }
    std::set<int> ids;
    for (const auto& record : records) {
        if (record.id <= 0 || record.title.empty() || !ids.insert(record.id).second) {
            setError(error, "Todo records require positive unique IDs and non-empty titles");
            return false;
        }
    }

    std::error_code filesystemError;
    if (!filePath_.parent_path().empty()) std::filesystem::create_directories(filePath_.parent_path(), filesystemError);
    if (filesystemError) { setError(error, "could not create Todo storage directory: " + filesystemError.message()); return false; }

    const auto temporary = temporaryPath();
    const auto backup = backupPath();
    std::filesystem::remove(temporary, filesystemError);
    filesystemError.clear();
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) { setError(error, "could not create temporary Todo store"); return false; }
        output << kHeader << '\n';
        for (const auto& record : records) {
            output << record.id << '\t' << (record.completed ? '1' : '0') << '\t' << (record.important ? '1' : '0') << '\t'
                   << (record.dueDateIso ? encodeField(*record.dueDateIso) : "-") << '\t' << encodeField(record.title) << '\n';
        }
        output.flush();
        if (!output) { output.close(); std::filesystem::remove(temporary, filesystemError); setError(error, "could not write temporary Todo store"); return false; }
    }

    const bool hadOriginal = std::filesystem::exists(filePath_, filesystemError);
    if (filesystemError) { std::filesystem::remove(temporary, filesystemError); setError(error, "could not inspect existing Todo store"); return false; }
    if (hadOriginal) {
        std::filesystem::remove(backup, filesystemError);
        filesystemError.clear();
        std::filesystem::rename(filePath_, backup, filesystemError);
        if (filesystemError) { std::filesystem::remove(temporary, filesystemError); setError(error, "could not prepare Todo store replacement: " + filesystemError.message()); return false; }
    }
    std::filesystem::rename(temporary, filePath_, filesystemError);
    if (filesystemError) {
        if (hadOriginal) { std::error_code restoreError; std::filesystem::rename(backup, filePath_, restoreError); }
        std::filesystem::remove(temporary, filesystemError);
        setError(error, "could not commit Todo store replacement: " + filesystemError.message());
        return false;
    }
    if (hadOriginal) std::filesystem::remove(backup, filesystemError);
    return true;
}

} // namespace whatsui::todo
