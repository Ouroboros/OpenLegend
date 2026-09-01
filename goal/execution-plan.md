# OpenLegend 执行 GOAL

当前阶段：B0–B9 功能映射补齐，尚未进入统一最终 REVIEW

当前工作项：B5 游戏菜单共享队员选择器的装备、修炼、使用和离队调用链

当前状态：医疗、解毒和查看状态路径已推进到 `implemented_pending_review`；共享选择器仍有模式和业务调用链未实现。B7、B8、B9 的有限 closure 已完成实现映射，但全项目最终汇编↔C++ REVIEW 仍为 0%。

下一步：从当前机器码和调用者独立恢复共享选择器剩余模式，先锁定参数、标题/数值布局、Escape/确认返回和业务副作用，再审计现有 C++；不得从已有 renderer 反推原行为。

## 1. 目标与完成条件

以现代 C++20、CMake 和 SDL3 为实现载体，对当前《金庸群侠传》DOS 版执行可观察行为 1:1 还原。现代模块拆分和宿主平台适配可以改变代码所有权与接入方式，但不能改变原程序的状态、顺序、整数语义、随机数流、输入边界、呈现结果、存档字节或音频命令。

全项目只有同时满足以下条件才可完成：

1. 当前 `Z.COM` / `Z.DAT` 的游戏自有函数、当前资产可达分支、事件 opcode 和平台调用链均已归属到唯一 owner。
2. 所有 closure 的 `pending_mapping`、`pending_implementation` 和 `implemented_pending_review` 均归零；关闭状态满足 `research/inventory/README.md` 的证据与最终 REVIEW 硬门。
3. 每个行为单元完成汇编→C++和C++→汇编双向逐基本块 REVIEW；发现差异后从入口重启，最后一轮完整 REVIEW 零新增差异。
4. 固定输入流程的状态、RNG、indexed framebuffer、调色板、存档字节和音频命令与原版一致；所有当前资产全量门禁通过。
5. Linux 与 Windows 的构建、测试、sanitizer、smoke、资产只读和正式 IDA 数据库门禁通过。
6. 原程序动态差分已完成，或其运行环境阻塞已被明确解决；不存在未经用户批准的平台偏差或兼容缺口。

“可运行”“可玩”“已有测试”或“实现映射完成”均不构成 GOAL 完成。

## 2. 本 GOAL 特有的长期约束

- 行为真值及仓库执行规则以 `AGENTS.md` 为准；本文件不复制 Git、TG、暂停、BUILD、临时目录或 IDA 操作规则。
- 允许先补齐 B0–B9 功能映射，再执行统一最终 REVIEW；在最终收敛前，已有实现最多标记为 `implemented_pending_review`。
- 当前资产可达范围是实现与验收边界；不可达代码只能以机器码、调用图和资产证据排除，不能以缺少测试样本推定无关。
- 宿主平台适配必须停留在窗口、事件、音频设备、文件系统和最终呈现边界；游戏语义仍由核心模块按原同步时序执行。
- 任何后续真实运行缺陷都会重新打开相关 closure，并从机器码入口重启完整 REVIEW；不得只修补症状。
- 本机当前没有可用 DOS 动态运行环境；`blocked_runtime_oracle` 是最终动态差分门禁的外部阻塞，不阻止继续完成静态机器码、真实资产和独立 oracle 工作，但不能被这些证据替代为“动态差分已完成”。

## 3. 当前执行状态

### 当前模块

B5 `ui + app`，同时涉及共享 `model` 状态和既有 `battle` 医疗/解毒公式。

### 当前工作包

补齐共享队员选择器的剩余游戏菜单调用链：

- 装备对象选择；
- 修炼对象选择；
- 使用对象选择；
- 离队选择后的完整业务与事件移交。

参数0医疗、参数1解毒和参数2查看状态已实现；参数6只有共享姓名选择器像素，离队业务尚未闭合。精确队列和持久状态以 `research/inventory/ui-closure.tsv`、`research/inventory/input-font-closure.tsv` 为准。

### 已经具备的结果

- 逆向 function catalog、owner、closure、模块依赖和状态所有权框架已建立并通过 validator。
- B7 场景/事件、B8 战斗、B9 持久化有限 closure 均已有实现映射，统一保持 `implemented_pending_review`。
- 游戏菜单医疗、解毒、查看状态已覆盖世界/场景背景、嵌套选择、状态写入和同步返回；最近验证门已全部通过。
- 原版资产保持只读，正式 IDA 数据库未被修改。

### 尚未完成的门禁

1. B5 共享选择器剩余模式与离队业务未实现。
2. `input-font`、`runtime-audio`、`ui`、`world-map` closure 仍有大量 `pending_mapping`，其中共享选择器仍有 `pending_implementation`。
3. B0–B3 尚缺覆盖完整模块范围的 closure/ownership 对账，不能因基础实现存在而关闭。
4. B0–B9 统一最终双向 REVIEW 尚未开始。
5. 最终全矩阵、sanitizer、smoke、资产只读、IDA 审计和动态原程序差分尚未作为全项目关闭门执行。

### 当前阻塞

- 当前实现工作无外部阻塞。
- 最终动态原程序差分受本机缺少 DOS runtime 阻塞，登记为 `blocked_runtime_oracle`。
- 世界人物移动后消失的用户报告尚未复现；现有诊断日志保留，若再次出现则重新打开 B6 对应调用链。

