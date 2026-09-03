# B7 场景、事件与对话证据

状态：进行中。本文已固定 B7 的资源、场景绘制、移动/碰撞、十一处天气场景、KDEF 调度核心和 app 同步进入/返回链；全部高阶剧情副作用和战斗回收仍在后续小提交逐项审计，不能据本文提前宣称 B7 完成。

## 1. 真值与证据

唯一行为真值仍是当前 `Z.COM` / `Z.DAT` 机器码和父目录原始资产。

- headless IDA 脚本：`research/ida/scripts/ida_b7_scene_xrefs.py`
- IDA 报告：`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`
  - SHA256：`9e2310396c323ba7647fa6afec3ecf27f5081dc7ed9f2a0139430833c977d4a9`
- 独立 oracle：`research/tools/generate_b7_scene_goldens.py`
- oracle 输出：`research/evidence/scene-goldens.json`
  - SHA256：`da1d1f35d6c504f6081765b12ebf687e290860a8830dfb78851ea703c8475cdf`

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

`sub_2DD77` opcode5已完成最终汇编→C++ REVIEW。入口为140字节、36条指令；loaded/raw SHA256分别为`029ba93c5d02ac74cde1c9cd81457e1b7a2d5d03fa5dd57ab9bb1148b2982e50`、`224f32fa14859850cd390ce56d0e8e6a15b4e15a3c392cad7c08ddad40953cb9`，9个差异字节逐一对应9个`raw+0x20000`线性地址重定位operand。唯一caller有符号压入真假offset，固定执行`PC+3+返回偏移`。机器在当前底图依次绘制`(61,40,187,27)`面板和`(71,45)`问句，使用阴影5/前景7，present一次后阻塞读取任意非零翻译键；仅大写Y返回真offset，其余键不经过过滤循环而立即返回假offset，按键后不额外重绘或present。全资产43次opcode5参数流SHA256为`97dfd093e7b5e3a8338317bb4f8a63820643e5795e6ea2587d339507b5c64d82`；除常见`(N,0)`外，script307 PC10和script308 PC5使用`(0,52)`反向布局。现代原文、panel、frame `0x5d8fc752d48d9a98`、真假选择和PC公式均一致；宿主帧循环替代函数内同步阻塞，归类`platform_adapted`。synthetic普通/异常真假向量通过，首轮完整复核零新增产品差异；文字与present wrapper自身仍由各自后续closure独立终审。

`sub_2DE39` opcode8已完成最终汇编→C++ REVIEW。入口为26字节、6条指令；loaded/raw SHA256分别为`5012c8521831e2ed1dfa774f797ca3a7cc4c1a2480d4b0d21db2d800fa7232d5`、`1e3041382c346c484567b5d4e21d23d2f3acd70478a1a044679e594c3395eabf`，唯一差异是`word_D2952` operand由raw `0xB2952`重定位为loaded `0xD2952`。唯一caller有符号压入music word、固定PC+2并忽略helper返回；helper只写覆盖字，不立即播放。`word_D2952`全部4条xref固定场景入口置-1、opcode覆盖、离场读取和强制播放后复位；覆盖分支不比较当前音乐，只有metadata离场音乐才比较`dword_C0B9C`并抑制相同曲目。后者全部5条xref又证明`sub_3E1B2`是唯一运行期写者。全资产15次opcode8均为music3，参数流SHA256为`c95fb3ed14b8cc28ac093196b62e813315e12ce56fc9fe4a8c89e7453238261b`。现代延迟到scene-exit发`force=true`命令，在下一帧world呈现前跳过current-music比较；script494与普通metadata出口回归固定即时无声、离场强制3和普通非强制10。调度归类`platform_adapted`，首轮完整复核零新增产品差异；delegated scene/audio主体仍按自身closure独立终审。

`sub_2DE7D` opcode9已完成最终汇编→C++ REVIEW。入口为145字节、37条指令；loaded/raw SHA256分别为`bbaddd1162733a447cb95055c3e87a400815b84532ef2d089424440ae6feaed6`、`17f05acd776abbc7b1add52c71897b59e31f4c167d7c0d4642d9201daa6763cb`，9个差异字节全是`raw+0x20000`地址重定位。唯一caller有符号传入真假offset并固定`PC+3+selected_offset`。机器清键、复制23字节`是否要求加入（Ｙ／Ｎ）`、绘制`(61,40,187,27)`面板和`(71,45)`阴影5/前景7文字、present后等待任意非零键；取得键后无条件重绘并present裸场景，再仅以大写Y选择真offset，其他键立即选假offset。全资产81次opcode9参数流SHA256为`8be8acd438f85e423576e905d78fbbb4f4c2aba1daf78ff85330b14a217c018c`，四条反向布局为scripts304/306的`(0,47)`与307/308的`(0,42)`。现代join专用`conditional_after_present`和runtime大写Y映射保持相同顺序；scene70 question frame为`0xbea93863a81cd9e0`，synthetic Y/非Y两路均固定按键后的裸场景present。宿主帧状态归类`platform_adapted`，首轮完整复核零新增产品差异；同址input-font与delegated UI closure继续独立pending。

