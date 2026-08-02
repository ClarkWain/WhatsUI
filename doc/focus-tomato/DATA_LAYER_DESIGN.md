# FocusTomato 数据层设计与验证门禁

状态：第一阶段已实现
范围：任务、设置、专注/休息会话、活动计时快照、完成结算、统计投影、本地原子文件仓库

## 1. 分层与职责

```text
examples/focus_tomato/
  domain/
    focus_data.*              只定义实体、枚举和聚合
    focus_data_validator.*    只定义数据不变量和诊断
    focus_statistics.*        只从会话事实生成只读统计
  application/
    focus_repository.h        持久化端口
    focus_data_service.*      事务式业务命令
  infrastructure/
    file_focus_repository.*   本地文件实现、原子替换和损坏隔离
```

- `domain` 不依赖 WhatsUI、窗口、文件系统或平台 API。
- `application` 不知道数据写入文件还是 SQLite。
- `infrastructure` 不处理页面状态，也不能绕过聚合校验。
- 后续 `presentation` 只消费 `DataCommandResult` 和不可变数据快照，不直接修改实体。

## 2. 五道数据防线

### 2.1 命令参数校验

在复制数据之前拒绝明显无效的参数，例如空 ID、非正时间、超出 1～180 分钟的时长。
这类错误返回 `DataCommandStatus::InvalidArgument`，不会调用仓库。

### 2.2 单记录字段校验

`validateFocusData()` 会收集所有问题，不会发现第一条错误后提前退出。当前覆盖：

- schema 版本；
- 枚举值；
- ID、UTF-8、可见标题、危险双向/控制字符；
- 设置、预计番茄、剩余时长和修订号范围；
- UTC 时间正值和前后顺序；
- 状态、时间字段和完成原因的合法组合；
- `idempotencyKey == session.id`。

### 2.3 跨记录聚合校验

当前覆盖：

- Task ID 和 FocusSession ID 各自唯一；
- 活动任务的 `sortOrder` 唯一；
- `FocusSession.taskId` 必须指向现存任务；
- 最多存在一个 running、paused 或 completion_pending 会话；
- `activeSessionId` 必须与唯一活动会话一致；
- 活动会话必须有 TimerSnapshot，终态不得遗留 TimerSnapshot；
- TimerSnapshot 的 session、状态、deadline 和 remaining 必须与活动会话一致；
- `Task.completedPomodoros` 必须等于 completed focus session 事实数量。

### 2.4 写入前二次校验

`FocusDataService` 在候选副本上完成全部变更，先校验，再调用 `FocusRepository::save()`。
仓库实现还会独立再校验一次。这样即使未来出现绕过 Service 的新调用者，无效聚合仍不能写盘。

### 2.5 读取后校验与损坏隔离

文件解析成功不等于数据可用。仓库在解析后再次运行聚合校验：

- 语法损坏和语义损坏都返回 `RepositoryLoadStatus::RejectedCorrupt`；
- 原文件移动到不覆盖的 `.corrupt[.N]` 路径；
- 不自动用空数据覆盖损坏文件；
- LoadResult 同时返回恢复路径、稳定诊断码和说明，供恢复 UI 展示。

## 3. 可观测错误

每条 `ValidationIssue` 都包含：

| 字段 | 用途 |
| --- | --- |
| `severity` | error 或 warning |
| `code` | 稳定机器码，例如 `timer_snapshot_mismatch` |
| `entityType` | task、session、timerSnapshot、settings 或 focusData |
| `entityId` | 定位具体记录 |
| `field` | 定位具体字段 |
| `message` | 日志和恢复界面使用的可读说明 |

上层不得只显示“保存失败”。应按状态区分：

- `ValidationRejected`：程序或导入数据不合法，显示诊断并禁止继续；
- `Conflict`：活动会话、乐观修订或当前状态冲突，允许用户刷新/返回当前会话；
- `PersistenceFailed`：磁盘或权限错误，保留当前已持久化状态并提供重试；
- `NoChange`：重复命令已经应用，作为幂等成功处理。

## 4. 事务与发布顺序

```text
接收命令
  → 检查参数和期望状态
  → 复制当前 FocusData
  → 在副本上完成整组修改
  → validateFocusData(candidate)
  → repository.save(candidate)
  → 保存成功后替换内存 FocusData
  → 通知 UI
```

任意一步失败都不会发布候选副本。UI 因此不会看到“任务计数已增加，但会话还没完成”一类半成功状态。

## 5. 计时和完成结算

