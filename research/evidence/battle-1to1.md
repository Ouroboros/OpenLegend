# B8 战斗 1:1 证据

状态：B8统一最终汇编→C++ REVIEW为55/81；其余26项为`implemented_pending_review`。

## 1. 物理范围与闭包

`sub_31C75 @ 0x31C75` 是 scene 与五轮试炼调用的 battle 入口。其后连续 battle 实现区间截止 `sub_3C6D3 @ 0x3C6D3..0x3CBE3`；`research/ida/reports/Z_DAT.b8_battle_xrefs.txt` 由当前 `Z_DAT.i64` 和 `idat.exe -A` headless 生成，枚举81个 FUNCTION 记录，报告规范为 LF，SHA256 为 `179b85c68ad87d03f175f7b22ff9af7ffbae68aed758eeaa0f0fe692ab67d488`。

`research/inventory/battle-closure.tsv` 以该报告为机械真值，当前0项为 `pending_mapping`、0项为 `pending_implementation`、26项为 `implemented_pending_review`、55项为`platform_adapted`。battle 区间调用到的 resource/render/input/time/random/audio 入口是共享 owner 边界，不随递归调用图吞入 battle closure。

## 2. scene ↔ battle 入口合同

`sub_2DE03 @ 0x2DE03..0x2DE2C` 已完成最终汇编→C++ REVIEW。41字节、12条指令的loaded/raw机器码完全相同，SHA256均为`eaa12eafb11dd75089bee3d4d81fbc331e646de5f19b50e4804fbcc144a4ffff`。唯一caller依次有符号压入get-exp、假偏移、真偏移和battle ID；wrapper只把battle ID与get-exp传给`sub_31C75`，调用返回后严格执行一次`cmp eax,1`，等于1时返回真偏移，否则返回假偏移，最终PC为`old_pc+5+selected_offset`。限域xref审计证明`sub_31C75`返回`word_E6ED2-1`，raw结果1显示`戰鬥失敗`、raw结果2显示`戰鬥勝利`，且结果word没有第三个终局写值。全KDEF共145次opcode6，参数流SHA256为`7558e4efa98b78c11e4d94643b990547801d751976fd76bb8016107a25011779`；battle ID均在0..135，get-exp仅0/1，19种offset pair含单条`(8,5)`。现代runtime持有异步`BattleSession`，但合法域的battle ID、经验bool、typed Victory/Defeat回收和脚本PC等价，归类`platform_adapted`；新增两侧非零offset synthetic向量通过，首轮完整复核零新增产品差异。

`sub_31C75 @ 0x31C75..0x31DA0`为299字节、72条指令；raw/loaded SHA256分别为`8f3e6d11d5d85a9c7882511546962fdd2793c5c3a11c8d580fc62bbec2e113b6`与`634a5e34a676f54ebc829e443e2bafcced1d3117dbf73eaa0a71485d802bdb64`，24项绝对地址均按`raw+0x20000` relocation归一化一致。其14个直接调用与状态顺序为：

1. 保存get-exp，写运行模式2并保存battle id；
2. 调`sub_31DA0`载入WAR记录与WARFLD，调`sub_31EB9/sub_3265C`建立瞬时态与队伍；
3. 队伍选择完成后先打开WDX/WMP，再对进入battle前的scene framebuffer执行`sub_3CC97` 64帧淡出；
4. 淡出完成后才打开EFT，使用跨战保留的render globals绘制并以黑色palette呈现首个battle framebuffer；
5. 首帧实际present后按WAR word8启动音乐，再调用`sub_3271E`；该入口排序、重定位、重画并present一个黑帧，随后`sub_3CD17` present 64个递增palette帧和最终原palette，共1+65=66次；
6. 主循环返回后保留最终battle framebuffer执行`sub_3CC97` 64帧淡出，之后恢复当前scene SMP/SDX与scene metadata word7音乐，值-1时传0；
7. 写运行模式1，返回有符号`word_E6ED2-1`。

现代`SceneStepResult`携带battle id与get-exp word；`LegacyGameRuntime`建立并拥有`BattleSession`，以宿主过渡状态持有两次必须冻结caller像素的64帧淡出，并在过渡期间屏蔽输入。`BattleRenderer`分阶段加载WDX/WMP与EFT；首个黑色battle present完成后才发音乐，Session随后执行机器的排序黑帧与65帧淡入。战后必须完成冻结最终战斗像素的64帧淡出，才保存render globals、释放Session、复用仍持有的`SceneSession`并恢复场景音乐，再以严格`Victory`/`Defeat`恢复事件真假PC。复用已加载scene资源替代机器重开SMP/SDX，合法域的结果与顺序一致，归类`platform_adapted`。

真实runtime测试由scene70 script691 opcode6实际发出battle4，覆盖入口、音乐、1+65帧初始序列、真实胜利结算、出口与场景音乐恢复；全140条WAR记录的battlefield id 0..25均有WDX/WMP，音乐只为5/6/7。battle golden现从只读Z.DAT独立提取入口299字节、14个near-call、24项重定位、两个caller的`eax == 1`判定和返回编码，并记录64帧入口淡出、黑色首帧、66帧delegated初始序列及64帧出口淡出；双次生成及与正式文件逐字节一致。Linux app Debug 14/14通过。AI逃跑仍是动作11的回合内handler，不是第三种battle返回值。`scene-event-closure.tsv`的`target_owner=scene`与`battle-closure.tsv`的同址battle-owner现已分别独立收敛关闭；`sub_31DA0/sub_31EB9/sub_3265C/sub_3271E/sub_3AA85`等callee仍不传播closure。

从机器入口重新执行完整battle-owner复核后，现有`BattleSession`分阶段资产所有权、`LegacyGameRuntime`宿主过渡、首帧后音乐、战后资源释放与typed结果回收均与上述机器顺序等价，本轮没有新增产品差异，最终归类`platform_adapted / converged_no_new_differences`。

## 3. 资产 oracle

`research/tools/generate_b8_battle_goldens.py` 只读取原版字节，不链接 OpenLegend C++；两次独立生成逐字节一致后才更新正式文件，第三次生成再与正式文件逐字节相同。正式 `research/evidence/battle-goldens.json` SHA256 为 `4c923e05b6739b8dcc097d1183517ca7f0ef6fcff58118cdde8dd20226f2141c`；本轮新增`battle_magic_selection_machine`，固定988-byte raw/loaded identity、45处重定位、8次call、唯一caller、10槽availability、稀疏显示BUG、Big5布局、多flag优先级/残留、零available异常和输入/presentation边界；既有入口、载入、参战者建立、回合、路径、攻击核心及动画合同均保留。

`research/tools/generate_b8_player_status_golden.py` 独立读取WAR、WARFLD、WDX/WMP、HDGRP、字体与palette，并从固定角色/装备/武功字节直接复算状态选择和两页像素；正式输出为`research/evidence/battle-player-status-golden.json`，SHA256为`833ad96506b856e9c58638c94f2a24ebd46900884d755f1a11379f62442b4a15`，不链接或调用OpenLegend C++；双生成及与正式文件逐字节一致。

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

`0xE87BC` 的64,000字节 indexed framebuffer 仍归 render；`0x9014C..` 角色记录、`0x9F5DC..0xA2744` 武功记录、`0xA2744..0xABBB4` 物品记录与 `0xC0B78..` 会话状态仍归 model。现代实现必须借用这些 owner，而不是复制第二份持久状态。

## 5. WAR/WARFLD 载入实现

`sub_31DA0 @ 0x31DA0..0x31EB9` 最终机器范围为281字节、80条指令；raw偏移`0x2B7A0`，raw/loaded SHA256分别为`df4cdea3b29f83fef687997157ccfa786e5e3bccdf7a92876040cacf6774480b`、`b066ca9a83fab8017129e73a9638af51d42563ae24e45d4851e8628c98d7436a`，14项绝对地址均为`raw+0x20000` relocation并归一化一致。唯一直接caller是已关闭入口中的一次同步调用；12个直接调用依次覆盖WAR打开/定位/读取/关闭、WARFLD IDX缓存缺失载入及失败终止、WARFLD GRP打开/定位/读取/关闭。

现代`openlegend::battle::BattleData`按有符号battle id选择186字节WAR记录并解码93个signed word，以definition word6选择WARFLD cumulative archive entry，只取前16,384字节为8,192个战场word，并在全部成功后把4,096个occupancy word清成-1。`PackedArchive`以`begin=0`及前一累计end形成entry起点，等价于机器在IDX缓冲前置合成0后用`offsets[field_id]`定位；每次现代构造读取受检archive替代机器cache tag 6。真实battle 0..139全部通过；battle 0/4/93/139的记录和战场哈希固定；-1、140、短entry及无效archive由现代安全适配返回错误而非沿用原机未受检I/O/终止行为。

从入口重新独立复核后，合法当前资产域的记录偏移、累计索引起点、固定前缀、signed word解码和y外/x内清零结果与C++逐项一致，本轮无新增产品差异。独立battle golden双生成及正式比较逐字节一致；Linux app Debug 14/14通过。最终归类`platform_adapted / converged_no_new_differences`。

## 6. 参战者建立单元

`sub_31EB9 @ 0x31EB9..0x3265C`、`sub_3265C @ 0x3265C..0x3271E` 与紧邻 helper `sub_3B1E6 @ 0x3B1E6..0x3B238` 已从入口到返回逐基本块恢复，详见对应函数证据：

- 参战者池严格为26槽×14 signed word，每槽28字节；初始化 word0/1/11/12 为-1，其余除 word8外为0；
- 固定队伍看 WAR words15..20，一旦任一非-1便完全跳过预置队伍和选择 UI；否则先建 WAR words9..14，再允许队伍前缀中的非 mandatory 成员切换0/1；
- 队伍写 side word=0、word4=2；敌方写 side word=1、word4=1；每次插入重算 word8、写 occupancy，再按16位递增 count；
- occupancy 以 `y*64+x` 寻址，无范围、重复或容量检查；战斗93证明重复格必须后写覆盖；
- `sub_3B1E6` 返回 `int16(8*role.word1 + word_556D4 + 2*word_556CC + 2*combatant.word4)`；空槽初始化会以 role=-1 对角色表前182字节读取。

`BattleSetup` 已实现26槽完整初值、固定/预置队伍、host-neutral cursor/0·1·2选择状态、按当前 count 取坐标追加、敌方建立、sprite word 和后写 occupancy 覆盖。预置角色先按WAR下标无条件追加，再扫描队伍前缀只标记所有匹配的mandatory状态；本轮完整基本块复核确认现实现顺序一致。`BattleSession`实际绘制圆角混色选择框、原Big5标题/确认文字、角色名和星号，逐键执行上下回绕与确认；runtime每轮先重绘scene背景，无scene caller的独立入口才恢复冻结背景。真实battle0 mandatory/手选顺序、battle4固定队伍、battle93 slot9→slot11覆盖及全140条记录均通过。

battle2队伍角色0/2得到初态`[2,0]`，确认后按原顺序得到队伍`[0,1,2]`并追加敌方4；建队完成不排序也不覆盖battle render globals。机器选择循环每轮调用scene renderer重绘背景后直接叠面板；现代runtime保持该路径，无scene caller的独立BattleSession入口才使用冻结背景回退。选择菜单的独立Python oracle/C++ FNV64均为`0x83f943240d14bb33`；程序初始globals为零时，确认后的首个战场帧使用`view=(0,0)`，FNV64均为`0x568240847c97700c`。机器identity/重定位/call序列、独立golden及Linux app Debug 14/14均通过，`sub_31EB9/sub_3265C`最终归类`platform_adapted / converged_no_new_differences`；`sub_3B1E6`仍为独立`audit_order=73`，不传播关闭。

## 7. 回合排序、玩家菜单与胜负核心

`sub_3271E/sub_32A51/sub_32B78/sub_32E59`已完成本轮最终汇编→C++ REVIEW；胜负检查及动作callee仍按独立closure待审。四函数分别为819/295/695/1806字节、166/69/131/476条指令、69/19/68/79项重定位，完整identity见各函数证据；主循环还覆盖物理范围外的共享返回尾，菜单另核对10项跳转表和32个直接调用。`BattleSetup`现实现：

