# Battle work package

## 状态

- 实现状态：`implemented_pending_review`（另已实现 `sub_32A51/sub_32B78` 排序交换与 `sub_36E06..sub_37245` 路径图/回溯；`sub_3271E/sub_3B238` 仅回合值/typed结果核心已实现；其余 battle 按 closure 待实现）。
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
- 独立资产 oracle：`research/tools/generate_b8_battle_goldens.py` 与 `research/evidence/battle-goldens.json`，不链接 OpenLegend C++。
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
2. `sub_31C75`：资源生命周期、主状态机调用和返回码边界；
3. `sub_31EB9` 的26槽、固定/预置/手选状态与 `sub_3265C` 敌方建立已映射为 `BattleSetup`；后者完整待 REVIEW，前者仍缺原选择框绘制/呈现/input flag 基本块；
4. `sub_3271E..sub_32E59` 已恢复：稳定速度降序、逐槽 swap、回合值，以及等待动作的逐槽队尾交换已实现；自动动作仅恢复flag，顶层 render/tick/分派及十项菜单UI与同步continuation仍待实现；
5. `sub_33C4D..sub_34550` 六个AI候选selector，以及路径图/回溯、逐格移动 state/stop、武功profile/每击提交、方形/十字/直线area和HP/MP伤害公式已实现；`sub_33599`候选优先级/逃跑/action handler同步与`sub_37734`方向选择/UI continuation待补；
6. `sub_3859E/sub_3884A/sub_38910` 的actor/effect/sample/damage逐帧时间线、`sub_38DAC` 的MP过滤武功菜单状态，以及 `sub_39776..sub_3A8A4` 的用毒/解毒/医疗、战斗物品筛选、暗器目标/伤害/中毒/库存消耗和休息状态核心已映射；FIGHT/sample/render/present/input接线及共享物品界面仍待BattleSession；
7. `sub_3AA17` 等待队尾重排为 `implemented_pending_review`；`sub_3AA4B` 已锁定重绘→present→置自动flag→AI的顺序；`sub_3AA85` 已恢复双32×32 pass、地形/overlay/物件/角色/effect/damage的typed命令计划，实际indexed像素执行和present仍待BattleSession；
8. 完成全部实现后逐函数执行不限次数双向 REVIEW，最后一轮零新增差异前只标 `implemented_pending_review`。

## 测试、真实资产与差分点

- 资产：92 FIGHT 包逐 archive 边界、4,992帧、140 WAR 记录、26 WARFLD entries。
- 状态：参战者建立、占位、16位回绕、RNG 消费、原 BUG、战后 commit。
- 结果：Victory / Defeat / Escape 与 scene 真/假偏移；get-exp 仅在机器码证明的路径生效。
- 像素/音频：固定战斗 trace 的 indexed framebuffer、palette 与音频命令序列。
- 动态原程序差分在本机无 DOS runtime 时登记 `blocked_runtime_oracle`，不替代静态机器码、真实资产和独立 oracle。

## Closure 统计与下一停点

- `battle-closure.tsv`：31项 `pending_mapping`、21项 `pending_implementation`、29项 `implemented_pending_review`，0项最终关闭。
- B7 尚余 `sub_2DE03/sub_31C75` 联合边界；只有 battle result 实际回送 scene 后才能把两项推进到 `implemented_pending_review`。
- 下一停点：独立恢复 `sub_33599` AI入口的候选调用优先级、逃跑判定与动作码分派，再接BattleSession的renderer executor、present与自动AI同步continuation。