### 恢复执行时的第一个动作

从共享选择器剩余模式及其直接调用者的机器码开始独立推导，先形成模式3–6的输入、显示、返回和副作用表；完成前不修改 C++，也不把医疗/解毒/状态路径的相似结构外推为事实。

## 4. 模块顺序与状态

| 阶段 | 范围 | 当前状态 | 权威入口 |
|---|---|---|---|
| A | 架构恢复 | 已关闭；只在新机器证据推翻边界时重开 | `research/architecture/program-architecture.md`、`research/architecture/rewrite-architecture.md` |
| B0 | `compat + platform_sdl3 + app` | 基础实现已有；runtime/platform closure 范围仍不完整，待映射与最终 REVIEW | `research/modules/runtime-platform.md` |
| B1 | `resource` | 实现和资产证据已有；待 ownership/closure 对账与最终 REVIEW | `research/evidence/resource-loader-1to1.md` |
| B2 | `render` | indexed framebuffer、字体、精灵和宿主展开实现已有；待最终 REVIEW | `research/evidence/render-1to1.md` |
| B3 | `model + persistence` 物理格式 | snapshot 与存档运输实现已有；待模块 closure 对账与最终 REVIEW | `research/evidence/model-persistence-1to1.md` |
| B4 | `input + time + random + audio` | 实现已有；`input-font` 与 `runtime-audio` 仍以 pending mapping 为主 | `research/evidence/input-time-random-audio-1to1.md`、`research/modules/runtime-platform.md` |
| B5 | `ui + app + persistence` 菜单流程 | **当前进行中**；医疗、解毒、状态已映射，装备/修炼/使用/离队及其余 UI 队列待处理 | `research/evidence/title-menu-new-game-1to1.md`、`research/inventory/ui-closure.tsv` |
| B6 | `world` | 实现已有，旧完成结论已撤销；closure 待映射，真实消失问题未复现 | `research/modules/world-map.md` |
| B7 | `scene` | 有限 closure 实现映射完成；最终 REVIEW 未开始 | `research/modules/scene-event.md` |
| B8 | `battle` | 有限 closure 实现映射完成；最终 REVIEW 未开始 | `research/modules/battle.md` |
| B9 | 全模块持久化与兼容整合 | 有限 closure 实现映射完成；最终 REVIEW 未开始 | `research/modules/persistence.md` |
| Final | B0→B9 统一 REVIEW 与发布门禁 | 待开始 | `research/inventory/`、本文件第1节 |

执行顺序：先完成 B5 当前工作包，再处理其余 `pending_implementation` 与 `pending_mapping`；全部功能映射完成后，按 inventory 锁定顺序从 B0 开始执行统一最终 REVIEW。

## 5. 权威资料

### 规则与目标

- 仓库长期规则：`AGENTS.md`
- 当前阶段、当前工作项和恢复入口：`goal/execution-plan.md`

### 架构与所有权

- 原程序架构：`research/architecture/program-architecture.md`
- 现代模块与依赖：`research/architecture/rewrite-architecture.md`
- owner、状态和依赖机械真值：`research/inventory/module-function-ownership.tsv`、`research/inventory/module-state-ownership.tsv`、`research/inventory/module-dependencies.tsv`

### 完整工作队列

- inventory 合同：`research/inventory/README.md`
- 各模块完整队列：`research/inventory/*-closure.tsv`
- 模块范围、接口和移交条件：`research/modules/README.md`、`research/modules/*.md`

### 当前工作项证据

- B5 综合规格：`research/evidence/title-menu-new-game-1to1.md`
- 共享选择器证据：`research/evidence/functions/Z_DAT/0x22090.md`
- 当前 UI 队列：`research/inventory/ui-closure.tsv`
- 当前输入边界队列：`research/inventory/input-font-closure.tsv`

函数级事实、地址、测试向量和差异记录只进入 `research/evidence/`；已完成工作的版本历史由 Git 保存。本项目当前不创建 `execution-history-pi.md`，因为没有发现无法从 inventory、模块文档、evidence 和 Git 恢复的唯一阶段决策。

## 6. 维护规则

- 当前状态变化时替换文件顶部和第3节的旧状态，不在文末追加“本轮完成”记录。
- 工作包完成后只移动当前指针并更新模块表；函数级结果写入 evidence，持久状态写入 inventory，模块接口或移交条件写入模块文档。
- inventory 是 closure 关闭状态的唯一真值；模块文档是范围、接口和移交条件的真值；本文件只保存当前执行状态和恢复入口。若出现冲突，先以 inventory 重建本文件，不在主 GOAL 另造状态。
- 不复制 `AGENTS.md` 的仓库规则，不记录版本标识、文件哈希、函数地址清单、逐测试过程或已完成阶段流水账。
- 不设置行数、字节数或章节数量硬阈值；内容必须直接回答目标、当前进度、下一步、仍有效约束或权威资料位置之一。
- 不按工作包创建历史文档，不引入自动归档器、数据库或 CI 文档大小门禁。
- 只有确实无法由现有权威资料和 Git 恢复的阶段级决策，才允许建立单一历史归档；该归档必须明确标记为非执行依据。
- 每次更新后验证所有引用路径有效，并确认新会话只阅读本文件即可确定当前断点、剩余门禁和第一个动作。
