# OpenLegend 首版重写架构

版本：v1
状态：首轮模块与依赖冻结稿
原则：架构优先、逐模块闭环，不按反编译函数粒度无限拆分产品文件

## 1. 固定原则

1. 原版机器行为、原始资源字节和可重复校验结果是兼容真值；IDA 伪码用于导航，不单独作为实现依据。
2. 先冻结模块职责、依赖方向和状态所有权；模块内部只在进入对应工作包后继续逆向。
3. 每份可变状态只有一个所有者。跨模块修改通过窄接口、同步请求/结果或显式快照完成。
4. 原程序同一调用栈、同一 tick 内生效的请求，现代实现也同步消费；不引入异步事件总线。
5. SDL3 和宿主 OS 类型只存在于平台后端；游戏核心不包含 VGA、DOS 中断或 SDL 类型。
6. 保留 `320×200` indexed framebuffer、原调色板、原资源 ID 和整数行为；DOS 索引字节在最终显示边界经受测兼容层逐像素转换为现代 RGBA8，窗口只做 nearest-neighbor 居中整数倍缩放，二者都不得反写核心缓冲。
7. 不提前设计 ECS、脚本框架、通用服务定位器或大规模继承层次。
8. 不按每个反编译函数创建一个生产文件；按业务职责组织，地址只出现在研究证据和测试说明中。

## 2. 目标模块

| CMake target | 职责 | 拥有状态 | 允许主要依赖 |
|---|---|---|---|
| `openlegend_compat` | 固定宽度类型、小端读取、legacy ID、结果码、字节视图 | 无业务可变状态 | 无 |
| `openlegend_resource` | 文件定位、IDX/GRP、SDX/WDX、地图层、RLE 帧、字体/调色板原始视图 | 文件缓存和已验证字节块 | compat |
| `openlegend_model` | 角色、物品、武功、商店、场景元数据和全局会话的规范化模型 | 当前 `GameState` | compat, resource |
| `openlegend_input` | set-1 键码翻译、last-key、按下/重复/释放 byte state | IRQ1 兼容键态 | compat |
| `openlegend_time` | PIT/BIOS tick 量化、tick 边沿等待与原 delay 除法 | 单调时钟锚点 | 无 |
| `openlegend_random` | 原 32-bit LCG、显式 seed 与消费流 | RNG state | 无 |
| `openlegend_render` | indexed framebuffer、RLE 精灵、文字、地图投影、画面效果、呈现请求 | framebuffer、palette、字形缓存 | compat, resource |
| `openlegend_audio` | Miles 顺序、raw WAV 八槽、XMI 合成和设备无关 mixer | 曲目、音效与播放状态 | resource；私有依赖 libADLMIDI |
| `openlegend_world` | 五层世界、128×128 缓存、移动/船/碰撞、入口、待机与天气 | 世界运行时状态 | resource, model, random, render |
| `openlegend_scene` | 64×64 场景、事件、对话和场景流程 | 场景会话与事件执行状态 | compat, resource, model, input, time, random, render, audio |
| `openlegend_ui` | 标题、菜单、状态、物品、商店、存读档选择和模态对话 | UI 模式状态 | compat, model, input, time, render, audio |
| `openlegend_battle` | 战斗建立、角色临时态、动作/AI、胜负出口 | 战斗会话 | compat, resource, model, input, time, random, render, audio |
| `openlegend_persistence` | 基线/工作副本/三槽格式、精确读取、快照导入导出 | 槽元数据和 I/O 事务 | compat, resource, model |
| `openlegend_app` | 生命周期、模式协调、同步请求消费、初始化与销毁顺序 | 顶层模式和转换请求 | 全部核心模块与平台端口 |
| `openlegend_platform_sdl3` | 窗口、事件映射、音频 stream、文件路径、indexed 画面上传 | SDL 句柄和宿主资源 | compat, input, time, audio, SDL3 |

`diagnostics` 首版保持为少量公共设施，不必先拆成独立业务 target；逆向工具永远不进入产品 target。

## 3. 依赖方向

```text
openlegend_app
├─ openlegend_persistence ──→ openlegend_model ──→ openlegend_resource
├─ openlegend_world ────────→ model/resource/random/render
├─ openlegend_scene ────────→ model/resource/input/time/random/render/audio
├─ openlegend_ui ───────────→ model/input/time/render/audio
├─ openlegend_battle ───────→ model/resource/input/time/random/render/audio
├─ openlegend_platform_sdl3 → compat/input/time/audio/SDL3
├─ openlegend_time / openlegend_random
└─ openlegend_compat  ← 所有 target 的共同底层
```

`world`、`scene`、`ui`、`battle` 彼此不直接链接。它们向 `app` 返回同步结果，由 `app` 在原程序对应顺序中切换模式。

