# FocusTomato WhatsUI 实现说明

## 当前可运行导航

```text
任务列表（根页面）
  ├─ 新建任务 → 设置名称、工作量、时长和声音偏好 → 保存 / 取消 → 任务列表
  ├─ 任务行“更多” → 编辑任务
  │    ├─ 保存 → 校验 revision 后更新任务信息和执行偏好
  │    └─ 删除 → 二次确认 → “已删除”筛选
  ├─ “已删除”筛选 → 恢复 → 原完成状态下的任务列表
  ├─ 点击进行中的任务 → 本轮准备（子页面）
  │    ├─ 可见返回 / Esc / Alt+Left → 任务列表
  │    ├─ “调整” → 返回该任务的完整编辑表单
  │    └─ Space / 主播放按钮 → 专注计时
  └─ 活动会话横幅 → 返回当前专注或休息

专注计时（活动会话子页面）
  ├─ 返回任务 / Esc / Alt+Left → 收起页面，计时继续
  ├─ 主按钮 → 暂停 / 继续
  ├─ 重置 / 提前结束 → 确认后才修改会话
  └─ 自然结束 → 完成提醒

完成提醒
  ├─ 返回任务 → 不创建新会话
  ├─ 继续专注 → 新一轮专注
  └─ 休息 → 推荐的短休息或长休息

休息计时（活动会话子页面）
  ├─ 返回任务 / Esc / Alt+Left → 收起页面，休息继续
  ├─ 主按钮 → 暂停 / 继续
  ├─ 重置 / 提前结束 → 确认后才修改会话
  └─ 自然结束 → 任务列表
```

返回和收起只修改导航栈，不写入任务、会话、计时快照或统计。重置、结束和跳过是独立的业务命令，必须明确确认。没有活动会话时窗口关闭会退出；存在活动会话且托盘可用时关闭只隐藏，托盘不可用时显示退出确认。页面切换由 `FocusRouter` 统一处理，页组件不直接操作导航栈。

## 分层与职责

- `domain/`：任务、会话、设置、计时快照、统计事实和跨记录不变量。
- `application/`：`FocusDataService` 是所有数据写操作的唯一入口；`FocusRepository` 是持久化边界。
- `infrastructure/`：本地文件仓库负责原子替换、备份恢复和写入前二次校验。
- `presentation/focus_view_model.*`：把页面意图转换为应用服务命令，并只暴露页面需要的状态。
- `presentation/focus_router.*`：负责页面进入、退出和完成后的流程分支；动态路由只在这里擦除为 `wui::View`，页面动作和延迟刷新使用 `CallbackLifetime` 防止 Router 销毁后的悬空回调。
- `presentation/components/`：页内导航操作、胶囊按钮、图标按钮和指标卡等可复用视觉组件。
- `presentation/pages/`：五个页面都是提供 `body()` 的 `ViewLike` Component；页面内部继续由有业务名称的小型 Builder 函数组成。
- `presentation/dialogs/`：新建、编辑任务 Dialog 都是 `body()` Component，输入草稿和错误反馈使用共享 `State`；只为首次聚焦保留一个最小范围的运行时 `Node*`。

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

## 任务管理规则

- 每个可见任务行都有独立的“更多”操作；编辑表单支持标题、预计番茄数（1～99）和执行偏好，保存使用打开表单时的 `revision` 做乐观并发校验，过期表单不会覆盖新数据。
- 删除是可恢复的软删除，不物理移除 `TaskRecord`，也不修改任何历史 `FocusSession`、`titleSnapshot`、已完成番茄数或统计事实；删除前必须二次确认。
- 软删除分别记录“由进行中删除”和“由已完成删除”。从“已删除”筛选恢复时，任务回到删除前的完成状态，并获得无冲突的新 `sortOrder`。
- 活动计时关联的任务可以改名和修改预计番茄数、时长与声音偏好，但不能删除；当前会话继续使用启动时的标题、计划时长和声音快照，修改只影响任务列表和未来会话。
- 持久化、校验或 revision 冲突时，候选聚合不会发布。编辑 Dialog 保留用户输入并显示错误，删除/恢复结果通过页面操作反馈呈现。

## 任务执行偏好

- `TaskExecutionPreferences.focusMinutes` 为空表示跟随全局番茄时长；有值时必须是 1～180 分钟。任务列表和本轮准备页始终展示计算后的有效时长，避免用户在开始后才发现配置不符。
- 声音偏好使用 `Inherit / Off / Soundscape` 三态。`Inherit` 跟随 `FocusSettings.defaultSoundscapeId`，`Off` 明确静音，`Soundscape` 保存稳定的声音 ID，而不是易变的展示名称。
- 开始任务专注时，优先级固定为“任务覆盖值 > 全局默认值”。`FocusSessionRecord` 冻结 `plannedDurationMs` 与 `soundscapeIdSnapshot`；之后编辑任务或全局设置都不会改变活动会话。
- 当前版本尚未接入音频播放器。表单明确标注“只保存任务偏好”，业务层不会伪造播放状态；后续播放器只消费会话的 `soundscapeIdSnapshot`，音频失败不得影响计时状态。
- 文件仓库继续读取旧字段行：旧任务迁移为跟随全局，旧设置补入 `rain` 默认声音，旧会话没有声音快照。新字段以尾部追加方式写入，读取后仍执行完整聚合校验。
- 新建、编辑和开始会话都通过同一解析、有效值计算和领域校验函数，避免不同页面各自解释数据。非法时长、非法声音 ID、模式与 ID 不一致都会在应用服务和仓库两层被拒绝。