- 基础speed加两件装备word53，每次加法16位回绕；固定first与所有later逐对signed比较，first<later才交换。不是稳定冒泡：等值不直接交换，但跨槽交换可把`A10,B10,C20`变成`C20,B10,A10`；
- 交换时按原顺序复制 word0..7/9..13、逐槽写 occupancy、最后重算两槽 word8；
- 每轮 word6=`max(0,effective_speed/15-role.hurt/40)`，signed division toward zero；
- hp<=0且未 hidden 的槽清 occupancy并写 word5=1；无队伍为 raw1/`戰鬥失敗`，无敌方为 raw2/`戰鬥勝利`，双方皆空由 raw2覆盖。

`BattleSession`在建队后使用跨战保留的battle render globals绘制首帧；首帧present并排入music后才首次排序、把slot0写入`secondary_cursor`、按signed clamp计算视图原点并执行黑场淡入，继承的`primary_cursor`不被覆盖。随后轮首先保存tick，仅排序一次，再只清effect帧偏移、effect可见和highlight模式三个globals（不额外清路径范围），完成全部slot的word6计算。每槽先采样三确认键当前状态，必要时清三键与automatic，再跳过hidden或更新次光标、居中present；present之后才清word7/10并按side/automatic进入player或AI。

玩家分支按原条件建立「移動、攻擊、用毒、解毒、醫療、物品、等待、狀態、休息、自動」十项0/1表，保留无武功时最低耗内哨兵1000；cursor严格是可用项ordinal，上下回绕，确认后再扫描映射原action id。菜单入口置次光标可见，完整绘制战场、圆角框、全部2321文字、6663选中文字和actor状态面板并present；输入轮只在保留帧上重画两遍文字并再次present，然后按down→up→Enter/Space/keypad Insert采样当前键态。确认清三键并关闭次光标；动作返回不重新开启。runtime/SDL运输make/break状态而非事件队列，present前释放不会触发，同帧down优先up和确认。十项全可用、cursor0时独立Python oracle与C++整帧FNV64均为`0x648d1a4f7c02fdbc`。

等待动作实际把当前actor逐槽交换到队尾且不写word7；机器要求先完整重画菜单/present，再因action6退出，外层索引继续处理交换后占据同槽的actor。休息提交原RNG体力/HP/MP恢复并写word7=1，直接退出菜单。共同尾对word7严格区分0、1和其他非零：0完整重绘，1退出，其他非零只跳过重绘而继续输入；菜单只读既有结果状态，不额外调用胜负扫描。返回外层后才按原顺序执行一次胜负检查、当前参战槽隐藏目标清理、hidden槽压缩与下一actor居中present。最后一槽后调用轮末异常状态，并仅在本轮开始时捕获的BIOS tick发生变化后开始下一轮。

玩家移动现实际执行movement路径图光标、四方向翻译键、Escape/三确认键、路径范围与主光标重画/present；确认后清路径上限、标记最短路并逐格提交状态，每格重画/present后按参数40等待两次BIOS tick变化。返回菜单时仅重检移动项，不重算其余九项。battle4 Session覆盖取消、`(26,24)→(26,25)→(25,25)`、word6 2→1→0及移动项失效；`sub_36A98`、`sub_36AF7`与`sub_37355`均已完成最终审计。

状态动作现按`sub_22066(...,2)`进入队伍前缀选择，执行圆角标题/列表、角色名NUL对齐、上下回绕、Escape取消和三确认键；确认后在battle背景上依次呈现`sub_22A59`两页角色状态，每页各等待任意非零键。第一页保留伤势/中毒/内力分档、非法内力类型复用中毒色、装备加成和30级阈值；第二页保留两件装备、修炼物经验分母与十项武功等级。动作返回菜单但不结束actor、不消费RNG。独立Python直接读取原WARFLD、HDGRP和字体资产复算，选择页/第一页/第二页与C++整帧FNV64分别一致为`0xfa1b21403051335c`、`0x1c5e879ce61d5b34`、`0x9592da33a3c151d4`；逐块回审修正了最大生命中毒色、修炼所需经验普通色和分母系数读取资质而非修炼经验三处差异。

自动动作重画并present时flag仍为0，present完成回调后才置flag并进入同actor AI；AI全部返回后回到玩家菜单共同尾。结果命中时，`sub_3B238`内部的战果panel/present、任意键与全部战后结算消息必须先完成，返回主循环后才清隐藏目标、执行一次轮末异常状态，再保持当前画面等待轮首tick发生变化，最后发布typed结果供入口淡出回收；hidden槽同样执行这条公共尾。独立golden双生成一致且正式SHA256为`aa606cc4949dc4c5da8e39edf42ad9e9c32dfa9aec85aa43b6a4fecdc12dfc22`；order5/8机器门、联合静态/golden门及最新Linux app Debug 14/14通过。从两入口重新逐块审计零新增差异，`sub_3271E/sub_32E59`最终归类`platform_adapted / converged_no_new_differences`。`sub_33599/sub_3B238/sub_3C6D3`及输入专属同址owner仍为独立待审项，不传播关闭。AI prelude整帧FNV64固定为`0xb02104139829a80d`。

## 8. 战场路径图与最短路回溯

`sub_36E06..sub_37245` 的十个 path 单元已映射为 `BattlePathing`：

- movement 图把 upper layer非0、occupancy非-1或 ground tile 命中9段 IDA 常量的格写555，其余写254；targeting 图只检查 upper layer；source 随后强制写0；
- 原255槽环形队列保留 `(0,-1)` sentinel、distance `%128` 和上→右→左→下扩展顺序，没有替换为无界 queue；
- 回溯从 target 开始写250，每层用 `(distance+127)%128` 并按同一方向顺序选择首个前驱；消费标记255由后续移动单元使用；
- `sub_37070` 的坐标比较允许64，现代仅保留线性 index仍在0..4095的别名，index>=4096安全拒绝；不可达回溯返回false而不进入原死循环。

独立 oracle 固定 battle0/93 空 occupancy、movement source occupancy强制归零、相邻单格占位、targeting upper-layer阻挡source强制归零、target距离14/22、回溯前后完整 FNV-1a 与首步 `(31,20)/(33,29)`；Linux Debug 14/14。`sub_36EF8`进一步固定为257 bytes、72条指令、17个跳转和8处重定位：先把64×64 path全部写0，再按x外/y内扫描；upper layer非0、occupancy非-1或signed ground命中九段原资产闭区间时写555，否则写254。现代省略不可观察的预清零并改用独立格线性扫描，最终4096 words一致；共享尾`0x39A3E`只回收ABI状态，唯一caller忽略返回63。`sub_36FF9`固定为119 bytes、39条指令、10个跳转和4处重定位；同样预清零并按x外/y内扫描，但只读upper layer，零写254、任一非零写555，完全不读occupancy或ground。唯一caller忽略返回8190；battle0/93非空occupancy和battle89 ground-only tile `0x166`区分向量均通过。`sub_37070`固定为246 bytes、76条指令、11个跳转和7处重定位；每step按sentinel使distance执行signed `%128`、上右左下扫描、signed坐标`0..64`、path恰254时先入队再写距离，正常返回0、连续sentinel返回-1，两个wrapper均据此回环或结束。现代内联全部step、私有化共享队列及void返回不可观察，真正线性越界安全拒绝归类平台适配；中心十字六步trace、一二层距离和battle0 `x=64`别名hash均通过。`sub_37166`固定为72 bytes、21条指令、0跳转和2处重定位；按同一signed读下标从255-word x/y双队列依次写出，第三次重读下标后signed `idiv 255`回写余数并返回商，合法254→0时商1、其他商0，两个caller均只读写出的y而忽略EAX。现代交错coordinate队列、私有index及省略商不可观察；253→254→0 trace和真实图457/822个可达格锁定环绕。`sub_371AE`固定为73 bytes、18条指令、0跳转和4处重定位；一次signed读取write index后，把x/y低16位依次写入255-word双数组同一slot，无full guard，再以signed `idiv255`回写余数并返回商。sentinel与候选caller均忽略EAX；现代交错coordinate、私有index与不可观察半写状态等价。253→254→0 signed极值trace和真实图457/822个可达格锁定write环绕。`sub_371F7`固定为39 bytes、10条指令、0跳转和1处重定位；以signed `2*(y*64+x)`字节偏移写value低16位并返回偏移，四caller分别写distance、250、250与255。现代直接word写与机器一致，x64存储内别名保留，负坐标和真正index>=4096安全拒绝归类平台适配；线性别名/越界向量及真实distance/marked/consume hash通过。`sub_3721E`固定为39 bytes、9条指令、0跳转和2处重定位；按signed `y*64+x`读取word，先写无读xref的scratch再`cwde`返回，四caller分别判254、保存distance、判distance和判250。现代signed int16读取一致，省略scratch不可观察；x64别名保留，负坐标和真正越界安全拒绝。`sub_37245`固定为272 bytes、75条指令、10跳转和10处重定位；target先标250，distance按signed `(d+127)%128`降层，上右左下首个匹配前驱及source均标250，正常经共享尾退出。现代合法BFS域一致，无前驱false与4096步上限替代机器无界回环。`sub_36E06/sub_36E7F/sub_36EF8/sub_36FF9/sub_37070/sub_37166/sub_371AE/sub_371F7/sub_3721E/sub_37245`十个pathing owner均已完成最终入口审计并归类`platform_adapted`。

`sub_37355`固定为991 bytes、215条指令、38个函数体跳转、61处重定位、11次direct call和两个caller；raw/loaded SHA256分别为`52f1523f186cd8da70fd563c4f9a50f861887289a0e04738ba7d6fb5bc143063`与`a267ef00226ba24e6842d195303e5e4f409b909c027dc66ee5b27e98f812a82c`。每格严格以上、右、左、下选择首个250邻格，并按旧path=255、旧occupancy=-1、新occupancy=actor、x/y、direction、sprite、条件physical-power 16位DEC+负夹0、round 16位DEC的顺序提交；随后更新view中心及`coordinate-11`到0..32夹值、render、present和参数40 delay，包括最终格也先完整呈现再判停。player只按destination停；AI mode0/3再看signed round<=0，mode1看Manhattan<=range，mode2还要求同x或同y。合法AI矩阵穷举一致，玩家合法`path_length<=initial_round`证明现代额外round stop冗余；无250邻格checked终止替代机器无界重试/伪destination停止。四方向、view、`INT16_MIN`回绕、完整stop矩阵、battle0/battle4/battle2 Session均通过；首轮完整入口REVIEW零产品差异，最终归类`platform_adapted / converged_no_new_differences`。

## 9. 武功攻击入口与每轮提交

`sub_37734`机器身份固定为3690 bytes、837条指令、106个函数体跳转、226处重定位、34次direct call和两个caller；raw/loaded SHA256分别为`e1e0b9a203500a28d37fbca2eba008c0d3cff9c507104c5ac1ab769a6808f833`与`b1c8388f04bc4660f3d13f0280000900a7e84beb4e5d562f2609487e8945b631`。入口十槽统计只计熟练度>0；唯一已学武功时原版仍选slot0的BUG保留；等级用熟练度unsigned `/100`，并缓存magic words 28+level、38+level、15、14、16的选择距离、攻击距离、area type、hurt type和need_mp。attack_twice严格等于1时轮数为2。

现代每轮提交严格保留 action word7=1、word13加2、`LegacyRandom::bounded(2)+1` 熟练度增长、unsigned 999 cap、跨百升级判定，以及 `(cost_scale/2)*need_mp` 内力扣除后 signed负值夹0；全部轮次后体力减3并夹0。seed1固定 state1103527590、299→300、cost scale3下 mp3→0、999 cap和体力2→0。

