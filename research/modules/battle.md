# Battle work package

## 状态

- 实现状态：`implemented_pending_review`（81/81函数均已实现并接入；战果、战后五类同步UI、音乐收尾及scene typed结果已闭环）。
- 最终 REVIEW：`not_started`。
- 当前唯一工作包：B8-WP01 战斗入口、资产、战场与参战者建立。

## 范围与非范围

- 物理 battle 区间锁定为 `Z_DAT 0x31C75..0x3CBE3`，专项 headless 报告枚举81个函数，机械审计顺序见 `research/inventory/battle-closure.tsv`。
- 范围包含战斗入口、WAR/WARFLD/FIGHT 资源、参战者临时态、输入、动作、AI、伤害/状态/物品、indexed 绘制、胜负/逃跑与战后提交。
- scene 包装器 `sub_2DE03` 仍归 scene closure；battle 只返回 typed result，不直接恢复 scene PC。
- 通用文件、packed archive、字体/精灵/像素、键盘、BIOS tick、LCG 与音频后端分别由 resource/render/input/time/random/audio owner 提供，不复制其实现。
- battle 不直接链接 scene、world、ui、platform_sdl3。

## 汇编与资产证据

- 机器码报告：`research/ida/reports/Z_DAT.b8_battle_xrefs.txt`，范围首尾与81个 FUNCTION 记录由 validator 固定。
- 独立资产 oracle：`research/tools/generate_b8_battle_goldens.py` / `research/evidence/battle-goldens.json`，以及状态三帧专用的`research/tools/generate_b8_player_status_golden.py` / `research/evidence/battle-player-status-golden.json`；均不链接 OpenLegend C++。
- 当前原版资产为92对 `FIGHTnnn.IDX/GRP`、共4,992帧；`WAR.STA` 为140条×186字节；`WARFLD.IDX/GRP` 为26条累计 archive entry。
- `sub_31EB9/sub_3265C` 证明参战者池是26槽×14 word（每槽28字节），occupancy 是64×64 `int16`；固定/预置队伍先写、敌方后写，重复格由后写槽覆盖。
- battle data xref 锁定瞬时状态簇：`0xDC72C..0xDEA04`、`0xE0A04..0xE6AC0`、`0xE6CBC..0xE6CBE`、`0xE6EBA..0xE6EEA`；`0xE87BC` framebuffer 仍由 render owner，`0x9014C..` 与 `0xC0B78..` 持久记录仍由 model owner。

## 接口与状态所有权

- 输入：原 battle id、get-exp 开关、借用的 `GameSnapshot` 窄视图、共享有序 RNG、宿主无关输入/tick。
- 输出：`BattleStepResult = Stay | Victory | Defeat | Escape`、indexed frame、音频命令和已按原顺序提交的持久状态。
- battle session 独占参战者临时态、战场缓存、选择/动画/AI transient；app 只拥有 session 生命周期并同步消费结果。
- `sub_31C75` 栈参数依次为 battle id 与 get-exp，返回 `word_E6ED2 - 1`；scene `sub_2DE03` 只在返回值严格等于1时选择真偏移。

## 当前实现队列

