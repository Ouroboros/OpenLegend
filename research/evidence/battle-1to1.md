# B8 战斗 1:1 证据

状态：B8统一最终汇编→C++ REVIEW为8/81；场景战斗入口、数据载入、参战者建立、回合主循环、排序/交换及玩家行动菜单已收敛为`platform_adapted`，其余73项为`implemented_pending_review`。

## 1. 物理范围与闭包

`sub_31C75 @ 0x31C75` 是 scene 与五轮试炼调用的 battle 入口。其后连续 battle 实现区间截止 `sub_3C6D3 @ 0x3C6D3..0x3CBE3`；`research/ida/reports/Z_DAT.b8_battle_xrefs.txt` 由当前 `Z_DAT.i64` 和 `idat.exe -A` headless 生成，枚举81个 FUNCTION 记录，报告规范为 LF，SHA256 为 `179b85c68ad87d03f175f7b22ff9af7ffbae68aed758eeaa0f0fe692ab67d488`。

`research/inventory/battle-closure.tsv` 以该报告为机械真值，当前0项为 `pending_mapping`、0项为 `pending_implementation`、73项为 `implemented_pending_review`、8项为`platform_adapted`。battle 区间调用到的 resource/render/input/time/random/audio 入口是共享 owner 边界，不随递归调用图吞入 battle closure。

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

`research/tools/generate_b8_battle_goldens.py` 只读取原版字节，不链接 OpenLegend C++；双生成逐字节一致并与正式文件逐字节相同。正式 `research/evidence/battle-goldens.json` SHA256 为 `aa606cc4949dc4c5da8e39edf42ad9e9c32dfa9aec85aa43b6a4fecdc12dfc22`；`battle_entry`、`battle_data_loader`、`battle_setup_machine`与`battle_round_machine`分别保存入口、载入、参战者建立和回合核心的raw/loaded identity、caller、call及语义合同。回合核心键为本包新增，全部旧键保持不变。

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

玩家移动现实际执行movement路径图光标、四方向翻译键、Escape/三确认键、路径范围与主光标重画/present；确认后清路径上限、标记最短路并逐格提交状态，每格重画/present后按参数40等待两次BIOS tick变化。返回菜单时仅重检移动项，不重算其余九项。battle4 Session覆盖取消、`(26,24)→(26,25)→(25,25)`、word6 2→1→0及移动项失效；`sub_36A98/sub_36AF7/sub_37355`均推进为`implemented_pending_review`。

状态动作现按`sub_22066(...,2)`进入队伍前缀选择，执行圆角标题/列表、角色名NUL对齐、上下回绕、Escape取消和三确认键；确认后在battle背景上依次呈现`sub_22A59`两页角色状态，每页各等待任意非零键。第一页保留伤势/中毒/内力分档、非法内力类型复用中毒色、装备加成和30级阈值；第二页保留两件装备、修炼物经验分母与十项武功等级。动作返回菜单但不结束actor、不消费RNG。独立Python直接读取原WARFLD、HDGRP和字体资产复算，选择页/第一页/第二页与C++整帧FNV64分别一致为`0xfa1b21403051335c`、`0x1c5e879ce61d5b34`、`0x9592da33a3c151d4`；逐块回审修正了最大生命中毒色、修炼所需经验普通色和分母系数读取资质而非修炼经验三处差异。

自动动作重画并present时flag仍为0，present完成回调后才置flag并进入同actor AI；AI全部返回后回到玩家菜单共同尾。结果命中时，`sub_3B238`内部的战果panel/present、任意键与全部战后结算消息必须先完成，返回主循环后才清隐藏目标、执行一次轮末异常状态，再保持当前画面等待轮首tick发生变化，最后发布typed结果供入口淡出回收；hidden槽同样执行这条公共尾。独立golden双生成一致且正式SHA256为`aa606cc4949dc4c5da8e39edf42ad9e9c32dfa9aec85aa43b6a4fecdc12dfc22`；order5/8机器门、联合静态/golden门及最新Linux app Debug 14/14通过。从两入口重新逐块审计零新增差异，`sub_3271E/sub_32E59`最终归类`platform_adapted / converged_no_new_differences`。`sub_33599/sub_3B238/sub_3C6D3`及输入专属同址owner仍为独立待审项，不传播关闭。AI prelude整帧FNV64固定为`0xb02104139829a80d`。