`BattleSession`现完整执行攻击入口：多武功菜单确认后，area type0/3进入mode1目标UI并支持Escape返回原ordinal，type1显示方向框并按0/1/2/3映射上/右/左/下，type2直接攻击。每击实际执行area伤害、FIGHT/EFT双bank动画、10帧damage、sprite刷新、重画/present/wait17，随后才提交word7/word13/熟练度/MP；跨百绘制升级框并present/wait500，双击结束后才扣体力并进入共享actor尾部。原版在循环外缓存范围，现代同样缓存初始profile；首击升级后第二击继续命中原范围，但伤害与cost scale按新熟练度重算。固定双击第二击hits1、cost scale4、内力20→15→5；方向提示与升级框整帧hash为`0x5e46c805f42677b0`、`0x1f0048d1945a4948`。四种area type、取消、双击和13次升级等待均由battle4 Session覆盖；AI automatic flag1路径也执行自动方向、直接或移动后攻击、10帧FIGHT、10帧damage、17tick提交、升级框13次tick、熟练度/MP/体力和外层action-done。固定AI首FIGHT、首damage、提交战场、升级框及移动后首FIGHT整帧hash分别为`0xe1d1b3cff84bc0c4`、`0x04c528de57fbffa0`、`0xdbee20f394fd7219`、`0xed97f52f9bedb836`、`0xacc58834b066ca07`。首轮入口REVIEW发现原`E6EC2`仅由HP核写、由HP核和攻击MP提交读且无战斗初始化清零，现代每场`BattleSetup`重置会丢失跨战陈旧scale；现由`LegacyGameRuntime`持有进程期word并经Session绑定Setup，HP核即时同步。新Setup继承scale3并按`(3/2)*need_mp4`把MP20扣至16的区分回归通过；修正后完整入口重审零剩余差异，`sub_37734`归类`platform_adapted / converged_no_new_differences`。

## 10. HP与MP伤害

`sub_39188` 已映射为 `apply_hp_damage`：扫描双方knowledge>80的存活可见参战者，按最高可支付层计算cost scale，合并角色攻防、装备、特殊加成和双方知识；保留两次bounded(20)、非正时两次bounded(4)、距离1..10线性衰减和>10固定2/3。固定damage30下，HP30恰好归零不发击杀奖励，而HP29下溢才加`10*level`；并按原顺序写hurt与poison。

`sub_395EC` 已映射为 `apply_mp_damage`：严格执行bounds `[3,3,add_mp/2,3,3]`，攻击者增加当前/最大内力并封顶，目标扣hurt_mp与随机差。seed1向量输出 `[2,1,3,1,1]`、state4182499122，攻击者10/20→23/23、目标50→35、返回15。两函数Linux Debug 14/14，均为 `implemented_pending_review`。

## 11. 武功area扫描

`sub_37734` 的方形与十字单轮状态边界已映射为 `clear_attack_effects/apply_attack_area`。方形按x外/y内扫描并按目标差更新方向；十字逐距离按上、下、左、右扫描且不改方向。友军格完全跳过，空格只写effect=1，敌方格写damage word9。type2忽略hurt_type并强制HP伤害；type0/3按hurt_type 0/1分派HP/MP，其他值只留effect。

固定方形8格hash `0xe5f47b0a810ce2bd`、十字7格hash `0x3144c415023d9464`、单格MP hash `0xab559939923b4f74`；伤害29/29/15，effect kind 1/1/3，MP分支保留上次HP cost scale3。四种area现均接入目标/方向输入、动画、present、提交与双击；area覆盖使用入口缓存profile，伤害函数每次命中仍读取当前熟练度。`sub_37734`已完成最终入口REVIEW并归类`platform_adapted / converged_no_new_differences`。

## 12. 直线area扫描

`sub_38999`机器身份固定为1043 bytes、248条指令、30个函数体跳转、69处加载时fixup、5次direct call和唯一caller；raw/loaded SHA256为`09606be94f8f5de92ffc88acaa7243b23643604ff6149662fe09afd6de0dace0`与`40d2e7e81e83001485a8b99019f765189b25923fd230eefae2b25d111a66d16e`。前置jump table `0x38989`映射0/1/2/3为上/右/左/下，raw/loaded hash为`466c05e8e910474415b130dc737efb9e70c199a8e1c6b8269a4778c81d91ffa4`与`516aa1e28958ddb5f2d367f72aff891a5d2ba4fa5391777f69d385bbdc67435f`。唯一caller `0x37E19`传缓存profile range、玩家输入或AI主轴方向与actor，忽略返回后续流`0x3826C`；全部出口跳独立共享尾`0x3CBDB`。

参数只看低16位；方向其他值直接退出。range按signed word：`<=0`零迭代、`1..32766`精确扫描，`32767`因BX回绕永不返回。全部15条真实area-type1 magic的150个select-distance均在1..7。每格依次执行bounds、occupancy、同方skip、effect1、空格skip、敌方kind1、`sub_39188(...,distance)`及返回damage写目标word9；越界/友军/空格/敌方都不截断后续distance，且不检查HP/hidden、不读取hurt_type、不直接耗RNG。

首轮汇编→C++对照发现现代将机器非终止`range=32767`误作有限32767步执行；现只对该signed值进入前安全拒绝。修正后从入口重新覆盖248条指令、30分支、69处fixup、四方向循环、唯一caller和全部共享尾出口，合法caller状态零剩余产品差异。四方向友军skip+敌方distance3 hash为`0x0c51a09fb032df25/0x80f86a7090dd8f15/0x53328f08db3e6d15/0xdd9b44614652df25`，先越界后重新入界hash `0x32329c4e241f2c3d`；原battle4空格+敌方hash `0xae7c1e4e161ac125`/damage29，友军skip后继续hash `0xab559939923b4f74`，空图hash `0xb9d103fd6854a325`。HP0且hidden1目标、连续两敌人、非法方向/zero/max range、方向不改写及双击复用首轮方向/缓存范围均有区分回归；提示整帧hash `0x5e46c805f42677b0`。

独立Golden三生成逐字节一致，SHA256为`e7c4b24495a6ddab76b449e11473d31dc982705a5bf9d37774a20fc17b9ea276`；Linux Clang 23根`./build.sh app --config Debug`通过14/14。order54独立归类`platform_adapted / converged_after_fix`；jump table、HP damage、direction prompt/input、attack core caller、FIGHT/EFT/damage与共享尾不传播closure。

## 13. 攻击动画时间线

`word_55ACE` 53项effect帧数表已由IDA data address `0x55ACE` 映射到原 `Z.DAT` 文件偏移324,814；表字节SHA256 `4c684877da272f4222fd3b73595971c81599ecfd456644498ac97841050da3a4`，word FNV `0xc35d41f03731784a`。RANGER 93条magic使用effect id 2..52。

`sub_3859E`机器身份固定为684 bytes、174条指令、13个函数体跳转、35处重定位、9次direct call和四个caller；raw/loaded SHA256为`e83dd2e31acf38e995b2df80d7f0b10b15250b144eca6f85c9c0b68681c22760`与`2806b1582a71c13da4dd2a79322873787ac9efacc89b6126f504dde793530304`。攻击传definition type/effect，用毒/解毒/医疗固定传`(0,30)/(0,36)/(0,0)`，四者均忽略EAX。入口严格先载入FIGHT、attack bank1与effect bank2，再作signed总帧检查；阈值只启动已载入sample。同帧先更新effect状态，再按attack bank1→effect bank2调用，随后render→present→delay17。固定type2/effect2向量共19帧，actor sprite`248..254`，effect从frame2显示`48..80`并启动，attack在frame4启动；总帧0仍预载三项资源但零次render/present/delay/start并清visible。

首轮入口REVIEW发现现代在阈值用`play=load+start`，改变旧sample停止时点和零帧边界；现以`BattleAudioAction::load/start_loaded`分离并由SDL调用相应后端。fresh xref又证明`E6ED6`只有玩家选择、攻击入口清0和AI选择三处写，support从不清零且战斗初始化不写；现代原AI毒/医/解清零、每Session丢失和AI仅在真正攻击时晚同步均已修正。runtime现在持有进程期共享武功槽；玩家攻击入口/选择和AI plan建立时同步，support只读。slot0 sample7/slot2 sample8的玩家三新Session、AI毒/医/解、AI移动首格前写入及攻击菜单取消留0回归均通过。修正后从入口重审174条指令、13分支、9个call及唯一出口零剩余产品差异，最终归类`platform_adapted / converged_no_new_differences`。

`sub_3884A`机器身份固定为198 bytes、56条指令、4个函数体跳转、9处重定位、9次direct call和三个caller；raw/loaded SHA256为`8a04b6dee92962bfdab94f0480f09d34b7d012866679e1e76370dcc737fe6126`与`c5feba51e0559ba30d74cb1797847e4f5815a758b683ee5ac35612521799c460`。入口严格load bank1 sample13→load bank2 effect→start-loaded bank1→delay100，期间没有render/present；随后只start-loaded bank2，再置effect visible/frame并逐帧render→present→delay17→frame+2，最后清visible。effect0/2/30分别为10/17/11帧，起始frame 0/48/772；负或>=53的原线性越界由现代安全拒绝。

首轮入口REVIEW发现现代默认play破坏双bank预载/启动边界，且前奏相位错误重绘无光标战场；现玩家/AI入口均排入`load, load, start_loaded`，100 tick后只`start_loaded` effect，prelude render保持caller framebuffer不变。玩家caller/prelude hash为`0x49aac6569a28fe89`，AI直接为`0x3f498f66e6357fff`；两路effect30/damage首帧hash分别为`0x370a4078e9de6172`/`0xd41fa068222d444a`和`0xc65b523bd75389e2`/`0x335fd35ea7e3f367`，AI移动后effect为`0x16a8f10ce319622b`。修正后从入口重审全部56条指令、两循环、唯一出口及三个caller零剩余owner差异，归类`platform_adapted / converged_after_fix`。玩家状态提交属于order68 caller，不随本owner传播关闭。

`sub_38910`机器身份固定为120 bytes、31条指令、5个函数体跳转、7处重定位、4次direct call和7个caller；raw/loaded SHA256为`bf23e1ade78c9c29d28bc79fd6e4805f3d5d4716102e66fff1961b59b8ee16c6`与`6fd5bc6baae469b2ccbbc81d073c4961c0e214a83fcd9a091b43b0b3f12494c5`。入口phase清0，固定10帧；每帧先按`frame<4 && low16(suppress)==0`写flash，再render→present→phase++→delay1。故renderer消费phase0..9、wait期间phase为1..10；最终kind清0。高16位不影响suppress，函数零RNG。

七个caller的suppress低字依次为`0,0,0,0,1,1,0`：AI两条暗器、attack、poison均normal，detox/medicine suppressed，玩家暗器normal；kind分别为AI/玩家暗器damage非零时1、attack type3时5否则1、poison2、detox3、medicine4。首轮对照发现AI暗器caller错误无条件清kind0，以及present确认后现代phase直到delay完成才递增两项差异；现恢复`damage!=0 ? 1 : 0`并在present确认时推进公开display phase。玩家支持/暗器及AI直接/移动暗器逐帧回归锁定present前phase0..9、wait phase1..10、flash、kind和终态清理；修正后重新覆盖order53全部31条指令、5分支、7个caller，并从受影响`sub_3598C`入口复核440条指令与全部出口，零剩余差异。原资产Golden三生成逐字节一致，SHA256为`a6e7c3adccb1cedd5c74296b305cba84da73dfb784906e0a28b6208974e46b1b`；Linux Debug 14/14。order53归类`platform_adapted / converged_after_fix`，renderer/present/delay及caller保持独立owner。

## 14. 武功选择菜单

`sub_38DAC`机器身份固定为988 bytes、259条指令、52个函数体跳转、45处fixup、8次direct call、唯一caller和本地RET；raw/loaded SHA256为`57ae8494ab7b83a2e4871a11b0cf63c5384b4ab426d4c09a4c7bfdf9cfac6f21`与`4458a76d92ee84dddd18b94410dea5a757202b911bddd20fcc7e18abd4bde181`，全部fixup逆`+0x20000`后与只读原字节一致。唯一caller传actor、正武功数和取消word并忽略返回；恰一项时绕过菜单直接写物理slot0的BUG属于caller边界，不随本owner关闭。

可用mask扫描全部10槽，只接受signed magic id>0且signed MP≥signed need_mp；cursor是可用项ordinal，左右回绕，确认再次扫描10槽映射实际slot并写共享`word_E6ED6`，Escape只写取消word1。输入flag优先级严格右→左→三确认→Escape，方向只清自身flag，确认只清三确认flag；未命中的异步flag跨重绘保留。每轮必须先战场重画、面板/名称绘制和present，再扫描输入。

面板固定`(20,10,90,17*learned_count+10)`；普通/选中色`0x2321/0x6663`，Big5长度1..5的x为`57/49/41/33/25`，y=`17*ordinal+15`。普通名称只扫描`slot<learned_count`，但availability、确认和选中名称扫描10槽，故稀疏槽后置武功普通态漏画、选中态显示的原BUG完整保留。零available机器域可负cursor、无效slot0或不终止，现代只对该异常域安全拒绝。