1. `sub_31DA0` 已映射为 `BattleData`：WAR 186字节记录、WARFLD entry 前16,384字节与64×64 occupancy 清空；状态仅为 `implemented_pending_review`；
2. `sub_31C75` 已由runtime-owned `BattleSession`覆盖资源生命周期、完整主状态机、WAR word8战斗music、scene word7恢复music、typed结果和严格exact-one scene真假边界；
3. `sub_31EB9` 的26槽、固定/预置/手选状态与 `sub_3265C` 敌方建立已映射为 `BattleSetup`；`BattleSession`实际绘制原圆角队伍选择框、Big5标题/确认文字、角色名/星号，逐键执行上下回绕与确认，并在每帧恢复冻结背景防止混色叠加；真实battle2选择和初始战场像素均与独立oracle一致；runtime已驱动render/present/input，前者推进为`implemented_pending_review`；
4. `sub_3271E..sub_32E59` 已恢复：稳定速度降序、逐槽swap、回合值，以及等待动作的逐槽队尾交换；`BattleSession`已执行初始actor居中战场实际绘制、逐帧黑场淡入、轮首prepare和actor-present后player/AI分界；玩家分支现按原门槛建立十项可用表，以可用项ordinal执行上下回绕和三确认键，并实际绘制圆角菜单、原Big5文字及当前actor状态面板，独立oracle与C++整帧hash一致；攻击已接入武功选择的战场重画、圆角面板、Big5名称、左右回绕、三确认键与Escape取消，并保留普通名称只扫`slot<learned_count`而确认扫10槽的稀疏显示BUG；等待会把当前actor移到队尾并继续同槽，休息提交状态后推进下一槽，两者均执行胜负/隐藏目标清理、隐藏槽跳过和下一actor present；actor present后按原顺序清word7/word10，轮末调用异常状态并等待本轮捕获的BIOS tick变化后再开始下一轮；自动动作按重绘→present→置flag→同actor AI推进；玩家移动已接入路径光标、逐格present/tick及菜单重建；用毒、解毒、医疗已按各自射程接入targeting四方向/Escape/确认、每轮路径重绘present、取消回原ordinal与确认目标保存，并在确认后完成状态、FIGHT/EFT动画、双bank音效、十帧damage、共享行动尾部和下一actor；攻击现已完成四类area的目标/方向分流、FIGHT/EFT、damage、双击逐轮提交与升级提示；物品已完成5×3 MMAP网格、分页/取消/三确认键、type3效果面板/任意键及零效果边界、type4目标/effect/damage/库存提交；状态动作已按共享`sub_22066/sub_22090/sub_22A59`合同接入队员选择、Escape/三确认键、两页角色状态和两次任意键等待，且不结束actor、不消费RNG；选择页/第一页/第二页的独立原资产oracle与C++整帧hash分别为`0xfa1b21403051335c`、`0x1c5e879ce61d5b34`、`0x9592da33a3c151d4`；战斗退出条件现进入胜败面板、战后消息队列并回送scene；
5. `sub_33599`的int16态势、候选优先级、逃跑、typed handler映射和handler后行动标记，以及`sub_33C4D..sub_34550`六个候选selector均已恢复；BattleSession已实际执行态势累计、战场重绘/present、延迟参数300对应的八次BIOS tick变化、selector、休息handler和逐actor后处理；`sub_34AD3`休息wrapper、`sub_3505B..sub_35372`四种攻击目标策略、`sub_355FF..sub_3570F`三个用毒目标selector已完整映射，`sub_34AEC`已恢复逃跑/物品重定位目的格计划，`sub_34C47`已恢复自动武功、装备加成、射程、移动后复检/重选的typed计划，`sub_3540E`已恢复用毒射程、移动后同目标复检和wrapped己方平均攻击回退typed计划，`sub_35803/sub_3582B`已恢复普通物品重定位后mode0使用和暗器目标/射程/移动复检/mode1使用或攻击回退typed计划，`sub_3598C`已恢复AI mode0共享物品23项状态、面板/等待参数与AI mode1暗器伤害/毒值，并按side扣队伍库存或敌方携带槽；`sub_361AC/sub_36209`已恢复请求医疗/解毒在正行动值时mode0/value0移动、恢复请求目标并自动攻击的共享typed计划，`sub_36210/sub_363AC`已恢复AI医疗/解毒射程、mode1移动、同目标重检及按actor攻击与wrapped己方平均值回退攻击/休息的typed计划；`sub_3650E`已恢复mode0..3目的格、双可达性检查、最短路标记和每次一格的实际状态continuation；BattleSession现从全部AI typed handler实际进入mode0..3移动，每格提交状态、更新视图、重绘/present并等待参数40对应两次BIOS tick变化，移动后恢复原typed计划；真实battle2逃跑三格后休息并推进actor，`sub_34AEC/sub_3650E/sub_37355`为`implemented_pending_review`；自动攻击现完成目标差方向、直接/移动后area伤害、FIGHT/EFT双bank、十帧damage、提交/升级等待、体力和外层actor尾部，`sub_34C47`为`implemented_pending_review`；AI用毒也完成直接/移动后状态、11帧effect30、十帧damage、攻击/休息回退与外层尾部，`sub_3540E`推进为`implemented_pending_review`；AI医疗/解毒完成直接和移动后状态、无flash动画、攻击/休息回退与外层尾部，请求医疗/解毒也在可选mode0移动后无条件进入完整自动攻击，`sub_361AC/sub_36209/sub_36210/sub_363AC`推进为`implemented_pending_review`；AI普通物品现完成重定位、效果面板、任意键、九tick等待与延后来源提交；AI暗器现完成直接/移动后复检、sample13前奏、EFT/damage、延后来源提交及二次超距自动攻击回退；玩家movement、攻击、用毒/解毒/医疗及type3/type4物品的targeting/方向、状态、FIGHT/EFT、damage、音频和行动尾部已完成；`sub_37734`额外锁定入口缓存范围、第二击动态伤害等级/cost scale、升级框与500延迟并推进为`implemented_pending_review`；
6. `sub_3859E/sub_3884A/sub_38910` 的actor/effect/sample/damage逐帧时间线、`sub_38DAC` 的MP过滤武功菜单，以及 `sub_39776..sub_3A8A4` 的用毒/解毒/医疗、战斗物品筛选、暗器目标/伤害/中毒/库存消耗和休息状态核心已映射；`sub_38DAC`已完整执行战场重画、原面板/Big5像素、左右输入、确认和取消并推进为`implemented_pending_review`；玩家用毒/解毒/医疗wrapper已完整执行targeting取消/确认、状态先提交、动态FIGHT/EFT绘制、attack/effect双bank音效、逐帧present/tick、damage flash规则及共享尾部；玩家物品已完整执行共享5×3界面、type3效果面板与type4暗器targeting、sample13前奏、EFT/damage、延后库存/行动/sprite提交，`sub_3A29C/sub_3A30B`推进为`implemented_pending_review`；AI自动攻击、用毒、医疗与解毒已复用FIGHT/EFT/damage同步相位，AI普通物品面板及AI暗器前奏/EFT/damage调用点均已接入，直接/移动后像素、RNG、音频、帧数与延后消费由真实资产回归锁定；
7. `sub_3AA17` 等待队尾重排为 `implemented_pending_review`；`sub_3AA4B` 锁定重绘→present→置自动flag→AI顺序；`sub_3AA85`双pass renderer已覆盖全部玩家/AI移动、攻击、支持、物品、状态、战果和战后调用点；战果胜/败帧及五类战后消息帧均有固定hash；
8. `sub_3B238..sub_3C2AC`已恢复胜负判定覆盖顺序、Big5战果框、敌方状态重置、经验均分、队伍下限、角色/练功/制造经验、等级/练功/武功/制造状态与RNG；`BattleSession`实际执行战果及五类消息的render/present/任意键队列并在耗尽后产出typed结果；
9. `sub_3C563/sub_3C672`已恢复回合hurt/poison扣血及hidden目标引用清理；`sub_3C6D3`三个xref均由玩家菜单或AI prelude/wait相位执行，独立面板与Session prelude像素hash均固定；
10. 完成全部实现后逐函数执行不限次数双向 REVIEW，最后一轮零新增差异前只标 `implemented_pending_review`。

## 测试、真实资产与差分点

- 资产：92 FIGHT 包逐 archive 边界、4,992帧、140 WAR 记录、26 WARFLD entries。
- 状态：参战者建立、占位、16位回绕、RNG 消费、原 BUG、战后 commit。
- 结果：Victory / Defeat / Escape 与 scene 真/假偏移；get-exp 仅在机器码证明的路径生效。
- 像素/音频：固定战斗 trace 的 indexed framebuffer、palette 与音频命令序列。
- 动态原程序差分在本机无 DOS runtime 时登记 `blocked_runtime_oracle`，不替代静态机器码、真实资产和独立 oracle。

## Closure 统计与下一停点

- `battle-closure.tsv`：0项`pending_mapping`、0项`pending_implementation`、81项`implemented_pending_review`，0项最终关闭；按81个函数实现状态口径为100%。
- B7 `sub_2DE03`联合边界已随typed battle result回送推进到`implemented_pending_review`。
- 下一停点：执行B8完整81函数汇编↔C++双向REVIEW；实现/测试通过不替代该门禁。
