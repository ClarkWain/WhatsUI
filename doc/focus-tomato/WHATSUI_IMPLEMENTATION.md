# FocusTomato WhatsUI 实现说明

## 当前可运行链路

```text
任务列表
  ├─ 新建任务 → 新建任务弹窗 → 保存 → 刷新任务列表
  └─ 点击进行中的任务 → 本轮准备
       ├─ Esc → 任务列表
       └─ Space / 主播放按钮 → 专注计时
            ├─ 主按钮 → 暂停 / 继续
            ├─ 重置 → 恢复本轮完整时长
            ├─ 提前结束 → 任务列表
            └─ 自然结束 → 完成提醒
                 ├─ 继续专注 → 新一轮专注计时
                 └─ 休息一下 → 短休息
                      ├─ 主按钮 → 暂停 / 继续
                      ├─ 重置 → 恢复完整休息时长
                      ├─ 提前结束 → 任务列表
                      └─ 自然结束 → 任务列表
```

窗口右上角关闭按钮退出应用。页面切换由 `FocusRouter` 统一处理，页面组件不直接操作导航栈。

## 分层与职责

- `domain/`：任务、会话、设置、计时快照、统计事实和跨记录不变量。
- `application/`：`FocusDataService` 是所有数据写操作的唯一入口；`FocusRepository` 是持久化边界。
- `infrastructure/`：本地文件仓库负责原子替换、备份恢复和写入前二次校验。
- `presentation/focus_view_model.*`：把页面意图转换为应用服务命令，并只暴露页面需要的状态。
- `presentation/focus_router.*`：负责页面进入、退出和完成后的流程分支；动态路由只在这里擦除为 `wui::View`，页面动作和延迟刷新使用 `CallbackLifetime` 防止 Router 销毁后的悬空回调。
- `presentation/components/`：窗口栏、胶囊按钮、图标按钮和指标卡等可复用视觉组件。
- `presentation/pages/`：五个页面都是提供 `body()` 的 `ViewLike` Component；页面内部继续由有业务名称的小型 Builder 函数组成。
- `presentation/dialogs/`：新建任务 Dialog 是 `body()` Component，输入草稿和错误反馈使用共享 `State`；只为首次聚焦保留一个最小范围的运行时 `Node*`。

## 数据错误如何被上层发现

每次命令先构造候选聚合，再执行 `validateFocusData`。只有完整聚合通过后，仓库才会接收写入；仓库在落盘前再次执行相同校验。

命令返回值区分：

- 参数错误；
- 记录不存在；
- 状态冲突；
- 领域校验拒绝；
- 持久化失败；
- 幂等的无变化结果。

自然完成采用两步提交：先保存 `CompletionPending` 恢复点，再原子写入完成事实、任务番茄计数、活动会话清理和快照清理。短休息提前结束保存为 `Skipped/UserSkipped`，不会伪装成取消专注。

## UI 声明约定

多行 `.children(`、`.content(` 的闭合括号必须单独成行，并与对应调用对齐。页面优先拆分为有业务名称的局部组件，避免深层缩进和单函数承担整页布局。

应用层不调用 `.build()`、`asNode()`，也不直接构造 `*Node`。静态辅助函数返回具体
Builder，页面由 `body()` Component 表达，只有路由分支使用动态 `View`。图片通过
`Image(ImageSource)` 声明，Node 物化统一留在 WhatsUI 所有权边界内部。

核心计时动作使用 68px 纯图标主按钮；重置、跳过使用 40px 次级图标按钮。主次操作不使用同尺寸文字按钮。

## Figma 对照

已对照的 Figma 节点：

- `18:20`：任务列表，640 × 820；
- `43:244`：本轮准备，520 × 720；
- `16:2`：专注计时，480 × 720；
- `22:126`：完成提醒，640 × 820；
- `17:11`：短休息，480 × 720。

实装截图由 `WhatsUIFocusTomatoCapture` 使用 WhatsCanvas Software 后端确定性生成。当前基准输出位于 `artifacts/focus_tomato_visual_v3/`。

## 构建与验证

```powershell
cmake --build build --target WhatsUIFocusTomatoApp WhatsUIFocusTomatoCapture -j 4
ctest --test-dir build -C Debug -R whatsui_focus_tomato --output-on-failure
.\build\examples\Debug\WhatsUIFocusTomatoCapture.exe .\artifacts\focus_tomato_visual
.\build\examples\Debug\WhatsUIFocusTomatoApp.exe
```

当前测试覆盖数据校验、应用服务、文件仓库、统计、时钟漂移，以及“任务 → 专注 → 暂停/重置 → 完成 → 短休息 → 跳过”的产品链路。

### Dialog 与页面替换验证

Dialog 内的输入事件可能请求关闭当前 Dialog，但 WhatsUI 会等到本次指针或键盘事件返回后才真正销毁它。因此，任何会替换底层页面的成功回调都必须通过 `scheduleStructuralUpdate` 放到下一安全帧执行。

新建任务回归测试分为两层：

- App 层：使用真实 Pointer Down/Up 点击“新建任务”和“保存任务”，验证 Dialog 先销毁、页面后刷新、数据只保存一次；同时覆盖 Enter 提交、空白标题和持久化失败。
- WhatsUI 层：故意在 Dialog 回调中同步替换底层页面，验证框架丢弃已失效的焦点恢复目标，而不是访问旧页面节点。

稳定性检查使用 `ctest --repeat until-fail:100` 重复执行新建任务回归测试。
