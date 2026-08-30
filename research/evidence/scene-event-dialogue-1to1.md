# B7 场景、事件与对话证据

状态：进行中。本文已固定 B7 的资源、场景绘制、移动/碰撞、十一处天气场景、KDEF 调度核心和 app 同步进入/返回链；全部高阶剧情副作用和战斗回收仍在后续小提交逐项审计，不能据本文提前宣称 B7 完成。

## 1. 真值与证据

唯一行为真值仍是当前 `Z.COM` / `Z.DAT` 机器码和父目录原始资产。

- headless IDA 脚本：`research/ida/scripts/ida_b7_scene_xrefs.py`
- IDA 报告：`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`
  - SHA256：`7e2ce337bfeebd9d53ee0ae47351b4ec63813295fcecc79ff86680a1394d13f0`
- 独立 oracle：`research/tools/generate_b7_scene_goldens.py`
- oracle 输出：`research/evidence/scene-goldens.json`
  - SHA256：`721ad6f9f793d2e94ccd90f463685f89f7f60f604ee868c32e2c1524bdab8dc5`

IDA 仅通过 `/mnt/d/Dev/Crack/IDA/idat.exe -A` 导出；导出后原 `.i64` 的 incidental 修改已恢复。

## 2. 原资源域

独立 Python oracle 不链接 OpenLegend，实现自己的 little-endian、累计 IDX、RLE 和 KDEF 宽度解析。

| 资源 | 当前原版规模 | SHA256 |
|---|---:|---|
| `ALLSIN.GRP` | 100 × 6 × 64 × 64 × int16 = 4,915,200 bytes | `830ae313ccabe310a16d330eac83647a9c81a6c23efce6069ca87dc653f0e154` |
| `ALLDEF.GRP` | 100 × 200 × 11 × int16 = 440,000 bytes | `3633122f6a43f0b5dd390c2fa2516766d735a064ca955c8766b73232230a4480` |
| `TALK.GRP` | 2,977 records | `5cde11862ed7a52ffd45920e8f19ff21a5065bfe4c02036c524a504a5cb91811` |
| `KDEF.GRP` | 1,018 scripts | `135c5e097a7fe561ee931046e1bebf55de9b469678e3d111d8e9f2c6bb600e06` |
| `HDGRP.GRP` | 115 portrait frames | `9b3ea687e0a2d82cc0dd83f580cff6a5cb48e7b62b6fd98dc4b63dad93a9c037` |

`TALK.GRP` 每条记录的正文逐字节 XOR `0xFF`，原记录末尾的零字节是终止符，不参与 XOR；解码后的 Big5 字节流重新以零终止。全部解码记录串联 SHA256 为 `f9af125e5c483ded03f4d444a615cddddcff8ab1ad416669abd1f561b9bb15eb`。

## 3. 场景会话

关键机器码入口：

- `sub_28E40`：场景会话建立、当前场景号、资源切换、主循环和退出；
- `sub_296E6`：入口/跳转坐标初始化；
- `sub_29819`、`sub_299A1`：水平/垂直移动、碰撞、方向和玩家图；
- `sub_29B3C`：64×64 事件格图片推进；
- `sub_29C36`：交互触发前重绘呈现；
- `sub_29D2D`：六层场景绘制；
- `sub_2B288`、`sub_2B308`、`sub_2B3B4`：物品菜单相邻格目标、物品事件与自动事件入口；
- `sub_2C0BB`：场景标题。

现代核心保持：

- `GameSnapshot::scene_maps` 是 100 个场景六层状态的唯一持久所有者；
- `GameSnapshot::scene_events` 是 100 × 200 条事件十一字段的唯一持久所有者；
- `SceneSession` 只持有当前场景号、坐标、方向、视口、动画计数、脚本 PC 和待确认输出；
- 场景写入直接落到 snapshot，未知字节不重编码；
- 场景精灵按原偶数 legacy ID 除二索引 `SDXnnn/SMPnnn`；
- indexed framebuffer 绘制顺序为低地表、带高度地表、建筑、事件、玩家、装饰，不把 SDL 类型带入核心；
- 每 tick 先推进事件图片，按左/上/下/右/交互/菜单只消费一个动作，再无条件重绘呈现；五 tick 周期、自动事件、出口和内部跳转均在该呈现边界后统一执行；
- 场景入口为淡入、标题、裸场景呈现、自动事件，出口和内部跳转均先淡出，内部跳转重载后重新执行淡入、标题和自动事件。