### 4.2 既有角色与物品副作用实现

此前实现切片覆盖 `sub_2D678`、`sub_2E1E8`、`sub_2E078`、`sub_2F3F0`、`sub_2F62F` 和 `sub_300FF`；除已在独立小节关闭的函数外，仍须按inventory顺序完成最终汇编→C++ REVIEW。当前实现包括：

- 添加物品或增加声望后，仅当主角声望不少于 200、物品 144–157 全部存在且物品 189 不存在时，才把场景 70 事件 11 改为原武林大会入口；
- 休息只恢复受伤值小于 33 且未中毒的队员，不替中毒或重伤角色清毒/治疗；
- 角色加入只清角色自身装备/修炼引用而不写全局物品user；角色离队则先解绑对应物品user再清角色字段；两者的队伍扫描均不包含slot0；
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

### 4.7 角色加入与携带物转移

`sub_2DF0E` opcode10已完成最终汇编→C++ REVIEW。入口为362字节、91条指令；loaded/raw SHA256分别为`a884afc61c5a9b4bc46539ae2b7b6e00eb0024455c36b669494a61840fcdb4a2`、`39407bc9f8117c533fa91b1ef4af9c5cef7a7a2e5a975203de8d9fcd29fd84bf`，21个差异字节全部是`raw+0x20000`地址重定位。唯一caller有符号载入role ID，返回后固定PC+2。helper只扫描队伍槽1..5，首个signed `<=0`槽接收角色；即使未找到槽，仍继续角色清理。

角色四个携带槽按0..3处理，只有ID `-1`为空。每个非空槽先把原count交给`sub_2E571`，再以Big5 `得到%s`和item record byte2名称绘动态面板；notice确认并裸场景present后才把当前槽清为`(-1,0)`。全部物品结束后才把角色word 23/24/61/62清为`-1,-1,-1,0`；机器没有任何`item.user`写入。

首轮REVIEW据此修正了ASCII编号notice、首个notice前预清全部状态及入队时错误解绑全局item.user三类差异；修正后从入口重审，第二轮零新增差异。全资产80次opcode10参数流SHA256为`bdad3c7d40a1a5a512ca7b6784924e527094b966b8a923b112b5c4a1d169f543`，26个role ID全在1..76；基准104个携带槽仅含`-1`或0..171有效item ID，count均非负。现代专用逐物品continuation归类`platform_adapted`；`sub_2E571`背包主体仍按其后续closure独立终审。

### 4.8 角色离队、队伍压缩与物品解绑

`sub_2E078`已完成最终汇编→C++ REVIEW。入口为221字节、45条指令；loaded/raw SHA256分别为`69caaca765139c6fb1333c9bddbce324f325f8583dde02dd6f8a6fc39e474cdc`、`83a294457130b648e49416cd3883755e376ef4de2a1153a98302e1d1c956901f`，14个差异字节全部是`raw+0x20000`地址重定位。两个caller分别是解释器opcode21和武林大会收尾。

helper只扫描team slots1..5，移除首个精确匹配role并左移后续槽，slot5写`-1`；未命中不改team但仍继续清理。它按角色word23、24、61读取两件装备与修炼物，对每个非`-1` item把word38 user写`-1`，再把角色word23/24/61/62写为`-1,-1,-1,0`。全KDEF 35次opcode21参数流SHA256为`d08335eb84f4c585ab7ed59750f0593730f09d05e5ed2eaf2a5796f709de97f2`，26个role ID全在1..76。

武林大会caller从物理index6递减到1；index6越过队伍数组并确定性别名inventory item0，现代case59显式保留该来源，再按每次清理后的最新team读取slots5..1。script932用inventory item0=role6固定“不在team仍清个人物品”，script950直接固定队伍左移、尾槽、三条item.user解绑及角色字段清理。首轮完整对照零产品差异；现代只对资产未使用的非法role/item越界访问增加安全拒绝，归类`platform_adapted`。`sub_30559`其余剧情副作用不由本closure提前关闭。

### 4.9 休息问答与真假偏移