## 8. 战场路径图与最短路回溯

`sub_36E06..sub_37245` 的十个 path 单元已映射为 `BattlePathing`：

- movement 图把 upper layer非0、occupancy非-1或 ground tile 命中9段 IDA 常量的格写555，其余写254；targeting 图只检查 upper layer；source 随后强制写0；
- 原255槽环形队列保留 `(0,-1)` sentinel、distance `%128` 和上→右→左→下扩展顺序，没有替换为无界 queue；
- 回溯从 target 开始写250，每层用 `(distance+127)%128` 并按同一方向顺序选择首个前驱；消费标记255由后续移动单元使用；
- `sub_37070` 的坐标比较允许64，现代仅保留线性 index仍在0..4095的别名，index>=4096安全拒绝；不可达回溯返回false而不进入原死循环。

独立 oracle 固定 battle0/93 空 occupancy、单格占位、target距离14/22、回溯前后完整 FNV-1a 与首步 `(31,20)/(33,29)`；Linux Debug 14/14。十项均为 `implemented_pending_review`。

`sub_37355` 另以每次一个同步边界实现逐格核心：旧 path=255、occupancy 搬移、x/y、方向、sprite、条件体力 DEC、行动值 DEC依机器顺序写入；destination、Manhattan range、aligned range和行动值耗尽停止规则也已映射。连续左移两格固定 direction2、sprite5110、physical power 1→0、round 5→4→3。玩家路径和AI mode0..3均已实际执行每格视图更新、render/present与参数40对应的两次BIOS tick变化等待；函数推进为 `implemented_pending_review`。

## 9. 武功攻击入口与每轮提交

`sub_37734` 已恢复十槽统计与 magic profile：只统计 ID>0；唯一已学武功时原版仍选 slot0 的 BUG 保留；等级用熟练度 unsigned `/100`，并从 magic words 28+level、38+level、15、14、16读取选择距离、攻击距离、area type、hurt type和 need_mp。attack_twice严格等于1时轮数为2。

现代每轮提交严格保留 action word7=1、word13加2、`LegacyRandom::bounded(2)+1` 熟练度增长、unsigned 999 cap、跨百升级判定，以及 `(cost_scale/2)*need_mp` 内力扣除后 signed负值夹0；全部轮次后体力减3并夹0。seed1固定 state1103527590、299→300、cost scale3下 mp3→0、999 cap和体力2→0。

`BattleSession`现完整执行攻击入口：多武功菜单确认后，area type0/3进入mode1目标UI并支持Escape返回原ordinal，type1显示方向框并按0/1/2/3映射上/右/左/下，type2直接攻击。每击实际执行area伤害、FIGHT/EFT双bank动画、10帧damage、sprite刷新、重画/present/wait17，随后才提交word7/word13/熟练度/MP；跨百绘制升级框并present/wait500，双击结束后才扣体力并进入共享actor尾部。原版在循环外缓存范围，现代同样缓存初始profile；首击升级后第二击继续命中原范围，但伤害与cost scale按新熟练度重算。固定双击第二击hits1、cost scale4、内力20→15→5；方向提示与升级框整帧hash为`0x5e46c805f42677b0`、`0x1f0048d1945a4948`。四种area type、取消、双击和13次升级等待均由battle4 Session覆盖；AI automatic flag1路径也执行自动方向、直接或移动后攻击、10帧FIGHT、10帧damage、17tick提交、升级框13次tick、熟练度/MP/体力和外层action-done。固定AI首FIGHT、首damage、提交战场、升级框及移动后首FIGHT整帧hash分别为`0xe1d1b3cff84bc0c4`、`0x04c528de57fbffa0`、`0xdbee20f394fd7219`、`0xed97f52f9bedb836`、`0xacc58834b066ca07`；`sub_37734`为`implemented_pending_review`。

## 10. HP与MP伤害

`sub_39188` 已映射为 `apply_hp_damage`：扫描双方knowledge>80的存活可见参战者，按最高可支付层计算cost scale，合并角色攻防、装备、特殊加成和双方知识；保留两次bounded(20)、非正时两次bounded(4)、距离1..10线性衰减和>10固定2/3。固定damage30下，HP30恰好归零不发击杀奖励，而HP29下溢才加`10*level`；并按原顺序写hurt与poison。

