# Battle work package

## 状态

- 实现状态：`pending_implementation`。
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
- battle data xref 锁定瞬时状态簇：`0xDC72C..0xDEA04`、`0xE0A04..0xE6AC0`、`0xE6CBC..0xE6CBE`、`0xE6EBA..0xE6EEA`；`0xE87BC` framebuffer 仍由 render owner，`0x9014C..` 与 `0xC0B78..` 持久记录仍由 model owner。

## 接口与状态所有权

- 输入：原 battle id、get-exp 开关、借用的 `GameSnapshot` 窄视图、共享有序 RNG、宿主无关输入/tick。
- 输出：`BattleStepResult = Stay | Victory | Defeat | Escape`、indexed frame、音频命令和已按原顺序提交的持久状态。
- battle session 独占参战者临时态、战场缓存、选择/动画/AI transient；app 只拥有 session 生命周期并同步消费结果。
- `sub_31C75` 栈参数依次为 battle id 与 get-exp，返回 `word_E6ED2 - 1`；scene `sub_2DE03` 只在返回值严格等于1时选择真偏移。

## 当前实现队列

1. `sub_31C75/sub_31DA0`：资源生命周期、WAR 186字节记录、WARFLD 16,384字节战场载入、返回码边界；
2. `sub_31EB9/sub_3265C`：队伍/敌方临时角色建立、坐标与 occupancy；
3. `sub_3271E..sub_32E59`：战斗顶层状态机、输入与胜负出口；
4. `sub_33599..sub_37734`：玩家动作、AI、移动、攻击、伤害、状态与物品；
5. `sub_3859E..sub_3A8A4`：动画、效果、数值提示与战后状态；
6. `sub_3AA17..sub_3C6D3`：绘制、菜单/信息与共享显示边界；
7. 完成全部实现后逐函数执行不限次数双向 REVIEW，最后一轮零新增差异前只标 `implemented_pending_review`。

## 测试、真实资产与差分点

- 资产：92 FIGHT 包逐 archive 边界、4,992帧、140 WAR 记录、26 WARFLD entries。
- 状态：参战者建立、占位、16位回绕、RNG 消费、原 BUG、战后 commit。
- 结果：Victory / Defeat / Escape 与 scene 真/假偏移；get-exp 仅在机器码证明的路径生效。
- 像素/音频：固定战斗 trace 的 indexed framebuffer、palette 与音频命令序列。
- 动态原程序差分在本机无 DOS runtime 时登记 `blocked_runtime_oracle`，不替代静态机器码、真实资产和独立 oracle。

## Closure 统计与下一停点

- `battle-closure.tsv`：79项 `pending_mapping`、2项 `pending_implementation`，0项关闭。
- B7 尚余 `sub_2DE03/sub_31C75` 联合边界；只有 battle result 实际回送 scene 后才能把两项推进到 `implemented_pending_review`。
- 下一停点：独立恢复 `sub_31EB9` 与 `sub_3265C` 的参战者记录布局和全部建立分支，再定义最小 `BattleSession` 公共接口。
