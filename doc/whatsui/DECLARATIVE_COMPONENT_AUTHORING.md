# Declarative Component Authoring

状态：Implemented（2026-08-01）

应用作者不需要理解 `build()`、`asNode()`、`NodePtr` 或节点物化。页面只组合无后缀
控件；可复用组件提供 `body()`。框架在容器、窗口、导航和浮层边界内部把 `ViewLike`
描述物化为 retained `*Node`。

## 最小心智模型

应用作者只需要三个概念：

1. `Text`、`Button`、`Column` 等声明式控件。
2. 提供 `body()` 的业务组件。
3. 用 `get()`、`set()`、`post()` 管理状态。

```cpp
struct TaskRowProps {
    std::string title;
    bool completed{false};
};

struct TaskRowCallbacks {
    std::function<void(bool)> onCompletedChange;
    std::function<void()> onOpen;
};

class TaskRow {
public:
    TaskRow(TaskRowProps props, TaskRowCallbacks callbacks)
        : props_(std::move(props))
        , callbacks_(std::move(callbacks))
    {
    }

    auto body()
    {
        return wui::Row()
            .children(
                wui::Checkbox(props_.completed)
                    .onChange(std::move(callbacks_.onCompletedChange)),
                wui::Button(std::move(props_.title))
                    .onClick(std::move(callbacks_.onOpen))
            );
    }

private:
    TaskRowProps props_;
    TaskRowCallbacks callbacks_;
};
```

组件和控件可以直接嵌套：

```cpp
window.content(
    wui::Column()
        .children(
            wui::Text("今日任务"),
            TaskRow(
                {.title = "Review API"},
                {.onOpen = openTask})
        )
);
```

`body()` 只执行一次以生成 retained tree；它不是 Flutter 式的持续重建生命周期。
组件描述对象在物化完成后可以立即销毁，因此节点回调不得捕获组件自身的裸 `this`。

## ViewLike 与动态 View

以下值都属于 `ViewLike`：

- 任意 WhatsUI Builder，例如 `Button`、`Column`。
- 提供合法 `body()` 的组件。
- `std::unique_ptr<NodeT>`，仅供节点作者和低层测试使用。
- move-only `View`。

`children()`、`content()`、具名槽位、`If/ForEach`、`UiRoot`、`UiWindow`、
`Navigator`、`OverlayHost` 和 `showDialog()` 都直接接受这些值。
GLFW 应用入口 `runGlfwApp()` 同样接受 `ViewLike`，包括直接根视图和接收
`UiWindow&` 后返回根视图的工厂；应用代码不需要为平台入口调用 `.build()`。

静态可知的辅助组件应返回具体 Builder：

```cpp
wui::Box metricCard(std::string label, std::string value);
```

只有路由、跨模块工厂或运行时分支必须存储不同具体类型时才使用 `View`：

```cpp
wui::View currentPage(Route route)
{
    switch (route) {
    case Route::Tasks: return TaskListPage{};
    case Route::Timer: return FocusTimerPage{};
    }
    throw std::logic_error("unknown route");
}
```

`View` 是一次性、move-only 类型擦除容器。它不会引入虚拟树或 diff，也不允许隐式转换
为 `NodePtr`。普通叶子和布局辅助函数不要返回 `View`，避免无意义的堆分配。

## 生命周期安全

Props 是构建时快照，Callbacks 表达用户意图。组件不直接持有 Window、Repository 或
ViewModel 的强引用。需要访问页面控制器时，由外部长寿命 owner 提供 guarded callback：

```cpp
class TaskPageController {
public:
    TaskRow row(const Task& task)
    {
        return TaskRow(
            {.title = task.title},
            {.onOpen = lifetime_.guard([this] { openTask(); })});
    }

private:
    void openTask();
    wui::CallbackLifetime lifetime_;
};
```

`invalidate()` 或 owner 析构后，guarded callback 成为空操作。执行中的 callback 会持有
token 直到返回，避免与并发失效形成悬空窗口。

## 响应式数据

- UI owner thread 内使用 `state.set(value)`。
- 后台任务使用 `state.post(value)`，由绑定的 `UiContext` 串行投递。
- `get()` 用于读取当前快照；不得从错误线程读取绑定 UI 的 `State`/`Computed`。
- 数据层先校验并产生领域结果，再更新 State；UI 不负责修补非法领域数据。
- 订阅随 Node teardown 解除；异步完成回调仍应使用 `CallbackLifetime` 或弱 owner。

## 控件作者边界

只有自定义布局、绘制、事件路由或平台桥接才需要实现 `XxxNode`，再通过
`BuilderBase<Xxx, XxxNode>` 暴露无后缀 `Xxx`。`build() &&` 是控件实现和低层节点测试
使用的一次性所有权出口，不属于应用作者工作流。

组合能力必须选择最小集合：leaf、any children、single content、typed children、
item factory 或 named slots。复杂 modifier 由一个私有 `apply...` 实现，公开 `&` 和
`&&` 两个薄转发重载。

## 页面边界检查

- 页面和业务组件默认不出现 `.build()`、`asNode()`、`NodePtr` 或 `*Node`。
- 需要焦点、平台句柄或自定义绘制时，才在最小范围保留 `*Node` 指针。
- 测试定位用 `automationId`，朗读名称用 `accessibleLabel`，日志用 `debugName`，
  列表复用用 `NodeKey`。
- 页面退出前取消后台工作；无法取消的完成事件通过 `post()` 和 lifetime guard 返回 UI。
- Dialog/Overlay 由 `UiWindow` 管理，不把裸 Node 指针跨页面保存。
