# OpenLegend 调研索引

当前状态：**功能实现已推进至B9，但 inventory 中仍有 `pending_*` 与 `implemented_pending_review`；统一最终汇编↔C++ REVIEW尚未开始。当前停点以 [`../goal/execution-plan.md`](../goal/execution-plan.md) 为准。**

## 执行与架构入口

- [`../goal/execution-plan.md`](../goal/execution-plan.md)：最小启动读集、当前停点和按需索引。
- [`../goal/execution-plan-details.md`](../goal/execution-plan-details.md)：项目目标、B0–B9范围及开始/完成门禁。
- [`inventory/README.md`](inventory/README.md)：函数、owner、closure和REVIEW状态合同。
- [`modules/README.md`](modules/README.md)：单模块work package合同。
- [`architecture/program-architecture.md`](architecture/program-architecture.md)：原DOS程序模块、状态和运行时交互。
- [`architecture/rewrite-architecture.md`](architecture/rewrite-architecture.md)：OpenLegend模块、依赖、目录和测试组织。

## 阶段综合证据

- [`evidence/resource-loader-1to1.md`](evidence/resource-loader-1to1.md)：B1 资源读取。
- [`evidence/render-1to1.md`](evidence/render-1to1.md)：B2 软件绘制。
- [`evidence/model-persistence-1to1.md`](evidence/model-persistence-1to1.md)：B3 状态模型与物理存档。
- [`evidence/input-time-random-audio-1to1.md`](evidence/input-time-random-audio-1to1.md)：B4 IRQ1 输入、BIOS tick、RNG 与 Miles 音频。
- [`evidence/title-menu-new-game-1to1.md`](evidence/title-menu-new-game-1to1.md)：B5 标题、六项菜单、姓名输入、新游戏属性与三槽流程。
- [`evidence/world-map-1to1.md`](evidence/world-map-1to1.md)：B6 五层世界、缓存、移动、船、入口、天气与绘制。
- [`evidence/scene-event-dialogue-1to1.md`](evidence/scene-event-dialogue-1to1.md)：B7 场景、事件、脚本与对话。
- [`evidence/battle-1to1.md`](evidence/battle-1to1.md)：B8 战斗建立、行动、AI、绘制、战果与战后提交。
- [`evidence/persistence-integration-1to1.md`](evidence/persistence-integration-1to1.md)：B9 新游戏、编号槽、会话交接、退出与日志整合。

这些文档记录实现和验证证据，不覆盖 inventory 的最终REVIEW状态。

## 独立 golden

- [`evidence/title-menu-new-game-goldens.json`](evidence/title-menu-new-game-goldens.json)：B5 标题 framebuffer 与初始属性 RNG。
- [`evidence/world-map-goldens.json`](evidence/world-map-goldens.json)：B6 五层 cache、固定轨迹、初始/天气 framebuffer。
- [`evidence/scene-goldens.json`](evidence/scene-goldens.json)：B7 场景、事件和对话固定轨迹。
- [`evidence/battle-goldens.json`](evidence/battle-goldens.json)：B8 战斗资源、状态与帧序列。
- [`evidence/battle-player-status-golden.json`](evidence/battle-player-status-golden.json)：B8 共享角色状态界面。

## 定点机器码报告

- [`ida/reports/Z_DAT.b4_runtime_xrefs.txt`](ida/reports/Z_DAT.b4_runtime_xrefs.txt)：输入、时间、随机、音频与平台调用边界。
- [`ida/reports/Z_DAT.b5_ui_xrefs.txt`](ida/reports/Z_DAT.b5_ui_xrefs.txt)：B5 UI/新游戏/三槽调用与数据引用。
- [`ida/reports/Z_DAT.b6_world_xrefs.txt`](ida/reports/Z_DAT.b6_world_xrefs.txt)：B6 世界主循环、五层缓存、碰撞、深度与天气 alpha。
- [`ida/reports/Z_DAT.b7_scene_xrefs.txt`](ida/reports/Z_DAT.b7_scene_xrefs.txt)：B7 场景、事件和脚本调用边界。
- [`ida/reports/Z_DAT.b8_battle_xrefs.txt`](ida/reports/Z_DAT.b8_battle_xrefs.txt)：B8 战斗函数有限范围与调用关系。
- [`ida/reports/Z_DAT.persistence_xrefs.txt`](ida/reports/Z_DAT.persistence_xrefs.txt)：B9 持久化与会话整合边界。

## IDA headless 档案

- `ida/scripts/`：IDAPython 只读导出与定点分析脚本。
- `ida/databases/`：`Z.COM` / `Z.DAT` IDA 数据库。
- `ida/logs/`：本地 `idat.exe -A` 运行日志；可重复生成，已忽略且不提交。
- `ida/reports/`：函数、伪码、定点调用链及输入/字体报告。

所有 IDA 分析均使用 `idat.exe -A` headless；不得启动 GUI 分析器。

## 调研停止线

已确认的启动、资源、会话、世界、场景、战斗、渲染、输入、音频和存档边界足以指导工程结构。后续只在进入具体模块工作包时补充该模块公共合同所需证据，不再做无边界逐函数调研。
