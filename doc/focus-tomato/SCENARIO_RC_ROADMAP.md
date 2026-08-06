# FocusTomato & WhatsUI RC 剩余工作路线图

状态：Draft v1（项目负责人排产）
更新日期：2026-08-06
基线提交：`1c1c067`（`codex/focus-tomato-logic-fixes` 相对 `origin/main` 领先 6 个 commit）

本文档是 [ROADMAP.md](../../ROADMAP.md) 剩余 M5 门禁与后续阶段的**执行侧**分解，把 `FOCUS_TOMATO_ENGINEERING_SCENARIO_REPORT.md` 中 42 个尚未实现场景 + M5 剩余 RC 门禁按主题聚类，并给出优先级、依赖、验收标准。

不写时间承诺；只写"必须先做什么"和"可以并行做什么"。

## 优先级模型

- **RC-blocker**：M5 RC 签核门禁必需，不做完不能出 1.0。
- **v0.2-preview**：M6 阶段桌面壳完整化，依托本轮 platform 层完成收尾。
- **v0.3-preview**：功能领域扩展（中断、音频、列表增强）。
- **v0.4-preview**：数据/迁移/长期运行等长尾能力。

## 分类总表

| # | 类别 | 场景数 | 优先级 | 依赖 |
| --- | --- | ---: | --- | --- |
| A | 桌面壳集成收尾 | 11 | v0.2-preview | 本轮 `DesktopServices` 已就绪 |
| B | 音频子系统 | 6 | v0.3-preview | 独立引入，或复用 WhatsCanvas 音频（若引入） |
| C | 数据导入/导出 | 8 | v0.4-preview | 无强依赖 |
| D | 中断实体 | 5 | v0.3-preview | 依赖状态机扩展 |
| E | 任务列表功能增强 | 4 | v0.3-preview | 依赖已有虚拟化能力 |
| F | 无障碍 / DPI / 系统主题实机 | 7 | **RC-blocker** | 依赖 Windows 实机环境 |
| G | 数据迁移 / 备份 / 同步 | 4 | v0.4-preview | 依赖 schemaVersion 提升 |
| H | 长期运行 / 性能长尾 | 2 | v0.4-preview | 依赖 F 与 CI 长跑机器 |
| I | 小型 UI/UX 补齐 | 15+ | 分层散落 | 各自独立 |

合计覆盖 42 项 + M5 剩余 RC 门禁 5 项。

---

## 类别 A — 桌面壳集成收尾（v0.2-preview）

依托 [ADR-007](ADR-007-desktop-services-and-custom-frame.md) 与 [DESKTOP_PLATFORM_CAPABILITIES.md](../DESKTOP_PLATFORM_CAPABILITIES.md) 完成的抽象，把 FocusTomato 桌面壳提升到完整产品级别。

| SC | 内容 | 拆解 |
| --- | --- | --- |
| SC-33 | 迷你模式窗口 | `WindowOptions` 支持"迷你尺寸预设 + always-on-top"；`FocusRouter` 提供 mini/full 切换命令；Software 截图基线 |
| SC-34 | 系统托盘命令 | 已实现暂停/继续/退出；补 "打开专注" / "跳过休息" 快捷菜单；托盘状态与 `ViewModel` 事件驱动 |
| SC-35 | 单实例锁 | Win32 `CreateMutex` 命名互斥；已有实例发送激活消息（`WM_COPYDATA` 携带 CLI 参数） |
| SC-36 | 托盘能力探测降级 | 使用现有 `DesktopCapabilities::tray` 探测；不可用时 "关闭到托盘" 命令自动切换为退出确认 |
| SC-37 | 活动会话退出流程 | 完整覆盖：托盘可用→隐藏；托盘不可用→确认退出保存中断快照；系统关机→写 unclean-shutdown 标记 |
| SC-38 | 系统完成通知 | 已有 Win32 fallback；提升为 Windows App SDK App Notifications 走 Notification.Actions；macOS/Linux 后端 |
| SC-41 | 通知权限流程 | 首次显示时用 Windows Toast 引导用户开启；被拒绝时降级为窗口内提示，不再重复申请 |
| SC-70 | 单实例与迁移协调 | 已有实例检测到 schema 升级时先延后启动，等其他实例落盘 |
| SC-71 | 窗口位置持久化 | `Settings.window` 存 `x/y/w/h/state/monitor-id`；跨显示器移动/断开屏幕的容错 |
| SC-73 | 保存/修复窗口坐标 | 越界坐标回退到主显示器中心；损坏坐标不阻塞启动 |
| SC-81 | 托盘/通知能力组合降级 | 通知有 + 托盘无、通知无 + 托盘有、两者均无 —— 三种降级路径都有明确产品行为 |