## 4. 切断旧循环依赖

| 旧式关系 | OpenLegend 合同 |
|---|---|
| 世界函数直接进入场景或菜单 | `WorldStepResult` 返回原始请求及参数，`app` 同步消费；机器码世界循环不直接开始战斗 |
| 场景事件直接调用战斗或世界主循环 | `SceneStepResult` 返回 `StartBattle` / `ReturnWorld` 等结果 |
| UI 直接写世界、战斗和存档全局区 | UI 返回命令；模型修改走 owner API；存档走 snapshot |
| 战斗结束直接跳回调用者内部状态 | `BattleStepResult` 保留明确出口，`app` 提交战后更新并恢复来源模式 |
| 存档代码覆盖任意裸内存 | `persistence` 解析为 `GameSnapshot`，`model` 校验并导入 |
| 渲染函数拥有剧情/战斗状态 | 业务模块产生 draw command；`render` 只拥有表现状态 |
| 业务代码直接读 SDL/OS 句柄 | 只依赖 `compat` 中定义的平台端口 |

禁止为“解耦”引入异步事件总线；原程序的同步顺序是兼容行为的一部分。

## 5. 顶层运行时合同

### 5.1 模式协调

```cpp
using AppMode = std::variant<
    StartupMode,
    TitleMode,
    WorldMode,
    SceneMode,
    UiMode,
    BattleMode,
    ExitMode>;
```

这里只表示所有权方向，不要求按此代码原样实现。`app` 每个逻辑 tick 只驱动当前模式，并立即处理其同步结果。

### 5.2 结果类型

```text
WorldStepResult  = Stay | Moved | EnterScene | OpenUi
SceneStepResult  = Stay | ReturnWorld | OpenUi | StartBattle | Quit
UiStepResult     = Stay | Close | NewGame | LoadSlot | SaveSlot | Quit
BattleStepResult = Stay | Victory | Defeat | Escape
```

结果携带原始场景号、战斗号、坐标、槽号等值，不在 `app` 中重新解释业务字段。

### 5.3 `GameState`

`openlegend_model` 是原 `RANGER.GRP` 及相关可变记录的唯一规范化所有者：

- 角色与队伍；
- 物品与库存；
- 武功与成长；
- 场景元数据与全局标志；
- 商店和可持久化全局变量。

世界、场景、UI 和战斗只获得窄视图或命令接口。模块自己的瞬时状态仍由模块自己拥有，不塞入一个万能 `GameState`。

## 6. 资源与序列化边界

### 6.1 原始视图与规范化模型分离

- `resource` 提供经过边界检查的原始字节视图和格式解析结果。
- `model` 把已理解字段转换为显式类型。
- 未知但必须 round-trip 的字节保留在 lossless sidecar 中，不凭猜测清零。
- `persistence` 负责物理文件合同和导入/导出顺序，不拥有规则。

### 6.2 强类型 legacy ID

首版至少定义：

- `SpriteId`：保留原始偶数编码，并提供受检索引转换；
- `SceneId`、`BattleId`、`TalkId`、`CharacterId`、`ItemId`；
- `PaletteIndex`、`WorldCoord`、`SceneCoord`。

禁止在跨模块接口中长期传递含义不明的裸 `int`。

### 6.3 文本

- 原版 Big5/XOR 字节作为兼容输入保留。
- 原字体渲染直接消费 `LegacyTextView`，避免 Unicode 往返破坏字形索引。
- 工具、日志和未来本地化可使用解码后的 UTF-8 视图。
- 文本解码不进入 `render` 的业务逻辑；`render` 只接收明确的 legacy 字形或现代文本命令。

## 7. 渲染架构

核心渲染输出固定为：

```text
IndexedFramebuffer
  width  = 320
  height = 200
  pixels = 64,000 palette indices
  palette = 256 × RGB6
```

建议内部层次：

1. `SpriteArchiveView`：只定位和验证帧；
2. `RleSpriteRenderer`：偏移、透明 run 和裁剪；
3. `LegacyFontRenderer`：8×16 ASCII、16×16 Big5、64 字形缓存合同；
4. `WorldRenderer` / `SceneRenderer`：产生有序绘制命令；
5. `IndexedFramebuffer`：最终像素真值；
6. `compat` 显示转换：对每个 index 读取 RGB6 palette 项，以位复制展开为 RGBA8；该纯转换必须有端点和任意 palette index 单测；
7. `platform_sdl3`：上传 RGBA8 streaming texture，以 nearest-neighbor 居中整数倍缩放，输出尺寸不足 `320×200` 时拒绝呈现并由窗口最小尺寸约束阻止该状态。