## UI 声明约定

多行 `.children(`、`.content(` 的闭合括号必须单独成行，并与对应调用对齐。页面优先拆分为有业务名称的局部组件，避免深层缩进和单函数承担整页布局。

应用层不调用 `.build()`、`asNode()`，也不直接构造 `*Node`。静态辅助函数返回具体
Builder，页面由 `body()` Component 表达，只有路由分支使用动态 `View`。图片通过
`Image(ImageSource)` 声明，Node 物化统一留在 WhatsUI 所有权边界内部。

核心计时动作使用 68px 纯图标主按钮；重置、跳过使用 40px 次级图标按钮。主次操作不使用同尺寸文字按钮。

应用启动时必须在创建任何 Widget 前安装 `style::focusTheme()`；视觉捕获使用同一主题，不能退回默认 `Theme{}`。番茄红色阶同时覆盖 Primary、Danger、输入焦点线、文本选区、单选/复选状态和兼容 accent 令牌，标准控件的中性色统一使用暖米色 surface/canvas/border。业务 Dialog 只声明语义 appearance，不逐个硬编码蓝色或局部品牌色。

窗口使用 `WindowFrameStyle::Custom` 去掉系统标题栏，应用标题栏固定 48px。标题栏只接收动作回调，不持有 `PlatformWindow`；Router 负责把最小化、关闭动作和平台命中区域注入组件。番茄钟当前是 520 × 720 固定窗，因此不会展示没有正确响应式布局支撑的最大化按钮。

## Figma 对照

原始 Figma 页面使用多组画板尺寸；产品实现统一收敛到 520 × 720 桌面窗口，以避免计时页左右留白过多，并保证跨页面窗口不跳变。原始参照节点：

- `18:20`：任务列表，640 × 820；
- `43:244`：本轮准备，520 × 720；
- `16:2`：专注计时，480 × 720；
- `22:126`：完成提醒，640 × 820；
- `17:11`：短休息，480 × 720。

实装截图由 `WhatsUIFocusTomatoCapture` 使用 WhatsCanvas Software 后端确定性生成。截图包含与真实应用相同的自绘标题栏和 672px 内容区，当前对照输出位于 `artifacts/focus_tomato_desktop_capabilities/`。

主题控件视觉基准输出为 `00-theme-controls.ppm`，同时包含聚焦 TextField、Danger、Outline 和 Primary 按钮，用于阻止默认 Fluent 蓝色重新进入番茄钟。

## 桌面系统集成

- 主窗：无系统标题栏；Win32 通过 `WM_NCHITTEST` 提供系统拖拽和 DPI 感知的边缘缩放；
- 托盘：打开窗口、暂停/继续当前计时、退出；Explorer 重启后自动重新注册；
- 通知：专注完成和休息完成使用系统通知，点击后恢复到对应业务页面；
- 线程：托盘和通知事件统一经 `UiDispatcher` 投递，原生回调不直接修改页面树；
- 降级：托盘安装失败时绝不隐藏成不可恢复的后台进程，活动会话退出前必须确认；
- 透明窗口：框架支持透明 framebuffer 与整窗 opacity，番茄钟主窗保持不透明以保证文字对比度。

公共契约与各平台实现状态见 `doc/DESKTOP_PLATFORM_CAPABILITIES.md`。当前完成并实机验证的是 Windows GLFW 后端；macOS 和 Linux 未实现的原生桥接会明确返回 `Unsupported`。

## 构建与验证

```powershell
cmake --build build --target WhatsUIFocusTomatoApp WhatsUIFocusTomatoCapture -j 4
ctest --test-dir build -C Debug -R whatsui_focus_tomato --output-on-failure
.\build\examples\Debug\WhatsUIFocusTomatoCapture.exe .\artifacts\focus_tomato_visual
.\build\examples\Debug\WhatsUIFocusTomatoApp.exe
```

当前测试覆盖数据校验、应用服务、文件仓库、统计、时钟漂移，以及任务、准备、专注、完成和休息之间的可逆导航。回归测试额外验证任务执行偏好的输入边界、旧文件兼容、有效值继承、会话时长与声音快照、任务编辑、删除确认、已删除筛选、恢复与 revision 冲突，收起计时后仍会结算、活动会话可从任务页恢复，且导航不产生仓库写入。

### Dialog 与页面替换验证

Dialog 内的输入事件可能请求关闭当前 Dialog，但 WhatsUI 会等到本次指针或键盘事件返回后才真正销毁它。因此，任何会替换底层页面的成功回调都必须通过 `scheduleStructuralUpdate` 放到下一安全帧执行。

新建任务回归测试分为两层：

- App 层：使用真实 Pointer Down/Up 点击“新建任务”和“保存任务”，验证 Dialog 先销毁、页面后刷新、数据只保存一次；同时覆盖 Enter 提交、空白标题和持久化失败。
- WhatsUI 层：故意在 Dialog 回调中同步替换底层页面，验证框架丢弃已失效的焦点恢复目标，而不是访问旧页面节点。

稳定性检查使用 `ctest --repeat until-fail:100` 重复执行新建任务回归测试。