首轮对照发现现代入口及方向后可在对应帧present前继续接受键；现以一次presentation门修正，门前translated event沿用已关闭cursor owner适配而忽略，方向后重新要求present，确认/取消清零门。修正后从入口重审259条指令、52分支、8次call、唯一caller及全部出口，零剩余合法域差异。固定状态/稀疏显示/多flag/零available hash为`0xc254d2cd83d7da76/0x9eeb370071c9a0af/0x7398c6fcaccb922c/0x47d47c419ce4142b`；Session两帧FNV64为`0x909332be9671b27c/0x6977ba7a0c3172a6`。Golden三生成一致SHA256为`4c923e05b6739b8dcc097d1183517ca7f0ef6fcff58118cdde8dd20226f2141c`，Linux Debug 14/14。order55独立归类`platform_adapted / converged_after_fix`；caller、callee、输入owner和后续目标/方向处理不传播closure。

## 15. 用毒目标与状态结算

`sub_39776` 的目标射程已映射为signed `use_poison/15+1`；原函数在目标选择out flag为1时返回-1，否则调用 `sub_397E5`。`sub_397E5` 按目标差更新方向，清4,096格effect；友军格完全跳过、空格只写effect、敌方格写effect kind2并调用 `sub_39A45`，随后重算全部sprite、置actor action_done、word13加1并将体力减2夹0。原函数错误地检查x<64两次而未检查y<64，现代实现对y>=64安全拒绝，登记待最终REVIEW。

`sub_39A45` 已映射为 `apply_poison_value`：signed `(use_poison-anti_poison)/4` 向零截断，先夹0..99，再按目标剩余容量 `99-poison` 限制；不消费RNG。固定use_poison80、anti_poison20、poison90得到raw15、实际9、目标99；射程6、方向3、effect hash `0xab559939923b4f74`、effect kind2、体力1→0、counter0→1，空格标记而友军不标记。`BattleSession`现从targeting确认先提交目标状态，再执行11帧effect30、attack7/effect30双bank音效、10帧damage kind2（前4帧flash）、共享尾部和下一actor；独立Session锁定poison0→7、体力100→98。三函数均推进为 `implemented_pending_review`。

## 16. 解毒目标与状态结算

`sub_39B1F` 的目标射程已映射为signed `detoxification/15+1`。`sub_39B8E` 与用毒使用同一方向和清effect顺序，但目标条件相反：敌方完全跳过，空格写effect，友方格写effect kind3并调用 `sub_39DA3`；随后执行effect36动画、抑制damage flash、重算sprite并跳入用毒的共享行动尾部。原函数同样重复检查x<64而未检查y<64，现代实现对y>=64安全拒绝。

`sub_39DA3` 已映射为 `apply_detox_value`：signed `detoxification/3` 后严格依次消费两次 `bounded(10)`，计算 `quotient+first-second`，夹0..99；目标毒值严格大于 `detoxification+20` 时归零，再受当前毒值限制。写回后仅poison<0夹0、poison>100夹99，故恰等于100保留。固定detoxification80、poison90、seed1得到RNG `[8,8]`、state2524885223、解毒26、目标64；射程6、方向3、effect hash `0xab559939923b4f74`、effect kind3、体力1→0、counter0→1。`BattleSession`现从targeting确认先提交目标状态，再执行9帧effect36、attack7/effect36双bank音效、10帧无flash damage kind3、共享尾部和下一actor；独立Session锁定poison20→10、体力100→98。三函数均推进为 `implemented_pending_review`。

## 17. 医疗目标与状态核心

`sub_39E88` 的目标射程已映射为signed `medicine/15+1`。`sub_39EF7` 只对友方或空格写effect，敌方完全跳过；友方格调用 `sub_3A10C`，effect kind4，effect0动画并抑制damage flash，之后重算sprite并跳入共享行动尾部。原函数同样漏掉y<64上界检查，现代实现安全拒绝。

`sub_3A10C` 已映射为 `apply_medicine_value`：actor体力<50立即返回且不消费RNG；medicine负值夹0，按target hurt的`<=25/26..50/51..75/>75`四档取`4/5、3/4、2/3、1/2`基数，再消费一次 `bounded(5)`。hurt严格大于原medicine+20时治疗与减伤都归零；治疗受maximum_hp限制，hurt按完整非负medicine扣减。固定medicine80、hurt40、HP100/200、seed1得到RNG3、state1103527590、治疗63、HP163、hurt0；方向3、effect hash `0xab559939923b4f74`、kind4，公式内体力51→49，共享尾部49→47。`BattleSession`现从targeting确认先提交目标状态，再执行10帧effect0、attack7/effect0双bank音效、10帧无flash damage kind4、共享尾部和下一actor；独立Session锁定HP50→77、hurt20→0、体力100→96。三函数均推进为 `implemented_pending_review`。

## 18. 战斗物品与休息状态核心

`sub_3A29C` 固定以参数4调用共享物品过滤，因此原菜单按库存顺序同时列出item type3和4，并且不检查数量；选择器返回4才进入暗器目标，返回1直接结束actor行动。现代Session已实际执行战场重画、三个共享框、5×3 MMAP图标、上下箭头、名称/简介、数量条件、四方向/PageUp/PageDown、Escape和三确认键。16项count2/count1菜单整帧FNV64为`0x68c3b70dfec20bba`/`0x17b6845a718a6cf8`。type3固定当前actor自用；非零效果执行23项状态、面板present、库存提交及任意键等待，面板FNV64为`0xd518fb664f3e0e3c`；全零效果不画面板、不扣库存、不消费RNG但仍结束行动。该wrapper推进为 `implemented_pending_review`。

`sub_3A30B` 以signed `hidden_weapon/15+1`选择目标；友军不标记，空格只标effect，敌方才执行暗器effect、伤害、中毒、damage kind1显示、库存减一和行动结束。HP负增量先按target hurt的0、1..33、34..66、>66四档取item `add_hp`的`1/4、1/3、1/2、1`并减一次`bounded(5)`，再以`(值-2*hidden_weapon)/3`结算；hurt按负增量四分之一上升。正`add_poison`分支不消费额外RNG，非正分支严格再消费两次`bounded(5)`，所以无毒暗器仍可能随机改变poison。Session已覆盖目标取消、空目标返回、敌方确认、sample13/100前奏/effect sample/11帧EFT/10帧damage、延后库存提交、sprite刷新与下一actor；前奏、首EFT和首damage FNV64为`0xdbee20f394fd7219`、`0x370a4078e9de6172`、`0xd41fa068222d444a`。库存数量按int16减一，不大于0时把后续槽全部左移并清尾槽。该函数推进为 `implemented_pending_review`。

`sub_3A8A4` 已映射为 `rest_actor`：先结束行动，按round value是否等于signed `speed/10`执行`bounded(3)+3/+2`恢复体力并只夹上限100；更新后体力不少于30时，以`physical_power/10-2`为bound严格依次恢复HP与MP，各加`bounded(bound)+3`并只封顶。固定向量得到体力50→55、HP95→99、MP48→50；低体力向量25→29且不消费后两次RNG。该函数为 `implemented_pending_review`。

## 19. 等待、自动flag与战场绘制命令计划

`sub_3AA17` 已映射为 `defer_turn_to_end`：从当前slot开始连续调用原swap语义，把角色逐槽移动到combatant尾并返回最终slot，不直接写action-done。真实battle3完成队伍选择后，slot1的角色由`[0,101,102,103,104]`移动为`[0,102,103,104,101]`并返回4；该函数为 `implemented_pending_review`。

`sub_3AA4B` 的顺序为完整战场重绘、present、自动flag写1、调用当前actor的AI。`BattleSession`已按该顺序实际重绘/present，在present前保持flag0、present完成后写flag1并以同一actor进入AI前导；后续执行态势累计、第二次重绘/present、参数300的八次BIOS tick变化及全部typed handler，AI返回后再进入玩家菜单共同尾。该callee实现已完整但仍保持自身`implemented_pending_review`，不随order8关闭。

`sub_3AA85` 已恢复为严格两次local-x外层/local-y内层的32×32命令计划：第一pass绘制WARFLD layer0；第二pass依次加入path overlay、主/副cursor、非0且非15000的layer1、normal或三种调色高亮角色、effect以及五种damage文字。path overlay与主cursor同受range严格大于0保护，secondary cursor由独立flag控制。普通sprite锚点为`145+18*(x-y), -81+9*(x+y)`；overlay左移18，damage再按offset上移。独立oracle以真实battle4资产和非对称view/cursor生成1,157条命令，哈希`0xb9f8a428699b3712`，C++逐字段复算一致；零range向量不产生cursor命令。`BattleRenderer`现按机器常量pointer基址0/6500/8000解析WDX/WMP、EFT与动态FIGHT，实际执行普通RLE、单色高亮、CLOUD第4/5帧alpha混色和damage字体，并从MMAP实际绘制物品图标；独立资产oracle与C++整帧FNV64均为`0x7d8a5211fe8c4eb0`。BattleSession已在初始战场、actor-present、玩家动作/武功/物品菜单、状态选择与两页界面、movement/targeting路径光标、每个玩家/AI移动步，以及全部玩家攻击/支持/物品、AI自动攻击、AI用毒和AI医疗/解毒的逐帧FIGHT/EFT与damage动画实际调用并由runtime present；状态三帧hash为`0xfa1b21403051335c`、`0x1c5e879ce61d5b34`、`0x9592da33a3c151d4`，攻击方向提示与升级框整帧hash分别为`0x5e46c805f42677b0`、`0x1f0048d1945a4948`。AI普通物品面板与AI暗器前奏/EFT/damage及移动后效果现也实际调用renderer并执行present；固定AI普通物品面板hash为`0xa7542240e4172664`。结果调用点已由回合结果continuation接入；绘制函数本体及当前全部调用类型已实现，`sub_3AA85`仍保持自身`implemented_pending_review`。

## 20. AI六个候选selector

`sub_33C4D`低生命selector的机器身份已固定为582 bytes、123条指令、24条分支与48处重定位。它依次选择自身医疗5、队伍200槽或敌方4槽首个`add_hp>0`物品6、首个同side未隐藏且医术达标的求援目标8；自疗门为医术>=20、体力>=50且医术严格大于伤势减30，求援医术则严格>20。两类物品均忽略数量字段，无选择不写既有动作。最低等号门、strict失败、体力49、敌方零数量随身物品及隐藏同伴跳过均有回归，最终重审零新增差异，归类`platform_adapted / converged_no_new_differences`。

`sub_33E93`中毒selector的机器身份已固定为582 bytes、118条指令、25条分支与48处重定位。它依次选择自身解毒4、队伍200槽或任意非零side四个随身槽的首个解毒物品6、首个同side未隐藏且解毒能力达标的求援目标9；自身与求援门均保留严格比较。队伍物品检查item word56为负，非零side随身物品检查word47为负，两者均忽略数量。三项strict边界、负side、零数量、字段分歧、隐藏/异side跳过及无动作保留均有回归，最终重审零新增差异，归类`platform_adapted / converged_no_new_differences`。

`sub_340D9`低内力selector的机器身份已固定为285 bytes、68条指令、11条分支与22处重定位。该owner本身不读取当前/最大MP，只按side扫描队伍200库存或任意非零side角色4个随身物品；item word50必须严格大于0，数量完全忽略，首个命中写用物品动作6，无命中不改动作。满MP直调、零/负属性、队伍首命中、负side随身零数量及无动作保留均有回归，最终重审零新增差异，归类`platform_adapted / converged_no_new_differences`。

`sub_341F6`医疗目标selector的机器身份已固定为484 bytes、117条指令、16条分支与28处重定位。它按槽序扫描同side未隐藏目标，先过actor医术严格大于目标伤势减30的门；请求医疗、HP<20或伤势>40均免RNG优先，其后依次为HP低于最大值1/2、1/3、1/4的`bounded(10)<7/<8/<9`短路概率，最后1/5确定命中。过滤、strict等号、免RNG优先、每级消费和seed331三次9后的1/5回退均有回归，最终重审零新增差异，归类`platform_adapted / converged_no_new_differences`。