`sub_395EC` 已映射为 `apply_mp_damage`：严格执行bounds `[3,3,add_mp/2,3,3]`，攻击者增加当前/最大内力并封顶，目标扣hurt_mp与随机差。seed1向量输出 `[2,1,3,1,1]`、state4182499122，攻击者10/20→23/23、目标50→35、返回15。两函数Linux Debug 14/14，均为 `implemented_pending_review`。

## 11. 武功area扫描

`sub_37734` 的方形与十字单轮状态边界已映射为 `clear_attack_effects/apply_attack_area`。方形按x外/y内扫描并按目标差更新方向；十字逐距离按上、下、左、右扫描且不改方向。友军格完全跳过，空格只写effect=1，敌方格写damage word9。type2忽略hurt_type并强制HP伤害；type0/3按hurt_type 0/1分派HP/MP，其他值只留effect。

固定方形8格hash `0xe5f47b0a810ce2bd`、十字7格hash `0x3144c415023d9464`、单格MP hash `0xab559939923b4f74`；伤害29/29/15，effect kind 1/1/3，MP分支保留上次HP cost scale3。四种area现均接入目标/方向输入、动画、present、提交与双击；area覆盖使用入口缓存profile，伤害函数每次命中仍读取当前熟练度。`sub_37734`为`implemented_pending_review`。

## 12. 直线area扫描

`sub_38999` 已映射为 `apply_line_attack_area`。方向0/1/2/3对应上/右/左/下，逐格扫描到select distance；越界只跳过当前格，不终止循环。友军格跳过，空格写effect，敌方格始终调用HP伤害而不读取hurt_type，命中后仍继续后续距离；非法方向无操作。

固定向量：方向3下方空格+敌方hash `0xae7c1e4e161ac125`、damage29；中间友军跳过后仍命中第二格hash `0xab559939923b4f74`；非法方向空hash `0xb9d103fd6854a325`且不消费seed1。BattleSession现实际绘制方向提示、消费方向键、复用首击方向/缓存范围，并执行伤害与动画提交；提示整帧hash为`0x5e46c805f42677b0`。函数完整状态边界为 `implemented_pending_review`。

## 13. 攻击动画时间线

`word_55ACE` 53项effect帧数表已由IDA data address `0x55ACE` 映射到原 `Z.DAT` 文件偏移324,814；表字节SHA256 `4c684877da272f4222fd3b73595971c81599ecfd456644498ac97841050da3a4`，word FNV `0xc35d41f03731784a`。RANGER 93条magic使用effect id 2..52。

`sub_3859E` 的actor sprite、effect frame/visible、channel1/2 sample分派点与每帧17 tick已映射为 `magic_animation_plan`；固定19帧hash `0x5aaffbb1d5697a73`。`sub_3884A` 的固定sample13、100 tick前奏与17帧effect2时间线hash为 `0x2b5c87d8e0c754d5`。`sub_38910` 的10个damage phase、每帧1 tick与前4帧flash hash为 `0x364953a2c8f42144`，抑制flash时为 `0xec7a73890ce825c4`。

三项typed时间线均已实现。玩家攻击、用毒、解毒、医疗现已把`sub_3859E/sub_38910`接入BattleSession：按动态FIGHT基址8000与EFT基址6500实际载入/绘制，分派attack/effect双bank音效，每帧present后等待一次BIOS tick变化，并在十帧damage后清显示状态。三条支持动作真实battle4向量分别执行11/9/10帧magic与10帧damage；用毒仅前4帧flash，解毒/医疗全部抑制。攻击每击固定10帧magic与10帧damage、前4帧flash，双击完整重复两组并在每组后执行独立提交等待；AI自动攻击、用毒、医疗和解毒也已接入同一逐帧链并使用独立AI相位日志，移动后医疗另锁定距离6/range2的四步continuation。玩家暗器现把`sub_3884A/sub_38910`接入BattleSession：sample13、100前奏、effect sample、11帧EFT及10帧damage均实际render/present/tick。AI暗器也执行独立AI owner的sample13前奏、effect sample、11帧EFT、10帧damage及动画后来源槽提交；直接前奏/首EFT/首damage和移动后首EFT hash为`0x49aac6569a28fe89`、`0xc65b523bd75389e2`、`0x335fd35ea7e3f367`、`0x16a8f10ce319622b`。`sub_3859E/sub_3884A/sub_38910`推进为`implemented_pending_review`。

