# ADR-009: Audio Subsystem for FocusTomato Soundscapes

状态：Draft（决策待议）
起草人：项目负责人
日期：2026-08-06

本 ADR 决定 FocusTomato 音频后端的选型与线程边界，不实现具体音频资产管线。目的是让 T7-B 子任务（SC-42/43/78/82/83/84）沿着统一契约推进。

## Context

本轮 platform 层已经冻结了任务级 `soundscapeIdSnapshot`，会话记录里也保留声音快照。但当前**没有播放器**：

- 声音 ID 是不透明字符串，没有对应资源；
- 会话开始不发声、切换不触发 crossfade；
- 系统音频设备切换、静音、失败降级完全未处理；
- 白噪音预览（不影响会话）也未实现。

FocusTomato 声音需求的**独特性**：

- 会话内声音是**持续 25 分钟**级的循环播放，不是一次性 SFX；
- 用户可能**同时**试听（预览）与主会话播放；
- 失败必须降级到静音——**不能把音频错误上升为会话错误**；
- 主进程可能在专注中被系统压制到后台，音频不能中断。

## Decision（初稿）

### 1. 后端选型：miniaudio

比较候选：

| 后端 | 头文件级 | 跨平台 | 依赖 | 许可 | 是否引入 |
| --- | --- | --- | --- | --- | --- |
| miniaudio | 是（单头） | Win/macOS/Linux/BSD | 无 | Public Domain / MIT-0 | **✓ 选中** |
| cubeb (Mozilla) | 否，需构建 | Win/macOS/Linux/Android | 需 CMake 集成 | ISPC | 备选 |
| OpenAL Soft | 否，需构建 | 全平台 | 大 | LGPL | 排除（LGPL 增加合规成本） |
| SDL_mixer | 否，需 SDL | 全平台 | 需 SDL | ZLIB | 排除（引入 SDL 太重） |
| SFML::Audio | 否 | 全平台 | 依赖 OpenAL | ZLIB | 排除（依赖链） |

**miniaudio 优势**：单头文件、Public Domain、Windows/macOS/Linux 后端天然抽象、内置 device notification（对 SC-43 有帮助）、支持流式解码、性能足够。

**miniaudio 劣势**：单头 = TU 编译时间敏感（需要用 `MINIAUDIO_IMPLEMENTATION` 精确控制）；文档不如 SDL 完善。

### 2. 线程边界

```
+---------------------+       +-----------------------+
| FocusRouter UI      |       | AudioService          |
| (UI thread)         | ----> | (UI thread facade)    |
+---------------------+       +-----------+-----------+
                                          |
                                          v
                              +-----------------------+
                              | miniaudio device thread|
                              | (managed by miniaudio) |
                              +------------------------+
```

- UI 线程调用的所有 `AudioService::*` 方法**不能阻塞**；
- miniaudio 的音频回调**绝不**触碰 WhatsUI 节点树或 ViewModel；
- 音频状态变化（正在播放 / 已失败 / 已切换设备）通过 `UiDispatcher::post` 投递回 UI 线程；
- 与 `DesktopServices::publishEvent` 使用相同的 shared-owned channel 模式，避免退出期悬垂。

### 3. `AudioService` 契约

```
class AudioService {
public:
    virtual ~AudioService() = default;

    virtual AudioCapabilities capabilities() const noexcept = 0;
    virtual std::vector<SoundscapeInfo> listSoundscapes() const = 0;

    // 会话播放：稳定 ID，无阻塞，失败降级到静音后 post 回失败事件。
    virtual void startSessionPlayback(std::string soundscapeId) = 0;
    virtual void stopSessionPlayback() = 0;
    virtual void setSessionVolume(float linear01) = 0;

    // 预览：与会话播放隔离，可同时进行。
    virtual void previewSoundscape(std::string soundscapeId) = 0;
    virtual void stopPreview() = 0;

    // 状态订阅：所有回调都在 UI 线程执行。
    virtual void setStateHandler(AudioStateHandler handler) = 0;
    virtual void setStateDispatcher(AudioStateDispatcher dispatcher) = 0;
};

struct AudioCapabilities {
    bool sessionPlayback{false};
    bool preview{false};
    bool deviceHotplug{false};
    bool crossfade{false};
};

struct AudioStateEvent {
    AudioStateEventKind kind;   // Started/Stopped/Failed/DeviceChanged
    std::string soundscapeId;
    std::string message;
};
```