`sub_343DA`解毒目标selector的机器身份已固定为374 bytes、88条指令、14条分支与22处重定位。它按槽序扫描同side未隐藏目标，先过actor解毒能力严格大于目标中毒值减30的门，请求解毒不能绕过能力门但命中时免RNG；其后依次为中毒值>10/>20/>30的`bounded(10)<4/<6/<8`短路概率，最后>40确定命中。过滤、能力等号、请求优先、10/20/30/40严格边界和零至三次消费均有回归，最终重审零新增差异，归类`platform_adapted / converged_no_new_differences`。

`sub_34550`攻势selector的机器身份已固定为1411 bytes、328条指令、57条分支与79处重定位。它读取AI入口预先冻结的双方int16回绕总值：两项危险strict条件成立时，医疗资格优先且无目标也不回退解毒；医疗缺失生命先以32位差值比较、命中后才回绕保存，解毒按strict最大正poison选择。否则固定消费`bounded(50)`后短路可选`bounded(150)`用毒，再按side 0队伍库存/任意非零side随身槽的不同伤害门槛与RNG扫描暗器，物品数量均忽略。无暗器后体力须strict大于10，十武功槽的最小need_mp初值1000，故无武功MP1000仍可攻击；动作2只返回而不写动作，动作3/4/5/10才写入。首轮REVIEW修正医疗missing HP过早16位回绕，随后从入口完整重审零新增差异；32768缺失、负最佳值后续替换、两项援助等号、医疗优先、用毒等号/短路、队伍与随身暗器阈值/RNG/零数量、体力10及MP999/1000均有回归，归类`platform_adapted / converged_no_new_differences`。六个候选selector现已逐owner关闭。

## 21. AI入口typed同步合同

`sub_33599`当前机器身份固定为1716 bytes、440条指令、64条分支、78处重定位与39个direct call；raw/loaded SHA256分别为`fe66621722777e5ada2987c67e39b5a56dc275feacc48e53684467ad4bbdeb68`和`d5a59d8b4e7fd757b088169c19223937575ea33ac11fcfb26676bee7f3ce77aa`。前置12项dispatch表hash为`4ff226b5a41d10d4d7b96fd71c4072ef0b3fef190c91194134e725ce8be6b8f0`，动作0与7共享rest入口。

入口按actor side对全部combatant的attack后HP逐项int16回绕累计，并在绘制前冻结双方总值与人数；随后只执行一次战场/status重绘和present，再按参数300等待八次BIOS tick变化，等待期不重绘。selector按低HP、中毒、低MP、医疗队友、解毒队友、逃跑、攻势顺序；低HP四档RNG阈值为3/5/7/9，中毒入口无条件消费一次`bounded(10)`并与signed `poison/10`比较，低MP四档为2/4/6/8，医疗/解毒20/40/60分别比较4/6/8且80无RNG兜底，逃跑先比较5再按HP的1/4和1/5档比较6/8。保留体力<10先置等待7、随后可被低HP selector返回0清除的原顺序BUG。

动作0/7共享休息handler，其余1..6、8..11逐项映射移动、攻击、用毒、解毒、医疗、物品、请求医疗、请求解毒、暗器和逃跑；所有typed continuation语义完成后才写word7并推进actor。逐块重审修正了两项共享时序差异：`ai_wait`原会重复重画，现冻结已present缓冲；攻势selector原会在延迟后重算双方值，现显式读取入口保存的prelude。等待期把可重算总值从330/220改为1530/620后仍按冻结值选择攻击2而非医疗5，四次RNG输出`[8,8,3,15]`、终态3295386429；正式golden双生成逐字节一致，SHA256为`f80e016364dc2512ae6a3294e9e1c72e4080f5a6afcae82d30d33954d46bf691`。两次修正均作废当轮结论，第三轮从入口覆盖全部机器范围、两个caller、RNG、12路dispatch及统一尾后零新增差异，故`sub_33599`归类`platform_adapted / converged_no_new_differences`；各handler内部仍按owner独立待审，不传播closure。

## 22. AI休息与逃跑目的格

动作0/7的`sub_34AD3`机器身份已固定为25 bytes、7条指令、0分支与0重定位。它仅作8字节Watcom栈空间检查，把signed actor参数原样调用一次`sub_3A8A4`并原样返回核心结果；六个caller覆盖AI休息dispatch、逃跑/物品重定位尾及攻击、用毒、医疗、解毒回退。现代省略编译器栈检查，把wrapper合并映射为`rest_actor`，六类接线均通过`finish_ai_handler(...,true)`保留休息后统一完成顺序。独立`Z.DAT`机器golden、全部caller和唯一出口最终重审零新增差异，归类`platform_adapted / converged_no_new_differences`；休息状态核心`sub_3A8A4`仍按order69独立待审。

动作11的`sub_34AEC`机器身份已固定为347 bytes、92条指令、9分支与13处重定位。它以actor坐标建立movement图，只扫描path恰等于round value的格；x外/y内候选对全部异side槽累计32位曼哈顿距离，不跳过hidden或HP0，strict更大才替换且零分无目的。真实field2 source `(10,20)`、round3选择`(7,20)`、最大和20；隐藏死亡敌人与source重合的round1四格同分向量保留首格`(9,20)`，全部同side无目的，round0则选择原地且得分14。得分正才调用mode0/value0移动；原始参数0随后休息，物品重定位传1并继续使用。现代typed plan保留有/无目的、原地目的、逐格render/present/tick及两种continuation顺序；全部92条指令、两个caller和六个call最终重审零新增差异，归类`platform_adapted / converged_no_new_differences`。

## 23. 自动攻击目标策略

`sub_3505B`机器身份固定为223 bytes、63条指令、9个显式跳转和6处重定位；raw/loaded SHA256为`10afcc5cd8f924d80d1749c1fe5f2e906fa60b9a191767fb15436d50e6bae992`与`bbbf59bbdc65946d1c1e0d2e093cb5ccb4d46d831002dd285ae13720f8f9bbdf`。攻击handler与暗器handler两个caller均传signed actor槽且不消费返回值；八次direct call顺序为栈检查、三次RNG及最高攻击/最低攻击/专长/最近四个selector。

控制流按signed morality>=75、morality<=25、IQ>=70依次短路；每个成立门槛才消费一次`bounded(10)`，结果严格<7才命中对应属性策略，全部未命中才无RNG选择最近目标。命中属性策略后无条件停止，即使delegated selector未写word11也不回退。高低morality不能同时成立，因此一次调用虽有三个RNG调用点，实际只消费0、1或2次。seed3首轮输出7必须失败并回退nearest，终态3310558080；signed morality最小值在seed6输出6时命中最低攻击，终态2326136519。既有seed9与seed1向量继续锁定一次命中和两次失败。

现代`choose_ai_attack_target`逐块保持属性顺序、含等号方向、短路RNG、roll边界、未写目标不回退与最终nearest；仅对非法actor/role及delegated selector内部索引错误安全返回。入口重审全部63条指令零新增差异，正式原资产golden双生成一致，SHA256为`296c8c9a0f8f790a165d701ca2b4a9b506657e9654d48a3bb67dbe5b2a3240c3`，Linux app Debug 14/14通过，故本owner归类`platform_adapted / converged_no_new_differences`。

`sub_3513A`机器身份固定为109 bytes、32条指令、5个显式跳转和7处重定位；raw/loaded SHA256为`ab91c4c972a5a17b4db00ceea5a803d7d0e5f6d949c4bccd0b37c97fe7dbcb0c`与`786b1e5e58c1c76dae493c53978e0f2568c230ec67c48fd9a9a55a4c140188ea`。唯一caller命中本策略后无条件停止且不消费EAX。函数按signed槽序扫描不同side且hidden严格等于0的目标，以signed best=0起步，仅attack严格大于best时写actor word11并继续扫描；不读HP，同值保留早槽，全部attack<=0时保留旧目标。现代逐块一致，仅对非法候选role安全返回。dead早槽tie、negative hidden、非正attack保留stale target和same-side过滤均已锁定；入口重审32条指令零新增差异，独立golden SHA256为`bfe4f2a9d4e7dcbef19679d7cff681c5bf91fa61810104f2b6c63919adf40e6b`，Linux app Debug 14/14通过，故本owner归类`platform_adapted / converged_no_new_differences`。

`sub_351A7`机器身份固定为112 bytes、32条指令、5个显式跳转和7处重定位；raw/loaded SHA256为`56611048416b38e1eca2c1cf738e488175de001df5cc2b851ef947fa85ec398f`与`76408936111ba57a30049c0dd11814af62191e1b81797757cee29e8f48488b46`。两个caller均不消费EAX。函数按signed槽序扫描不同side且hidden严格等于0的目标，以signed best=1000起步，仅attack严格小于best时写actor word11并继续扫描；不读HP，同值保留早槽，负attack可选，attack等于或高于1000时保留旧目标。现代逐块一致，仅对非法候选role安全返回。dead早槽tie、negative hidden、signed负值、1000边界stale目标和same-side过滤均已锁定；入口重审32条指令零新增差异，独立golden SHA256为`86772ef8670b75ef913c55dc83268d316a73116f595fa9536c4865327e27df47`，Linux app Debug 14/14通过，故本owner归类`platform_adapted / converged_no_new_differences`。

`sub_35217`机器身份固定为347 bytes、91条指令、19个显式跳转和19处重定位；raw/loaded SHA256为`461f8b3d53f24104b1ca7305af927bd28945715d425f93bd48e793ab423f6916`与`cdb3563325b0382c5bf05d0f4744cc041519d55d94120fc35b6fcc3ad5f4216c`。唯一caller调用后直接置策略命中且不读EAX；业务callee仅尾部最低攻击selector。第一段扫描全部同side槽且不排除自身、hidden或HP0，以signed `use_poison>20`置触发flag；触发后以shared signed best0选可见敌方strict最大detoxification，更新先写word11再以值`>=20`置独立detox flag。detox flag置位会跳过medicine，但尾部只检查独立medicine flag，因此必然调用最低攻击fallback。detox未达20时medicine扫描不重置best，继续以strict更大值写目标并按`>=20`置medicine flag；阈值下暂写最终也由fallback覆盖。隐藏死亡盟友触发、detox20加高medicine仍fallback、shared best15阻止medicine14/12、detox10后medicine20成功、medicine同值HP0早槽及negative hidden敌人均已锁定。现代逐块一致，仅对非法role安全返回；入口重审91条指令零新增差异，独立golden SHA256为`1c9de1701b9b7290c61029c0c53bb40521e98cc086551b548b6d5dd99bd8d85d`，Linux app Debug 14/14通过，故本owner归类`platform_adapted / converged_no_new_differences`。

`sub_35372`机器身份固定为156 bytes、39条指令、5个显式跳转和12处重定位；raw/loaded SHA256为`c20a0bad9f0b84b7d6c757dd0f2338f9bf6427e022c3c7644c8a378d06ff17f1`与`d5a95b43e79f688fad6798458fd71c41bc91e1e00aaa225b380e22a24e8a457c`。两个caller均只依赖word11且不读EAX。函数按signed槽序筛选不同side且hidden严格等于0的目标，不读HP；best为signed 1000，每个合格候选都以相同actor坐标重建targeting图，再按candidate坐标读取signed距离，仅strict更小时写目标，同距保留早槽。现代将确定性图合并为单次构建，正常域全部距离与最终结果相同，且无RNG或宿主时序差异；越界坐标安全映射为blocked555。真实距离`[6,8]`、同距HP0早槽、negative hidden、无候选stale word11及原资产blocked格`(23,9)`距离555可选均已锁定。入口重审39条指令零新增差异，独立golden SHA256为`83376071c3aa86e8fcae0a8221bae7b64cbe36bd33abdad12dc99211a65b31f1`，Linux app Debug 14/14通过，故本owner归类`platform_adapted / converged_no_new_differences`。

## 24. 自动攻击主handler计划

`sub_34C47`机器身份固定为1044 bytes、248条指令、42条显式分支和73处重定位；raw/loaded SHA256为`16f17deba817156bdfe4744fa5f49e8bdcabb30d7724f734b953d6698b593af0`与`3cd7c4306cfd7df22a3569f3a658f7c91aedf3efd876eefcaec69cd8f23cb213`。六个caller均传signed actor槽，十次direct call序列为栈检查、RNG、目标策略、三次targeting图、攻击、移动、最近目标与休息。入口统计十个magic id中严格正值的数量并调用`bounded(count)`，结果直接作为packed槽；计数0/1返回槽0且不耗RNG。熟练度按unsigned word除100，magic记录步长136字节。seed9、两个已学槽先得magic槽0，再得目标roll5，终态2878571567。七项特殊攻击表来自Z.DAT file offset324920，匹配不提前退出，重复键末项覆盖。