不得把 DOS framebuffer 的 index 字节直接当现代颜色提交，也不得先把原精灵解码为 RGBA 后再把 RGBA 当游戏真值；否则会破坏调色板动画、覆盖顺序和逐像素验证。现代显示转换是明确的兼容边界，不是对核心像素算法的改写。

## 8. 时间、输入与音频端口

平台只提供：

- 单调时间或逻辑 tick 采样；
- 键盘/手柄的宿主事件；
- 音频设备提交；
- 让出/睡眠；
- 文件根目录和窗口生命周期。

核心负责：

- 以 `1,193,182 / 65,536 Hz` 生成 BIOS tick，并在 `0x1800B0` 回绕；主循环只等待值变化，不做宿主 delta-time 补帧；
- 84-byte set-1 翻译表、相邻内存别名、last-key 和 256 项 byte state；
- `state = state * 0x41C64E6D + 0x3039` 的显式 32-bit RNG 流；
- 8 个 raw unsigned mono sample slot、强制 11025 Hz、legacy volume/loop 参数和 Miles 调用顺序；
- XMI 内存加载、无限循环、停止与 2000/1000 淡入淡出边界。

B4 冻结 libADLMIDI v1.6.1，只启用 DOSBox OPL3、内嵌 AIL bank 0、AIL volume model、MIDI sequencer 与 XMI；具体 C API 和类型不泄漏到业务模块公共接口。SDL3 只从 mixer 拉取 interleaved S16，并在设备不可用时静默降级。

## 9. 正式目录

```text
OpenLegend/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ cmake/
├─ include/openlegend/
│  ├─ compat/ resource/ model/ input/ time/ random/ render/ audio/
│  └─ world/ scene/ ui/ battle/ persistence/ app/
├─ src/
│  ├─ platform/sdl3/
│  ├─ resource/ model/ input/ time/ random/ render/ audio/
│  └─ world/ scene/ ui/ battle/ persistence/ app/
├─ tests/
│  ├─ support/
│  ├─ unit/<module>/
│  ├─ assets/
│  └─ integration/
├─ research/
│  ├─ architecture/
│  ├─ ida/
│  ├─ formats/
│  ├─ evidence/
│  └─ tools/
└─ goal/
   └─ execution-plan.md
```

公共头只放其他模块确实需要的值和接口；解析器内部结构、缓存和状态机实现留在 `src/<module>/`。

## 10. 文件与命名规则

采用业务名，而不是地址名：

```text
resource/idx_grp_archive.cpp
resource/legacy_sprite_frame.cpp
render/indexed_framebuffer.cpp
render/rle_sprite_renderer.cpp
render/legacy_font_renderer.cpp
world/world_map.cpp
render/world_projection.cpp
render/world_depth_order.cpp
scene/scene_event_runtime.cpp
battle/battle_session.cpp
persistence/save_slot.cpp
app/mode_coordinator.cpp
```

`legacy_` 只用于刻意保留的原格式/算法合同，不应成为所有文件的默认前缀。反汇编地址放在 `research/evidence`，测试名描述行为，不以地址作为主要业务命名。

## 11. 测试组织

每个模块至少有三类验证：

1. **纯单元测试**：边界、整数、状态迁移和错误路径；
2. **真实资产测试**：在原数据目录存在时验证全部索引、尺寸、RLE、记录和 round-trip；
3. **集成/黄金测试**：固定输入和状态生成 framebuffer hash、调色板、请求序列或存档字节。

首批关键黄金量：

- 全量 `.IDX/.GRP` 边界与 RLE 解码；
- 字体 glyph bitmap；
- 固定世界坐标的 320×200 indexed frame；
- 固定输入序列的移动请求；
- 一个槽的无损读取/写回；
- 一场战斗的建立与四类出口。

测试 CMake 按模块拆分，避免形成一个数千行总清单。

## 12. 工程方法边界

采用：

- 架构阶段立即设停止线；
- 一个状态一个所有者；
- `app` 同步协调上行请求；
- 公共头、私有实现和模块镜像测试；
- 研究证据与产品构建隔离；
- 逐模块“逆向 → 规格 → 测试 → 实现 → 真实数据验证”。

不复制：

- 在架构阶段要求理解所有函数；
- 以反编译函数为粒度无限拆文件；
- 数百页流水式 GOAL 变更记录；
- 单个巨型 `tests/CMakeLists.txt`；
- 用已有 C++ 或测试通过反向证明原程序行为。

## 13. 架构冻结条件

本稿发布后，只有新证据证明以下内容错误时才修改顶层架构：

- 模块所有权；
- 主要依赖方向；
- 顶层同步切换顺序；
- 平台与业务边界；
- indexed framebuffer 作为像素真值的决定。

模块内部字段、opcode、战斗公式和特效细节不得成为延长全局调研的理由。
