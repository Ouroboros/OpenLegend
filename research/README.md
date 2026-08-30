# OpenLegend 调研索引

当前状态：**B0–B6 已按汇编与真实资产完成；当前进入 B7 场景、事件与脚本。**

## 架构主文档

- [`architecture/program-architecture.md`](architecture/program-architecture.md)：原 DOS 程序模块、状态和运行时交互。
- [`architecture/rewrite-architecture.md`](architecture/rewrite-architecture.md)：OpenLegend 模块、依赖、目录和测试组织。
- [`../goal/execution-plan.md`](../goal/execution-plan.md)：阶段计划、开始/完成条件和当前停止线。

## 已完成阶段证据

- [`evidence/resource-loader-1to1.md`](evidence/resource-loader-1to1.md)：B1 资源读取。
- [`evidence/render-1to1.md`](evidence/render-1to1.md)：B2 软件绘制。
- [`evidence/model-persistence-1to1.md`](evidence/model-persistence-1to1.md)：B3 状态模型与物理存档。
- [`evidence/input-time-random-audio-1to1.md`](evidence/input-time-random-audio-1to1.md)：B4 IRQ1 输入、BIOS tick、RNG 与 Miles 音频。
- [`evidence/title-menu-new-game-1to1.md`](evidence/title-menu-new-game-1to1.md)：B5 标题、六项菜单、姓名输入、新游戏属性与三槽流程。
- [`evidence/world-map-1to1.md`](evidence/world-map-1to1.md)：B6 五层世界、缓存、移动、船、入口、天气与绘制。

## 独立 golden

- [`evidence/title-menu-new-game-goldens.json`](evidence/title-menu-new-game-goldens.json)：B5 标题 framebuffer 与初始属性 RNG。
- [`evidence/world-map-goldens.json`](evidence/world-map-goldens.json)：B6 五层 cache、固定轨迹、初始/天气 framebuffer。

## 定点机器码报告

- [`ida/reports/Z_DAT.b5_ui_xrefs.txt`](ida/reports/Z_DAT.b5_ui_xrefs.txt)：B5 UI/新游戏/三槽调用与数据引用。
- [`ida/reports/Z_DAT.b6_world_xrefs.txt`](ida/reports/Z_DAT.b6_world_xrefs.txt)：B6 世界主循环、五层缓存、碰撞、深度与天气 alpha。

## IDA headless 档案

- `ida/scripts/`：IDAPython 只读导出与定点分析脚本。
- `ida/databases/`：`Z.COM` / `Z.DAT` IDA 数据库。
- `ida/logs/`：`idat.exe -A` 日志。
- `ida/reports/`：函数、伪码、定点调用链及输入/字体报告。

所有 IDA 分析均使用 `idat.exe -A` headless；不得启动 GUI 分析器。

## 调研停止线

已确认的启动、资源、会话、世界、场景、战斗、渲染、输入、音频和存档边界足以指导工程结构。后续只在进入具体模块工作包时补充该模块公共合同所需证据，不再做无边界逐函数调研。