`sub_2E155`已完成最终汇编→C++ REVIEW。入口为147字节、37条指令；loaded/raw SHA256分别为`fb29e361de2fee971ffaf9d5c210d25e90f8b91b3060d4d98852278934f79cd3`、`9d4b0c7d1e4ef096ee12bb3e010ca7181a9fb561b9c94eadd27d4edb78a4a5c3`，10个差异字节全部是`raw+0x20000`地址重定位。唯一caller为解释器opcode11。

helper把原Big5`是否住宿過夜（Ｙ／Ｎ）`复制到缓冲区，在caller当前framebuffer的`(61,40,187,27)`绘混色圆角面板，于`(71,45)`以颜色`5/7`绘字并present；阻塞读取前再次清键。只有ASCII大写Y返回true offset，其他任意键立即返回false offset；按键后不重绘或额外present裸场景。caller最终PC为`old_pc+3+selected_offset`。

全KDEF仅7次opcode11且偏移均为`(1,0)`，完整参数流SHA256为`99755f8a57634d2a2d4ae1b53ceb30ffde7bc40daeb84eba58a0395452355399`。synthetic Y/非Y分别直达notice/stay，固定分支方向与按键后无present；首轮完整对照零产品差异。宿主按键continuation替代DOS函数内忙等，归类`platform_adapted`；委托文字和present函数继续按各自closure终审。

### 4.10 队伍休息恢复

`sub_2E1E8`已完成最终汇编→C++ REVIEW。入口为144字节、35条指令；loaded/raw SHA256分别为`bea92342233ace5d16c75fe711c95309f226fbcb7332c656a7b738153356c80a`、`456ef3b7091131fb8e51bd73c7fb56560b5f3392d5e30518e86b7cfc35553a11`，10个差异字节全部是`raw+0x20000`地址重定位。唯一caller为解释器opcode12，固定PC+1且忽略返回0。

helper从team slot1扫描首个signed `<=0`哨兵；若没有哨兵则队伍尾为6。它只处理`[slot0,队伍尾)`，不跨队伍洞；每名角色仅当signed伤势小于33且中毒值恰为0时，依次写伤势0、体力100、内力上限和生命上限。伤势恰33、中毒非零和洞后角色均不恢复。

全KDEF仅7次opcode12，位置流SHA256为`290cc136d68a25418cc28074407fe9dcc421ada2be21a1b9b874ee773e6696a6`。真实script931固定伤势32恢复、中毒不恢复、伤势33不恢复和不跨洞；synthetic无哨兵满六人固定slots0..5全部恢复。首轮完整对照零产品差异；非法role由现代安全边界稳定跳过，归类`platform_adapted`。

### 4.11 场景从黑淡入回调

`sub_2E278`已完成最终汇编→C++ REVIEW。wrapper仅18字节、5条指令，loaded/raw SHA256均为`b2c3f06340020f016c1ea1bdc785c29984c9380a13df8ffcf3b03a3aa56edf24`；它无参数调用委托淡入函数，清EAX并返回。4个callsite分别是解释器opcode13，以及武林大会战后胜利、轮间恢复和终局事件禁用后的三个时序点，均忽略返回0。

全KDEF有346次opcode13，位置流SHA256为`719e01b8842f5d467f82cd3160e1cfcdbbf35e7ba602ca2148f6aa6920dd935e`。synthetic `[13,14,-1]`固定淡入完成后才产生淡出；武林大会全胜路径固定15次战后、4次轮间和1次终局淡入。现代宿主逐帧continuation替代DOS同步调色板循环，后续PC或对话仍只在效果完成后继续，归类`platform_adapted`；淡入算法本体继续按其UI closure独立终审。

### 4.12 场景淡出至黑回调

`sub_2E28A`已完成最终汇编→C++ REVIEW。wrapper为18字节、5条指令，loaded/raw SHA256均为`9676e06789973719b49261b6257f5fd6a124ed06284db962d5a4b1785a848cc4`；无参数调用委托淡出函数，清EAX并返回。3个callsite分别是解释器opcode14，以及武林大会轮间、终局的两个时序点，均忽略返回0。

全KDEF有171次opcode14，位置流SHA256为`f40aad4e13d6cf6bf668883a8d3243002c7a5e01d4a1664a779d4542ae9d9575`。武林大会全胜路径固定4次轮间、1次终局淡出；轮间机器淡出后的300计数延迟换算为额外8 ticks，只在64帧淡出完成后于黑屏消耗。现代逐帧continuation保持PC、恢复和事件禁用顺序，归类`platform_adapted`；淡出算法本体继续按其UI closure独立终审。

### 4.13 全六槽队伍成员条件