## 14. 武功选择菜单

`sub_38DAC` 已映射并执行为 `begin_magic_selection/apply_magic_selection` 与 `BattleSession`独立选择相位。可用mask扫描全部10槽，只接受magic id>0且当前MP≥need_mp；cursor是可用项ordinal，左右在可用数内回绕，确认再次扫描10槽映射到实际slot，取消写out flag1并回到原动作ordinal。Enter、Space、keypad Insert均为确认键。

每轮先重画战场，再以 `(20,10,90,17*learned_count+10)` 绘制圆角面板。普通名称颜色`0x2321`、选中名称颜色`0x6663`；Big5名称按长度居中到x=57/49/41/33/25，y=`17*ordinal+15`。普通名称只扫描slot `<learned_count`，但可用mask/确认扫描10槽，故稀疏槽位存在原显示BUG；选中名称仍按实际slot单独重绘。

固定slots `[6,0,5,0,7,...]`、MP6得到learned3、available `[0,2]`、状态hash `0xc254d2cd83d7da76`；next→next→previous后确认slot2，取消flag1。Session稀疏slots `[5,0,6,...]` 固定初始菜单FNV64 `0x909332be9671b27c`、cursor1/实际slot2选中菜单 `0x6977ba7a0c3172a6`，并锁定ready/cursor/cancel/selected日志。Linux Debug完整BUILD 14/14，函数推进为 `implemented_pending_review`。

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

`sub_33C4D/sub_33E93/sub_340D9` 已恢复低HP、已中毒和低MP候选。三者均先保留自身能力的strict门槛，再按side使用队伍200库存或敌方4个随身物品，首个命中即返回。中毒selector保留原字段不一致：队伍解毒物品检查item word56为负，敌方随身物品检查word47为负。仍无物品时，低HP/中毒selector可写请求医疗8或请求解毒9并指定首个符合能力门槛的未隐藏同伴。

`sub_341F6/sub_343DA` 已恢复医疗/解毒目标选择：按combatant槽序扫描同side未隐藏目标，请求动作优先；其后分别按HP或poison四档短路。固定HP24/100和poison35均以seed1消费三次`bounded(10)`得到`[8,8,3]`、终态662824084，再选择slot1；早档命中不得消费后续RNG。

`sub_34550` 已恢复攻势selector。双方`HP+attack`按int16逐次回绕累计且不跳过隐藏槽；危险条件成立时选择缺HP或中毒值最大的同伴。否则固定先执行用毒的`bounded(50)`，条件成立才继续`bounded(150)`；随后按side使用不同暗器强度和RNG门槛。无暗器后，体力严格大于10且当前MP不少于十槽非零武功的最小need_mp才返回攻击2。原函数对攻击2只返回而不写word10，动作3/4/5/10才写入。六个selector均为 `implemented_pending_review`。

## 21. AI入口typed同步合同

`sub_33599`当前机器身份固定为1716 bytes、440条指令、64条分支、78处重定位与39个direct call；raw/loaded SHA256分别为`fe66621722777e5ada2987c67e39b5a56dc275feacc48e53684467ad4bbdeb68`和`d5a59d8b4e7fd757b088169c19223937575ea33ac11fcfb26676bee7f3ce77aa`。前置12项dispatch表hash为`4ff226b5a41d10d4d7b96fd71c4072ef0b3fef190c91194134e725ce8be6b8f0`，动作0与7共享rest入口。

入口按actor side对全部combatant的attack后HP逐项int16回绕累计，并在绘制前冻结双方总值与人数；随后只执行一次战场/status重绘和present，再按参数300等待八次BIOS tick变化，等待期不重绘。selector按低HP、中毒、低MP、医疗队友、解毒队友、逃跑、攻势顺序；低HP四档RNG阈值为3/5/7/9，中毒入口无条件消费一次`bounded(10)`并与signed `poison/10`比较，低MP四档为2/4/6/8，医疗/解毒20/40/60分别比较4/6/8且80无RNG兜底，逃跑先比较5再按HP的1/4和1/5档比较6/8。保留体力<10先置等待7、随后可被低HP selector返回0清除的原顺序BUG。

