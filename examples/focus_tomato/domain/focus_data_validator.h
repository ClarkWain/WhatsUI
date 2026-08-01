#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "focus_data.h"

namespace whatsui::focus_tomato {

enum class ValidationSeverity {
    Warning,
    Error,
};

enum class ValidationCode {
    UnsupportedSchemaVersion,
    InvalidEnumValue,
    MissingId,
    DuplicateId,
    EmptyTaskTitle,
    EmptySessionTitleSnapshot,
    InvalidUtf8,
    ForbiddenTextControl,
    ValueOutOfRange,
    InvalidTimestampOrder,
    DuplicateActiveSortOrder,
    DanglingTaskReference,
    InvalidSessionState,
    InvalidCompletionReason,
    InvalidIdempotencyKey,
    MultipleActiveSessions,
    ActiveSessionIdMismatch,
    MissingTimerSnapshot,
    UnexpectedTimerSnapshot,
    TimerSnapshotMismatch,
    TaskCompletionCacheMismatch,
};

struct ValidationIssue {
    ValidationSeverity severity{ValidationSeverity::Error};
    ValidationCode code{ValidationCode::InvalidSessionState};
    std::string entityType;
    std::string entityId;
    std::string field;
    std::string message;
};

class ValidationReport {
public:
    void add(ValidationIssue issue);

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] bool has(ValidationCode code) const noexcept;
    [[nodiscard]] std::size_t errorCount() const noexcept;
    [[nodiscard]] std::size_t warningCount() const noexcept;
    [[nodiscard]] const std::vector<ValidationIssue>& issues() const noexcept;
    [[nodiscard]] std::string summary() const;

private:
    std::vector<ValidationIssue> issues_;
};

[[nodiscard]] const char* validationCodeName(ValidationCode code) noexcept;
[[nodiscard]] ValidationReport validateFocusData(const FocusData& data);

} // namespace whatsui::focus_tomato
