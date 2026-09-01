# OpenLegend 执行计划

版本：v1

定位：本文件只负责**启动执行、定位当前工作和索引权威资料**。阶段定义、技术合同、函数数据、验证记录和历史结论分别保留在其专属文档中；不得把这些细节复制回本文件。

## 1. 最小启动读集

新会话、上下文压缩恢复或暂停后继续时，按以下顺序读取：

1. 完整读取 [`../AGENTS.md`](../AGENTS.md) 与 `/root/.pi/agent/APPEND_SYSTEM.md`；
2. 读取本文件，确认当前工作指针和按需索引；
3. 用 Git 确认工作树、分支和最近提交，不覆盖用户改动；
4. 只读取当前工作包对应的 module、closure 行和 evidence；
5. 需要判断阶段门禁时，再读取 [`execution-plan-details.md`](execution-plan-details.md) 的对应章节。

除非当前任务需要，不预读全部函数证据、全部 closure、全部 golden 或完整 IDA 报告。

## 2. 权威来源与信息所有权

| 问题 | 权威来源 | 本文件是否重复 |
| --- | --- | --- |
| 当前原版行为是什么 | `Z.COM` / `Z.DAT` 机器码、完整汇编、原始资源字节 | 否 |
| Agent、进程、BUILD、Git、TG 如何执行 | [`../AGENTS.md`](../AGENTS.md) 与 `/root/.pi/agent/APPEND_SYSTEM.md` | 否 |
| 项目目标、B0–B9范围、开始/完成门禁 | [`execution-plan-details.md`](execution-plan-details.md) | 否 |
| 函数范围、owner、closure与REVIEW状态 | [`../research/inventory/README.md`](../research/inventory/README.md) 及各TSV | 否 |
| 当前模块范围、接口、状态所有权和下一停点 | [`../research/modules/README.md`](../research/modules/README.md) 及对应work package | 否 |
| 单函数/紧耦合组的ABI、基本块和差异证据 | `../research/evidence/functions/` | 否 |
| 阶段综合证据、golden和已知偏差 | `../research/evidence/` | 否 |
| 原程序与现代架构边界 | `../research/architecture/` | 否 |
| IDA导航材料与可重复导出 | `../research/ida/` | 否 |

冲突处理：

- 行为结论冲突时，无条件回到当前机器码和原始资源；
- 状态冲突时，以 inventory 的未关闭状态为准，不从测试、摘要或现有C++继承完成结论；
- 执行方式冲突时，以最新完整读取的 `AGENTS.md` 和 `APPEND_SYSTEM.md` 为准；
- module/evidence 与 inventory 不一致时，先修正文档和 inventory，再继续实现或 REVIEW。

## 3. 当前执行指针

当前处于 **B0–B9 功能缺口收敛阶段**；统一最终汇编↔C++ REVIEW 尚未开始。最近完成的独立切片是游戏菜单医疗、解毒和共享角色状态界面，状态仍为 `implemented_pending_review`。

### 当前唯一业务停点

完成共享角色选择器尚未覆盖的参数与调用方业务，重点是参数3..5及参数6离队流程。开始前只需读取：

- [`../research/inventory/ui-closure.tsv`](../research/inventory/ui-closure.tsv)
- [`../research/inventory/input-font-closure.tsv`](../research/inventory/input-font-closure.tsv)
- [`../research/evidence/functions/Z_DAT/0x22090.md`](../research/evidence/functions/Z_DAT/0x22090.md)
- [`../research/evidence/title-menu-new-game-1to1.md`](../research/evidence/title-menu-new-game-1to1.md)
- 与离队调用链直接相关的函数证据；不要预读无关模块全部材料。

### 后续固定顺序

1. 补齐共享选择器与离队业务后，继续映射其余 `pending_mapping` / `pending_implementation` closure；
2. 功能缺口清零后，从B0开始按锁定workpack执行不限次数双向逐基本块REVIEW；
3. 每次发现差异都同步修正实现、测试、evidence与inventory，并从函数入口重启完整REVIEW；
4. 全部closure关闭后，执行 [`execution-plan-details.md`](execution-plan-details.md) 第9节规定的全集成验收。

已知但不改变当前停点的问题：