动作0/7共享休息handler，其余1..6、8..11逐项映射移动、攻击、用毒、解毒、医疗、物品、请求医疗、请求解毒、暗器和逃跑；所有typed continuation语义完成后才写word7并推进actor。逐块重审修正了两项共享时序差异：`ai_wait`原会重复重画，现冻结已present缓冲；攻势selector原会在延迟后重算双方值，现显式读取入口保存的prelude。等待期把可重算总值从330/220改为1530/620后仍按冻结值选择攻击2而非医疗5，四次RNG输出`[8,8,3,15]`、终态3295386429；正式golden双生成逐字节一致，SHA256为`f80e016364dc2512ae6a3294e9e1c72e4080f5a6afcae82d30d33954d46bf691`。两次修正均作废当轮结论，第三轮从入口覆盖全部机器范围、两个caller、RNG、12路dispatch及统一尾后零新增差异，故`sub_33599`归类`platform_adapted / converged_no_new_differences`；各handler内部仍按owner独立待审，不传播closure。

## 22. AI休息与逃跑目的格

动作0/7的`sub_34AD3`仅做遗留栈清理并直接调用`sub_3A8A4`，现代完整映射到`rest_actor`，无额外状态，标为 `implemented_pending_review`。

动作11的`sub_34AEC`先以actor坐标建立movement图，再只扫描路径值恰等于round value的格。扫描严格为x外层、y内层；候选分数是与全部不同side combatant的曼哈顿距离和，不跳过隐藏或死亡槽；仅strict更大才替换，故同分保留早格。真实field2 synthetic向量source `(10,20)`、round value 3、敌方 `(13,23)/(14,24)`选择`(7,20)`，最大和20。入口参数0表示移动后休息，物品wrapper传1表示只重定位。`BattleSession`现对两种入口均实际逐格移动、render/present并在每格等待两次BIOS tick变化；参数0随后休息并完成actor，参数1恢复物品typed计划而不休息。真实battle2逃跑从`(30,24)`三格移动到`(31,22)`并验证局部动作11不写word10；该函数推进为 `implemented_pending_review`。

## 23. 自动攻击目标策略

`sub_3505B`按morality与IQ调度四种目标策略。morality>=75、morality<=25、IQ>=70分支各自在条件成立时消费一次`bounded(10)`并要求结果<7；命中后分别调用最高攻击、最低攻击和专长selector，三者均未命中才无RNG选择最近目标。一旦策略门槛命中，即使selector没有写目标也不回退。seed9输出2、终态1341714958；morality75与IQ70在seed1下依次失败`[8,8]`后选最近目标，终态2524885223。

`sub_3513A/sub_351A7`只扫描不同side且未隐藏目标，分别以best 0/1000选择signed attack严格最大/最小值，同值保留早槽；最高攻击策略面对`[0,0]`不写word11，顶层仍停止。`sub_35372`使用targeting图的strict最短距离，真实field2距离`[6,8]`选slot3且无RNG。

`sub_35217`保留独立flag BUG：发现同side任意use_poison>20后先找detoxification最大敌人；即使该值达到20并暂写目标，medicine达到flag仍为0，函数尾仍调用最低攻击selector覆盖它。固定detox`[30,0]`先暂选slot3，最终按attack`[50,10]`改为slot4。五个函数均为 `implemented_pending_review`。

## 24. 自动攻击主handler计划

`sub_34C47`先统计正magic id并用`bounded(count)`直接选槽，再执行目标策略，因此两次RNG顺序不可互换。seed9、两个已学槽先得magic槽0，再得目标roll5，终态2878571567。特殊攻击表从Z.DAT file offset324920直接解析；首列是角色第一装备槽word23，不是角色ID，七组装备/武功/加成为`106/57/100`、`107/49/50`、`108/49/50`、`110/54/80`、`115/63/50`、`116/67/70`、`119/68/100`。

