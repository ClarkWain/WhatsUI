#include "wui/ui_dispatcher.h"

#include "wui/thread_check.h"

#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace wui {

namespace detail {

class UiDispatchCore {
public:
    mutable std::mutex mutex;
    std::deque<UiContext::Task> tasks;
    UiDispatcher::WakeCallback wake;
    UiDiagnosticHandler diagnosticHandler;
    std::thread::id owner;
    bool bound{false};
    bool running{true};
};

} // namespace detail

UiContext::UiContext(std::shared_ptr<detail::UiDispatchCore> core) noexcept
    : core_(std::move(core))
{
}

bool UiContext::isValid() const noexcept
{
    return static_cast<bool>(core_);
}

bool UiContext::isAlive() const noexcept
{
    if (!core_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(core_->mutex);
    return core_->running;
}

bool UiContext::isCurrentThread() const noexcept
{
    if (!core_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(core_->mutex);
    return core_->running && core_->bound
        && core_->owner == std::this_thread::get_id();
}

DispatchResult UiContext::post(Task task) const
{
    if (!core_) {
        return DispatchResult::Stopped;
    }

    UiDispatcher::WakeCallback wake;
    {
        std::lock_guard<std::mutex> lock(core_->mutex);
        if (!core_->running) {
            return DispatchResult::Stopped;
        }
        if (!task) {
            return DispatchResult::Scheduled;
        }
        const bool wasEmpty = core_->tasks.empty();
        core_->tasks.push_back(std::move(task));
        if (wasEmpty) {
            wake = core_->wake;
        }
    }
    if (wake) {
        wake();
    }
    return DispatchResult::Scheduled;
}

void UiContext::requireCurrentThread() const
{
    if (!isCurrentThread()) {
        reportDiagnostic({
            UiDiagnosticCode::WrongThreadMutation,
            "UI mutation attempted outside its owning context",
            {},
        });
        throw std::logic_error(
            "This operation must run on its owning UI context");
    }
}

void UiContext::reportDiagnostic(UiDiagnostic diagnostic) const noexcept
{
    try {
        if (!core_) {
            return;
        }
        if (!isCurrentThread()) {
            (void)post([context = *this,
                        diagnostic = std::move(diagnostic)]() mutable {
                context.reportDiagnostic(std::move(diagnostic));
            });
            return;
        }

        UiDiagnosticHandler handler;
        {
            std::lock_guard<std::mutex> lock(core_->mutex);
            if (!core_->running) {
                return;
            }
            handler = core_->diagnosticHandler;
        }
        if (handler) {
            try {
                handler(diagnostic);
            } catch (...) {
                // Diagnostic observers must never change framework behavior.
            }
        }
    } catch (...) {
        // Reporting is best-effort and is safe from destructors/noexcept paths.
    }
}

UiDispatcher::UiDispatcher()
    : core_(std::make_shared<detail::UiDispatchCore>())
{
}

UiDispatcher::~UiDispatcher()
{
    std::lock_guard<std::mutex> lock(core_->mutex);
    core_->running = false;
    core_->tasks.clear();
    core_->wake = {};
}

void UiDispatcher::bindToCurrentThread()
{
    const std::thread::id current = std::this_thread::get_id();
    registerUiThread();
    {
        std::lock_guard<std::mutex> lock(core_->mutex);
        if (!core_->running) {
            throw std::logic_error("UiDispatcher is stopped");
        }
        if (core_->bound && core_->owner != current) {
            throw std::logic_error(
                "UiDispatcher is already bound to another thread");
        }
        core_->owner = current;
        core_->bound = true;
    }
}

bool UiDispatcher::isBound() const noexcept
{
    std::lock_guard<std::mutex> lock(core_->mutex);
    return core_->running && core_->bound;
}

bool UiDispatcher::isOwnerThread() const noexcept
{
    return context().isCurrentThread();
}

void UiDispatcher::setWakeCallback(WakeCallback callback)
{
    std::lock_guard<std::mutex> lock(core_->mutex);
    if (core_->running) {
        core_->wake = std::move(callback);
    }
}

void UiDispatcher::setDiagnosticHandler(UiDiagnosticHandler handler)
{
    std::lock_guard<std::mutex> lock(core_->mutex);
    if (core_->running) {
        core_->diagnosticHandler = std::move(handler);
    }
}

void UiDispatcher::post(Task task)
{
    (void)context().post(std::move(task));
}

UiContext UiDispatcher::context() const noexcept
{
    return UiContext(core_);
}

bool UiDispatcher::hasPending() const noexcept
{
    std::lock_guard<std::mutex> lock(core_->mutex);
    return core_->running && !core_->tasks.empty();
}

std::size_t UiDispatcher::drain()
{
    if (!isOwnerThread()) {
        throw std::logic_error(
            "UiDispatcher::drain must run on its owning UI thread");
    }

    constexpr std::size_t kMaximumTasksPerFrame = 4096;
    std::size_t completed = 0;
    while (completed < kMaximumTasksPerFrame) {
        Task task;
        {
            std::lock_guard<std::mutex> lock(core_->mutex);
            if (!core_->running || core_->tasks.empty()) {
                break;
            }
            task = std::move(core_->tasks.front());
            core_->tasks.pop_front();
        }
        if (task) {
            task();
            ++completed;
        }
    }

    if (hasPending()) {
        WakeCallback wake;
        {
            std::lock_guard<std::mutex> lock(core_->mutex);
            wake = core_->wake;
        }
        if (wake) wake();
    }
    return completed;
}

} // namespace wui
