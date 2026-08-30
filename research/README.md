# OpenLegend 调研索引

当前状态：**B0–B4 已按汇编与真实资产完成；当前进入 B5 标题、菜单与新建/读取流程。**

## 架构主文档

- [`architecture/program-architecture.md`](architecture/program-architecture.md)：原 DOS 程序模块、状态和运行时交互。
- [`architecture/rewrite-architecture.md`](architecture/rewrite-architecture.md)：OpenLegend 模块、依赖、目录和测试组织。
- [`../goal/execution-plan.md`](../goal/execution-plan.md)：阶段计划、开始/完成条件和当前停止线。

## 已完成阶段证据

- [`evidence/resource-loader-1to1.md`](evidence/resource-loader-1to1.md)：B1 资源读取。
- [`evidence/render-1to1.md`](evidence/render-1to1.md)：B2 软件绘制。
- [`evidence/model-persistence-1to1.md`](evidence/model-persistence-1to1.md)：B3 状态模型与物理存档。
- [`evidence/input-time-random-audio-1to1.md`](evidence/input-time-random-audio-1to1.md)：B4 IRQ1 输入、BIOS tick、RNG 与 Miles 音频。

## IDA headless 档案

- `ida/scripts/`：IDAPython 只读导出与定点分析脚本。
- `ida/databases/`：`Z.COM` / `Z.DAT` IDA 数据库。
- `ida/logs/`：`idat.exe -A` 日志。
- `ida/reports/`：函数、伪码、定点调用链及输入/字体报告。

所有 IDA 分析均使用 `idat.exe -A` headless；不得启动 GUI 分析器。

## 调研停止线

已确认的启动、资源、会话、世界、场景、战斗、渲染、输入、音频和存档边界足以指导工程结构。后续只在进入具体模块工作包时补充该模块公共合同所需证据，不再做无边界逐函数调研。