area type0/3在targeting距离不大于select distance时命中并传movement mode1；type1/2还必须同x或同y并传mode2；其他type永不直接命中并传mode0。初次命中调用automatic flag1攻击；未命中且round value<=0直接结束而不休息，否则移动。移动后先复检原目标，仍失败才强制改选最近目标；二次命中则攻击，否则休息。固定field2初始距离为6/8，相邻重选距离1。

现代`begin_ai_attack_plan/resume_ai_attack_after_move`已恢复全部typed决策及尾部action_done要求；`BattleSession`按area type传mode1/2或其他type的mode0，逐格执行移动状态、render/present与两次BIOS tick变化，移动后复检/重选，恢复结果为rest时实际休息。命中时现按automatic flag1自动决定方向，执行对应area伤害、10帧FIGHT/EFT、双bank音效、10帧damage、sprite刷新、重画/present/wait17、熟练度/MP提交、升级框wait500和体力尾部，再由AI外层写action-done并推进actor；直接命中与mode1移动一格后命中均有Session回归。因此`sub_34C47`推进为 `implemented_pending_review`。目标selector未写slot时现代安全拒绝原版slot -1线性越界读取，留待最终REVIEW。

## 25. AI用毒handler计划

`sub_355FF`仅在actor IQ严格大于60时消费一次`bounded(10)`；结果<7先选合格目标中signed attack严格最大者，否则及最大selector未写目标时回退`sub_3570F`。合格条件为敌对、未隐藏、poison<95且target anti_poison严格小于actor use_poison。最高攻击best从0开始，同值保留早槽，所有attack<=0时不写word12。

`sub_3570F`并非最近目标：循环对每个合格候选都重复建立targeting图，再错误读取陈旧全局目标slot的距离。固定stale slot4距离8、候选真实距离`[6,8]`时，两次比较值相同，strict更新仅让slot3首个合格目标写入。无合格候选时原版不读取stale slot。seed9输出2、终态1341714958；IQ恰60无RNG。

`sub_3540E`的射程为signed use_poison除15加1。round value恰0且在射程首次检查直接用毒；round value>0时即使已在射程仍请求movement mode3；负值跳过移动并重复建立targeting图后在第二次检查用毒。移动后只复检原目标，不重选。仍超距时比较`2*target attack`与wrapped int16己方`attack+HP`总和的`2*total/count`，strict更大回退自动攻击，否则休息。固定total330/count3阈值220，target attack50休息、200攻击。

现代已恢复三个selector及typed handler计划；`BattleSession`对move分支按mode3逐格执行移动状态、render/present与两次BIOS tick变化，随后复检原目标。命中时调用共享用毒状态核心，执行11帧effect30、双bank音效、10帧damage与共享提交，再由AI外层推进actor；无合法目标或强敌条件命中时重新进入完整自动攻击，rest分支仍按原休息。真实battle4 seed2直接向量锁定目标poison0→25、体力100→98、word13加1、最终RNG 2993822286，首magic/damage整帧hash为`0x47286fa4af30fce4`/`0xd76de7fa195a1ac3`；另一路固定poison95无合法目标，保留动作码3并进入attack effect sample0。因此`sub_3540E`推进为`implemented_pending_review`。

## 26. AI物品与暗器handler计划

`sub_35803`先调用`sub_34AEC(actor,1)`，复用逃跑最远格算法但不在移动后休息；随后无条件调用`sub_3598C(actor,0)`。固定field2 source `(10,20)`、round value3时目的格`(7,20)`、敌方曼哈顿和20。party side0的全局item slot索引200槽inventory，非零side索引角色4槽taking-item，handler不先检查数量。现代将目的格move与移动后mode0使用拆为两个同步步骤。

`sub_3582B`先调用攻击目标策略，再按actor hidden_weapon signed除15加1计算射程；首次targeting距离命中即调用`sub_3598C(actor,1)`，超距且round value>0请求movement mode1，移动后只复检同一目标，仍超距回退`sub_34C47`。round value<=0跳过移动但重复第二次targeting检查。hidden_weapon80得射程6；最近slot3距离6立即使用且无RNG；morality75、seed9输出2选slot4距离8，移动前1次检查，未改变位置后累计2次并回退攻击，移动到`(13,23)`后距离2则使用。最高攻击门槛命中但attack均为0时不写word11，原版继续使用合法stale target。