**验收 gate**：
- Windows 100/125/150/200% DPI 下手动走完托盘、通知、mini/full、关机、单实例覆盖。
- 每种能力探测降级都有自动化契约测试（`WhatsUIWindowTests` 扩展）。
- `focus_tomato_desktop_capabilities/` 视觉基线覆盖 mini 模式。

---

## 类别 B — 音频子系统（v0.3-preview）

本轮已冻结任务级 `soundscapeIdSnapshot` 契约，但没有播放器。

| SC | 内容 |
| --- | --- |
| SC-42 | 播放器、资源加载、失败降级 |
| SC-43 | 系统音频设备切换感知 |
| SC-78 | 白噪音预览（不影响会话） |
| SC-82 | 音频输出管理（音量、静音、系统会话） |
| SC-83 | 异步音频解码 |
| SC-84 | 声音目录与空态 |

**关键设计决策**（待 ADR）：
- 音频后端是引入独立库（miniaudio / cubeb），还是复用 WhatsCanvas 生态；
- 音频线程与 `UiDispatcher` 的边界（音频回调绝不进 UI 树）；
- 声音资产的打包与热更新策略。

**验收 gate**：
- 会话失败降级路径无声不炸；
- 设备热插拔在 10 秒内切换到默认新设备；
- 白噪音预览与专注播放互不干扰。

---

## 类别 C — 数据导入/导出（v0.4-preview）

| SC | 内容 |
| --- | --- |
| SC-03 | CSV 文件选择/预览/事务导入 |
| SC-49 | 文件导入通用能力 |
| SC-85 | 大 CSV 流式解析（不阻塞 UI） |
| SC-86 | 导入 staging model（预览、编辑、提交/放弃） |
| SC-87 | 导入取消/放弃 |
| SC-88 | 文件权限错误 UI |
| SC-97 | 脱敏诊断包导出 |
| SC-99 | 区域数字/时长解析（不完全实现前先拒绝非 ASCII 分隔符） |

**关键设计决策**：
- 是否引入独立 CSV 解析器（避免 fmt/ranges 依赖）；
- staging 事务模型（内存 vs 临时文件）。

---

## 类别 D — 中断实体（v0.3-preview）

| SC | 内容 |
| --- | --- |
| SC-19 | 中断实体、原因、备注表单 |
| SC-20 | 中断页面 |
| SC-21 | 中断与终止联合事务 |
| SC-22 | 中断草稿流程（保存草稿、恢复） |
| SC-69 | 通知消费状态跟中断状态联动 |

**关键设计决策**（待 ADR）：
- 中断是"事件"还是"实体"？影响是否可编辑、是否入统计；
- 中断与提前结束的语义差异（用户主动 vs 外部事件）。

---

## 类别 E — 任务列表功能增强（v0.3-preview）

| SC | 内容 |
| --- | --- |
| SC-07 | 拖拽 / 键盘排序 / sortOrder 压缩命令 |
| SC-90 | sortOrder 溢出事务重排（当前返回 Conflict） |
| SC-54 | 任务列表 10k 虚拟化性能基准 |
| SC-55 | 100k 会话统计基准 |

依赖：已经完成的 `VirtualList` 与 keyed recycle 基础。

**验收 gate**：
- 10k 任务列表滚动帧稳态 < 8ms（Software backend 基准）；
- 100k 会话统计计算首屏 < 200ms。

---

## 类别 F — 无障碍 / DPI / 系统主题实机（**RC-blocker**）

M5 门禁必须完成的实机证据矩阵。

| SC | 内容 |
| --- | --- |
| SC-50 | 屏幕阅读器 + DPI 实机验收 |
| SC-72 | 跨显示器视觉回归（100/125/150/200% + 混合 DPI） |
| SC-74 | reduced-motion 系统偏好接入 |
| SC-75 | 高对比度系统主题接入 |
| SC-76 | 全键盘可达性回归 |
| SC-79 | 屏幕阅读器倒计时节流（不喋喋不休） |
| SC-80 | 字体回退矩阵截图（CJK/emoji/RTL） |

**验收 gate**（同 RELEASE_CHECKLIST 门禁）：
- 100/150/200% DPI × Narrator/JAWS/NVDA 三阅读器实机记录；
- IME 100/150/200% 候选窗定位实机截图；
- 高对比度 + reduced-motion 两个开关的视觉回归；
- 每种字体场景手动审查一张截图基线。