area type0/3在signed targeting距离不大于select distance时命中并传movement mode1；type1/2还必须同x或同y并传mode2；其他type永不命中并传mode0。首次命中调用automatic flag1攻击；未命中且signed round value<=0直接结束而不休息，否则调用移动。移动后先完整复检原目标，失败才调用最近目标selector、重读word11并作第三次完整复检，命中攻击，否则休息；全部出口统一写action_done。

首轮REVIEW修正两项stale-target差异：目标策略未写新槽但旧word11合法时，现代不再因`target_written=false`拒绝；移动后最近目标selector未写时，现代不再报错，而是保留合法旧槽执行第三次判定并落入rest。非法负数或越界索引仍由现代安全拒绝。修正后从入口重审全部248条指令零新增差异。`BattleSession`继续按mode0/1/2逐格render/present及两次tick等待，命中后执行自动方向、area伤害、10帧FIGHT/EFT、双bank音效、10帧damage、sprite刷新、提交、升级等待、体力尾和外层action-done；直接与移动后攻击回归保持五个整帧hash。正式原资产golden双生成一致，SHA256为`cf98384f461753277742b56dd0fb0692e7064391eafbc148ef794575913cd241`；Linux app Debug 14/14通过。故`sub_34C47`归类`platform_adapted / converged_no_new_differences`，各delegated callee继续按owner独立待审。

## 25. AI用毒handler计划

`sub_355FF`机器身份固定为104 bytes、32条指令、3个显式跳转和2处重定位；raw/loaded SHA256为`7821ab41dc1724bec3b199715ed04e2cd819aa105b5b69e59810f583e088afe7`与`940a59f0582883b27b20b1f91cdc27c3326670fbe0053281a1cfae35e2d038cc`。唯一caller在返回sign-extend后与-1比较，四次direct call依次为栈探测、RNG、最高攻击用毒目标selector和陈旧距离selector。函数仅把局部EBX初始化为-1，不直接写actor word12；IQ按signed严格大于60才消费一次`bounded(10)`，roll按signed严格小于7才调用最高攻击selector，否则以及其返回-1时调用陈旧距离selector。两个callee返回均经BX低16位截断再sign-extend，单次直接消费0或1次RNG。

首轮REVIEW发现现代把局部EBX=-1误实现为入口清actor word12，已删除该写入；修正后从入口覆盖全部32条指令和出口零新增差异。seed3固定IQ61、roll恰7、终态3310558080并走fallback slot3；seed9固定roll2、终态1341714958并选最高攻击slot4；最高攻击全0固定-1后fallback，IQ60固定不消费RNG。无目标plan以word12=99及完整Session以word12=1分别证明返回-1触发自动攻击但不清旧word12。两个selector内部候选规则与陈旧共享目标来源保持order26/27独立owner，不从本项传播closure。正式原资产golden双生成SHA256为`8953f9289c39431a59f777cb84a09da4140f7d28011ef9b81853f77093ff07b6`，Linux app Debug 14/14通过。

`sub_35667`机器身份固定为168 bytes、46条指令、8个显式跳转和11处重定位；raw/loaded SHA256为`a8d1da0042ee44038243a0bb88e57f997936834ebfdc210d6bce7efe90996b93`与`12572b96cfa2b9999aee3f02ba635490c26c9cf6268d83fe2a2ce04a34d9912d`。唯一caller只据返回1/-1决定是否fallback，唯一call为栈探测且无RNG。selector按signed槽序扫描异方hidden恰0目标，不读HP；poison<95、anti_poison<actor use_poison和attack>best0均为signed strict门。每次更大即写word12并继续，同值保留早槽，非正attack不写且返回-1保留旧word12。dead同值首槽、negative hidden和signed负poison/anti等号向量补齐后，首轮入口REVIEW覆盖全部46条指令零产品差异。正式原资产golden双生成SHA256为`4b2490ea98be72e6b863709fe3dc8479a18896f5f1fa89f82b2ac6c5b7844dbe`，Linux app Debug 14/14通过。

`sub_3570F`机器身份固定为244 bytes、56条指令、8个显式跳转和18处重定位；raw/loaded SHA256为`3bf9b8e1b03074465262d0a5b08257cde7d2ce0243a56f51cef7892cee30562e`与`b06d503b356f1c7587888e743a9dfcda7851d38e86999acf2d7098156322d2b9`。唯一caller在roll>=7或最高攻击返回-1时进入；两次call为栈探测和每合格候选一次的targeting图构建，无RNG。selector复用order26的signed候选门，但距离始终读取全局`word_E6EE0`陈旧槽，而非当前候选；best1000作signed strict最小比较，所以陈旧距离<1000时只写首个合格slot，后续相同距离不覆盖，无候选或距离>=1000则保留word12并返回-1。

首轮宿主REVIEW发现Session错误以actor旧word12替代独立`word_E6EE0`。原`sub_3540E`先调用本selector，成功后才把新word12复制到全局；全xref也证明该槽由AI目标selector及攻击、用毒、暗器handler独立更新。现代已增加Session级legacy目标scratch，并按AI选择、攻击初选/重选、成功用毒及暗器的原写点更新。旧word12=99而scratch初值槽0的完整Session仍选slot1，另有stale4距离8忽略候选真实`[6,8]`、negative hidden+HP0和signed负poison/anti等号回归。修正后从入口重审56条指令零新增差异；正式原资产golden双生成SHA256为`c19c2c95f241882966f965803f8355202b929d3d74beca305494f38e691d0d5c`，Linux app Debug 14/14通过。

`sub_3540E`机器身份固定为497 bytes、115条指令、8个显式跳转和40处重定位；raw/loaded SHA256为`d153ed4a0997bc2735ea31b5a00ecc62ad634b9f574ce57dcf06c867c9546e1a`与`8be7484923c0296d3c6060e77fe945fe29440cc56df79a3c8b8c545ae2c4c335`。唯一caller不读EAX，十次direct call依次为栈检查、目标selector、自动攻击、两次无副作用abs、targeting图、用毒、mode3移动、targeting图和休息。射程为signed use_poison除15向零截断加1；round value恰0且首次在射程直接用毒，正值即使已命中仍移动，负值跳移动但执行第二次同目标检查。移动后只复检原目标，不重选。

fallback在`0x3556F`已把EBX覆写为actor记录偏移，故严格比较`2*actor.attack`与AI外层在handler前冻结的wrapped int16己方总值`2*total/count`，不是target.attack，也不在移动后重扫。首轮REVIEW修正target/actor字段差异，第二轮重审修正宿主resume时重算总值差异；第三轮从入口覆盖115条指令零新增差异。回归锁定use_poison-16射程0、移动等待中盟友HP100→1000仍使用冻结330/3、actor attack160使320==320休息、target attack50而actor attack200使400>346自动攻击。

`BattleSession`对mode3逐格执行render/present与两次tick，命中后执行共享用毒状态、11帧effect30、双bank音效、10帧damage及外层完成；真实battle4 seed2保持poison0→25、体力100→98、word13加1、最终RNG2993822286，首magic/damage hash为`0x47286fa4af30fce4`/`0xd76de7fa195a1ac3`。正式原资产golden双生成SHA256为`cf647cc258b768f4b0ddc1162a5f9d1b7a3364a6bb59e0624fa3805555ccf890`，Linux app Debug 14/14通过，故本owner归类`platform_adapted / converged_no_new_differences`；`sub_3570F`及其他callee继续独立待审。

## 26. AI物品与暗器handler计划

`sub_35803`机器身份固定为40 bytes、14条指令、0个显式跳转和0处重定位；raw/loaded SHA256均为`59b666e9af54bafb5ba99c95859d8e3a9bcda425348fa15267ef3bfdfb9a2bc5`。唯一caller不读EAX并在返回后统一写action_done；三次call严格为栈探测、`sub_34AEC(actor,1)`和`sub_3598C(actor,0)`。重定位callee返回值被忽略，mode0物品callee无条件执行，其EAX仅透传；wrapper无直接RNG或状态写入。现代有目的时按mode0/value0逐格移动、完成后resume并使用，无目的时直接使用，两路均不休息。完整Session覆盖效果面板、按键被忽略、九次tick内来源不变、随后扣减/左移来源及外层完成；新增全同side最大敌方距离和0、无目的、直接use_item回归。入口REVIEW零产品差异，正式原资产golden双生成SHA256为`4092e3040d55a902a18093af2fd72c47ee9fd432c2588a68d2c4b5bc21d3d09d`，Linux app Debug 14/14通过；`sub_34AEC`和`sub_3598C`均不传播closure。

`sub_3582B`机器身份固定为353 bytes、84条指令、3个显式条件跳转和30处重定位；raw/loaded SHA256为`4dbe948d5cbd3b588e9638016ce6a44c3b24f99d014071eeabb9200b7e471ded`与`f0d4f1e000b8ae30590805156b76ceda27c2e93672876ca5fce4989a796e60b2`。唯一caller不读EAX并在返回后统一写action_done；七次call为栈探测、一次攻击目标策略、首次targeting、暗器、mode1移动、同目标第二次targeting和自动攻击。selector返回后actor word11无条件复制到独立scratch，故未写时合法stale目标仍固定使用；射程为signed hidden_weapon IDIV15向零截断加1，DI/path作signed比较。首检命中直接use_mode1；未命中且signed round value>0才移动，零/负值跳移动但仍二检；二检不重选，命中用暗器，否则不消费物品并回退完整自动攻击。hidden_weapon80得range6的直接/移动后命中/移动后二检失败、round0二检、attack全0 stale目标均已锁定；新增hidden_weapon=-16得range0且round=-1跳移动仍二检，以及Session回退保留动作码10。入口REVIEW零产品差异，正式原资产golden双生成SHA256为`edee6ce7fa23eaffa84d26f33b564ef50b2378b47d500ee1828d85415cdce977`，Linux app Debug 14/14通过；五个callee均不传播closure。

`sub_36133`机器身份固定为121 bytes、28条指令、2个显式跳转和8处重定位；raw/loaded SHA256为`589cedb4fed28fa00c4fbe852d4111d772f44dc3454354d697161d47f4cd493c`与`761bb81a8e1063e29e693892d293c56d63cb9995aedaa7ee645e15f505f2b331`。唯一caller只在敌方当前携带槽signed数量减至`<=0`时调用并忽略EAX；唯一call为独立栈探测。函数以signed DX<3控制前向循环，将每个后续item ID和匹配数量同步复制到前槽，最后无条件清第4槽`[-1,0]`；slot0/1/2/3分别循环3/2/1/0次。现代一次role查找等价于机器逐轮重读，bool返回在typed caller有效域等价；高槽、负槽及非法actor/role安全拒绝归为异常域适配。四个合法槽、无效槽不变及非法actor/role回归补齐后，首轮入口REVIEW零产品差异。正式原资产golden双生成SHA256为`2569d16082a63705fdfe2ee6d68235dc806a5c0fe4d982c1684a6917ef3eb639`，Linux app Debug 14/14通过；callee与caller均不传播closure。

`sub_3598C`的AI mode1暗器状态与手动`sub_3A30B`并不共用毒值公式：item add_poison非负时直接加到目标poison，负值才算`(add_poison-hidden_weapon)/2`，两者都不读取anti_poison且不追加RNG。真实item102在seed1下伤害21、hurt40→45、HP100→79、poison10→50，仅消费一次RNG；无毒item96在seed2下伤害16、poison10保持10，也仅消费一次RNG。队伍方耗尽200槽inventory后左移，敌方耗尽4槽taking-item后调用`sub_36133`；AI分支不改actor方向且不提前写action_done。

`sub_2B483`共享物品效果已恢复23项状态数组：HP正负分支各自保留严格第二次RNG短路，毒值正负公式、HP/MP/体力/上限夹取和13项能力signed相加均按机器码执行；add_morality与add_attack_twice只进入显示数组却不写角色字段的BUG保留。真实item19在seed1下得到HP100→200、hurt40→0、poison50→0、体力30→100、MP10→100，三次RNG终态662824084。typed结果锁定battle重绘与`(70,18,148,20*n+30)`效果面板；玩家caller等待任意键，AI caller不读取输入而固定等待9次tick变化。

