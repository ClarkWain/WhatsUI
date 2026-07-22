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
//   return wui::runGlfwApp("Title", {800, 600}, std::move(rootNode));

#include <memory>
#include <string>
#include <functional>

#include "wui/node.h"
#include "wui/app.h"
#include "wui/platform.h"
#include "wui/types.h"

namespace wui {

// Create a GLFW-backed PlatformHost. Call once at program start.
[[nodiscard]] std::unique_ptr<PlatformHost> createGlfwPlatformHost();

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

} // namespace wui
