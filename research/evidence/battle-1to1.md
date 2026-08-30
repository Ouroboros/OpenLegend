# B8 战斗 1:1 证据

状态：`pending_implementation`；最终汇编↔C++ REVIEW 尚未开始。

## 1. 物理范围与闭包

`sub_31C75 @ 0x31C75` 是 scene 与五轮试炼调用的 battle 入口。其后连续 battle 实现区间截止 `sub_3C6D3 @ 0x3C6D3..0x3CBE3`；`research/ida/reports/Z_DAT.b8_battle_xrefs.txt` 由当前 `Z_DAT.i64` 和 `idat.exe -A` headless 生成，枚举81个 FUNCTION 记录，报告规范为 LF，SHA256 为 `179b85c68ad87d03f175f7b22ff9af7ffbae68aed758eeaa0f0fe692ab67d488`。

`research/inventory/battle-closure.tsv` 以该报告为机械真值，当前79项为 `pending_mapping`、2项为 `pending_implementation`。battle 区间调用到的 resource/render/input/time/random/audio 入口是共享 owner 边界，不随递归调用图吞入 battle closure。

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

`research/tools/generate_b8_battle_goldens.py` 只读取原版字节，不链接 OpenLegend C++；双生成逐字节一致。正式 `research/evidence/battle-goldens.json` SHA256 为 `5aa2e052d59bdbcc51dc2ee661d6a00793df4547d74aa65342445ba1259ce98f`。

- 92对 `FIGHTnnn.IDX/GRP`，ID 范围0..109，中间缺18个编号；累计4,992帧；每包最后累计 offset 必须等于对应 GRP 大小。
- `WAR.STA` 26,040字节，严格为140条×186字节，SHA256 `98e3f66912c5ba4a0be00aaeff3462eb8c99f4d591d92a754930070dde9649b6`。
- `WARFLD.IDX` 26个累计 offset；`WARFLD.GRP` 532,480字节，最后 offset 与文件大小一致。

FIGHT 包数不等于可用 battle id 数：`WAR.STA` 有140条建立记录；FIGHT 文件是角色/动作图像包，缺号必须保留，不能重编号或补洞。

## 4. 状态 ownership

B8 报告记录253个 data target。battle transient 的高密度 xref 簇位于：

- `0xDC72C..0xDEA04`：参战者临时记录及相关数组；
- `0xE0A04..0xE6A04`：16,384字节战场与8,192字节 occupancy/标记区；
- `0xE6A04..0xE6AC0`：当前186字节 WAR 建立记录；
- `0xE6CBC..0xE6CBE`、`0xE6EBA..0xE6EEA`：battle 控制/选择/结果 globals。

`0xE87BC` 的64,000字节 indexed framebuffer 仍归 render；`0x9014C..` 角色记录、`0x9F5DC..` 物品与 `0xC0B78..` 会话状态仍归 model。现代实现必须借用这些 owner，而不是复制第二份持久状态。

## 5. 下一 REVIEW 单元

从 `sub_31EB9 @ 0x31EB9..0x3265C` 与 `sub_3265C @ 0x3265C..0x3271E` 入口独立恢复：

- 参战者槽数量、14字节 transient 布局与源角色索引；
- 队伍/敌方枚举、空槽/负值哨兵、坐标、方向、图片与状态初值；
- 64×64 occupancy 写入顺序、重复/越界行为和全部提前出口；
- battle id/get-exp 对建立和战后提交的实际影响。

完成该单元前不定义超出证据的 C++ gameplay API。