`sub_3598C`机器身份固定为1959 bytes、440条指令、47个显式分支和125处重定位；raw/loaded SHA256为`039adfbd48a1e5a282cbddce9a80b7499745277228c1c432cbf4a590263ed2a5`与`a1318ae160d72c1a5093248aaa7c60dc601be62adc83b6114a30b3dad8895e47`。两个正常入口为mode0 `0x35821`和mode1 `0x358E2`，另有三个外部owner跳入共享epilogue `0x3612C`，不传播closure；17次call覆盖两侧物品效果、延迟、EFT、互斥RNG路径、绝对值、damage和两种来源压缩。

mode0在共享效果面板present后不读键，无论效果数是否为零都执行`sub_3DB83(340)`的9次tick变化，再signed减来源数量。mode1中两侧动画和扣减均取当前AI来源槽；敌方payload也取来源槽，但队伍payload错误地读取初值-1且只由有效玩家物品确认写入的陈旧`word_54B74`。状态算术保留hurt四档、恰一次`bounded(5)`、随机化add_hp先截signed word、hidden-weapon缩放、hurt/HP/damage、无anti-poison的毒值公式；完整EFT后才提交状态，damage非零写kind1且固定调用`sub_38910(0)`，10帧damage前4帧闪烁，之后才消费来源。order53 caller交叉审计纠正了先前把“固定suppress0”误实现成“固定kind0”的回归。

入口REVIEW修正队伍来源/payload槽分离、AI面板移除输入等待、暗器状态延后到EFT后及随机后add_hp中间word回绕；随后正确锁定damage helper的suppress参数固定0，但曾误把它实现为damage kind固定0。order53现恢复两条caller的`damage!=0 ? kind1 : kind0`。真实source item102/payload item96向量得到effect30、damage19、HP100→81、hurt40→44、poison10不变；add_hp=-32768向量得到damage10921。完整Session锁定动画前状态不变、动画后提交和来源延后；普通物品面板、暗器前奏/首EFT/首damage、移动后首EFT hash为`0xa7542240e4172664`、`0x3f498f66e6357fff`、`0xc65b523bd75389e2`、`0x335fd35ea7e3f367`、`0x16a8f10ce319622b`。纠错后从入口重审440条指令、47个分支、125处fixup、17次call及全部出口，零剩余差异；当前正式原资产golden SHA256为`a6e7c3adccb1cedd5c74296b305cba84da73dfb784906e0a28b6208974e46b1b`，Linux app Debug 14/14通过。`sub_35803`、`sub_3582B`与`sub_3598C`均按各自独立owner关闭；delegated callee与共享尾caller不传播closure。

`sub_361AC`请求医疗owner机器身份固定为93 bytes、22条指令、1个signed分支和6处重定位；raw/loaded SHA256为`717b2ff358eec5fc59796f2eff00fa93a0662ef80c4a4ed99e0dbc021c4255cb`与`a92c3af77c368fcb219065d9a1e1cef84d0a4388fffdccfb50d4eaa2d2d41aa9`。唯一直接caller为`sub_33599`动作8case，忽略EAX并在共享尾写action_done；独立`sub_36209`请求解毒owner自行压栈后跳入`0x361B1`复用主体，但不传播closure。actor signed行动值严格大于0时调用`sub_3650E(actor,mode0,value0)`并忽略返回，零或负值跳过；汇合后重读全局请求目标槽及当前x/y，再无条件调用完整`sub_34C47`自动攻击。现代typed plan、实际逐格Session continuation及外层完成逐块等价；补齐目标移动后重读、正/零/负行动值和非法域回归后，两轮入口审计均零产品差异。真实battle2继续锁定mode0一格移动、请求目标恢复、动作码8、首magic hash`0xcc6a249ebb919a23`及RNG3295386429。order32正式原资产golden双生成SHA256为`5c491aa130bc3b016f38d4dcea71df94a1a02a893344aa4b01398e86fe0be8f4`并已独立关闭。`sub_36209`请求解毒owner另由`68 10 00 00 00 EB A1`组成，严格7 bytes/2条指令/1个short jump、零fixup/call/local RET，raw/loaded SHA256同为`39c017dd2b811b83553060d2eafb40e83c07e19d3e6685c6382075fffa133f09`；唯一caller是动作9case，尾跳`0x361B1`的栈形状和最终返回与请求医疗入口完全相同。动作9正/零/负行动值、移动后坐标重读和非法域回归补齐后，两轮wrapper入口审计零产品差异；其closure不反向复制order32主体。最新正式golden双生成SHA256为`4849e78fac9f2808920b8d20adf386c20e6dc229c7754e15e28d074eaeacb0b5`，Linux app Debug 14/14通过，两项owner现均独立关闭。

`sub_36210` AI医疗handler固定为412 bytes、97条指令、6个分支、33处重定位、唯一caller及9次direct call；signed medicine/15向零截断+1、两次signed距离比较、只在signed行动值正数时mode1/range移动、移动后目标坐标重读、两个无副作用abs返回丢弃、live actor attack及入口冻结己方总值/人数的strict回退门均已逐块审计。首轮发现现代在移动continuation后重扫队友attack/HP，修正为计划携带`sub_33599`入口prelude的wrapped total/count；区分向量锁定冻结平均300时actor双攻600选择攻击，而错误重扫会得到2300并休息。修正后从入口覆盖全部指令和三种delegated出口，零新增差异；动作5正/零/负行动值、signed负医术、目标重读、冻结突变及非法域测试补齐。最新正式golden双生成SHA256为`56dd722def3f65427014e232ada7b12d3345a12ec4bdf45dd9edfb472453fa6a`，Linux app Debug 14/14通过，本owner独立关闭。

`sub_363AC` AI解毒handler固定为354 bytes、79条指令、7个分支、28处重定位、唯一caller及7次direct call；signed detoxification/15向零截断+1、两次signed距离比较、只在signed行动值正数时mode1/range移动、移动后目标坐标重读，以及live actor attack对入口冻结己方总值/人数的strict回退门均已逐块审计。该owner与医疗入口不同，不调用`sub_3F50B`，且解毒、攻击、休息三路都尾跳外部`loc_39A3E`丢弃delegated参数并恢复寄存器后返回。现代共享support plan已正确携带入口prelude总值/人数；动作4正/零/负行动值、signed负解毒、目标重读、冻结突变、wrapped阈值和非法域回归补齐，首轮入口审计零产品差异。最新正式golden双生成SHA256为`7358979c860b6dc41acde03c5213306e5bba6c821a38db385e7dc63f31758041`，Linux app Debug 14/14通过，本owner独立关闭。

`sub_36210` AI医疗与`sub_363AC` AI解毒使用各自ability signed除15加1为射程；首轮目标图命中即执行动作，超距且行动值严格正时调用`sub_3650E(actor,mode1,value=range)`，移动后恢复同一目标再建图。零/负行动值虽然不移动，仍执行第二次建图。第二次仍超距时仅当`2*actor.attack`严格大于`2*wrapped_allied_total/allied_count`才自动攻击，否则休息。完整callee汇编确认IDIV余数和医疗分支两次`sub_3F50B`返回值均无行为效果。现代Session已执行直接与移动后支持状态、FIGHT/EFT双bank、无flash damage及AI外层完成；直接医疗/解毒首magic分别为`0xbec9ef2738ca79b4`/`0xae0f13fbbc4c8083`，医疗距离6/range2移动4格后首magic为`0x2138cfdf8041c6bb`。攻击/休息回退进入完整共享continuation；两个owner均已独立完成入口审计并关闭。

`sub_3650E`机器身份固定为1418 bytes、311条指令、57个分支、80处重定位、8个正常入口和14次direct call；raw/loaded SHA256为`31785d54867d026266133cad9cd3ae56f0d9efe11b6627ef1bb62781029a97f7`与`cf5d93c2bda18388199c1a72e5c0cf20207281276bf2a360cb0f304ac2d65e63`。完整目的格合同为：mode2仅在signed `target_distance-round<=range`时按range向下找轴向exact layer，mode3无轴向限制，其他分支从目标周围按上、右、左、下先找actor同轴可达格、再找任意可达格，仍失败则x优先/y逐格退向actor；射程层按x外/y内扫描并以strict Manhattan保留首tie。目的格须连续通过当前图和重建actor movement图两次signed `path<128`检查，再标最短路并delegated调用`sub_37355(actor,1,mode,range)`。新增mode2超出本回合转generic、非同轴第二轮、全占用退回source和降层终止后二检失败四组独立oracle回归；首轮入口REVIEW零产品差异。`advance_ai_movement`与`BattleSession`边界继续锁定每格path255、occupancy、坐标、方向、sprite、体力/行动值、render/present及参数40的两次BIOS tick后恢复八类typed continuation；delegated `sub_37355`仍为独立owner。正式golden双生成SHA256为`a96e3edb9de949cc441fd9fd5818ccb0c8a4b3423214cd86505be0d008e00ab7`，Linux app Debug 14/14通过，本owner独立关闭。

`sub_36A98`玩家移动wrapper机器身份固定为95 bytes、32条指令、1个JNZ、1处重定位、唯一caller及4次direct call；raw/loaded SHA256为`b750d0562b8ff3c8f0a518ff963fa69f417a3fa55b51f55f16acebc125734b84`与`b3383e9b8b3d33025818f2bf512f47b48c4d938a59c55115aa5abf74e5c4a79e`。入口将signed actor行动值、mode0和cancel local地址传给`sub_36AF7`；local low word恰等于1时不标路、不移动并返回-1，否则依次调用`sub_37245`与`sub_37355(actor,0,0,0)`并固定返回0。唯一玩家动作0 caller随即以IMUL覆盖EAX，返回值不控制菜单尾。现代取消直接重建菜单且不创建移动plan，确认后复制选择期路径图、标路并逐格present/参数40两tick；新增取消前后坐标`(26,24)`、行动值2和体力10不变回归。入口REVIEW零产品差异；正式golden双生成SHA256为`b37f083ef1430a2477fff897853899915a5dbc49952e8baff88acb85bca6ba34`，Linux app Debug 14/14通过，本wrapper独立关闭，三个业务callee继续独立审计。

`sub_36AF7`通用玩家光标handler机器身份固定为783 bytes、157条指令、35个分支、72处重定位、六个正常caller和7次direct call；raw/loaded SHA256为`880246a653a60137ff63b21d437f2722ffbfb14bb4a0dfa6f4b06d64d34215e9`与`cb0b440727ac96038276f0de9a9b139c820184b9dba7a5772b45c4b4800ed9de`。入口按mode0/1建立movement/targeting图，方向优先级下、右、左、上，signed 64列线性索引允许边缘别名；path不大于上限或occupancy非空可移动，mode0确认严格要求`0<path<=limit`，mode1允许`0<=path<=limit`，Escape清path上限并写cancel word1。首轮发现现代Session首键前只present一次，而机器在首键前present两次且每次移动/拒绝确认后再present一次；现以入口计数2、后续计数1修正，并增加两次早期Escape被忽略及source拒绝确认后立即方向被忽略的完整Session回归。修正后从入口重新审计157条指令和全部出口，零剩余产品差异；非法mode/actor和真正存储越界安全拒绝归类平台适配。正式golden双生成SHA256为`36ff2d77f9602e6501a11a54dc89e19d8cfd87877df6f3bdb2a039a6ca5fc78b`，Linux app Debug 14/14通过，本handler独立关闭，两个路径图callee、render/present callee及共享`0x3677E`尾继续独立审计。

`sub_36E06`移动寻路图wrapper机器身份固定为121 bytes、20条指令、1个循环分支、12处重定位、八个callsite和3次direct call；raw/loaded SHA256为`776fc38943f45416200cbac30378f6acbfb2fe5d956570ed4d3a2ade0d23dce3`与`68e70ab2c5fca8aaed97f6d2bf40562a34dfd90f3e7fe3069e987d05fc5ec860`。wrapper先调用`sub_36EF8`，再以signed `y*64+x`把source无条件写0，并初始化255槽队列为读0、写2、distance0、slot0 `(0,-1)`、slot1 source；至少调用一次`sub_37070`且仅在EAX为0时回环，首个非零EAX透传但三个caller的八个续接点全部忽略。现代movement build的合法caller域逐块一致，局部队列额外清零不可观察，非法source安全拒绝归类平台适配。battle0/93 source occupancy新回归分别保持完整图hash `0x773478cb5fde310d`与`0xc4e9944b25f2c2bb`且source为0；首轮完整入口审计零产品差异。正式golden双生成SHA256为`07e752feffd6573ed9bdd3d5eb6930ac9869315ba40379cf3f09ae52cfaa4b50`，Linux app Debug 14/14通过；`sub_36EF8/sub_37070`现均已按独立owner关闭。

