# B7 场景、事件与对话证据

状态：进行中。本文已固定 B7 的资源、场景绘制、移动/碰撞、KDEF 调度核心和 app 同步进入/返回链；天气、全部高阶剧情副作用和战斗回收在后续小提交继续逐项审计，不能据本文提前宣称 B7 完成。

## 1. 真值与证据

唯一行为真值仍是当前 `Z.COM` / `Z.DAT` 机器码和父目录原始资产。

- headless IDA 脚本：`research/ida/scripts/ida_b7_scene_xrefs.py`
- IDA 报告：`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`
  - SHA256：`27e62d0e49fd397665738df88fbe9df31e2ae5f9223dac33b0cc728653307c59`
- IDA 日志：`research/ida/logs/Z_DAT.b7_scene_xrefs.log`
  - SHA256：`5c5bbb566361c73beb6c5be73c6fe8182fa571b5d465b40563ff2e32f23c02bf`
- 独立 oracle：`research/tools/generate_b7_scene_goldens.py`
- oracle 输出：`research/evidence/scene-goldens.json`
  - SHA256：`575d6b99040b0dd5353a3cee6df51f7d43d7c1d253ae874af31beb88cf52e758`

IDA 仅通过 `/mnt/d/Dev/Crack/IDA/idat.exe -A` 导出；导出后原 `.i64` 的 incidental 修改已恢复。

## 2. 原资源域

独立 Python oracle 不链接 OpenLegend，实现自己的 little-endian、累计 IDX、RLE 和 KDEF 宽度解析。

| 资源 | 当前原版规模 | SHA256 |
|---|---:|---|
| `ALLSIN.GRP` | 100 × 6 × 64 × 64 × int16 = 4,915,200 bytes | `830ae313ccabe310a16d330eac83647a9c81a6c23efce6069ca87dc653f0e154` |
| `ALLDEF.GRP` | 100 × 200 × 11 × int16 = 440,000 bytes | `3633122f6a43f0b5dd390c2fa2516766d735a064ca955c8766b73232230a4480` |
| `TALK.GRP` | 2,977 records | `5cde11862ed7a52ffd45920e8f19ff21a5065bfe4c02036c524a504a5cb91811` |
| `KDEF.GRP` | 1,018 scripts | `135c5e097a7fe561ee931046e1bebf55de9b469678e3d111d8e9f2c6bb600e06` |

`TALK.GRP` 每条记录的正文逐字节 XOR `0xFF`，原记录末尾的零字节是终止符，不参与 XOR；解码后的 Big5 字节流重新以零终止。全部解码记录串联 SHA256 为 `f9af125e5c483ded03f4d444a615cddddcff8ab1ad416669abd1f561b9bb15eb`。

## 3. 场景会话

关键机器码入口：

- `sub_28E40`：场景会话建立、当前场景号、资源切换、主循环和退出；
- `sub_296E6`：入口/跳转坐标初始化；
- `sub_29819`、`sub_299A1`、`sub_29B3C`：事件动画和六层场景绘制；
- `sub_29C36`、`sub_29D2D`：场景移动后的绘制/同步；
- `sub_2B308`、`sub_2B3B4`：移动、碰撞和事件触发；
- `sub_2C0BB`：场景标题。

现代核心保持：

- `GameSnapshot::scene_maps` 是 100 个场景六层状态的唯一持久所有者；
- `GameSnapshot::scene_events` 是 100 × 200 条事件十一字段的唯一持久所有者；
- `SceneSession` 只持有当前场景号、坐标、方向、视口、动画计数、脚本 PC 和待确认输出；
- 场景写入直接落到 snapshot，未知字节不重编码；
- 场景精灵按原偶数 legacy ID 除二索引 `SDXnnn/SMPnnn`；
- indexed framebuffer 绘制顺序为低地表、带高度地表、建筑、事件、玩家、装饰，不把 SDL 类型带入核心。

场景 70 独立 oracle 固定值：

