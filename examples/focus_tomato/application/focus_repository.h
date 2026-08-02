#pragma once

#include <string>

#include "../domain/focus_data.h"
#include "../domain/focus_data_validator.h"

namespace whatsui::focus_tomato {

enum class RepositoryWriteStatus {
    Saved,
    ValidationRejected,
    IoError,
};

struct RepositoryWriteResult {
    RepositoryWriteStatus status{RepositoryWriteStatus::Saved};
    ValidationReport validation;
    std::string message;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == RepositoryWriteStatus::Saved;
    }
};

// Persistence is an application boundary. Implementations must validate again
// before writing, so future callers cannot bypass domain invariants by skipping
// FocusDataService.
class FocusRepository {
public:
    virtual ~FocusRepository() = default;
    [[nodiscard]] virtual RepositoryWriteResult save(const FocusData& data) = 0;
};

} // namespace whatsui::focus_tomato
