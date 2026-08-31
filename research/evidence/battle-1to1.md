# B8 战斗 1:1 证据

状态：`pending_implementation`；最终汇编↔C++ REVIEW 尚未开始。

## 1. 物理范围与闭包

`sub_31C75 @ 0x31C75` 是 scene 与五轮试炼调用的 battle 入口。其后连续 battle 实现区间截止 `sub_3C6D3 @ 0x3C6D3..0x3CBE3`；`research/ida/reports/Z_DAT.b8_battle_xrefs.txt` 由当前 `Z_DAT.i64` 和 `idat.exe -A` headless 生成，枚举81个 FUNCTION 记录，报告规范为 LF，SHA256 为 `179b85c68ad87d03f175f7b22ff9af7ffbae68aed758eeaa0f0fe692ab67d488`。

`research/inventory/battle-closure.tsv` 以该报告为机械真值，当前76项为 `pending_mapping`、2项为 `pending_implementation`、3项为 `implemented_pending_review`。battle 区间调用到的 resource/render/input/time/random/audio 入口是共享 owner 边界，不随递归调用图吞入 battle closure。

## 2. scene ↔ battle 入口合同

`sub_2DE03 @ 0x2DE03..0x2DE2C` 依次把 KDEF battle id 与 get-exp word 压栈调用 `sub_31C75`。调用返回后只执行一次 `cmp eax,1`：严格等于1时返回真偏移，否则返回假偏移。

`sub_31C75 @ 0x31C75..0x31DA0`：

1. 保存 get-exp 到 battle global，写运行模式2，保存 battle id；
2. 调 `sub_31DA0` 载入 `WAR.STA` 记录和 `WARFLD` 战场；
3. 调 `sub_31EB9/sub_3265C` 建立参战者和瞬时态；
4. 打开对应 WMP/WDX 与 EFT 资源、淡出、装入效果、呈现；
5. 启动战斗音乐并调用 `sub_3271E` 主状态机；
6. 淡出，关闭 SMP/SDX，并按主角记录决定停止或恢复原音乐；
7. 写运行模式1，返回 `word_E6ED2 - 1`。

因此现有 scene 仅存储 `battle_get_exp_` 但没有 battle session 回传结果，不能把 `sub_2DE03/sub_31C75` 提前标为已实现。

## 3. 资产 oracle

`research/tools/generate_b8_battle_goldens.py` 只读取原版字节，不链接 OpenLegend C++；双生成逐字节一致。正式 `research/evidence/battle-goldens.json` SHA256 为 `e4aaceb3eb082302f2b68040e5fbfe5a56e00907d5676d12f8fec90dac03852d`。

- 92对 `FIGHTnnn.IDX/GRP`，ID 范围0..109，中间缺18个编号；累计4,992帧；每包最后累计 offset 必须等于对应 GRP 大小。
- `WAR.STA` 26,040字节，严格为140条×186字节，SHA256 `98e3f66912c5ba4a0be00aaeff3462eb8c99f4d591d92a754930070dde9649b6`。
- `WARFLD.IDX` 26个累计 offset；`WARFLD.GRP` 532,480字节，最后 offset 与文件大小一致。
- 参战者建立向量固定26槽×14 word/28字节；56条记录走固定队伍、84条走预置+选择；固定最多1人、预置最多2人、敌方最多20人，静态建立最多21人。
- 全部静态坐标在0..63。战斗93的敌方源下标8与10都写 `(13,34)`，occupancy slot9 被后写 slot11 覆盖。

FIGHT 包数不等于可用 battle id 数：`WAR.STA` 有140条建立记录；FIGHT 文件是角色/动作图像包，缺号必须保留，不能重编号或补洞。

## 4. 状态 ownership

B8 报告记录253个 data target。battle transient 的高密度 xref 簇位于：

- `0xDC72C..0xDEA04`：参战者临时记录及相关数组；
- `0xE0A04..0xE6A04`：16,384字节战场与8,192字节 occupancy/标记区；
- `0xE6A04..0xE6AC0`：当前186字节 WAR 建立记录；
- `0xE6CBC..0xE6CBE`、`0xE6EBA..0xE6EEA`：battle 控制/选择/结果 globals。

`0xE87BC` 的64,000字节 indexed framebuffer 仍归 render；`0x9014C..` 角色记录、`0x9F5DC..` 物品与 `0xC0B78..` 会话状态仍归 model。现代实现必须借用这些 owner，而不是复制第二份持久状态。

## 5. WAR/WARFLD 载入实现

`sub_31DA0 @ 0x31DA0..0x31EB9` 已映射为 `openlegend::battle::BattleData`：按 `battle_id*186` 解码93个 signed word，以 definition word6 选择 WARFLD cumulative archive entry，只取前16,384字节为8,192个战场 word，并把4,096个 occupancy word 清成-1。真实 battle 0..139 全部通过；-1和140由现代安全适配拒绝；battle 0/4/93/139 的记录和战场哈希固定。Linux Debug BUILD 14/14。

该函数只标 `implemented_pending_review`；IDX cache 的重载时机和错误路径清空次序留待总 REVIEW，未标 `assembly_exact`。

## 6. 参战者建立单元

`sub_31EB9 @ 0x31EB9..0x3265C`、`sub_3265C @ 0x3265C..0x3271E` 与紧邻 helper `sub_3B1E6 @ 0x3B1E6..0x3B238` 已从入口到返回逐基本块恢复，详见对应函数证据：

- 参战者池严格为26槽×14 signed word，每槽28字节；初始化 word0/1/11/12 为-1，其余除 word8外为0；
- 固定队伍看 WAR words15..20，一旦任一非-1便完全跳过预置队伍和选择 UI；否则先建 WAR words9..14，再允许队伍前缀中的非 mandatory 成员切换0/1；
- 队伍写 side word=0、word4=2；敌方写 side word=1、word4=1；每次插入重算 word8、写 occupancy，再按16位递增 count；
- occupancy 以 `y*64+x` 寻址，无范围、重复或容量检查；战斗93证明重复格必须后写覆盖；
- `sub_3B1E6` 返回 `int16(8*role.word1 + word_556D4 + 2*word_556CC + 2*combatant.word4)`；空槽初始化会以 role=-1 对角色表前182字节读取。

`BattleSetup` 已实现26槽完整初值、固定/预置队伍、host-neutral cursor/0·1·2选择状态、按当前 count 取坐标追加、敌方建立、sprite word 和后写 occupancy 覆盖。真实 battle0 固定手选顺序、battle4 固定队伍、battle93 slot9→slot11覆盖及全140条记录均通过 Linux Debug 14/14。

因此 `sub_3265C/sub_3B1E6` 已推进为 `implemented_pending_review`；`sub_31EB9` 仍缺原像素选择框、present 与 input flag 清除基本块，继续保持 `pending_implementation`。下一单元恢复 `sub_3271E..sub_32E59` 顶层状态机和共享输入/绘制边界后再补全该 UI，不用默认选择或自动全选占位。
