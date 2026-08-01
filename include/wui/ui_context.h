#pragma once

#include <functional>
#include <memory>
#include <string>

namespace wui {

namespace detail {
class UiDispatchCore;
}

enum class DispatchResult {
    Scheduled,
    Stopped,
};

enum class UiDiagnosticCode {
    DuplicateAutomationId,
    InvalidNodeKey,
    MissingAccessibleName,
    LifecycleCallbackException,
    WrongThreadMutation,
};

struct UiDiagnostic {
    UiDiagnosticCode code{UiDiagnosticCode::WrongThreadMutation};
    std::string message;
    std::string debugName;
};

using UiDiagnosticHandler = std::function<void(const UiDiagnostic&)>;

// A copyable, thread-safe handle to one UI execution context. It deliberately
// exposes no application or platform details, so view models can publish UI
// work without retaining UiApp, a window, or a native event-loop object.
class UiContext {
public:
    using Task = std::function<void()>;

    UiContext() noexcept = default;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool isAlive() const noexcept;
    [[nodiscard]] bool isCurrentThread() const noexcept;

    [[nodiscard]] DispatchResult post(Task task) const;
    void requireCurrentThread() const;
    void reportDiagnostic(UiDiagnostic diagnostic) const noexcept;

private:
    explicit UiContext(std::shared_ptr<detail::UiDispatchCore> core) noexcept;

    std::shared_ptr<detail::UiDispatchCore> core_;

    friend class UiDispatcher;
};

} // namespace wui