**归属**：RELEASE_CHECKLIST 中的 "Text input/IME manual matrix" + "Windows UI Automation bridge plus a Narrator/screen-reader validation matrix" 两项门禁。

---

## 类别 G — 数据迁移 / 备份 / 同步（v0.4-preview）

| SC | 内容 |
| --- | --- |
| SC-56 | 迁移执行器（schemaVersion N → N+1） |
| SC-57 | 迁移事务与回滚 UI |
| SC-96 | outbox 与云同步 |
| SC-98 | 在线数据库备份（当前是原子文件） |

**关键决策**：
- 是否引入 SQLite？（当前设计是纯文件）；
- 云同步是否直接进 v1？（推荐延后）。

---

## 类别 H — 长期运行 / 性能长尾（v0.4-preview）

| SC | 内容 |
| --- | --- |
| SC-100 | 30 天 soak、句柄和内存增长基准 |

依赖：CI 长跑机器；已有 `WhatsUIBenchmarks` 目标可扩展。

---

## 类别 I — 小型 UI/UX 补齐（分层散落）

15+ 项零散补齐，按依赖归到对应大类：

- **归 A**：SC-04 重名 UI、SC-11 loading/去重、SC-12/SC-39 设置页入口、SC-40 字段级设置错误、SC-77 ModalCoordinator 优先级队列
- **归 F**：SC-29 跳过确认 UI 自动化、SC-45 时区 UI、SC-46 异步查询失败态、SC-59 磁盘满故障注入、SC-61/62/64/65/66/68 集成实机测试
- **归 G**：SC-60 版本迁移器（重复项，与 SC-56/57 合并）
- **归 D**：SC-32 "下一任务"选择策略
- **归 E**：SC-44 周统计图表页
- **独立**：无（全部归类）

---

## 独立于 42 场景的 M5 RC 门禁

除了类别 F 之外，剩余 M5 门禁：

| 门禁 | 归属 | 状态 |
| --- | --- | --- |
| 1.0 source/ABI 兼容性策略 approval | ADR + release owner | [COMPATIBILITY_POLICY_1_0_DRAFT.md](COMPATIBILITY_POLICY_1_0_DRAFT.md) 已存在草案 |
| Release-candidate 审批（tag / clean checkout / archive） | release owner | 需 CI tag 流水线 + 归档 SHA-256 记录 |
| 传递性 legal / SBOM 审批 | 法律 owner + [SBOM.md](SBOM.md) | 手动审批 |
| macOS / Linux `DesktopServices` 后端 | 平台 subagent | 契约已定义、后端可延后到 v0.2-preview 之后 |

---

## 建议的下一步落地顺序

1. **完成 RC-blocker（类别 F）**：这是 1.0 tag 的必要条件；无 F 就不能签发。
2. **push 当前 6 个 commit → 开 PR**（治理决策）：让本轮 platform + FocusTomato 改动进入代码审查流水线，不阻塞 F 的准备。
3. **类别 A（桌面壳收尾）**：依托本轮 platform 层，投入产出比最高，能让 FocusTomato 达到"完整桌面产品"标准。
4. **类别 D + E 并行**：两者都不依赖 F 的实机结果，可以由不同 subagent 承担。
5. **类别 B（音频）与 C（导入）延后**：先出 ADR 决定引入哪套依赖，再开工。
6. **类别 G + H 收尾**：属长尾扩展，与 1.0 gate 无关。

---

## 后续任务分派设想

| 任务代号 | 内容 | 类型 |
| --- | --- | --- |
| T7-A | 类别 A 桌面壳收尾（含 mini 模式、单实例、通知权限） | 产品 subagent |
| T7-B | 音频 ADR 起草（miniaudio vs cubeb） | 决策 subagent — **已 Accepted 为 ADR-009** |
| T7-C | CSV 导入 ADR 起草（staging 模型） | 决策 subagent |
| T7-D | 中断实体 ADR 起草（事件 vs 实体） | 决策 subagent — **已 Accepted 为 ADR-008** |
| T7-E | 虚拟化 10k/100k 基准执行 | QA subagent |
| T7-F | 无障碍/DPI 实机矩阵记录（RC-blocker） | 项目负责人 + 实机 |
| T7-G | schemaVersion 迁移框架起草 | 平台 subagent — **已 Accepted 为 ADR-010** |

每个 T7-* 子任务必须回到本文档更新对应场景状态。
