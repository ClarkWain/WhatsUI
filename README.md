# WhatsUI

[![CI](https://github.com/ClarkWain/WhatsUI/actions/workflows/ci.yml/badge.svg)](https://github.com/ClarkWain/WhatsUI/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](CMakeLists.txt)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C.svg)](CMakeLists.txt)

WhatsUI 是一个基于 WhatsCanvas 的轻量级原生 UI 系统实验项目，目标场景是桌面工具界面、设置面板、调试器面板、启动器和嵌入式 UI。

当前仓库包含：

- `doc/whatsui/` 下的架构文档、ADR 和专题设计文档
- `include/wui/` 下的公开 API 骨架
- `src/whatsui/` 下的运行时、控件、输入与文本输入最小实现
- `tests/` 下的基础 smoke tests

## 构建

首次克隆建议直接递归拉取 submodule：

```powershell
git clone --recursive https://github.com/ClarkWain/WhatsUI.git
```

如果已经克隆过，再补拉依赖：

```powershell
git submodule update --init --recursive
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Sanitizer validation

Runtime sanitizers are opt-in so ordinary Debug and release builds keep their
usual flags.  On Clang and GCC, the option enables AddressSanitizer and
UndefinedBehaviorSanitizer; on Windows/MSVC it enables the supported
AddressSanitizer equivalent (MSVC does not ship UBSan).  CI runs both the
Windows/MSVC ASan test suite and the headless Linux/Clang ASan+UBSan suite.
For MSVC test executables, CMake copies the matching ASan runtime DLL next to
the binary so `ctest` does not depend on a developer's `PATH`.

```powershell
# Windows / Visual Studio: AddressSanitizer
cmake -S . -B build-asan -DWHATSUI_ENABLE_SANITIZERS=ON
cmake --build build-asan --config Debug
ctest --test-dir build-asan -C Debug --output-on-failure
```

```bash
# Linux or macOS with Clang/GCC: AddressSanitizer + UBSan
cmake -S . -B build-sanitizers -DCMAKE_BUILD_TYPE=Debug -DWHATSUI_ENABLE_SANITIZERS=ON
cmake --build build-sanitizers --parallel
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \\
  ctest --test-dir build-sanitizers --output-on-failure
```

The sanitizer configuration intentionally covers the headless runtime only.
Do not combine it with `WHATSUI_WITH_WHATSCANVAS=ON` until that third-party
rendering dependency has its own supported sanitizer matrix.

## 安装与外部 CMake 消费（Developer Preview）

当前 Developer Preview 提供无渲染后端的核心包，可安装并通过
`find_package(WhatsUI CONFIG REQUIRED)` 被其他 CMake 项目使用：

```powershell
cmake -S . -B build-package -DWHATSUI_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX="$PWD/install"
cmake --build build-package --config Debug
cmake --install build-package --config Debug
cmake -S tests/package_consumer -B build-consumer -DCMAKE_PREFIX_PATH="$PWD/install"
cmake --build build-consumer --config Debug
& .\build-consumer\Debug\whatsui_consumer_smoke.exe
```

消费者只需链接导出的目标：

```cmake
find_package(WhatsUI 0.1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE WhatsUI::WhatsUI)
```

此包目前不包含 `WHATSUI_WITH_WHATSCANVAS=ON` 的渲染/GLFW 集成；该集成仍是
源码树内的实验性接线，需按照下一节从源构建。

## WhatsCanvas

`third_party/WhatsCanvas` 以 Git submodule 形式接入：

```powershell
git submodule update --init --recursive
```

启用与 WhatsCanvas 的真实绘制接线时，还需要确保其子模块已完整初始化。

```powershell
cmake -S . -B build-wsc -DWHATSUI_WITH_WHATSCANVAS=ON -DWHATSUI_BUILD_TESTS=OFF
cmake --build build-wsc --config Debug
```

### Todo demo

When examples are enabled, the deterministic Software capture and interactive
GLFW Todo demo are available as `WhatsUITodoApp` and `WhatsUITodoGlfw`.
See [Todo demo delivery](doc/whatsui/TODO_DEMO_DELIVERY.md) for commands and
the fixed visual-regression scene set.

## GitHub 配置

仓库当前已经带有：

- GitHub Actions CI 工作流
- Issue 模板
- Pull Request 模板

如果后续要补 `LICENSE`，建议再单独明确许可证选择，而不是默认替你做法律层面的假设。
