# B7 场景、事件与对话证据

状态：进行中。本文已固定 B7 的资源、场景绘制、移动/碰撞、十一处天气场景、KDEF 调度核心和 app 同步进入/返回链；全部高阶剧情副作用和战斗回收仍在后续小提交逐项审计，不能据本文提前宣称 B7 完成。

## 1. 真值与证据

唯一行为真值仍是当前 `Z.COM` / `Z.DAT` 机器码和父目录原始资产。

- headless IDA 脚本：`research/ida/scripts/ida_b7_scene_xrefs.py`
- IDA 报告：`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`
  - SHA256：`9e2310396c323ba7647fa6afec3ecf27f5081dc7ed9f2a0139430833c977d4a9`
- 独立 oracle：`research/tools/generate_b7_scene_goldens.py`
- oracle 输出：`research/evidence/scene-goldens.json`
  - SHA256：`9cc8cd0e52d551782140e0cb2d363bb651b955111b37ece2b378547467c66a7d`

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

当前核心已建立 0–67 的同步 PC/偏移执行边界、对话/问题/商店/战斗请求队列及 snapshot 副作用入口。opcode6把battle id与get-exp word交给runtime-owned `BattleSession`并进入实际battle view；消息队列耗尽后，runtime销毁battle资源、恢复scene音乐，并仅把`Victory`映射到`battle_victory`真偏移，`Defeat`映射到`battle_defeat`假偏移；AI逃跑不离开战斗。`sub_2DE03`推进为`implemented_pending_review`。高阶剧情的每个基本块仍需最终双向REVIEW，本文不把“已有 switch”当作全部语义已闭环。

### 4.1 场景与事件状态写入

`sub_2D841`已完成最终汇编→C++ REVIEW。入口为947字节、224条指令；IDA加载字节与原始`Z.DAT`字节SHA256分别为`440fa743904d2e706164bdb13f2e93b07c3f64e305576de29fdea2dcac133882`、`c21b05d61621b5bae5da0af482bcec71b1cc853d58092d63531369263b809f77`，53个差异字节全部由DOS加载基址重定位解释。完整xref含65个callsite、11个owner，覆盖解释器、四类动画、十四书门禁、武林大会、结尾事件禁用、商店退出和商人刷新。

- opcode3 对十一事件字段逐项把 `-2` 解释为保持原值；当前归档中scene `-2`与event `-2`指向当前上下文，event `-1`还会先清实际触发格；外部归档路径直接使用显式event并整区读改写4,400字节事件数据；
- 坐标修改必须在写新x/y前保存旧值，再清当前工作scene旧事件格并写新格；即使事件记录属于外部scene，地图副作用仍固定落在当前工作scene；
- 全资产2,320次opcode3的完整参数流SHA256为`b2f3719272af716d0428a923b56d213e0c9bb7acc8e4ce5daf2dff1bd47ebee0`：2,009次当前scene、311次显式scene、461次当前event；30次坐标修改全部成对提供x/y且目标当前scene，坐标流SHA256为`c4ce5ae5c8b844ef50f3c618232b9e9ffb34b3d79efb491267a128e0c3673921`；
- 首轮对照发现`sub_312A6`的现代商店退出只写event_3=939，而机器6个callsite实际把fields0..7重置为`0,0,-1,-1,939,-1,-1,-1`；修正后作废首轮结论并从入口重审，第二轮零新增差异；
- 现代100场景常驻snapshot替代外部归档即时I/O，并安全拒绝真实资产未使用的外部负event、单轴坐标和越界参数，归类`platform_adapted`。

synthetic KDEF固定三条独立路径：当前event坐标迁移；event `-1`清触发格并保持记录坐标；外部scene69选择性修改事件字段/坐标但只更新当前scene70地图。商店六个scene的取消路径把fields0..7预置777后核对完整复位。`sub_2E337` opcode17仍按自身后续closure独立终审；其`4096*layer + 64*y + x`写入不能由本helper提前关闭。

