# ADR-008: Interruption Entity for FocusTomato

状态：Accepted（3 项待议由项目负责人于 2026-08-06 决定；见 [Decisions locked in](#decisions-locked-in)）
起草人：项目负责人
日期：2026-08-06

本 ADR 只固定"中断如何被建模"的边界，不实现具体 UI/持久化细节。目的是让后续 T7-D 子任务（SC-19、SC-20、SC-21、SC-22、SC-69）沿着同一条契约推进。

## Context

`FOCUS_TOMATO_ENGINEERING_SCENARIO_REPORT.md` 中 5 个场景把"中断"作为 FocusTomato 会话生命周期的一等公民：

- SC-19：中断表单要求原因、备注、时间戳；
- SC-20：中断页面允许用户填写并提交；
- SC-21：中断与随后可能的"提前结束"是**一个事务**（先中断，再由用户决定继续或结束）；
- SC-22：进入中断后再退出会保留草稿；
- SC-69：系统通知（比如托盘"离开电脑"）需要被 FocusTomato 感知并映射到中断。

当前实现（截至 `1c1c067`）：

- 会话状态机只覆盖 `Pending / Running / Paused / CompletionPending / Completed / Aborted / Skipped`。
- `Aborted` 语义混合了"用户主动放弃"和"进程崩溃 recovered"两种含义（见 `CompletionReason::Recovered / UserAborted`）。
- 没有独立的中断实体、原因枚举、备注字段或时间戳。
- 系统级"退出应用 / 关机"事件已通过 `WindowCloseRequestHandler` 决策，但**没有落到会话数据**。

## Decision（初稿）

### 1. 中断是**事件**，不是实体

我们不引入新表 `InterruptionRecord`。而是在既有 `FocusSessionRecord` 上追加：

```
struct InterruptionEvent {
    InterruptionReason reason;
    std::string note;              // 可选自由文本
    std::int64_t occurredAtUtcMs;
    std::int64_t detectedAtUtcMs;  // 与 occurredAt 可差 tick 级偏移
    InterruptionSource source;     // User / System / Application
};

struct FocusSessionRecord {
    /* 既有字段 */
    std::vector<InterruptionEvent> interruptions;
};
```

**理由**：中断的所有事实（时间、原因、备注）都天然归属某个会话；跨会话查询"这次中断因什么"没有独立含义。事件模型比实体模型少一层 join，也不需要 sortOrder。

### 2. `InterruptionReason` 保守取值

```
enum class InterruptionReason {
    UserPause,           // 用户手动暂停但没写理由（等同今天的 Paused）
    UserAway,            // 用户显式声明"我离开一下"
    Meeting,
    Emergency,
    SystemLock,          // OS 锁屏 / 睡眠 (SC-69)
    ApplicationClose,    // 应用被系统关闭
    NetworkOffline,      // Reserved（未来 outbox 相关）
    Other,
};
```

新增 reason 必须默认映射到 `Other`；旧数据（无 interruptions 字段）读取时视为空 vector，向后兼容。

### 3. 中断与状态机的关系

- 进入 `Paused` 状态时必然产生**至少一个** `InterruptionEvent`；
- `UserPause`（无理由）是默认；
- 用户显式点"标注原因"打开表单后，最后一个事件的 `reason/note` 被替换（不生成第二条）；
- 从 `Paused → Running` 恢复时**不新增事件**；下一次 pause 会产生下一条；
- `Aborted` 时必然带一条 `InterruptionEvent`，`reason = UserAway/Emergency/Other`，同时 `CompletionReason = UserAborted`。

### 4. 联合事务（SC-21）

用户在中断表单里可选"结束本次会话"或"继续"。这两者是**同一个命令**：

```
enum class ResumeDecision {
    Continue,
    EndSession,
    SkipRest,   // 仅休息态
};

app.recordInterruption(session, event, decision)
```

`recordInterruption` 内部作为一个原子写：先追加事件，再根据 `decision` 转换状态。中间任意一步失败都不发布任何变化。

### 5. 草稿保留（SC-22）

- 中断表单是**页面**（`InterruptionPage`）而非 modal；
- ViewModel 内维护 `pendingInterruptionDraft: InterruptionEvent`；
- 用户返回时草稿保留在内存；
- 完成会话或用户 explicit `discardDraft()` 才清除；
- **不持久化草稿**——重启后草稿丢失，会话回到 `Paused` 无理由态。

### 6. 系统事件映射（SC-69）

- `DesktopEvent::TrayAction` 若 payload 为约定字符串（例如 `"awaymode"`）→ 产生 `SystemLock` 中断；
- Win32 `WM_POWERBROADCAST(PBT_APMSUSPEND)` → `SystemLock`；
- `WM_CLOSE`（应用关闭）→ `ApplicationClose`；
- 用户在 UI 上仍可修正 reason 或补 note。

## 待议决策

_（已在下述 Decisions locked in 中定夺，本节保留仅供历史参考。）_

1. **草稿是否持久化**：初稿说"否"。若产品希望"用户离开电脑 8 小时回来仍能填理由"，需要落盘（引入 SC-22 的 outbox）。**倾向：不持久化**。
2. **多次 pause 是合并还是分别记录**：本 ADR 说"分别记录"。若统计视图会显得吵，也可以合并（keep first reason，累加时长）。**倾向：分别记录**（保持事实完整）。
3. **是否允许"中断"在 `Completed` 后追加**（比如用户回顾今天为什么效率低）：**倾向：不允许**。已完成会话是不可变事实。

## Decisions locked in

项目负责人于 2026-08-06 对上述 3 项待议做出以下决定：

1. **草稿不持久化**。草稿仅存于 `FocusViewModel::pendingInterruptionDraft` 内存中；应用退出或崩溃后草稿丢失，会话回到 `Paused` 无理由态。理由：避免"用户忘了处理"的心智负担；持久化会引入 outbox / 冲突解决等复杂度，与产品价值不匹配。
2. **多次 pause 分别记录**。每次 `Running → Paused` 追加一条 `InterruptionEvent`；`Running → Running` 内部不生成事件。理由：保持事实完整性优先，为统计和用户回顾提供最准确的原始数据；未来若统计视图需要聚合，在展示层做即可。
3. **不允许 Completed 后追加中断**。已完成会话的 `interruptions` 字段进入不可变态；任何"回顾式"补录尝试都返回 `InvalidTransition`。理由：已完成的事实不可变是 FocusTomato 核心不变量之一（SC-92 已建立），中断补录会破坏统计可重现性。

本 ADR 从此进入 Accepted 状态；后续实施只对 §1..§6 契约做**兼容性补充**，不得修改上述 3 项决策，除非再走新 ADR。

## Consequences

- 会话记录 schema 变大；`schemaVersion` 从 1 → 2（触发 SC-56 迁移执行器）。
- `whatsui_focus_tomato_data_service_tests` / `product_flow_tests` 需要新增：中断事件追加、决策原子性、草稿隔离、系统事件映射。
- `WHATSUI_IMPLEMENTATION.md` 需要在"状态机"和"数据错误如何被上层发现"章节补描述。
- Statistics 需要一列"平均中断次数/时长"（可选）。
- 与 ADR-007 桌面事件通道联动：`SystemLock` 依赖 `WindowCloseRequestHandler` 与未来的 power broadcast。

## Non-goals

- 不建立跨会话的"中断趋势"分析仪表板（属长期统计）；
- 不映射 macOS/Linux 的 idle 检测（后续跨平台后端各自补齐）；
- 不做"AI 判断你为什么中断"这种功能。

## Verification（实现后需要覆盖）

- 追加事件不改变现有 pomodoro 计数；
- `recordInterruption` 事务失败不留副作用；
- 草稿在页面重进后仍存在，但重启后消失；
- 旧会话文件（无 interruptions 字段）读取安全；
- Win32 suspend 事件在测试里可用 headless dispatcher 注入；
- 100+ 中断事件的会话仍能正常序列化并统计。
