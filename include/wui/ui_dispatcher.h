#pragma once

#include <cstddef>
#include <functional>
#include <memory>

#include "wui/ui_context.h"

namespace wui {

// Per-application bridge from worker threads to the owning UI thread.
// Tasks may be posted from any thread, but are always drained by the UI loop.
class UiDispatcher {
public:
    using Task = std::function<void()>;
    using WakeCallback = std::function<void()>;

    UiDispatcher();
    ~UiDispatcher();

    UiDispatcher(const UiDispatcher&) = delete;
    UiDispatcher& operator=(const UiDispatcher&) = delete;

    void bindToCurrentThread();
    [[nodiscard]] bool isBound() const noexcept;
    [[nodiscard]] bool isOwnerThread() const noexcept;

    void setWakeCallback(WakeCallback callback);
    void setDiagnosticHandler(UiDiagnosticHandler handler);
    void post(Task task);
    [[nodiscard]] UiContext context() const noexcept;
    [[nodiscard]] bool hasPending() const noexcept;
    std::size_t drain();

private:
    std::shared_ptr<detail::UiDispatchCore> core_;
};

} // namespace wui