`sub_2DBF4` opcode26已完成最终汇编→C++ REVIEW。入口为337字节、83条指令；loaded/raw函数SHA256分别为`d2e9050a19528c43b63e2eb23b5e0bba12d90d42e14016c994e20d9d763574d2`、`e00185c2c24269ac8363297748f1b62ccb6d0aca63330049ab12c475a47755dc`，21个差异字节全部是DOS重定位。唯一caller为解释器case26：有符号载入5个参数，返回后回收20字节并固定PC+6。当前路径解析scene/event `-2`，外部归档路径整区读改写目标scene的4,400字节事件区；两条路径均依次对event_1/2/3执行低16位回绕加法并返回0。全资产121次opcode26的参数流SHA256为`5637e9a38a976f3ad7d1aa8aa1eb0e54d039ecf305cfece583e8b28d82660b9c`：115次当前scene、6次显式scene，event无哨兵，delta仅为`(0,0,1)`21次和`(0,1,0)`100次。现代常驻snapshot与显式`wrapping_add`在合法域一致，并安全拒绝外部负event和越界索引，归类`platform_adapted`；现有synthetic当前event回绕与外部scene隔离向量均通过，首轮完整复核零新增产品差异。

`sub_2DD45` opcode4已完成最终汇编→C++ REVIEW。入口为50字节、11条指令；loaded/raw SHA256分别为`9761e098dd4e85e0fac653e44db28a12dc85e5d991ff117d58c5959bd85eafd6`、`c224a06282aa4d8827a79d7eb8d16b8ac82f16101e9bc83a3e267e989dec8696`，3个差异字节全部是DOS重定位。唯一caller把expected item ID、真偏移、假偏移传入，固定执行`PC+4+返回偏移`。机器按最后确认的实际库存slot重读当前item ID，命中时写无人读取的`word_54B7A=1`并返回真偏移，失败仅返回假偏移；相邻地址逐字节xref排除了重叠dword读取。全资产167次opcode4均为PC0、真偏移1、假偏移0，完整参数流SHA256为`89690e50d4231de72f458f6e5c5f1aae8390024b7872323674258ce752ef3e96`；40条ALLDEF引用流SHA256为`8ecd2bae575f1720bdabf6b7e5656a9d6c57cca80180c9c55706eb6e3986207b`，且全部位于event_2物品使用脚本。现代在确认slot时保存item ID，当前合法域比较前没有库存变化；省略死写并安全拒绝非法索引，归类`platform_adapted`，synthetic命中/失败偏移通过，首轮完整复核零新增产品差异。

### 4.2 已复核的角色与物品副作用

本切片逐基本块对照 `sub_2D678`、`sub_2DF0E`、`sub_2E1E8`、`sub_2E078`、`sub_2F3F0`、`sub_2F62F` 和 `sub_300FF`，固定：

- 添加物品或增加声望后，仅当主角声望不少于 200、物品 144–157 全部存在且物品 189 不存在时，才把场景 70 事件 11 改为原武林大会入口；
- 休息只恢复受伤值小于 33 且未中毒的队员，不替中毒或重伤角色清毒/治疗；
- 角色加入和离开队伍时按原版清除两件装备、修炼物品及修炼经验，并把对应物品 user 恢复为 `-1`；队伍槽 0 始终保留主角；
- 学习武功和指定武功槽在没有空槽时覆盖槽 0，`-1` 槽参数按原版先寻找空槽。

真实资产测试使用 KDEF 脚本 36、931、581 和 950 验证上述状态，不用合成脚本替代当前资产路径。

### 4.3 逐帧呈现与淡入淡出

逐基本块对照 `sub_2C319` 的 opcode 0、13、14 分派以及 `sub_3CC97`、`sub_3CD17` 后，事件解释器不再把视觉调用当作无输出指令：

