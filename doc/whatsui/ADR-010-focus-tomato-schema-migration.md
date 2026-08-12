# ADR-010: FocusTomato SchemaVersion Migration Framework

状态：Accepted
起草人：项目负责人
日期：2026-08-06

本 ADR 定义 FocusTomato 数据文件的版本升级路径，让后续引入 `InterruptionEvent`（ADR-008）、窗口位置（SC-71/73）、音频偏好（ADR-009）等破坏性字段时，用户数据无损升级。

## Context

当前实现（截至 `422192d`）：

- `kCurrentSchemaVersion = 1`（[focus_data.h](../../examples/focus_tomato/domain/focus_data.h)）；
- `file_focus_repository` 直接读入 v1 文件；
- `focus_data_validator` 在 `snapshot.schemaVersion != kCurrentSchemaVersion` 时报 `UnsupportedSchemaVersion` 并**拒绝加载**；
- 没有升级路径——用户升级 WhatsUI 后遇到旧格式必然报错，等同数据丢失。

已经在场景清单中标记为"尚未实现"的 4 项直接触发本框架：

- SC-56 迁移执行器；
- SC-57 迁移事务与回滚 UI；
- SC-60 版本迁移器（重复项）；
- 未来 SC-70 单实例与迁移协调。

同时 ADR-008 中断实体一旦落地就会把 `kCurrentSchemaVersion` 从 1 提升到 2，本 ADR 是它的直接前置。

## Decision

### 1. Schema 版本升级契约

- **schemaVersion 是不透明单调整数**，从 1 开始，只允许 `N → N+1` 的单步升级；
- 跨版本升级（例如 v1 → v3）必须走链式：v1 → v2 → v3；
- 单个提升 patch 必须**幂等** — 对已经是 v(N+1) 的数据再跑不会破坏；
- 降级 unsupported — 用户如果拿新版数据回退到旧 WhatsUI，看到 `UnsupportedSchemaVersion` 拒绝加载（现状保持）。

### 2. 三类文件独立版本化

FocusTomato 有三种落盘文件，各自独立追踪 schemaVersion：

- `focus_data.jsonl`（`FocusData`：任务 + 会话 + 设置）
- `timer_snapshot.jsonl`（`TimerSnapshot`：崩溃恢复用）
- `settings.json`（`FocusSettings` 单文件）

三者的版本号命名空间独立；升级 `FocusData` 不必同步 `TimerSnapshot`。

### 3. Migration 注册与查找

```cpp
struct FocusDataMigration {
    int fromVersion;        // must be strictly < toVersion
    int toVersion;
    std::string name;
    // Applied on the parsed in-memory representation, not on raw bytes.
    // Any error must be surfaced as MigrationError, not as an exception.
    std::function<
        std::variant<FocusData, MigrationError>(FocusData)> apply;
};

class FocusDataMigrator {
public:
    void registerMigration(FocusDataMigration migration);
    std::variant<FocusData, MigrationError> migrateTo(
        FocusData input, int targetVersion) const;
};
```

Migrations 在应用启动时静态注册（`registerBuiltinMigrations`）。同 fromVersion 只允许一条 migration 存在（重复注册报编译期/启动期错）。

### 4. 事务与回滚（SC-57）

**磁盘层的保守事务模型**：

1. 读取原始文件到内存；
2. 立刻把原始字节复制到 `<filename>.pre-migration-v<N>.bak`；
3. 应用链式 migration；
4. 全部成功后写入新文件（走 file_focus_repository 现有的原子替换路径）；
5. 若任一步骤失败：**删除写入中的临时文件**、**保留 `.bak`**、报 `MigrationError` 给应用层，让 UI 决定是继续以只读模式打开还是让用户手动介入。

`.bak` 文件永不自动清理 — 由用户在设置里显式"清理迁移备份"，或者 90 天后 UI 提示。

### 5. UI 层反馈（SC-57）

- 首次启动检测到需要迁移 → 显示 **进度对话框**（阻塞式，因为迁移期间不能进入正常状态机）；
- 每一步 migration 报告名称与结果，用户可以取消（取消 = 保留 `.bak` + 拒绝启动）；
- 成功后进入正常应用；
- 失败对话框显示 `MigrationError` 详情，附上 `.bak` 路径。

**该对话框走 `wui::Dialog` 而非 `OverlayHost` menu**（因为迁移期间 ViewModel 还没准备好）。

### 6. 与单实例协调（SC-70）

在 T7-A / SC-35 单实例锁引入后：

- 已有实例正在跑 → 新启动的进程**不做迁移**，直接发送激活消息给已有实例；
- 已有实例检测到自己是唯一持有者后再执行迁移；
- 迁移期间**不接受**其它实例激活（激活消息被丢弃 + 日志）。

### 7. 测试策略

- 每条 migration 必须自带**同目录 fixture**：`tests/focus_tomato_migrations/v<N>_to_v<N+1>_before.jsonl` + `_after.jsonl`；
- 契约测试遍历 fixture 目录，读入 before → 跑对应 migration → 比对 after；
- 链式测试：v1 → vN 的每条链路都要有 end-to-end 契约测试；
- 幂等测试：对已经是目标版本的数据再跑一次 migration，输出必须字节一致；
- 破坏性测试：中间 migration 抛错，验证 `.bak` 保留、目标文件未落盘、错误正确冒泡。

## Non-goals

- **不做**在线云同步或跨设备版本协调；
- **不做**自动版本降级；
- **不做**"用户可选是否升级"—— 迁移是启动路径的强制步骤（否则会话状态机无法建立）；
- **不做**跨文件的联合事务（三个文件各自独立迁移，一个文件失败不阻塞其它文件）。

## Consequences

- `focus_data_validator` 的 `UnsupportedSchemaVersion` 语义收窄为"检测到的版本比当前实现新"—— 旧版本不再直接拒绝；
- `file_focus_repository::read*` 增加返回类型：`variant<Data, NeedsMigration, Error>`；
- 每次 `kCurrentSchemaVersion` 提升必须 PR 里附上：
  - 新的 migration 实现
  - fixture before/after
  - 版本升级文档段落
  - Release notes 里的"数据格式变更"条目
- 视觉审查增加"迁移对话框" screenshot 基线；
- `WHATSUI_IMPLEMENTATION.md` 增加"数据迁移路径"章节，说明用户可见的行为。

## Verification（实现后必须覆盖）

- v1 → v2 (interruption) fixture 契约测试通过；
- v(N) → v(N) 幂等测试通过（多跑一次不改字节）；
- 中断 migration 后 `.bak` 存在、`.jsonl` 保留旧格式、目标文件未写；
- UI 层的迁移对话框在 Software backend 有截图基线；
- 单实例场景：并发启动 2 个进程时只有先启动的执行迁移；
- MigrationError 冒泡到应用层不会导致 crash 或 hang。