- 初始坐标 `(44, 29)`，视口原点 `(33, 18)`；
- 初始无 UI 地图 framebuffer FNV-1a64：`0x38fbaa07b733ad79`；
- 右、上、左、下轨迹：`(45,29)`、`(45,28)`、左侧阻挡保持 `(45,28)`、`(45,29)`；
- 碰撞保留九组原版地表闭区间以及“目标高度 - 当前高度 >= 10”单向判定。

## 4. KDEF 调度

关键机器码：

- `sub_2C319`：事件解释器主循环和 0x00–0x43 分派；
- `sub_2D841`：当前/外部场景事件十一字段修改；
- `sub_2E337`：当前/外部场景六层单元写入；
- `sub_2CC21`：TALK 读取、窗口位置和阻塞分页；
- `sub_2DE03`：战斗请求与真假偏移；
- `sub_301D1`、`sub_30379`、`sub_30C3D`：高阶剧情、武林大会和结尾路径。

从当前 1,018 条 KDEF 脚本按解释器真实宽度顺序解析得到：

- 全部 1,018 条脚本均以 `-1` 终止；
- 共 13,315 条指令；
- 68 个分派槽位中实际使用 67 个；
- 唯一未出现的是 opcode 24；解释器仍保留该槽位；
- opcode 使用频次完整数组写入 `scene-goldens.json`，C++ 真实资产测试逐项对照，不以少量示例脚本替代全量扫描。

当前核心已建立 0–67 的同步 PC/偏移执行边界、对话/问题/商店/战斗请求队列及 snapshot 副作用入口。高阶剧情的每个基本块仍需在后续小提交逐条复核，本文不把“已有 switch”当作全部语义已闭环。

## 5. TALK 分页

`sub_2CC21` 在固定 `218×57` 对话框内调用文本例程并在每页后阻塞等待输入。当前资产使用 ASCII `'*'` 作为显式换行；Big5 trail byte 合法范围不包含 `0x2A`，因此可无歧义识别。

核心按以下规则运输对话页：

- 每页最多三行；
- `'*'` 结束当前行；
- 超过对话框 208 像素正文宽度时只在完整 ASCII/Big5 token 边界软换行；
- 每页作为同步 `SceneStepKind::dialogue` 返回，app 确认后才继续同一事件 PC；
- 字体仍由 `FONT.X16` / `FONT.C16` 在 index8 framebuffer 上绘制。

## 6. app 同步消费

`LegacyGameRuntime` 在世界移动产生 `WorldStepKind::enter_scene` 的同一调用点构造 `SceneSession`，不引入异步事件总线。场景结果由 app 同步消费：

- 场景标题、对话和通知按键确认后继续同一事件 PC；
- 问题、商店选择和战斗请求保持阻塞结果，不提前执行后续指令；
- 世界菜单保持六项，场景菜单切换到原有 `GameMenuContext::scene` 四项边界；
- 场景出口先写回 header，再销毁场景瞬态并从同一 snapshot 重建世界会话；
- 场景音乐/音效先运输到 app 队列，再由 SDL 边界的 `LegacyAudioController` 同步消费；即使场景结果同时返回世界，音频命令也不会随 `SceneSession` 销毁而丢失；
- SDL 主循环仍只处理宿主事件、最终 indexed frame 上传和音频设备，不持有场景语义。

集成测试固定验证 `世界 → 场景 70 标题 → 场景四项菜单 → 场景 → 右侧出口 → 世界`，并检查 `in_sub_map` 清零和 scene request 回收。

## 7. 当前验证

Linux core Debug：10/10 测试通过，包括：

- 2,977 条 TALK 数量与首尾记录解码；
- 1,018 条 KDEF 全量终止、opcode 合法域、13,315 条频次；
- 四个核心 GRP 的 FNV-1a64；
- 场景 70 初始像素和碰撞轨迹；
- 真实脚本 36 的背包副作用、274 的场景层写入、69 的 TALK 暂停/恢复；
- 所有既有 model/resource/render/world/persistence/ui/audio/core 测试无回归。

Linux app Debug：11/11 测试通过，包含上述场景同步链和 SDL dummy smoke。

后续 B7 门禁仍包括天气、全 opcode 基本块审计、Linux/Windows Debug/Release、ASan+UBSan、原资产只读和 `.i64` 审计。