- opcode 0 返回 `SceneStepKind::present`，至少完成一次宿主 framebuffer 上传后才恢复事件 PC；
- opcode 13 返回 `fade_from_black`，按 65 帧序列从全黑恢复场景 RGB6 调色板；
- opcode 14 返回 `fade_to_black`，按 64 帧逐通道递减序列得到全黑调色板；
- 每帧仍先由 `SceneSession` 重绘同一 index8 场景，`LegacyGameRuntime` 只在最终呈现边界替换 palette，不修改像素索引，也不把 SDL 类型引入核心；
- 视觉步骤呈现完毕前脚本 PC、天气和玩家输入保持阻塞，完成后 app 同步调用同一 `SceneSession::resume()`；连续 opcode 14、场景修改、opcode 0、opcode 13 因而保持原基本块顺序。

真实资产测试使用脚本 274 验证 opcode 0 在两次场景层写入后产生呈现边界，使用脚本 931 验证 `对话 → present → fade_to_black → 休息/换位 → present → fade_from_black → 对话` 的阻塞次序；64/65 帧 RGB6 序列继续由 render 单元测试逐帧固定。

### 4.4 物品格直角边框 primitive

`sub_2D501`只负责依次绘制矩形上、左、右、下四条1像素边，不填充内部。唯一owner `sub_2A186` 的两个callsite对5×3物品格先绘制color0的 `40×40` 普通框和物品图，再以color255重画当前格。现代 `IndexedFramebuffer::outline_rectangle` 保留四次填充顺序，已由battle物品选择路径按原几何调用；独立背景index7 oracle固定普通框 `0x63eb8c2a7f900ed9`、选中框 `0xe154c07ba899cba5`，内部 `(56,63)` 保持7。

当前world/scene普通菜单仍是8行文字列表，未接线原5×3轮廓primitive；`sub_2A186`的背景、图标、分页、详情和输入整体继续在 `ui-closure.tsv` 保持待审，不由本helper closure提前关闭。

### 4.5 HDGRP头像记录与锚点

`sub_2D590`以head ID直接索引 `HDGRP.IDX/GRP`，不执行普通legacy sprite编号除二；读取单帧后把 `(x,y,frame,framebuffer)` 转交RLE绘制并固定返回1。完整xref包含角色状态两页的 `(78,68)`、对话四个头像框的 `(x+2,y+59)`，以及战斗角色状态面板双方的 `(242-side_offset,82)`。现代 `SceneSession::draw_portrait` 与 `BattleRenderer::draw_portrait` 均直接读取 `PackedArchive::entry(head_id)` 并转交同一RLE renderer，覆盖5个物理callsite。

独立oracle逐帧验证全部115个HDGRP记录：IDX末偏移恰覆盖249,276字节GRP，共7,568个run、226,957个非透明像素；七类caller锚点上逐个绘制所有记录后的串联SHA256为 `92ec7ccffd3068c0c04831d6eaebec18e917892ddfcd27cdc2e5792150d2b0db`。RANGER的320个角色head ID范围0..109且全部有效；3,561次opcode1中实际绘制style的head ID也无越界。现代一次性缓存archive并拒绝损坏资源/非法ID，替代机器共享IDX类型缓存及越界访问，合法静态资产输出一致。

### 4.6 显式裸场景呈现

`sub_2D653`固定先调用`sub_29D2D`重绘scene，再经`sub_3D6D1` tail-jump到`sub_20039`，从全局buffer向VGA `0xA0000`复制`0x3E80`个dword，恰覆盖64,000个index8像素字节，最后返回0。完整xref为48个callsite、19个owner，不限于KDEF opcode0；独立oracle与IDA报告逐地址相等，并分为10处模态关闭恢复、22处延迟动画帧、1处脚本行走站立终帧、12处武林大会边界和3处商店反馈边界。