- running 会话保存绝对 UTC `targetEndAtUtcMs`，暂停时保存
  `remainingMs = max(0, targetEndAtUtcMs - nowUtcMs)`。
- 继续时生成新的 `targetEndAtUtcMs = nowUtcMs + remainingMs`。
- 到点先写 `completion_pending` 和可恢复 TimerSnapshot。
- 最终完成在一次候选提交中同时：
  - 将会话改为 completed；
  - 写唯一 `endedAtUtcMs`；
  - 增加任务缓存；
  - 清除 `activeSessionId`；
  - 清除 TimerSnapshot。
- 最终写入失败时，内存仍停在已持久化的 `completion_pending`，可使用同一 session ID 重试。
- 再次提交已完成 session 返回 `NoChange`，不会重复增加番茄。

## 6. 任务与设置

- 新任务使用调用者提供的稳定 ID，初始化 `revision=1`。
- 当前排序以 1024 为间隔，预留拖拽插入空间；溢出前返回显式 Conflict，后续由重排事务处理。
- 归档是软删除，只改状态、修订号和更新时间；历史 session 不变。
- 任务编辑/归档使用 expected revision 做乐观并发检查。
- 设置更新同样经过完整聚合校验。已启动 session 持有时长快照，因此设置变更只影响下一次会话。

## 7. 系统时间异常

运行中的进程同时保留 UTC wall-clock 和 monotonic checkpoint。
`detectClockDrift()` 比较两者的 elapsed：

- 误差不超过 2 分钟：正常继续；
- wall-clock 大幅向前或向后：返回有方向的恢复状态，由恢复 UI 让用户选择；
- monotonic 倒退：视为进程或系统边界，放弃这组比较，改用已持久化 UTC snapshot 恢复；
- 时区变化不改变 UTC，不会误报为系统时间被修改。

## 8. 统计口径

`calculateFocusStatistics()` 只读取 `FocusSessionRecord`：

- 只统计 `type=focus && status=completed`；
- 排除 short break、long break、aborted 和 skipped；
- 包含 taskId 为空的自由专注；
- 归档任务的历史仍按 taskId 查询；
- 自然完成和恢复完成记 planned duration；
- 手动完成最多记 `endedAt-startedAt`，且不超过 planned duration；
- 查询窗口按 `endedAtUtcMs` 使用 `[from, to)`。

Task 缓存和未来的 DailyFocusSummary 都不是事实来源，可从 session 重建。

## 9. 当前文件仓库

第一阶段使用可移植的版本化行格式，支持 UTF-8 和字段转义。保存过程：

1. 校验完整聚合；
2. 写 `<store>.tmp` 并完整关闭；
3. 将旧文件移动为 `<store>.bak`；
4. 将 tmp 原子重命名为正式文件；
5. 发布失败时恢复 backup；
6. 成功后删除 backup。

启动时若正式文件缺失而 backup 完整，则自动恢复并返回
`RepositoryLoadStatus::RecoveredBackup`。

这是可替换基础设施，不是最终数据库承诺。进入多表查询、在线备份和版本迁移阶段时，应新增
SQLite `FocusRepository`，保持 domain 和 application 接口不变，并为事务、迁移失败、online
backup 和唯一活动会话索引增加集成测试。

## 10. 已自动验证

```powershell
cmake --build build --config Debug `
  --target WhatsUIFocusTomatoDataValidationTests `
           WhatsUIFocusTomatoDataServiceTests `
           WhatsUIFocusTomatoFileRepositoryTests `
           WhatsUIFocusTomatoStatisticsTests `
           WhatsUIFocusTomatoClockDriftTests

ctest --test-dir build -C Debug `
  -R "whatsui_focus_tomato_.*_tests" --output-on-failure
```

覆盖范围包括字段错误、跨记录错误、快照漂移、重复活动会话、暂停/继续 deadline 算法、
completion_pending 恢复、完成写入失败、幂等重试、归档乐观并发、统计排除规则、完整
round-trip、无效写入不覆盖旧数据、损坏文件隔离和中断替换的 backup 恢复。

## 11. UI 阶段门禁

UI 不以“能运行”作为完成标准。每个页面需要：

1. 页面、组件和 view-model 分文件，页面函数只组合组件；
2. 使用 DataCommandResult 显示具体错误/冲突/重试状态；
3. 在固定逻辑尺寸生成 Software renderer 基准截图；
4. 与 Figma 对比窗口宽度、内容轨道、留白、文字层级、主次操作、颜色、圆角和关键状态；
5. 记录偏差并修正后，再提交视觉回归基线。
