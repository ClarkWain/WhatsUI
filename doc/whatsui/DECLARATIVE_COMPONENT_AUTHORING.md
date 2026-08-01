# Declarative Component Authoring

状态：Implemented

WhatsUI 不要求业务组件继承框架基类。默认组件是一个返回 `NodePtr` 的普通函数，
输入按职责拆成 Props（数据）与 Callbacks（意图）。只有自定义布局、绘制或平台桥接
才需要编写 `*Node`。

## 推荐形状

```cpp
struct TaskRowProps {
    std::string title;
    bool completed{false};
};

struct TaskRowCallbacks {
    std::function<void(bool)> onCompletedChange;
    std::function<void()> onOpen;
};

wui::NodePtr TaskRow(TaskRowProps props, TaskRowCallbacks callbacks)
{
    using namespace wui;
    return Row()
        .children(
            Checkbox(props.completed)
                .onChange(std::move(callbacks.onCompletedChange)),
            Button(std::move(props.title))
                .onClick(std::move(callbacks.onOpen)))
        .build();
}
```

Props 是构建时快照，Callbacks 表达用户意图。组件不直接持有 Window、Repository 或
ViewModel 的强引用；这些对象由页面/控制器拥有。

## 生命周期安全

Node 可能比创建它的页面对象活得更久。若回调需要捕获 `this`，页面持有
`CallbackLifetime`，并把 intent callback 包装后传入：

```cpp
class TaskPage {
public:
    wui::NodePtr build()
    {
        return TaskRow(
            {.title = "Review API"},
            {.onOpen = lifetime_.guard([this] { openTask(); })});
    }

private:
    void openTask();
    wui::CallbackLifetime lifetime_;
};
```

`invalidate()` 或析构后，guarded callback 成为空操作。执行中的 callback 会持有 token
直到返回，避免与并发失效形成悬空窗口。它只接受返回 `void` 的 intent callback；需要
返回值的查询应读取稳定的数据服务，而不是回调已销毁的页面。

## 响应式数据

- UI owner thread 内使用 `state.set(value)`。
- 后台任务使用 `state.post(value)`，由绑定的 `UiContext` 串行投递。
- `get()` 用于读取当前快照；不得从错误线程读取绑定 UI 的 `State`/`Computed`。
- 数据层先校验并产生领域结果，再更新 State；UI 不负责修补非法领域数据。
- 订阅随 Node teardown 解除；异步完成回调仍应使用 `CallbackLifetime` 或弱 owner。

## 何时创建自定义 Builder

需要可复用的运行时 Node 时，公开 `XxxNode`，再用 `BuilderBase<Xxx, XxxNode>` 提供
无后缀 `Xxx` Builder。组合能力必须选择最小集合：leaf、any children、single content、
typed children、item factory 或 named slots。不要仅因为 `XxxNode` 继承
`ContainerNode` 就暴露通用 `children()`。

复杂 modifier 由一个私有 `apply...` 实现，公开 `&` 和 `&&` 两个薄转发重载。
`build() &&` 是一次性所有权转移，并非 Flutter/ArkUI 中会反复执行的生命周期 `build()`。

## 页面边界检查

- 页面代码默认只出现 Builder；需要焦点、平台句柄或自定义绘制时才保留 `*Node` 指针。
- 测试定位用 `automationId`，朗读名称用 `accessibleLabel`，日志用 `debugName`，
  列表复用用 `NodeKey`。
- 页面退出前取消其后台工作；无法取消的完成事件必须通过 `post()` 和 lifetime guard。
- Dialog/Overlay 由 `UiWindow` 管理，不把裸 Node 指针跨页面保存。
