#pragma once

// GLFW platform backend for WhatsUI.
//
// Provides a concrete PlatformHost implementation using GLFW + WhatsCanvas OpenGL.
// Two usage modes:
//
// 1. Full control:
//   auto host = wui::createGlfwPlatformHost();
//   wui::UiApp app(std::move(host));
//   auto& window = app.openWindow("Hello", {800, 600});
//   window.setRoot(buildUi());
//   return wui::runGlfwUiApp(app);
//
// 2. One-liner (for simple apps):
//   return wui::runGlfwApp("Title", {800, 600},
//                          wui::Text("Hello"));

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "wui/node.h"
#include "wui/app.h"
#include "wui/platform.h"
#include "wui/types.h"

namespace wui {

// Create a GLFW-backed PlatformHost. Call once at program start.
[[nodiscard]] std::unique_ptr<PlatformHost> createGlfwPlatformHost();

// Returns true only for a host created by createGlfwPlatformHost(). This is
// primarily useful for full-control application wrappers that accept an
// abstract PlatformHost but require the GLFW frame driver.
[[nodiscard]] bool isGlfwPlatformHost(const PlatformHost* host) noexcept;

// Drive an existing UiApp created with createGlfwPlatformHost(). This installs
// the same input bridge and frame pipeline used by the convenience overloads.
// Windows and their UI trees remain owned by the caller's UiApp.
int runGlfwUiApp(UiApp& app);

// Convenience: create a single-window app and run the event loop.
// Returns the exit code (0 on normal close).
int runGlfwApp(std::string title, SizeF size, std::unique_ptr<Node> root);

// Variant for applications whose root needs access to its window, for example
// to present a modal confirmation dialog from a declarative control callback.
using GlfwRootFactory = std::function<std::unique_ptr<Node>(UiWindow&)>;
int runGlfwApp(std::string title, SizeF size, GlfwRootFactory rootFactory);

namespace detail {

template <class Factory, class = void>
struct IsGlfwViewFactory : std::false_type {
};

template <class Factory>
struct IsGlfwViewFactory<
    Factory,
    std::void_t<std::invoke_result_t<std::decay_t<Factory>&, UiWindow&>>>
    : std::bool_constant<isViewLikeV<
          std::invoke_result_t<std::decay_t<Factory>&, UiWindow&>>> {
};

} // namespace detail

// Declarative root overload. Application code passes a Builder, Component,
// View, or concrete unique_ptr<NodeT>; the platform boundary owns materialization.
template <
    class Content,
    std::enable_if_t<isViewLikeV<Content>, int> = 0>
int runGlfwApp(std::string title, SizeF size, Content&& content)
{
    return runGlfwApp(
        std::move(title),
        size,
        detail::materialize(std::forward<Content>(content)));
}

// Window-aware declarative factory. A shared one-shot holder keeps even a
// move-only factory compatible with the type-erased GlfwRootFactory boundary.
template <
    class Factory,
    std::enable_if_t<
        !isViewLikeV<Factory>
        && detail::IsGlfwViewFactory<Factory>::value
        && std::is_constructible_v<std::decay_t<Factory>, Factory&&>,
        int> = 0>
int runGlfwApp(std::string title, SizeF size, Factory&& factory)
{
    auto retained = std::make_shared<std::decay_t<Factory>>(
        std::forward<Factory>(factory));
    return runGlfwApp(
        std::move(title),
        size,
        GlfwRootFactory(
            [retained = std::move(retained)](UiWindow& window) mutable {
                return detail::materialize(std::invoke(*retained, window));
            }));
}

} // namespace wui