场景 70 独立 oracle 固定值：

- 初始坐标 `(44, 29)`，视口原点 `(33, 18)`；
- 初始无 UI 地图 framebuffer FNV-1a64：`0x38fbaa07b733ad79`；
- 右、上、左、下轨迹：`(45,29)`、`(45,28)`、左侧阻挡保持 `(45,28)`、`(45,29)`；
- 碰撞保留九组原版地表闭区间以及“目标高度 - 当前高度 >= 10”单向判定；
- 84条 metadata 共10处内部跳转、252个出口格，首20 tick 的周期更新点为 `[1,6,11,16]`。

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
- 唯一未出现的是 opcode 24；其合法槽位仍按机器码实现载入三档进度与退出确认菜单，并由 synthetic KDEF 覆盖；
- opcode 使用频次完整数组写入 `scene-goldens.json`，C++ 真实资产测试逐项对照，不以少量示例脚本替代全量扫描。

当前核心已建立 0–67 的同步 PC/偏移执行边界、对话/问题/商店/战斗请求队列及 snapshot 副作用入口。高阶剧情的每个基本块仍需在后续小提交逐条复核，本文不把“已有 switch”当作全部语义已闭环。

### 4.1 已复核的角色与物品副作用

本切片逐基本块对照 `sub_2D678`、`sub_2DF0E`、`sub_2E1E8`、`sub_2E078`、`sub_2F3F0`、`sub_2F62F` 和 `sub_300FF`，固定：

- 添加物品或增加声望后，仅当主角声望不少于 200、物品 144–157 全部存在且物品 189 不存在时，才把场景 70 事件 11 改为原武林大会入口；
- 休息只恢复受伤值小于 33 且未中毒的队员，不替中毒或重伤角色清毒/治疗；
- 角色加入和离开队伍时按原版清除两件装备、修炼物品及修炼经验，并把对应物品 user 恢复为 `-1`；队伍槽 0 始终保留主角；
- 学习武功和指定武功槽在没有空槽时覆盖槽 0，`-1` 槽参数按原版先寻找空槽。

真实资产测试使用 KDEF 脚本 36、931、581 和 950 验证上述状态，不用合成脚本替代当前资产路径。

### 4.2 逐帧呈现与淡入淡出

逐基本块对照 `sub_2C319` 的 opcode 0、13、14 分派以及 `sub_3CC97`、`sub_3CD17` 后，事件解释器不再把视觉调用当作无输出指令：

- opcode 0 返回 `SceneStepKind::present`，至少完成一次宿主 framebuffer 上传后才恢复事件 PC；
- opcode 13 返回 `fade_from_black`，按 65 帧序列从全黑恢复场景 RGB6 调色板；
- opcode 14 返回 `fade_to_black`，按 64 帧逐通道递减序列得到全黑调色板；
- 每帧仍先由 `SceneSession` 重绘同一 index8 场景，`LegacyGameRuntime` 只在最终呈现边界替换 palette，不修改像素索引，也不把 SDL 类型引入核心；
- 视觉步骤呈现完毕前脚本 PC、天气和玩家输入保持阻塞，完成后 app 同步调用同一 `SceneSession::resume()`；连续 opcode 14、场景修改、opcode 0、opcode 13 因而保持原基本块顺序。

真实资产测试使用脚本 274 验证 opcode 0 在两次场景层写入后产生呈现边界，使用脚本 931 验证 `对话 → present → fade_to_black → 休息/换位 → present → fade_from_black → 对话` 的阻塞次序；64/65 帧 RGB6 序列继续由 render 单元测试逐帧固定。

## 5. TALK 分页

`sub_2CC21` 在固定 `218×57` 对话框内调用文本例程并在每页后阻塞等待输入。当前资产使用 ASCII `'*'` 作为显式换行；Big5 trail byte 合法范围不包含 `0x2A`，因此可无歧义识别。

核心按以下规则运输对话页：

- 每页最多三行；
- `'*'` 结束当前行；
- 不按对话框宽度软换行；只有 `'*'` 增加17像素行距，第三次显式换行结束本次调用；
- 第三个 `'*'` 后恰逢记录终止时仍保留下一次终止符调用形成的空白末页；
- 正文固定从面板 `(x+13,y+3)` 开始，ASCII 前进8像素、Big5前进16像素；
- `sub_20615/sub_20663` 不裁剪横向越界，超过319的置位像素按线性 framebuffer 地址落到后续扫描线；
- style0/1/4/5 绘制 `60×62` 混色头像框并按 head ID 直接读取 HDGRP，style2/3 不读取头像；
- 每页作为同步 `SceneStepKind::dialogue` 返回，app 确认后才继续同一事件 PC；
- 当前2,977条 TALK 最大显式行宽为344像素（talk1841），当前 KDEF 实际 style 为0/1/2/4。

