# OpenLegend 逆向工程与行为真值

本文档集中说明 OpenLegend 的原版行为依据、逆向边界、B0–B9 验证状态与研究资料入口。

## 唯一行为真值

OpenLegend 的目标不是“玩法相近”或“能够通关”的现代复刻，而是让现代实现的可观察 Legacy 行为与当前原版一致。

> **一切以当前原版 `Z.COM`、`Z.DAT` 的机器码、完整汇编和原始资产为准。**

反编译伪码、研究文档、测试、开源端口和当前 C++ 实现都只是辅助证据；发生冲突时必须修正它们，而不是修改原版行为。

基本边界：

- 合法数据上的新增保护不得改变原版行为；
- 不修复原逻辑 BUG，不改善 AI，不改变数值、流程、随机数消费或像素覆盖顺序；
- `original` 保存原版行为基线；
- `main` 的现代配置、存档列表、输入 repeat 与 RGBA UI 不得传播到 `original`；
- 共享模块只能承载通用类型和原版合同。

## B0–B9 验证状态

- **B0 已完成**：C++20/CMake 工程、同步模式协调、SDL3 窗口与 Indexed framebuffer 上传。
- **B1 已完成**：普通 IDX、MMAP 特例、SDX/WDX 哨兵、RLE、字体、调色板和世界层读取。
- **B2 已完成**：framebuffer、RLE 四向裁剪、ASCII/Big5 字形、palette 淡变、shadow-mask 与地图深度顺序。
- **B3 已完成**：RANGER 六段、基线/工作副本/三槽 R/S/D、lossless snapshot 与逐字节回写。
- **B4 已完成**：IRQ1/set-1 键态、BIOS tick、原 LCG、Miles 命令顺序、8 槽 raw WAV 与 XMI/OPL3 音频。
- **B5 已完成**：标题/三槽逐像素流程、CFONT 姓名输入、初始属性、六项菜单、状态/物品基础 UI、隔离存读错误路径与 SDL 会话链。
- **B6 已完成**：五层世界、128×128 缓存、陆地/船移动、碰撞与入口、待机/天气周期和逐像素世界绘制。
- **B7 已完成**：场景、事件、对话与场景/世界往返；`scene-event` closure 为 100/100。
- **B8 已完成**：战斗入口、选择、玩家动作、AI、动画、结算与战后提交；battle closure 为 81/81。
- **B9 已完成**：577 项函数 catalog 全部分类，7 张 closure 表 349 行对应 284 个物理函数，全部收敛且 pending/unverified 为 0；Linux/Windows core 与 app 矩阵、Sanitizer、SDL smoke、全量资产、Golden、IDA 与原文件哈希门均已通过。

B0–B9 执行计划已经关闭。最终验收见 [`research/evidence/b9-final-acceptance.md`](research/evidence/b9-final-acceptance.md)，函数分母与分类见 [`research/evidence/function-catalog-coverage.md`](research/evidence/function-catalog-coverage.md)。

“能够启动”“能够探索”或“一场战斗可运行”只属于中间里程碑，不代表 1:1 还原完成。完整验收条件见 [`goal/execution-plan.md`](goal/execution-plan.md)。

## 已验证的原版数据合同

当前真实资产测试覆盖：

- 118 对 `.IDX/.GRP`；
- 84 对 `SDX/SMP` 与 26 对 `WDX/WMP`；
- 12,927 个普通非空 RLE 帧；
- 65,087 个哨兵索引非空 RLE 帧；
- `MMAP` 前 3,731 个有效索引；
- `FONT3.E16`、`FONT3.C16` 与 `MMAP.COL`；
- 五个 `480×480×int16le` 世界层；
- RANGER 六段、100 个 S 场景状态、100 个 D 事件状态、基线/工作副本和三槽存档；
- 84-byte 键盘翻译表、tick 舍入/回绕和 4 组独立 RNG 向量；
- 24 个 `ATK*.WAV`、53 个 `E*.WAV` 与 24 个 `GAME*.XMI`，逐项长度、格式与 XMI PCM 生成；
- 9 个标题帧、主/读档/等待逐像素 hash、CFONT 候选、四组新游戏 RNG 向量、双页状态/物品渲染与三槽成功/损坏/写失败会话链；
- 五层 128×128 cache hash、固定陆地/船轨迹、场景 70/IQ 条件、初始世界与 300 tick 半透明天气 framebuffer；
- `ALLSIN` 100 个六层场景、`ALLDEF` 100×200 条十一字段事件、2,977 条对话与 1,018 份 KDEF 脚本，以及场景输入、跳转、对话和事件 opcode 的独立 Golden；
- 140 条 `WAR.STA` 记录、26 个战场、92 套 `FIGHT` 资源共 4,992 帧、26 槽参战者状态，以及覆盖战斗入口、行动、AI、伤害、状态面板、结算/升级/练功/制造的 76 项独立 Golden。

## 证据入口

- [资源加载合同](research/evidence/resource-loader-1to1.md)
- [渲染合同](research/evidence/render-1to1.md)
- [模型与存档合同](research/evidence/model-persistence-1to1.md)
- [输入、时间、随机与音频合同](research/evidence/input-time-random-audio-1to1.md)
- [标题、菜单与新游戏合同](research/evidence/title-menu-new-game-1to1.md)
- [世界地图合同](research/evidence/world-map-1to1.md)
- [场景、事件与对话合同](research/evidence/scene-event-dialogue-1to1.md)
- [战斗合同](research/evidence/battle-1to1.md)
- [原程序架构](research/architecture/program-architecture.md)
- [现代实现架构](research/architecture/rewrite-architecture.md)
- [函数 catalog 覆盖](research/evidence/function-catalog-coverage.md)
- [研究资料总索引](research/README.md)

## 原版数据与 Oracle 边界

仓库不包含、也不会分发原版游戏文件。原版资源、可执行文件和存档只读使用，不得提交；测试、构建和临时生成物必须写入仓库内已忽略目录。

原 DOS 程序动态运行 oracle 因当前环境缺少可执行宿主而登记为 `blocked_runtime_oracle`。该状态不冒充现代 CTest 或独立资产 oracle，也不掩盖未登记的产品差异。

测试和 Golden 不能以当前 C++ 实现自证正确。新增或修复行为必须能够追溯到机器码、原始资产或已经登记的独立 oracle。

## IDA 工作流

- 使用 headless IDA 批处理，不手工依赖 GUI 状态；
- 正式 `.i64` 数据库不得直接交给并发批处理任务；
- 由主会话统一复制临时数据库并调度 IDAT；
- 临时文件写入 `tmp/`；
- IDAPython 批处理使用 `ida_pro.qexit(0)` 或 `ida_pro.qexit(1)` 明确结束；
- 每个已确认结论写入对应函数证据、模块 closure 或专项合同文档。
