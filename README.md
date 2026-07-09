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

## GitHub 配置

仓库当前已经带有：

- GitHub Actions CI 工作流
- Issue 模板
- Pull Request 模板

如果后续要补 `LICENSE`，建议再单独明确许可证选择，而不是默认替你做法律层面的假设。