## 6. 场景天气

`sub_28E40` 只为场景 `5, 7, 10, 41, 42, 46, 65, 66, 67, 72, 79` 打开天气路径。现代场景会话复用 B6 已由机器码闭环的 `CLOUD.IDX/GRP` 合同：

- 三粒子 kind、weight、x、y 按原 LCG 调用次数生成；`bounded(1)` 不推进 RNG；
- 活跃粒子每 tick 横向加一，全部越过 500 后才重新生成；
- 场景移动按菱形投影反向平移粒子；
- RGB6 以 `source*weight/32 + destination*(8-weight)/32` 混合，再经 RGB4 最近色表写回 index8；
- 对话/问题等阻塞输出期间不推进天气。

独立 oracle 对场景 5 固定：入口 `(17,48)`；300 tick 后 RNG `0xaf1cf0fb`，粒子为 `(kind,weight,x,y) = (2,7,9,49), (2,7,18,-4000), (1,8,11,160)`，framebuffer FNV-1a64 为 `0xb3e2b127988e5690`。

## 7. app 同步消费

`LegacyGameRuntime` 在世界移动产生 `WorldStepKind::enter_scene` 的同一调用点构造 `SceneSession`，不引入异步事件总线。场景结果由 app 同步消费：

- 场景标题、对话和通知按键确认后继续同一事件 PC；
- 问题、商店选择和战斗请求保持阻塞结果，不提前执行后续指令；
- 世界菜单保持六项，场景菜单切换到原有 `GameMenuContext::scene` 四项边界；
- 同 tick 的场景方向、交互和菜单请求由 app 聚合，按机器码优先级只消费一个动作；菜单返回后继续未完成 tick；
- 场景出口先写回 header，再销毁场景瞬态并从同一 snapshot 重建世界会话，世界方向按 `[3,2,1,0]` 反转；
- 场景音乐/音效先运输到 app 队列，再由 SDL 边界的 `LegacyAudioController` 同步消费；opcode8 只写离场音乐覆盖并延迟到出口，opcode8/66 音乐命令强制执行，普通 metadata 请求才与 controller 当前曲目比较；即使场景结果同时返回世界，音频命令也不会随 `SceneSession` 销毁而丢失；
- SDL 主循环仍只处理宿主事件、最终 indexed frame 上传和音频设备，不持有场景语义。

集成测试固定验证 `世界 → 场景 70 标题 → 场景四项菜单 → 场景 → 右侧出口 → 世界`，并检查 `in_sub_map` 清零和 scene request 回收。

## 8. 当前验证

Linux app Debug BUILD 脚本：13/13 测试通过，包括：

- 2,977 条 TALK 数量、首尾记录解码、显式三行分页、第三换行后的空白末页与最大344像素行；
- HDGRP 115帧以及真实 scripts1/142/244/515 的 style0/1/2/4、头像、无头像和线性越界 framebuffer hashes；
- 1,018 条 KDEF 全量终止、opcode 合法域、13,315 条频次，以及 synthetic opcode4/6/13/14/16/24/68 的条件、同步、载入菜单与 PC 不推进边界；
- 四个核心 GRP 的 FNV-1a64；
- 场景 70 初始像素、碰撞轨迹、入口/主循环/出口/内部跳转 continuation；
- 真实 script494 的 opcode8 立即空音频与离场 music3 覆盖；
- 场景 5 的 300 tick RNG、粒子位置和半透明天气像素；
- 真实脚本 36 的背包与十四天书事件解锁、931 的条件休息、581 的满武功槽与入队清理、950 的离队清理；
- 真实脚本 274 的场景层写入和 opcode 0 呈现边界、931 的 opcode 13/14 淡入淡出顺序、69 的 TALK 暂停/恢复；
- 所有既有 model/resource/render/world/persistence/ui/audio/core 测试无回归。

同一13项包含上述场景同步链、全部既有模块测试和 SDL dummy smoke。

后续 B7 门禁仍包括全 opcode 基本块审计、Linux/Windows Debug/Release、ASan+UBSan、原资产只读和 `.i64` 审计。