`sub_2E29C`已完成最终汇编→C++ REVIEW。59字节、21条指令；loaded/raw SHA256分别为`a7ea8f35b08b716d80015bbba9399d4fa5ca2c2841db6c8603cc6b569bf66fca`、`da0eda07dea8d49ba683890ffb22810eb54adfd88b3bd25d840c69a6945d9d2a`，唯一差异是队伍数组绝对地址加`0x20000`的DOS重定位。

helper始终扫描slots0..5，不因非正值停止；任一signed角色ID匹配即选择真偏移，否则选择假偏移。全KDEF有80次opcode16，参数流SHA256为`7d7884ffea7b6dcb4f7d0e79f30273b98b1ee5ceceba807c68da412d5b5e6e73`，角色参数为17种有效正ID。synthetic同时固定空洞后slot5命中和完整未命中两条路径；现代提前返回只省略无副作用的剩余只读扫描，所有输入的可观察结果一致，归类`assembly_exact`。

### 4.14 队伍尾槽满员条件

`sub_2E2D7`已完成最终汇编→C++ REVIEW。30字节、8条指令；loaded/raw SHA256分别为`e665180158e7ce7fda541b7ccaa3d1505a80fc9f43e909bf011afa2fa9311848`、`246a70b7a63137d2e8da922049101dc74fa08732fddfbb6418679363be6e0abc`，唯一差异是slot5绝对地址加`0x20000`的DOS重定位。

helper不扫描队伍，只判断signed slot5是否严格大于0；全KDEF有82次opcode20，参数流SHA256为`0737d0189d784163a00c0ef3ad8b71c9f86ee1c18259fe5cd1111f8c85baa831`。真实script11固定尾槽`-1/0`均走对话30、前方有空洞但尾槽9仍走对话175。现代case20比较和PC偏移完全一致，归类`assembly_exact`；计划旧“声望增加”标签已按机器职责校正。

### 4.15 背包物品ID存在条件

`sub_2E2F5`已完成最终汇编→C++ REVIEW。66字节、22条指令；loaded/raw SHA256分别为`4558ac4d5836ca13bd20fd042834b3555bcf550eb84a6062fd93e01ba0d9daf6`、`91a65940ab92f424e14810ac5ccf3ffc31df8955e45e6f1b1b813e03666854ca`，唯一差异是背包item ID基址加`0x20000`的DOS重定位。

helper扫描slots0..199并在首个item ID匹配时停止，完全不读取count。全KDEF仅script37/38两次opcode18，参数流SHA256为`3f25065164137f7c2ec64615393422cc449b804fa50db0183a5ed188bd19ab6f`。真实script37固定slot0 count0仍命中、完全缺失走对话139、仅slot199 count-32768仍命中；现代ID存在判断和PC偏移完全一致，归类`assembly_exact`。

### 4.16 场景图层格写入

`sub_2E337`已完成最终汇编→C++ REVIEW。308字节、85条指令；loaded/raw SHA256分别为`6c1c25cc2e99999f7b1988d692fea01e5b3049f7c8e0da50efdc7c4c3c684a1f`、`1086a5e957eef0bba5d7d11dc4a82ac3e6cef5fb2f2b2b9d427e89c10794a883`，15个差异字节均为绝对地址加`0x20000`的DOS重定位。

`scene==-2`按word索引`4096*layer+64*y+x`直接写当前场景；显式scene在原机先保存当前场景、换入并回写目标场景、再重载当前场景。全KDEF有127次opcode17，参数流SHA256为`14b12ef0f37fd4a7d005791d94c30b376193b1c7c383665940a90b65fb51d526`且场景/层/坐标全合法。现代100场景常驻快照保持目标word和活动场景不变的可观察结果，非法参数稳定拒绝，归类`platform_adapted`；底层归档callee继续独立终审。

### 4.17 场景位置与视口重定位

`sub_2E46B`已完成最终汇编→C++ REVIEW。203字节、38条指令；loaded/raw SHA256分别为`eb441b79259ba6aa32de956c9810d7e941a8887dcc81ad9432402edcdbaa889f`、`eafef387d4fdbb2b4550bff9b8dc83d246fc0101c9fe7e12a683fe55f5f62ead`，19个差异字节均为四个场景状态word绝对地址加`0x20000`的DOS重定位。

helper把signed x/y钳位到`0..63`，再将各自减11并钳位到`0..36`作为视口原点；不绘制或present。全KDEF有15次opcode19，参数流SHA256为`59063796bd864eb36610866d9bdca4e490bde722bb3bee802fa481bced4c2d0b`。真实script235固定`(14,14)→位置(14,14)/视口(3,3)`，synthetic覆盖两个signed极值到对角边界；现代最终四word和PC推进完全一致，归类`assembly_exact`。

