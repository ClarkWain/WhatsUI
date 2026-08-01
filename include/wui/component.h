#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace wui {

// Own this token beside a page/component. Guarded callbacks become no-ops
// after invalidate() or destruction, so Nodes may safely outlive the object
// whose intent handler they captured.
class CallbackLifetime {
public:
    CallbackLifetime()
        : token_(std::make_shared<Token>())
    {
    }

    CallbackLifetime(CallbackLifetime&&) noexcept = default;
    CallbackLifetime& operator=(CallbackLifetime&&) noexcept = default;
    CallbackLifetime(const CallbackLifetime&) = delete;
    CallbackLifetime& operator=(const CallbackLifetime&) = delete;

    ~CallbackLifetime() { invalidate(); }

    void invalidate() noexcept { token_.reset(); }
    [[nodiscard]] bool isAlive() const noexcept
    {
        return static_cast<bool>(token_);
    }

    template <class Callback>
    [[nodiscard]] auto guard(Callback callback) const
    {
        std::weak_ptr<Token> weak = token_;
        return [weak, callback = std::move(callback)](auto&&... arguments) mutable {
            using Result = std::invoke_result_t<
                Callback&, decltype(arguments)...>;
            static_assert(
                std::is_void_v<Result>,
                "CallbackLifetime::guard supports intent callbacks returning void");
            auto lifetime = weak.lock();
            if (!lifetime) {
                return;
            }
            std::invoke(
                callback,
                std::forward<decltype(arguments)>(arguments)...);
        };
    }

private:
    struct Token {
    };

    std::shared_ptr<Token> token_;
};

} // namespace wui
