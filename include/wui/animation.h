#pragma once

// Animation/Ticker framework (WHATSUI_ARCHITECTURE extension).
//
// Provides a frame-driven animation system that integrates with the runtime
// frame pipeline. Animations are driven by a Ticker that ticks each frame,
// advancing all active animations by the elapsed time.
//
// Usage:
//   auto& ticker = wui::Ticker::instance();
//   auto anim = wui::Animation(0.3f, [widget](float t) {
//       widget->setOpacity(t);  // 0→1 over 300ms
//   });
//   ticker.add(std::move(anim));
//
// The runtime calls `Ticker::tick(dt)` once per frame before layout.

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

namespace wui {

// Easing function type: takes normalized time [0,1], returns curved value.
using EasingFn = std::function<float(float)>;

// Built-in easing functions.
namespace easing {

inline float linear(float t) noexcept { return t; }

inline float easeInQuad(float t) noexcept { return t * t; }

inline float easeOutQuad(float t) noexcept { return t * (2.0f - t); }

inline float easeInOutQuad(float t) noexcept
{
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

inline float easeInCubic(float t) noexcept { return t * t * t; }

inline float easeOutCubic(float t) noexcept
{
    const float u = t - 1.0f;
    return u * u * u + 1.0f;
}

inline float easeInOutCubic(float t) noexcept
{
    return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
}

} // namespace easing

// Represents a single animation with duration, easing, and update callback.
class Animation {
public:
    using UpdateFn = std::function<void(float value)>;
    using CompleteFn = std::function<void()>;

    // Create an animation with the given duration (seconds) and update callback.
    // The update callback receives the eased value in [0, 1].
    Animation(float durationSeconds, UpdateFn onUpdate, EasingFn easing = easing::linear)
        : duration_(durationSeconds)
        , onUpdate_(std::move(onUpdate))
        , easing_(std::move(easing))
    {
    }

    // Set a completion callback that fires when the animation finishes.
    Animation& onComplete(CompleteFn callback)
    {
        onComplete_ = std::move(callback);
        return *this;
    }

    // Set repeat count (0 = play once, >0 = repeat N additional times,
    // -1 = infinite).
    Animation& repeat(int count) noexcept
    {
        repeatCount_ = count;
        repeatsRemaining_ = count;
        return *this;
    }

    // Set whether to reverse on each repeat (ping-pong).
    Animation& reverseOnRepeat(bool reverse) noexcept
    {
        reverseOnRepeat_ = reverse;
        return *this;
    }

    // Advance the animation by dt seconds. Returns true if still active.
    bool tick(float dt)
    {
        if (finished_) {
            return false;
        }

        elapsed_ += dt;

        if (elapsed_ >= duration_) {
            if (repeatsRemaining_ > 0 || repeatCount_ < 0) {
                // Repeat
                elapsed_ -= duration_;
                if (repeatsRemaining_ > 0) {
                    --repeatsRemaining_;
                }
                if (reverseOnRepeat_) {
                    reversed_ = !reversed_;
                }
            } else {
                // Finished
                elapsed_ = duration_;
                finished_ = true;
                const float finalT = reversed_ ? 0.0f : 1.0f;
                if (onUpdate_) {
                    onUpdate_(finalT);
                }
                if (onComplete_) {
                    onComplete_();
                }
                return false;
            }
        }

        float t = duration_ > 0.0f ? elapsed_ / duration_ : 1.0f;
        if (reversed_) {
            t = 1.0f - t;
        }
        const float easedT = easing_ ? easing_(t) : t;
        if (onUpdate_) {
            onUpdate_(easedT);
        }
        return true;
    }

    [[nodiscard]] bool isFinished() const noexcept { return finished_; }
    [[nodiscard]] float elapsed() const noexcept { return elapsed_; }
    [[nodiscard]] float duration() const noexcept { return duration_; }

private:
    float duration_{0.0f};
    float elapsed_{0.0f};
    UpdateFn onUpdate_;
    EasingFn easing_;
    CompleteFn onComplete_;
    int repeatCount_{0};
    int repeatsRemaining_{0};
    bool reverseOnRepeat_{false};
    bool reversed_{false};
    bool finished_{false};
};

// Unique handle for tracking animations in the Ticker.
using AnimationId = std::size_t;

// Frame-driven animation ticker. Manages a collection of active animations.
// Call tick(dt) once per frame (before layout) to advance all animations.
class Ticker {
public:
    // Global singleton (UI-thread only, like State<T>).
    static Ticker& instance() noexcept
    {
        static Ticker ticker;
        return ticker;
    }

    // Add an animation and return its ID for later cancellation.
    [[nodiscard]] AnimationId add(Animation animation)
    {
        const auto id = nextId_++;
        entries_.push_back({id, std::move(animation)});
        return id;
    }

    // Cancel a running animation by ID.
    void cancel(AnimationId id)
    {
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(),
                           [id](const Entry& e) { return e.id == id; }),
            entries_.end());
    }

    // Advance all animations by dt seconds. Removes finished ones.
    void tick(float dt)
    {
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(),
                           [dt](Entry& e) { return !e.animation.tick(dt); }),
            entries_.end());
    }

    // Whether any animations are currently running.
    [[nodiscard]] bool hasActive() const noexcept
    {
        return !entries_.empty();
    }

    // Number of active animations.
    [[nodiscard]] std::size_t activeCount() const noexcept
    {
        return entries_.size();
    }

    // Cancel all active animations.
    void cancelAll() noexcept
    {
        entries_.clear();
    }

private:
    Ticker() = default;

    struct Entry {
        AnimationId id{0};
        Animation animation;
    };

    AnimationId nextId_{1};
    std::vector<Entry> entries_;
};

} // namespace wui