- 本机缺少DOS运行oracle，动态原程序差分保持 `blocked_runtime_oracle`；
- 世界人物移动后消失的用户报告尚未复现，诊断范围见 [`../research/modules/world-map.md`](../research/modules/world-map.md)；
- runtime/platform专项closure尚未建立完整覆盖，见 [`../research/modules/runtime-platform.md`](../research/modules/runtime-platform.md)。

## 4. 按需读取索引

| 阶段/任务 | 首读资料 | 需要时再读 |
| --- | --- | --- |
| B0 工程、runtime、SDL边界 | [`../research/modules/runtime-platform.md`](../research/modules/runtime-platform.md) | 架构文档、runtime/audio closure、平台函数evidence |
| B1 资源格式 | [`../research/evidence/resource-loader-1to1.md`](../research/evidence/resource-loader-1to1.md) | 资源IDA报告、资产扫描工具与测试 |
| B2 像素、字体、呈现 | [`../research/evidence/render-1to1.md`](../research/evidence/render-1to1.md) | input-font/UI closure、golden生成器 |
| B3 模型与物理存档 | [`../research/evidence/model-persistence-1to1.md`](../research/evidence/model-persistence-1to1.md) | persistence closure、状态owner inventory |
| B4 输入、时间、随机、音频 | [`../research/evidence/input-time-random-audio-1to1.md`](../research/evidence/input-time-random-audio-1to1.md) | runtime-audio/input-font closure与专项报告 |
| B5 标题、菜单、新建/读取 | [`../research/evidence/title-menu-new-game-1to1.md`](../research/evidence/title-menu-new-game-1to1.md) | UI closure、对应函数evidence、B5 golden |
| B6 世界地图 | [`../research/modules/world-map.md`](../research/modules/world-map.md) | world-map closure、综合证据与world golden |
| B7 场景、事件、对话 | [`../research/modules/scene-event.md`](../research/modules/scene-event.md) | scene-event closure、函数evidence与scene golden |
| B8 战斗 | [`../research/modules/battle.md`](../research/modules/battle.md) | battle closure、battle综合证据与golden |
| B9 持久化整合 | [`../research/modules/persistence.md`](../research/modules/persistence.md) | persistence closure、两份持久化综合证据 |
| 架构或owner争议 | [`../research/architecture/rewrite-architecture.md`](../research/architecture/rewrite-architecture.md) | program architecture与三份owner/dependency inventory |
| 逆向框架一致性 | [`../research/inventory/README.md`](../research/inventory/README.md) | 生成器、validator及专项headless报告 |

总览入口见 [`../research/README.md`](../research/README.md)。它是导航页，不覆盖 inventory 状态。

## 5. 单工作包执行循环

本节只给执行顺序；完整门禁见 [`execution-plan-details.md`](execution-plan-details.md) 第0.1、6、7节和 [`../research/inventory/README.md`](../research/inventory/README.md)。

1. 在module work package和closure中锁定唯一范围、owner、调用边界与停止线；
2. 不看现有C++，从机器码独立恢复ABI、基本块、位宽、分支、状态副作用、调用顺序和全部出口；
3. 从汇编独立派生测试向量，再审计现有实现与测试；
4. 做最小实现或修正，只执行与切片匹配的BUILD脚本验证；
5. 同步 implementation mapping、evidence、verification、remaining 和当前module停点；
6. 最终REVIEW阶段执行正反向逐基本块收敛；发现差异即从入口重来；
7. 到达可独立回退且验证通过的边界后，严格按 `AGENTS.md` 完成暂存、commit、push、TG和规则重读。

实现或测试通过只允许推进到 `implemented_pending_review`。只有 inventory 的关闭条件和最终零新增差异REVIEW同时满足，才能使用 `assembly_exact` / `platform_adapted`。

## 6. 计划维护规则

- 本文件只维护：入口读序、权威索引、当前唯一停点、后续固定顺序和已登记阻塞；
- 阶段范围、技术决定和完成条件写入 [`execution-plan-details.md`](execution-plan-details.md)；
- 计数、地址集合、实现映射和REVIEW状态只写入 inventory；
- 机器码、伪码、字段、hash、测试向量和差异记录只写入 evidence、golden或IDA报告；
- 模块实现队列和精确下一停点只写入对应 `research/modules/*.md`，本文件只链接当前项；
- 阶段提交历史不追加到本文件，使用Git历史查询；
- 当前停点变化时递增版本号，并检查所有链接仍存在、索引没有复制过期数据。