`sub_36E7F`目标寻路图wrapper机器身份固定为121 bytes、20条指令、1个循环分支、12处重定位、十五个callsite和3次direct call；raw/loaded SHA256为`7de2240639b89bd98d88ad212cdce7dadf53b0d35255af2524d7155fcda890a6`与`7de0062638c1709694717aef2703770addcd76495c6ee171a327d8cfd69d5610`。中和三条CALL后与movement wrapper其余116字节完全相同；唯一业务差异是先调用`sub_36FF9`。source归零、255槽队列种子和EAX零回环合同相同，九个caller的十五个续接点均在任何读取/分支前覆盖EAX。现代targeting build合法域逐块一致，局部队列额外清零不可观察，非法source安全拒绝归类平台适配。真实battle0 `(19,11)`与battle93 `(2,0)`的upper layer均非零，建图后source仍为0，完整图hash固定为`0x27f67ce66dece4d6`与`0x804d8a6ca5fb4034`；首轮完整入口审计零产品差异。正式golden双生成SHA256为`ab6dbe9a82f54d08e5a6e3f5e5e35b54d8c16888bdf30dd21760e077f4a6d306`，Linux app Debug 14/14通过；`sub_36FF9/sub_37070`现均已按独立owner关闭。

`sub_37070` flood step机器身份固定为246 bytes、76条指令、11个跳转和7处重定位；raw/loaded SHA256为`0f08f89615cfeef431df99ad4136751a7dd8264ce0e724472b5926e51ba0116d`与`2230edd43668c7b5259fd41f3e68a03316edcb1adb6c67597f94fd0569e1473e`。movement/targeting两个wrapper caller均在EAX为0时再次调用，非0时直接返回；每step先出队，signed y<0时使distance按signed `%128`递增、先入队sentinel再出队，连续sentinel经保持EAX的7-byte共享尾返回-1。普通节点按上右左下，以16位坐标加法和signed闭区间`0..64`扫描；delegated path read恰为254时先入队再写当前distance，四向完成返回0。现代在单次`build`内联全部step并私有化queue/index/distance，中间无外部可观察调用；`x=64,y<63`线性别名保留，仅真正index>=4096的delegated越界安全拒绝归类平台适配。中心十字六次step trace、battle0/93一二层距离、battle0 `(64,0)`与`(0,1)`共享index64及完整hash`0x7e0528a84512f654`均通过；最新正式Golden双生成SHA256为`8714fc3066aff8792675fa1c4e1a1fbcd5a0a02ca33f906078246f6093d6fd99`，Linux app Debug 14/14通过。首轮完整入口REVIEW零产品差异，本owner独立关闭；四个业务callee与共享尾其他caller不传播closure。

`sub_37166` dequeue机器身份固定为72 bytes、21条指令、0个跳转和2处重定位；raw/loaded SHA256为`1fab3fba7ceae1e002dce08eebdcf2e7b48ce91dc7ccc61ef494e40a27ccdf9b`与`3bd60c28edc86229bb6609c2dfac3809e6b1a6d4077a2e5af4295a0ddc7d2e6a`。第三参数signed读下标在x读取、y读取和更新前各重读一次，分别从`0xE6ABE`与`0xE6CBC`起始的255-word双数组先写x再写y，随后对`index+1`执行signed `idiv 255`，余数回写、商由EAX返回。两个真实caller传入互异x/y local与共享index，返回后均只比较y；合法下标254→0返回商1，其他0..253返回商0但均被忽略。现代交错`BattlePathCoord`、一次读取、私有index和省略商返回均无观察差异；synthetic 253→254→0 trace锁定signed极值与环绕，battle0/93回溯前457/822个可达格证明真实图跨过255次出队。正式Golden双生成SHA256为`94cb55ff4a200ddb35d840435805c45dbaafe0768998063acdfff59b912caa35`，Linux app Debug 14/14通过；首轮入口REVIEW零产品差异，本owner独立关闭，`sub_371AE`保持独立。

`sub_371AE` enqueue机器身份固定为73 bytes、18条指令、0个跳转和4处重定位；raw/loaded SHA256为`b06189eb16e1f2aac51e7242d7f18221427a0f26de02589ef40c835a688c1976`与`76a887a959bf8e3c83699ae298131217201ff6f4e59fb84f2e7e3e4ee0d0df92`。入口只signed读取一次`word_E6ED0`，先把x低16位写入`0xE6ABE`双数组slot，再把y低16位写入`0xE6CBC`同一slot；两次写完才对`index+1`执行signed `idiv 255`，余数回写、商由EAX返回，且没有full guard或read/write碰撞检查。`0x370BE`入队`(0,-1)`sentinel，`0x3713E`入队候选`(x,y)`；合法下标254→0返回商1，其他0..253返回商0，两caller均忽略。现代交错`BattlePathCoord`、私有index、一次复合coordinate写和省略商返回均无观察差异；synthetic 253→254→0 trace锁定signed极值、同slot配对、环绕及覆盖，battle0/93回溯前457/822个可达格锁定真实write环绕。正式Golden双生成SHA256为`bfeb9bb56520ef0801401614bfdcf99bcd338ae76be69cf445041e6cb64fbf98`，Linux app Debug 14/14通过；首轮入口REVIEW零产品差异，本owner独立关闭，`sub_371F7`保持独立。

`sub_371F7` path write机器身份固定为39 bytes、10条指令、0个跳转和1处path-base重定位；raw/loaded SHA256为`0f9bcfcda1b52e720a4fb6736dc5e5e0c0b52f40425664a967c5667294755af1`与`a84edfc8630f751b1984e24720466eff6228f26a00ab1172c98c99470cc95e01`。y/x分别按signed low word扩展，以32位`(y<<7)+2*x`得到相对`0xDCA04`的字节偏移；第三参数仅写DX低16位，EAX原样返回字节偏移。四caller分别写flood distance、target 250、predecessor 250和movement consumed 255；三处忽略或覆盖EAX，首个mark caller仅替换AX但合法offset高字为0且后续坐标运算只消费low word。现代四个直接`std::int16_t`写点等价，`(64,0)==(0,1)`的index64别名保留；机器负坐标别名和index4096越界写由现代安全拒绝，归类平台适配。synthetic向量锁定`(1,2,0x12345678)->index129/offset258/word0x5678`、x64与负坐标别名，以及`(0,64)/(64,63)->index4096/offset8192`边界；battle0/93 distance、marked、first-step、两次consume与battle0 x64 hash通过。正式Golden双生成SHA256为`d154682b9ab072f9499c7cdce5f633a15dc2505057330bae9637111fdc175525`，Linux app Debug 14/14通过；首轮入口REVIEW零产品差异，本owner独立关闭，`sub_3721E`保持独立。

`sub_3721E` path read机器身份固定为39 bytes、9条指令、0个跳转和2处重定位；raw/loaded SHA256为`2244d0f46a9d69c9b7090714a652d300b4bb9fa283aa9557381261d960ef0dd7`与`b205961e08547954cf7361471c63c0e6e9517eade4bd0c1d6de4ed30330173ff`。y/x按signed low word扩展，从`0xDCA04[y*64+x]`读取word，先把原始16位位型复制到`word_E6ECE`，再`cwde`返回signed EAX。四caller分别判254、保存target distance、判predecessor distance和判250，均实际使用返回；IDA枚举scratch全xref只有本函数写入、无读取。现代直接`std::int16_t`读取和公共`value`保持signed结果，省略scratch无观察差异；`(64,0)==(0,1)`的index64别名保留，负坐标和index4096越界读取安全拒绝归类平台适配。synthetic backing锁定word0x5678、EAX -1/-32768/-32767、scratch位型、负坐标别名和`(0,64)/(64,63)`越界边界；battle0/93 distance、target distance14/22、marked、first-step、consume与battle0 x64 hash通过。正式Golden双生成SHA256为`59060272e6070678127767ec5a5c65370dfab3c780ead1950c1cb4baafaff47c`，Linux app Debug 14/14通过；首轮入口REVIEW零产品差异，本owner独立关闭，`sub_37245`保持独立。

`sub_37245` shortest-path marking机器身份固定为272 bytes、75条指令、10个跳转和10处重定位；raw/loaded SHA256为`7e87f0821e6915f00b3d5c5aceee8d131dfd822cb2a6909b879e8c928ba09f4c`与`63725cf01080f6e5e365eae1baa68ccd719dfa4b8aa651af56a50e024bf3c02e`。函数先读target distance并标target 250，再以signed `(distance+127) IDIV128`余数降层，按上右左下及signed `0<=x,y<64`选择首个等值前驱并标250，直至source也被标记后经外部共享尾`0x34ACB`退出。两个caller分别为signed path word `<128`复检后的AI移动和mode0光标确认后的玩家移动，均不观察机器EAX；合法BFS前驱链严格降低真实距离且最多4095步，现代bool与4096步上限不改变合法行为。机器无前驱时采用最后下方candidate并无界回环，现代false安全终止归类平台适配。straight、上优先tie、0→127、source==target及可证明不终止的malformed向量通过；battle0/93 target distance14/22、marked hash `0x3555eec69bfdbfc0`/`0x6760d37356a2b33a`、首步与consume通过。正式Golden双生成SHA256为`94e210aeefebf79cad6fc5f3668a56c28736d0f829ef91806a647dbafc72dbda`，Linux app Debug 14/14通过；首轮入口REVIEW零产品差异，本owner独立关闭，`sub_37355`保持独立。

`sub_3B387..sub_3C2AC`战后进度状态与同步UI均已恢复：敌方满HP/MP、体力100并清内伤/中毒；胜利经验均分、队伍HP/体力下限、角色/练功/制造经验封顶，以及等级、练功、武功升级、制造RNG/状态均保持原顺序。`BattleSession`依次执行经验固定框、升级固定框、练功动态框、武功等级动态框和制造固定框，每项重画战场、present并等待任意非零键。经验在其消息前提交；升级和练功以副本/RNG副本预演，按键后才执行真实提交；制造配方选择在消息前消费共享RNG，库存及数量RNG在按键后提交，保留原同步可观察边界；五帧FNV64依次为`0xa699bf683f037936`、`0xef2c8987fe26a127`、`0xdd4c7e74171e8ee5`、`0x0f4783440328986e`、`0xb980de17004d5b6c`。四函数均推进为`implemented_pending_review`。

`sub_3C563`回合异常状态更新保留`hurt>0`优先分支、poison的HP/体力/hidden门槛、两次有符号除法，以及HP/体力仅严格负值夹1；`sub_3C672`对0..25槽（含当前活动数之外）仅在目标hidden严格等于1时清word11/12。`sub_3C6D3`三个机器码xref均已覆盖：玩家菜单内两处由同一菜单重绘相位执行，AI prelude调用由prelude/wait相位执行；独立面板oracle为`0x630a82d57e1d8715`，Session AI prelude为`0xb02104139829a80d`。三函数均为`implemented_pending_review`。

## 16. B8 实现差异审计关闭

B9前的B8入口实现审计从`sub_31C75`机器顺序发现并修正五处差异：首帧present后才排battle music；手选确认后才加载WDX/WMP/EFT；WAR/WARFLD成功读取后清occupancy且不重复清空；首帧present后、fade前才首次速度排序；首帧继承跨battle保留的完整render globals，present后才按排序后slot0重定位。对应回归锁定延迟battle资源加载、首帧插入顺序、present后排序、继承视角与独立`view=(0,0)`首帧hash。

最终源码通过Linux与Windows的core/app × Debug/Release完整8项BUILD矩阵；Linux/Windows Debug当前9个同名B8 hash逐字节一致，独立battle golden双生成逐字节一致。该实现差异审计关闭时81项closure仍全部为`implemented_pending_review`，本节本身不产生`assembly_exact`结论；当前统一最终双向REVIEW进度以上方状态与inventory为准。