首轮汇编→C++ REVIEW修正三类差异：物品/能力/品德notice关闭后补裸场景present；opcode30清零行走offset并恢复方向基础图后补无额外延迟的站立present；商店成功/失败反馈改为`present→dialogue→present`。这同时保证下一条dialogue首屏冻结的是裸场景或站立帧，而不是旧notice、shop或最后行走帧。视口平移、图片/雕像/结尾动画和武林大会既有present数量与延迟未发现产品差异。

真实script343最后五个wait3行走帧后新增位置`(28,19)`、方向0、图号5002、wait1站立帧，FNV-1a64为`0x83bc0f8904252115`；正常与阻挡路径均在该帧之后才进入talk1248。scripts36/149/581/825、world菜单物品事件、武林大会奖励及商店成功/失败测试固定全部恢复顺序；opcode53声望查询因机器无此恢复调用，仍直接结束。

现代以同步`SceneStepKind::present`运输，`LegacyGameRuntime`重绘64,000字节indexed framebuffer，SDL完成RGBA转换、纹理上传及`SDL_RenderPresent`后才恢复事件；不写VGA地址及render/upload错误返回属于平台适配，合法像素与阻塞顺序一致。

### 4.7 添加物品提示与十四书门禁

`sub_2D678`固定扫描全部200个背包槽：所有匹配物品ID的count均做16位回绕加法；完全无匹配时只使用首个ID为`-1`的槽，并在该槽残留count上相加；库存满时不修改但仍继续提示。机器从190字节物品记录byte 2读取名称，以Big5 `得到%s`生成提示；面板按名称字节数`N`取`x=150-(4*N+16)`、`width=8*N+52`，在caller当前framebuffer上绘style4圆角框和index `5/7`文字，等待任意键后恢复裸场景。

首轮汇编→C++ REVIEW修正了显式word回绕、ASCII编号/固定黑框、提示前错误重绘和重复render叠加，并为武林大会物品143奖励补上同一函数内必经的十四书门禁。门禁在库存修改后按有符号声望`>=200`、物品144..157 ID全部存在且物品189 ID不存在判断，完全忽略count；满足时把scene70/event11改为script932和picture7968。

全KDEF共325次opcode2、148个物品ID，RANGER全部200个名称在20字节字段内NUL终止且长度4..16。独立oracle固定物品109 `得到倚天劍`帧`0x8397ba508b05051f`、最长16字节名称布局、全合法提示hash流、库存满仍提示、重复槽回绕、残留count、count0 presence和大会caller门禁；现代对非法ID和非终止字段的安全边界归类为平台适配。

## 5. TALK 分页

`sub_2CC21` 在固定 `218×57` 对话框内调用文本例程并在每页后阻塞等待输入。当前资产使用 ASCII `'*'` 作为显式换行；Big5 trail byte 合法范围不包含 `0x2A`，因此可无歧义识别。

核心按以下规则运输对话页：

- 每页最多三行；
- `'*'` 结束当前行；
- 不按对话框宽度软换行；只有 `'*'` 增加17像素行距，第三次显式换行结束本次调用；
- 第三个 `'*'` 后恰逢记录终止时仍保留下一次终止符调用形成的空白末页；
- 正文固定从面板 `(x+13,y+3)` 开始，ASCII 前进8像素、Big5前进16像素；
- `sub_20615/sub_20663` 不裁剪横向越界，超过319的置位像素按线性 framebuffer 地址落到后续扫描线；
- style0/1/4/5绘制 `60×62` 头像框：十一段圆角区域混色3,660像素，再按head ID读取HDGRP，最后十二段index255白边覆盖220像素；style2/3不读取头像；
- 每页作为同步 `SceneStepKind::dialogue` 返回，app 确认后才继续同一事件 PC；
- 第一页直接叠到caller现有framebuffer，不预先重绘scene；同页宿主重画恢复冻结底图，第二页及以后才在每页首次绘制前执行一次裸scene重绘；
- scene5真实script18两页frame分别固定为 `0x8d9f538b1482e95e`、`0x372fe4647b884671`，对应首屏0次与第二页2次RNG消费；
- 当前2,977条TALK最大为talk1360的21页，最大显式行宽为344像素（talk1841）；3,561次KDEF opcode1实际style为0/1/2/4，talk/head均在合法域。