`sub_36133`在敌方携带物品数量耗尽后，从指定slot起同步左移后续item ID和数量并清空第4槽；`[5,6,7,8]/[1,2,3,4]`删除slot1严格得到`[5,7,8,-1]/[1,3,4,0]`，已完整映射为`remove_carried_item_slot`。

`sub_3598C`的AI mode1暗器状态与手动`sub_3A30B`并不共用毒值公式：item add_poison非负时直接加到目标poison，负值才算`(add_poison-hidden_weapon)/2`，两者都不读取anti_poison且不追加RNG。真实item102在seed1下伤害21、hurt40→45、HP100→79、poison10→50，仅消费一次RNG；无毒item96在seed2下伤害16、poison10保持10，也仅消费一次RNG。队伍方耗尽200槽inventory后左移，敌方耗尽4槽taking-item后调用`sub_36133`；AI分支不改actor方向且不提前写action_done。

`sub_2B483`共享物品效果已恢复23项状态数组：HP正负分支各自保留严格第二次RNG短路，毒值正负公式、HP/MP/体力/上限夹取和13项能力signed相加均按机器码执行；add_morality与add_attack_twice只进入显示数组却不写角色字段的BUG保留。真实item19在seed1下得到HP100→200、hurt40→0、poison50→0、体力30→100、MP10→100，三次RNG终态662824084。typed结果锁定battle重绘、`(70,18,148,20*n+30)`效果面板、等待输入及调用者固定9次tick等待。

现代现完整执行两个handler：AI mode0先按逃跑算法逐格重定位，再应用共享物品状态、实际绘制效果面板、present、任意键确认、等待九次BIOS tick变化并延后提交队伍inventory或敌方carried slot；AI mode1保持原目标做首次射程、可选逐格移动与同目标二次检查，命中时执行sample13前奏、EFT、damage与动画后来源提交，仍超距时不消费暗器并调用完整`sub_34C47`自动攻击入口。固定普通物品面板、暗器前奏/首EFT/首damage、移动后首EFT hash为`0xa7542240e4172664`、`0x49aac6569a28fe89`、`0xc65b523bd75389e2`、`0x335fd35ea7e3f367`、`0x16a8f10ce319622b`；外层仅在handler完成后写action_done。三个函数均推进为`implemented_pending_review`。

`sub_361AC`请求医疗与`sub_36209`请求解毒实际共享同一函数体；后者仅建立相同栈帧后跳入前者。actor行动值严格大于0时先调用`sub_3650E(actor, mode0, value0)`，零或负值跳过；随后无条件从请求目标槽恢复x/y并调用`sub_34C47`自动攻击。现代Session保留move→automatic_attack同步边界、两项零参数和请求目标恢复，现已执行mode0逐格render/present/tick、完整自动攻击及外层完成。真实battle2正行动值请求医疗向量以round value1执行mode0一格移动并恢复请求目标，保留动作码8，首magic整帧hash`0xcc6a249ebb919a23`、最终RNG3295386429、MP20→15、体力100→99→96；两函数均推进为`implemented_pending_review`。

`sub_36210` AI医疗与`sub_363AC` AI解毒使用各自ability signed除15加1为射程；首轮目标图命中即执行动作，超距且行动值严格正时调用`sub_3650E(actor,mode1,value=range)`，移动后恢复同一目标再建图。零/负行动值虽然不移动，仍执行第二次建图。第二次仍超距时仅当`2*actor.attack`严格大于`2*wrapped_allied_total/allied_count`才自动攻击，否则休息。完整callee汇编确认IDIV余数和医疗分支两次`sub_3F50B`返回值均无行为效果。现代Session已执行直接与移动后支持状态、FIGHT/EFT双bank、无flash damage及AI外层完成；直接医疗/解毒首magic分别为`0xbec9ef2738ca79b4`/`0xae0f13fbbc4c8083`，医疗距离6/range2移动4格后首magic为`0x2138cfdf8041c6bb`。攻击/休息回退进入完整共享continuation；两函数均推进为`implemented_pending_review`。