**为什么与 `DesktopServices` 平行而非合并**：桌面服务是"进程级壳"，音频是"进程级引擎"；合并会让 tray/notification 卡在音频线程边界。

### 4. Soundscape ID 与资产

- `soundscapeId` 是**稳定 slug**（例如 `rain`, `whitenoise`, `campfire`）；
- ID → 文件路径的解析由 `SoundscapeCatalog` 提供，可以通过配置扩展；
- 未识别 ID → `AudioService` 报 `Failed`，会话继续但静音；
- 内置声音打包为 `assets/soundscapes/*.ogg`（Vorbis 兼顾体积与许可）。

### 5. 失败降级契约（SC-42）

- 找不到资产、解码失败、设备失败 → `AudioStateEvent::Failed` 上抛；
- **绝不**影响 `TimerStore` 状态；
- UI 显示"当前会话静音"的 pill，用户可点击重试；
- 连续 3 次失败在同一会话内 → UI 不再重试直到用户显式操作。

### 6. 设备热插拔（SC-43）

miniaudio 的 device notification 用于：

- 触发 `AudioStateEventKind::DeviceChanged`；
- `AudioService` 自动切换到系统新默认设备；
- UI 只在切换失败时提示。

## 待议决策

1. **是否内置声音资产**：初稿说"是"，打包 3-5 个（rain, whitenoise, campfire, ocean, forest）。若产品希望用户自导入，需要走 T7-C 导入路径。**倾向：内置最小集**。
2. **声音资产格式**：Vorbis (.ogg) vs Opus (.opus)。Vorbis 兼容更广，Opus 体积更小但要检查 miniaudio 支持。**倾向：Vorbis**。
3. **音频与会话开始的耦合**：初稿说"会话开始 → startSessionPlayback"。是否允许延迟启动（用户先看到"3, 2, 1"再播）？**倾向：立即开始，但用户可全局禁用**。
4. **crossfade**：初稿说"可选"。会话之间切换声音（例如休息切静音）需要 crossfade 避免突兀，但 miniaudio 不原生支持——需要在 mixer 层手写。**倾向：v0.3 preview 引入基础 crossfade，未来优化**。

## Consequences

- CMake 引入 miniaudio 单头依赖（`third_party/miniaudio`），Package 里加入 audio 组件；
- FocusTomato core 依赖 `AudioService`（可注入 mock 用于测试）；
- 测试新增：`AudioServiceMock` + 契约测试（`startSessionPlayback` 失败不炸、状态回 UI 线程、预览与主会话独立）；
- 会话记录不加"是否成功播放"字段（初稿）；音频失败通过通知/统计外的 UI 层显式表达；
- Release checklist 增加"音频设备热插拔实机验证"项。

## Non-goals

- 不实现均衡器 / 空间音频 / 高级 mixer；
- 不实现"根据情境自动选音"这种 AI feature；
- 不实现节拍器（会话结束提示音是单独的 SFX 通道，非 soundscape）；
- 不为 v0.2 preview 支持自定义声音导入。

## Verification（实现后需要覆盖）

- `startSessionPlayback` 在 UI 线程 < 5ms 返回；
- 解码失败不 crash、不阻塞会话计时；
- 设备切换后 500ms 内恢复播放；
- 预览与会话播放独立音量控制；
- 10 次连续启停不 leak（sanitizer 通过）；
- 音频线程回调完全不触碰 WhatsUI 节点（可用 ThreadSanitizer 校验）。