### 4.18 清零当前队伍角色内力

`sub_2E536`已完成最终汇编→C++ REVIEW。59字节、15条指令；loaded/raw SHA256分别为`e53b0d8946ce0db18f1724391d0f162516aada5f2ddbbeef5541eaeef0db7ccc`、`a181178559c2a742c28d42aa2d817d1cbf80f764c6332397c0521ac23f03ec4d`，三个差异字节均为队伍/角色绝对地址加`0x20000`的DOS重定位。

helper固定访问六槽：slot0无条件清对应role的mp，slots1..5仅signed role ID`>0`时清mp，遇非正槽不停止。全KDEF仅`script20,PC10`一次opcode22。真实流程固定slot0 role1被清、后续role0保持、负空洞后的role2仍被清；现代非法role范围保护替代原机未定义越界，归类`platform_adapted`。计划旧“内力增加”标签已按机器职责校正。

### 4.19 共享背包物品增加

`sub_2E571`已完成最终汇编→C++ REVIEW。117字节、37条指令；loaded/raw SHA256分别为`7da4e540edaf77edb68a05f3a85e3b31cb02be40f4e056b90f010c6d30e0b94b`、`75b865d78c74b0e3e7b5870fbec15802322cd0c3e412d5b65adda7fced77a035`，五个差异字节均为背包ID/count地址加`0x20000`的DOS重定位。

helper第一轮固定扫描200槽并对全部目标ID匹配槽做16位回绕加法；仅完全未命中时，第二轮写首个ID为`-1`的槽，并在其残留count上相加。全满未命中不修改；目标ID为`-1`时全部空槽在第一轮视为匹配。两个唯一caller为入队携带物转移与商店购买，均不消费偶然EAX返回值；现代`add_inventory`逐项一致，归类`assembly_exact`。

### 4.20 写入角色用毒能力

`sub_2E639`已完成最终汇编→C++ REVIEW。32字节、7条指令；loaded/raw SHA256分别为`b3b2507c471dd0ca376f333d960f71852b018c789f0d5583aab3f8bb5d6c9709`、`b65a65a395a3aa0f7223295061cebe037d1eeca0b4f0272f0a10352cecbccaf8`，唯一差异字节为角色`use_poison`字段地址加`0x20000`的DOS重定位。

opcode23把第二个signed word直接覆盖到指定角色记录word47，不读取旧值、不加法或钳位，并固定推进3 words。全KDEF仅script28 PC65一次`(role4,99)`；现代合法域字段和后续battle边界一致，非法role范围保护替代原机未定义数组外写，归类`platform_adapted`。

### 4.21 添加物品提示与十四书门禁

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
- 1,018 条 KDEF 全量终止、opcode 合法域、13,315 条频次，以及 synthetic opcode3/4/5/6/9/11/13/14/16/17/24/26/68 的状态写入、条件、问题框、同步、载入菜单与 PC 不推进边界；其中opcode11另固定全部7个资产调用、原Big5休息问题、仅大写Y为真及按键后不额外present；
- 四个核心 GRP 的 FNV-1a64；
- 场景 70 初始像素、碰撞轨迹、入口/主循环/出口/内部跳转 continuation；
- 真实 script494 的 opcode8 立即空音频与离场 music3 覆盖；
- 场景 5 的 300 tick RNG、粒子位置和半透明天气像素；
- 真实脚本36的背包与十四天书事件解锁、931的条件休息、581的满武功槽与入队清理、950的离队清理；opcode10固定原Big5物品提示、逐物品present后清槽且保持全局item.user，opcode12固定伤势32恢复、中毒/伤势33/队伍洞后不恢复及满六人分支，opcode21固定队伍压缩、三件item.user解绑及角色字段清理；
- 真实脚本 274 的场景层写入和 opcode 0 呈现边界、931 的 opcode 13/14 淡入淡出顺序、69 的 TALK 暂停/恢复；
- 48个显式scene present callsite的完整地址集与五类无重复分区，script343站立终帧像素，notice/商店/武林大会恢复序列及world菜单回收；
- 325条opcode2和大会奖励caller的库存word回绕、Big5物品名动态面板、caller底图/RNG不重绘，以及十四书与武林帖ID presence门禁；
- 所有既有 model/resource/render/world/persistence/ui/audio/core 测试无回归。

同一14项包含上述场景同步链、全部既有模块测试和 SDL dummy smoke。

后续 B7 门禁仍包括全 opcode 基本块审计、Linux/Windows Debug/Release、ASan+UBSan、原资产只读和 `.i64` 审计。