`sub_3650E`已恢复完整目的格状态逻辑：mode2在本回合可入射程时按range向下找轴向层，mode3无轴向限制，其他分支从目标周围按上、右、左、下找同轴可达格、任意可达格或逐轴退向actor。目的格须连续通过当前图和重建actor movement图两次严格`path<128`检查，随后按原tie-break标最短路。battle4固定mode0/1 `(26,25)`、mode2 `(23,26)`、mode3 `(25,24)`。`advance_ai_movement`已实际逐格提交path255、occupancy、坐标、方向、sprite、体力和行动值，并恢复mode0/3行动值停止、mode1距离停止和mode2轴向停止；`BattleSession`已对mode0..3统一执行每格render/present及参数40对应的两次BIOS tick变化，并恢复调用方typed计划，该函数推进为`implemented_pending_review`。

`sub_36AF7`通用玩家光标选择已恢复为typed状态：入口按mode建立movement或targeting图，方向优先级下、右、左、上；相邻path不大于上限即可移动，path超限但occupancy非空仍可悬停；movement确认严格要求`0<path<=limit`，targeting允许`0<=path<=limit`，Escape清path上限并写取消结果。`sub_36A98`以actor行动值启动mode0选择，取消返回-1，否则复制选择期路径图、标最短路并逐格移动。battle4已实际接入movement与targeting的四方向/Escape/三确认键和每轮路径/光标重画present；movement继续逐格重画present与两次BIOS tick变化，targeting现由玩家用毒/解毒/医疗按各自能力射程进入，取消保持原菜单ordinal，确认敌方占位格`(26,26)`并保存目标。攻击与暗器在各自武功/物品选择接入后复用同一相位；`b8-battle-targeting.log`保留三入口、取消、逐格光标和最终确认的完整可读轨迹，Linux与Windows Debug完整BUILD全部14项测试及逆向validator通过；本函数推进为`implemented_pending_review`。

`sub_3B387..sub_3C2AC`战后进度状态与同步UI均已恢复：敌方满HP/MP、体力100并清内伤/中毒；胜利经验均分、队伍HP/体力下限、角色/练功/制造经验封顶，以及等级、练功、武功升级、制造RNG/状态均保持原顺序。`BattleSession`依次执行经验固定框、升级固定框、练功动态框、武功等级动态框和制造固定框，每项重画战场、present并等待任意非零键。经验在其消息前提交；升级和练功以副本/RNG副本预演，按键后才执行真实提交；制造配方选择在消息前消费共享RNG，库存及数量RNG在按键后提交，保留原同步可观察边界；五帧FNV64依次为`0xa699bf683f037936`、`0xef2c8987fe26a127`、`0xdd4c7e74171e8ee5`、`0x0f4783440328986e`、`0xb980de17004d5b6c`。四函数均推进为`implemented_pending_review`。

`sub_3C563`回合异常状态更新保留`hurt>0`优先分支、poison的HP/体力/hidden门槛、两次有符号除法，以及HP/体力仅严格负值夹1；`sub_3C672`对0..25槽（含当前活动数之外）仅在目标hidden严格等于1时清word11/12。`sub_3C6D3`三个机器码xref均已覆盖：玩家菜单内两处由同一菜单重绘相位执行，AI prelude调用由prelude/wait相位执行；独立面板oracle为`0x630a82d57e1d8715`，Session AI prelude为`0xb02104139829a80d`。三函数均为`implemented_pending_review`。

## 16. B8 实现差异审计关闭

B9前的B8入口实现审计从`sub_31C75`机器顺序发现并修正五处差异：首帧present后才排battle music；手选确认后才加载WDX/WMP/EFT；WAR/WARFLD成功读取后清occupancy且不重复清空；首帧present后、fade前才首次速度排序；首帧继承跨battle保留的完整render globals，present后才按排序后slot0重定位。对应回归锁定延迟battle资源加载、首帧插入顺序、present后排序、继承视角与独立`view=(0,0)`首帧hash。

最终源码通过Linux与Windows的core/app × Debug/Release完整8项BUILD矩阵；Linux/Windows Debug当前9个同名B8 hash逐字节一致，独立battle golden双生成逐字节一致。81项closure仍全部为`implemented_pending_review`，本节只关闭B8实现差异审计；B0→B9统一最终双向REVIEW尚未开始，不产生`assembly_exact`结论。