## 6. 场景天气

`sub_28E40` 只为场景 `5, 7, 10, 41, 42, 46, 65, 66, 67, 72, 79` 打开天气路径。现代场景会话复用 B6 已由机器码闭环的 `CLOUD.IDX/GRP` 合同：

- 三粒子 kind、weight、x、y 按原 LCG 调用次数生成；`bounded(1)` 不推进 RNG；
- 活跃粒子每 tick 横向加一，全部越过 500 后才重新生成；
- 场景移动按菱形投影反向平移粒子；
- RGB6 以 `source*weight/32 + destination*(8-weight)/32` 混合，再经 RGB4 最近色表写回 index8；
- 对话/问题等阻塞输出期间不推进天气。

独立 oracle 对场景 5 固定：入口 `(17,48)`；300 tick 后 RNG `0xaf1cf0fb`，粒子为 `(kind,weight,x,y) = (2,7,9,49), (2,7,18,-4000), (1,8,11,160)`，framebuffer FNV-1a64 为 `0xb3e2b127988e5690`。

## 7. app 同步消费

`main @ 0x20D35` 已映射到 SDL 顶层、`LegacyGameRuntime` 与 legacy keyboard edge：世界 tick 按 left→up→down→right→menu→idle 选一个分支，只有实际打开菜单才消费 Esc odd edge；方向命中时保留键态到后续 tick。成功 present 后另按原计数右旋 palette entries 224..231 与244..252，使结果从下一帧生效；计数由 runtime 跨 world/scene 持有，scene 只在原外层 present continuation 后推进并回写，模态等待帧不推进。其启动/退出所委托的 UI、world、input、audio 与 platform callees 仍按各自 inventory 等待最终 REVIEW。

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

Linux app Debug BUILD 脚本：14/14 测试通过，包括：

- 2,977 条 TALK 数量、首尾记录解码、显式三行分页、第三换行后的空白末页与最大344像素行；
- HDGRP 115帧的记录边界、全部7,568个run、320个角色head域、七类caller锚点逐帧聚合hash，以及真实 scripts1/142/244/515 的 style0/1/2/4、头像、无头像和线性越界 framebuffer hashes；
- 1,018 条 KDEF 全量终止、opcode 合法域、13,315 条频次，以及 synthetic opcode3/4/5/6/9/11/13/14/16/17/24/26/68 的状态写入、条件、问题框、同步、载入菜单与 PC 不推进边界；
- 四个核心 GRP 的 FNV-1a64；
- 场景 70 初始像素、碰撞轨迹、入口/主循环/出口/内部跳转 continuation；
- 真实 script494 的 opcode8 立即空音频与离场 music3 覆盖；
- 场景 5 的 300 tick RNG、粒子位置和半透明天气像素；
- 真实脚本 36 的背包与十四天书事件解锁、931 的条件休息、581 的满武功槽与入队清理、950 的离队清理；
- 真实脚本 274 的场景层写入和 opcode 0 呈现边界、931 的 opcode 13/14 淡入淡出顺序、69 的 TALK 暂停/恢复；
- 48个显式scene present callsite的完整地址集与五类无重复分区，script343站立终帧像素，notice/商店/武林大会恢复序列及world菜单回收；
- 325条opcode2和大会奖励caller的库存word回绕、Big5物品名动态面板、caller底图/RNG不重绘，以及十四书与武林帖ID presence门禁；
- 所有既有 model/resource/render/world/persistence/ui/audio/core 测试无回归。

同一14项包含上述场景同步链、全部既有模块测试和 SDL dummy smoke。

后续 B7 门禁仍包括全 opcode 基本块审计、Linux/Windows Debug/Release、ASan+UBSan、原资产只读和 `.i64` 审计。
