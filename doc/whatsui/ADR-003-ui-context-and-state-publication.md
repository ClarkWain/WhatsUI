# ADR-003: UI Context And State Publication

状态：Accepted

## Context

WhatsUI 的窗口、节点、响应式状态和渲染资源属于 UI 执行上下文，后台线程不能直接修改这些对象。仅使用进程级 Debug 断言无法覆盖 Release，也无法表达应用退出后的投递结果。让每次状态更新显式接收 `UiDispatcher` 又会泄漏运行时基础设施，并允许调用方传错 Dispatcher。

## Decision

- `UiDispatcher` 拥有一个共享的执行核心；`UiContext` 是它对业务层暴露的轻量句柄。
- `UiApp::uiContext()` 返回 Context。App 创建时立即将 Dispatcher 绑定到当前 UI 线程，保证首屏构建阶段即可使用。
- Dispatcher 销毁时将执行核心置为 stopped，清空未执行任务；存活的 Context 会明确返回 `DispatchResult::Stopped`。
- 需要后台发布能力的 `State<T>` 在构造时绑定 `UiContext`。
- `set` 同步执行并始终验证所属 Context；`post` 可从任意线程调用，并把更新投递到所属 Context。
- `State<T>` 是可复制的共享响应式句柄。绑定可以保留 StateCore，而不依赖 ViewModel 对象地址。
- 队列任务只保存 `weak_ptr<StateCore<T>>`；State 销毁后任务安全失效。
- 每次写入分配单调递增版本号。旧的异步结果不得覆盖更新的同步值。
- 多个尚未执行的 `post` 合并到最新值。
- `StateSubscription` 通过弱 Core 和原子 ObserverSlot 管理生命周期，State 与 Subscription 可以按任意顺序销毁。

## Public API

```cpp
wui::UiApp app;
wui::State<std::string> title{app.uiContext(), "Loading"};

title.set("Ready");       // owning UI context only
title.post("Downloaded"); // any thread
```

未绑定 Context 的旧式 State 暂时保留用于纯同步控件和兼容迁移，但调用 `post` 会明确抛出 `logic_error`。

## Consequences

- ViewModel 只依赖 `UiContext`，不依赖 App、Window 或平台事件循环。
- 错误 Dispatcher、后台同步写入和停止后的投递均可被确定性发现。
- UI 绑定不再要求原始 State 句柄覆盖节点生命周期。
- `set` 与 `post` 的最后调用顺序可预测，不继承 LiveData 中旧 post 覆盖新 set 的意外行为。
- 后续 Node、Scheduler 和 Ticker 应逐步从进程级线程断言迁移到同一个 UiContext。
