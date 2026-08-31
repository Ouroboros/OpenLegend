# B8 战斗 1:1 证据

状态：`pending_implementation`；最终汇编↔C++ REVIEW 尚未开始。

## 1. 物理范围与闭包

`sub_31C75 @ 0x31C75` 是 scene 与五轮试炼调用的 battle 入口。其后连续 battle 实现区间截止 `sub_3C6D3 @ 0x3C6D3..0x3CBE3`；`research/ida/reports/Z_DAT.b8_battle_xrefs.txt` 由当前 `Z_DAT.i64` 和 `idat.exe -A` headless 生成，枚举81个 FUNCTION 记录，报告规范为 LF，SHA256 为 `179b85c68ad87d03f175f7b22ff9af7ffbae68aed758eeaa0f0fe692ab67d488`。

`research/inventory/battle-closure.tsv` 以该报告为机械真值，当前0项为 `pending_mapping`、39项为 `pending_implementation`、42项为 `implemented_pending_review`。battle 区间调用到的 resource/render/input/time/random/audio 入口是共享 owner 边界，不随递归调用图吞入 battle closure。

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

`SceneStepResult`现携带battle id与get-exp word；`LegacyGameRuntime`在opcode6请求后建立并拥有`BattleSession`，切换battle view，实际驱动render、present完成回调、翻译键盘输入和轮首advance。Session保留get-exp布尔值，并对初始化失败、选择变化/完成、初始视图、淡入结束、轮首actor和player/AI分派记录关键路径日志。动作循环、资源/音频收尾及Victory/Defeat/Escape回送scene仍未实现，因此`sub_2DE03/sub_31C75`继续保持`pending_implementation`。

## 3. 资产 oracle

`research/tools/generate_b8_battle_goldens.py` 只读取原版字节，不链接 OpenLegend C++；双生成逐字节一致。正式 `research/evidence/battle-goldens.json` SHA256 为 `f84e51e8f3887a207e0ae107eb5adb363a5cf2f24489884928fb64f9e95437f4`。

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

`sub_31DA0 @ 0x31DA0..0x31EB9` 已映射为 `openlegend::battle::BattleData`：按 `battle_id*186` 解码93个 signed word，以 definition word6 选择 WARFLD cumulative archive entry，只取前16,384字节为8,192个战场 word，并把4,096个 occupancy word 清成-1。真实 battle 0..139 全部通过；-1和140由现代安全适配拒绝；battle 0/4/93/139 的记录和战场哈希固定。Linux Debug BUILD 14/14。

该函数只标 `implemented_pending_review`；IDX cache 的重载时机和错误路径清空次序留待总 REVIEW，未标 `assembly_exact`。

## 6. 参战者建立单元

`sub_31EB9 @ 0x31EB9..0x3265C`、`sub_3265C @ 0x3265C..0x3271E` 与紧邻 helper `sub_3B1E6 @ 0x3B1E6..0x3B238` 已从入口到返回逐基本块恢复，详见对应函数证据：

- 参战者池严格为26槽×14 signed word，每槽28字节；初始化 word0/1/11/12 为-1，其余除 word8外为0；
- 固定队伍看 WAR words15..20，一旦任一非-1便完全跳过预置队伍和选择 UI；否则先建 WAR words9..14，再允许队伍前缀中的非 mandatory 成员切换0/1；
- 队伍写 side word=0、word4=2；敌方写 side word=1、word4=1；每次插入重算 word8、写 occupancy，再按16位递增 count；
- occupancy 以 `y*64+x` 寻址，无范围、重复或容量检查；战斗93证明重复格必须后写覆盖；
- `sub_3B1E6` 返回 `int16(8*role.word1 + word_556D4 + 2*word_556CC + 2*combatant.word4)`；空槽初始化会以 role=-1 对角色表前182字节读取。

`BattleSetup` 已实现26槽完整初值、固定/预置队伍、host-neutral cursor/0·1·2选择状态、按当前 count 取坐标追加、敌方建立、sprite word 和后写 occupancy 覆盖。`BattleSession`已实际绘制圆角混色选择框、原Big5标题/确认文字、角色名和星号，逐键执行上下回绕与确认，并在每次选择重绘前恢复冻结背景。真实 battle0 固定手选顺序、battle4 固定队伍、battle93 slot9→slot11覆盖及全140条记录均通过。

battle2队伍角色0/2得到初态`[2,0]`，确认后按原顺序得到队伍`[0,1,2]`并追加敌方4；选择菜单与初始战场的独立Python oracle/C++ FNV64分别一致。runtime已实际驱动Session render/present/input，因此`sub_31EB9/sub_3265C/sub_3B1E6`均推进为`implemented_pending_review`。

## 7. 回合排序、玩家菜单与胜负核心

`sub_3271E/sub_32A51/sub_32B78/sub_32E59/sub_3B238` 已从入口到返回完整恢复。`BattleSetup` 现实现：

- `int16(role.speed + 两件已装备物品的 add_speed word53)` signed降序，等值不交换；
- 交换时按原顺序复制 word0..7/9..13、逐槽写 occupancy、最后重算两槽 word8；
- 每轮 word6=`max(0,effective_speed/15-role.hurt/40)`，signed division toward zero；
- hp<=0且未 hidden 的槽清 occupancy并写 word5=1；无队伍为 raw1/`戰鬥失敗`，无敌方为 raw2/`戰鬥勝利`，双方皆空由 raw2覆盖。

`BattleSession`现已在建队后按slot0计算clamp视图原点、实际绘制初始战场、逐present帧执行黑场淡入，随后完成轮首排序/word6计算、actor居中呈现及player/AI动作分界。玩家分支按原条件建立「移動、攻擊、用毒、解毒、醫療、物品、等待、狀態、休息、自動」十项0/1表，保留无武功时最低耗内哨兵1000；cursor严格是可用项ordinal，上下回绕，Enter/Space/keypad Insert确认后再扫描映射原action id。菜单每帧重绘战场、圆角混色框、原Big5文字和右侧actor状态面板；十项全可用、cursor0时独立Python oracle与C++整帧FNV64均为`0x7d062c289e7f933a`。

等待动作现实际把当前actor逐槽交换到队尾，并保持外层索引继续处理交换后占据同槽的actor；休息实际提交原RNG体力/HP/MP恢复和action-done后推进下一槽。两项完成后均按原顺序执行胜负检查、26槽隐藏目标清理、隐藏槽跳过和下一actor居中present；每个actor present后再清word7/word10。最后一槽后调用轮末异常状态，并仅在本轮开始时捕获的BIOS tick发生变化后开始下一轮。

玩家移动现实际执行movement路径图光标、四方向翻译键、Escape/三确认键、路径范围与主光标重画/present；确认后清路径上限、标记最短路并逐格提交状态，每格重画/present后按参数40等待两次BIOS tick变化。返回菜单时仅重检移动项，不重算其余九项。battle4 Session覆盖取消、`(26,24)→(26,25)→(25,25)`、word6 2→1→0及移动项失效；`sub_36A98`推进为`implemented_pending_review`，通用`sub_36AF7/sub_37355`仍因targeting/AI路径未全部接入而保持`pending_implementation`。

自动动作实际重画并present时flag仍为0，present完成回调后才置flag并进入同actor AI；随后实际累计双方int16态势、第二次重绘/present、按原延迟参数300等待八次BIOS tick变化，再执行严格selector。动作0/7的休息handler已完成并进入逐actor后处理，固定seed1连续两个角色体力增量为5、4。`sub_32A51/sub_32B78/sub_36A98` 已为 `implemented_pending_review`；其余六个玩家handler、targeting、十类AI handler、结果panel/战后提交尚未实现，所以 `sub_3271E/sub_32E59/sub_33599/sub_3B238` 保持 `pending_implementation`。

## 8. 战场路径图与最短路回溯

`sub_36E06..sub_37245` 的十个 path 单元已映射为 `BattlePathing`：

- movement 图把 upper layer非0、occupancy非-1或 ground tile 命中9段 IDA 常量的格写555，其余写254；targeting 图只检查 upper layer；source 随后强制写0；
- 原255槽环形队列保留 `(0,-1)` sentinel、distance `%128` 和上→右→左→下扩展顺序，没有替换为无界 queue；
- 回溯从 target 开始写250，每层用 `(distance+127)%128` 并按同一方向顺序选择首个前驱；消费标记255由后续移动单元使用；
- `sub_37070` 的坐标比较允许64，现代仅保留线性 index仍在0..4095的别名，index>=4096安全拒绝；不可达回溯返回false而不进入原死循环。

独立 oracle 固定 battle0/93 空 occupancy、单格占位、target距离14/22、回溯前后完整 FNV-1a 与首步 `(31,20)/(33,29)`；Linux Debug 14/14。十项均为 `implemented_pending_review`。

`sub_37355` 另以每次一个同步边界实现逐格核心：旧 path=255、occupancy 搬移、x/y、方向、sprite、条件体力 DEC、行动值 DEC依机器顺序写入；destination、Manhattan range、aligned range和行动值耗尽停止规则也已映射。连续左移两格固定 direction2、sprite5110、physical power 1→0、round 5→4→3。玩家路径已实际执行每格视图更新、render/present与参数40对应的两次BIOS tick变化等待；AI mode0..3仍未接入同步呈现，因此函数保持 `pending_implementation`。

## 9. 武功攻击入口与每轮提交

`sub_37734` 已恢复十槽统计与 magic profile：只统计 ID>0；唯一已学武功时原版仍选 slot0 的 BUG 保留；等级用熟练度 unsigned `/100`，并从 magic words 28+level、38+level、15、14、16读取选择距离、攻击距离、area type、hurt type和 need_mp。attack_twice严格等于1时轮数为2。

现代每轮提交严格保留 action word7=1、word13加2、`LegacyRandom::bounded(2)+1` 熟练度增长、unsigned 999 cap、跨百升级判定，以及 `(cost_scale/2)*need_mp` 内力扣除后 signed负值夹0；全部轮次后体力减3并夹0。seed1固定 state1103527590、299→300、cost scale3下 mp3→0、999 cap和体力2→0，Linux Debug 14/14。area生成、目标UI、动画/present/升级框仍待实现，所以 `sub_37734` 保持 `pending_implementation`。

## 10. HP与MP伤害

`sub_39188` 已映射为 `apply_hp_damage`：扫描双方knowledge>80的存活可见参战者，按最高可支付层计算cost scale，合并角色攻防、装备、特殊加成和双方知识；保留两次bounded(20)、非正时两次bounded(4)、距离1..10线性衰减和>10固定2/3。固定damage30下，HP30恰好归零不发击杀奖励，而HP29下溢才加`10*level`；并按原顺序写hurt与poison。

`sub_395EC` 已映射为 `apply_mp_damage`：严格执行bounds `[3,3,add_mp/2,3,3]`，攻击者增加当前/最大内力并封顶，目标扣hurt_mp与随机差。seed1向量输出 `[2,1,3,1,1]`、state4182499122，攻击者10/20→23/23、目标50→35、返回15。两函数Linux Debug 14/14，均为 `implemented_pending_review`。

## 11. 武功area扫描

`sub_37734` 的方形与十字单轮状态边界已映射为 `clear_attack_effects/apply_attack_area`。方形按x外/y内扫描并按目标差更新方向；十字逐距离按上、下、左、右扫描且不改方向。友军格完全跳过，空格只写effect=1，敌方格写damage word9。type2忽略hurt_type并强制HP伤害；type0/3按hurt_type 0/1分派HP/MP，其他值只留effect。

固定方形8格hash `0xe5f47b0a810ce2bd`、十字7格hash `0x3144c415023d9464`、单格MP hash `0xab559939923b4f74`；伤害29/29/15，effect kind 1/1/3，MP分支保留上次HP cost scale3。目标选择、type1方向选择接线、攻击动画与present仍待实现，故 `sub_37734` 继续 `pending_implementation`。

## 12. 直线area扫描

`sub_38999` 已映射为 `apply_line_attack_area`。方向0/1/2/3对应上/右/左/下，逐格扫描到select distance；越界只跳过当前格，不终止循环。友军格跳过，空格写effect，敌方格始终调用HP伤害而不读取hurt_type，命中后仍继续后续距离；非法方向无操作。

固定向量：方向3下方空格+敌方hash `0xae7c1e4e161ac125`、damage29；中间友军跳过后仍命中第二格hash `0xab559939923b4f74`；非法方向空hash `0xb9d103fd6854a325`且不消费seed1。函数完整状态边界为 `implemented_pending_review`。

## 13. 攻击动画时间线

`word_55ACE` 53项effect帧数表已由IDA data address `0x55ACE` 映射到原 `Z.DAT` 文件偏移324,814；表字节SHA256 `4c684877da272f4222fd3b73595971c81599ecfd456644498ac97841050da3a4`，word FNV `0xc35d41f03731784a`。RANGER 93条magic使用effect id 2..52。

`sub_3859E` 的actor sprite、effect frame/visible、channel1/2 sample分派点与每帧17 tick已映射为 `magic_animation_plan`；固定19帧hash `0x5aaffbb1d5697a73`。`sub_3884A` 的固定sample13、100 tick前奏与17帧effect2时间线hash为 `0x2b5c87d8e0c754d5`。`sub_38910` 的10个damage phase、每帧1 tick与前4帧flash hash为 `0x364953a2c8f42144`，抑制flash时为 `0xec7a73890ce825c4`。

三项typed时间线均已实现，但FIGHT载入、sample调用、render/present/wait和结束清理尚未接入BattleSession，因此三函数均为 `pending_implementation`。

## 14. 武功选择菜单状态

`sub_38DAC` 已映射为 `begin_magic_selection/apply_magic_selection`。可用mask扫描全部10槽，只接受magic id>0且当前MP≥need_mp；cursor是可用项ordinal，左右在可用数内回绕，确认再次扫描10槽映射到实际slot，取消写out flag1。

调用者传入的a7是已学武功数。面板宽为 `17*a7+10`，普通名称绘制也只扫描slot `<a7`，但可用mask/确认扫描10槽，故稀疏槽位存在原显示BUG。固定slots `[6,0,5,0,7,...]`、MP6得到learned3、available `[0,2]`、状态hash `0xc254d2cd83d7da76`；next→next→previous后确认slot2，取消flag1。panel、名称与input/present continuation待接，函数为 `pending_implementation`。

## 15. 用毒目标与状态结算

`sub_39776` 的目标射程已映射为signed `use_poison/15+1`；原函数在目标选择out flag为1时返回-1，否则调用 `sub_397E5`。`sub_397E5` 按目标差更新方向，清4,096格effect；友军格完全跳过、空格只写effect、敌方格写effect kind2并调用 `sub_39A45`，随后重算全部sprite、置actor action_done、word13加1并将体力减2夹0。原函数错误地检查x<64两次而未检查y<64，现代实现对y>=64安全拒绝，登记待最终REVIEW。

`sub_39A45` 已映射为 `apply_poison_value`：signed `(use_poison-anti_poison)/4` 向零截断，先夹0..99，再按目标剩余容量 `99-poison` 限制；不消费RNG。固定use_poison80、anti_poison20、poison90得到raw15、实际9、目标99；射程6、方向3、effect hash `0xab559939923b4f74`、effect kind2、体力1→0、counter0→1，空格标记而友军不标记。`sub_39A45` 为 `implemented_pending_review`；`sub_39776/sub_397E5` 因目标UI与动画/render/present/wait未接入保持 `pending_implementation`。

## 16. 解毒目标与状态结算

`sub_39B1F` 的目标射程已映射为signed `detoxification/15+1`。`sub_39B8E` 与用毒使用同一方向和清effect顺序，但目标条件相反：敌方完全跳过，空格写effect，友方格写effect kind3并调用 `sub_39DA3`；随后执行effect36动画、抑制damage flash、重算sprite并跳入用毒的共享行动尾部。原函数同样重复检查x<64而未检查y<64，现代实现对y>=64安全拒绝。

`sub_39DA3` 已映射为 `apply_detox_value`：signed `detoxification/3` 后严格依次消费两次 `bounded(10)`，计算 `quotient+first-second`，夹0..99；目标毒值严格大于 `detoxification+20` 时归零，再受当前毒值限制。写回后仅poison<0夹0、poison>100夹99，故恰等于100保留。固定detoxification80、poison90、seed1得到RNG `[8,8]`、state2524885223、解毒26、目标64；射程6、方向3、effect hash `0xab559939923b4f74`、effect kind3、体力1→0、counter0→1。`sub_39DA3` 为 `implemented_pending_review`；`sub_39B1F/sub_39B8E` 因目标UI与动画/render/present/wait未接入保持 `pending_implementation`。

## 17. 医疗目标与状态核心

`sub_39E88` 的目标射程已映射为signed `medicine/15+1`。`sub_39EF7` 只对友方或空格写effect，敌方完全跳过；友方格调用 `sub_3A10C`，effect kind4，effect0动画并抑制damage flash，之后重算sprite并跳入共享行动尾部。原函数同样漏掉y<64上界检查，现代实现安全拒绝。

`sub_3A10C` 已映射为 `apply_medicine_value`：actor体力<50立即返回且不消费RNG；medicine负值夹0，按target hurt的`<=25/26..50/51..75/>75`四档取`4/5、3/4、2/3、1/2`基数，再消费一次 `bounded(5)`。hurt严格大于原medicine+20时治疗与减伤都归零；治疗受maximum_hp限制，hurt按完整非负medicine扣减。固定medicine80、hurt40、HP100/200、seed1得到RNG3、state1103527590、治疗63、HP163、hurt0；方向3、effect hash `0xab559939923b4f74`、kind4，公式内体力51→49，共享尾部49→47。当前只完成状态核心：`sub_3A10C` 为 `implemented_pending_review`，`sub_39E88/sub_39EF7` 的目标UI与动画/render/present/wait仍为 `pending_implementation`。

## 18. 战斗物品与休息状态核心

`sub_3A29C` 固定以参数4调用共享物品过滤，因此原菜单按库存顺序同时列出item type3和4，并且不检查数量；选择器返回4才进入暗器目标，返回1直接结束actor行动。现代 `begin_item_selection` 只恢复该typed槽表，物品格/详情/type3使用与同步返回仍未接入，因此保持 `pending_implementation`。

`sub_3A30B` 以signed `hidden_weapon/15+1`选择目标；友军不标记，空格只标effect，敌方才执行暗器effect、伤害、中毒、damage kind0动画、库存减一和行动结束。HP负增量先按target hurt的0、1..33、34..66、>66四档取item `add_hp`的`1/4、1/3、1/2、1`并减一次`bounded(5)`，再以`(值-2*hidden_weapon)/3`结算；hurt按负增量四分之一上升。正`add_poison`分支不消费额外RNG，非正分支严格再消费两次`bounded(5)`，所以无毒暗器仍可能随机改变poison。库存数量按int16减一，不大于0时把后续槽全部左移并清尾槽。当前只完成状态核心与动画参数返回；目标UI、effect/damage render/present/wait未接入，故保持 `pending_implementation`。

`sub_3A8A4` 已映射为 `rest_actor`：先结束行动，按round value是否等于signed `speed/10`执行`bounded(3)+3/+2`恢复体力并只夹上限100；更新后体力不少于30时，以`physical_power/10-2`为bound严格依次恢复HP与MP，各加`bounded(bound)+3`并只封顶。固定向量得到体力50→55、HP95→99、MP48→50；低体力向量25→29且不消费后两次RNG。该函数为 `implemented_pending_review`。

## 19. 等待、自动flag与战场绘制命令计划

`sub_3AA17` 已映射为 `defer_turn_to_end`：从当前slot开始连续调用原swap语义，把角色逐槽移动到combatant尾并返回最终slot，不直接写action-done。真实battle3完成队伍选择后，slot1的角色由`[0,101,102,103,104]`移动为`[0,102,103,104,101]`并返回4；该函数为 `implemented_pending_review`。

`sub_3AA4B` 的顺序为完整战场重绘、present、自动flag写1、调用当前actor的AI。现代仅恢复 `automatic_enabled` 状态与可供前置重绘使用的typed计划；present与AI尚未同步接线，因此该函数保持 `pending_implementation`。

`sub_3AA85` 已恢复为严格两次local-x外层/local-y内层的32×32命令计划：第一pass绘制WARFLD layer0；第二pass依次加入path overlay、主/副cursor、非0且非15000的layer1、normal或三种调色高亮角色、effect以及五种damage文字。path overlay与主cursor同受range严格大于0保护，secondary cursor由独立flag控制。普通sprite锚点为`145+18*(x-y), -81+9*(x+y)`；overlay左移18，damage再按offset上移。独立oracle以真实battle4资产和非对称view/cursor生成1,157条命令，哈希`0xb9f8a428699b3712`，C++逐字段复算一致；零range向量不产生cursor命令。`BattleRenderer`现按机器常量pointer基址0/6500/8000解析WDX/WMP、EFT与动态FIGHT，实际执行普通RLE、单色高亮、CLOUD第4/5帧alpha混色和damage字体；独立资产oracle与C++整帧FNV64均为`0x7d8a5211fe8c4eb0`。BattleSession已在初始战场、actor-present及玩家动作菜单每帧重绘中实际调用并由runtime present；移动、目标、动画和结果调用点仍未接入，故原绘制函数保持 `pending_implementation`。

## 20. AI六个候选selector

`sub_33C4D/sub_33E93/sub_340D9` 已恢复低HP、已中毒和低MP候选。三者均先保留自身能力的strict门槛，再按side使用队伍200库存或敌方4个随身物品，首个命中即返回。中毒selector保留原字段不一致：队伍解毒物品检查item word56为负，敌方随身物品检查word47为负。仍无物品时，低HP/中毒selector可写请求医疗8或请求解毒9并指定首个符合能力门槛的未隐藏同伴。

`sub_341F6/sub_343DA` 已恢复医疗/解毒目标选择：按combatant槽序扫描同side未隐藏目标，请求动作优先；其后分别按HP或poison四档短路。固定HP24/100和poison35均以seed1消费三次`bounded(10)`得到`[8,8,3]`、终态662824084，再选择slot1；早档命中不得消费后续RNG。

`sub_34550` 已恢复攻势selector。双方`HP+attack`按int16逐次回绕累计且不跳过隐藏槽；危险条件成立时选择缺HP或中毒值最大的同伴。否则固定先执行用毒的`bounded(50)`，条件成立才继续`bounded(150)`；随后按side使用不同暗器强度和RNG门槛。无暗器后，体力严格大于10且当前MP不少于十槽非零武功的最小need_mp才返回攻击2。原函数对攻击2只返回而不写word10，动作3/4/5/10才写入。六个selector均为 `implemented_pending_review`。

## 21. AI入口typed同步合同

`sub_33599`已拆成三个严格边界：`begin_ai_turn`在任何绘制前按actor side对全部combatant的`HP+attack`进行int16回绕累计，并要求后续依次重绘、present、等待300 tick；`choose_ai_turn_action`在该等待后按低HP、中毒、低MP、医疗队友、解毒队友、逃跑、攻势selector顺序执行；`finish_ai_turn`仅在对应动作handler返回后写combatant word7为1。

低HP四档RNG阈值为3/5/7/9，中毒入口无条件消费一次`bounded(10)`并与`poison/10`比较，低MP四档为2/4/6/8；医疗和解毒能力20/40/60分别比较4/6/8，80最终无RNG兜底；逃跑先比较5，再按HP的1/4和1/5档比较6/8。保留原顺序BUG：体力<10先选等待7，但低HP selector若被调用后返回0，会把等待清零并继续后续决策。固定seed1清零等待向量为`[8,8,13]`、终态662824084；seed10逃跑向量为`[3,4]`、终态1849040536。

动作0/7共享休息handler，其余1..6、8..11逐项映射移动、攻击、用毒、解毒、医疗、物品、请求医疗、请求解毒、暗器和逃跑。当前现代代码只生成typed handler并提供handler后最终写入；render/present/300 tick和11个实际handler尚未由BattleSession同步执行，故`sub_33599`保持 `pending_implementation`。

## 22. AI休息与逃跑目的格

动作0/7的`sub_34AD3`仅做遗留栈清理并直接调用`sub_3A8A4`，现代完整映射到`rest_actor`，无额外状态，标为 `implemented_pending_review`。

动作11的`sub_34AEC`先以actor坐标建立movement图，再只扫描路径值恰等于round value的格。扫描严格为x外层、y内层；候选分数是与全部不同side combatant的曼哈顿距离和，不跳过隐藏或死亡槽；仅strict更大才替换，故同分保留早格。真实field2 synthetic向量source `(10,20)`、round value 3、敌方 `(13,23)/(14,24)`选择`(7,20)`，最大和20。入口参数0表示移动后休息，物品wrapper传1表示只重定位。现代已生成typed目的格与条件休息计划，实际逐格移动/render/present/tick及休息调用尚未接线，故保持 `pending_implementation`。

## 23. 自动攻击目标策略

`sub_3505B`按morality与IQ调度四种目标策略。morality>=75、morality<=25、IQ>=70分支各自在条件成立时消费一次`bounded(10)`并要求结果<7；命中后分别调用最高攻击、最低攻击和专长selector，三者均未命中才无RNG选择最近目标。一旦策略门槛命中，即使selector没有写目标也不回退。seed9输出2、终态1341714958；morality75与IQ70在seed1下依次失败`[8,8]`后选最近目标，终态2524885223。

`sub_3513A/sub_351A7`只扫描不同side且未隐藏目标，分别以best 0/1000选择signed attack严格最大/最小值，同值保留早槽；最高攻击策略面对`[0,0]`不写word11，顶层仍停止。`sub_35372`使用targeting图的strict最短距离，真实field2距离`[6,8]`选slot3且无RNG。

`sub_35217`保留独立flag BUG：发现同side任意use_poison>20后先找detoxification最大敌人；即使该值达到20并暂写目标，medicine达到flag仍为0，函数尾仍调用最低攻击selector覆盖它。固定detox`[30,0]`先暂选slot3，最终按attack`[50,10]`改为slot4。五个函数均为 `implemented_pending_review`。

## 24. 自动攻击主handler计划

`sub_34C47`先统计正magic id并用`bounded(count)`直接选槽，再执行目标策略，因此两次RNG顺序不可互换。seed9、两个已学槽先得magic槽0，再得目标roll5，终态2878571567。特殊攻击表从Z.DAT file offset324920直接解析；首列是角色第一装备槽word23，不是角色ID，七组装备/武功/加成为`106/57/100`、`107/49/50`、`108/49/50`、`110/54/80`、`115/63/50`、`116/67/70`、`119/68/100`。

area type0/3在targeting距离不大于select distance时命中并传movement mode1；type1/2还必须同x或同y并传mode2；其他type永不直接命中并传mode0。初次命中调用automatic flag1攻击；未命中且round value<=0直接结束而不休息，否则移动。移动后先复检原目标，仍失败才强制改选最近目标；二次命中则攻击，否则休息。固定field2初始距离为6/8，相邻重选距离1。

现代`begin_ai_attack_plan/resume_ai_attack_after_move`已恢复全部typed决策及尾部action_done要求，但未实际执行`sub_3650E`逐格移动、`sub_37734`动画/伤害/攻击提交、休息及handler自身完成写入，故`sub_34C47`保持 `pending_implementation`。目标selector未写slot时现代安全拒绝原版slot -1线性越界读取，留待最终REVIEW。

## 25. AI用毒handler计划

`sub_355FF`仅在actor IQ严格大于60时消费一次`bounded(10)`；结果<7先选合格目标中signed attack严格最大者，否则及最大selector未写目标时回退`sub_3570F`。合格条件为敌对、未隐藏、poison<95且target anti_poison严格小于actor use_poison。最高攻击best从0开始，同值保留早槽，所有attack<=0时不写word12。

`sub_3570F`并非最近目标：循环对每个合格候选都重复建立targeting图，再错误读取陈旧全局目标slot的距离。固定stale slot4距离8、候选真实距离`[6,8]`时，两次比较值相同，strict更新仅让slot3首个合格目标写入。无合格候选时原版不读取stale slot。seed9输出2、终态1341714958；IQ恰60无RNG。

`sub_3540E`的射程为signed use_poison除15加1。round value恰0且在射程首次检查直接用毒；round value>0时即使已在射程仍请求movement mode3；负值跳过移动并重复建立targeting图后在第二次检查用毒。移动后只复检原目标，不重选。仍超距时比较`2*target attack`与wrapped int16己方`attack+HP`总和的`2*total/count`，strict更大回退自动攻击，否则休息。固定total330/count3阈值220，target attack50休息、200攻击。

现代已恢复三个selector及typed handler计划；实际`sub_3650E`移动、`sub_397E5`用毒、`sub_34C47`攻击回退、`sub_34AD3`休息和外层AI action_done continuation尚未接线，故主handler保持`pending_implementation`。

## 26. AI物品与暗器handler计划

`sub_35803`先调用`sub_34AEC(actor,1)`，复用逃跑最远格算法但不在移动后休息；随后无条件调用`sub_3598C(actor,0)`。固定field2 source `(10,20)`、round value3时目的格`(7,20)`、敌方曼哈顿和20。party side0的全局item slot索引200槽inventory，非零side索引角色4槽taking-item，handler不先检查数量。现代将目的格move与移动后mode0使用拆为两个同步步骤。

`sub_3582B`先调用攻击目标策略，再按actor hidden_weapon signed除15加1计算射程；首次targeting距离命中即调用`sub_3598C(actor,1)`，超距且round value>0请求movement mode1，移动后只复检同一目标，仍超距回退`sub_34C47`。round value<=0跳过移动但重复第二次targeting检查。hidden_weapon80得射程6；最近slot3距离6立即使用且无RNG；morality75、seed9输出2选slot4距离8，移动前1次检查，未改变位置后累计2次并回退攻击，移动到`(13,23)`后距离2则使用。最高攻击门槛命中但attack均为0时不写word11，原版继续使用合法stale target。

`sub_36133`在敌方携带物品数量耗尽后，从指定slot起同步左移后续item ID和数量并清空第4槽；`[5,6,7,8]/[1,2,3,4]`删除slot1严格得到`[5,7,8,-1]/[1,3,4,0]`，已完整映射为`remove_carried_item_slot`。

`sub_3598C`的AI mode1暗器状态与手动`sub_3A30B`并不共用毒值公式：item add_poison非负时直接加到目标poison，负值才算`(add_poison-hidden_weapon)/2`，两者都不读取anti_poison且不追加RNG。真实item102在seed1下伤害21、hurt40→45、HP100→79、poison10→50，仅消费一次RNG；无毒item96在seed2下伤害16、poison10保持10，也仅消费一次RNG。队伍方耗尽200槽inventory后左移，敌方耗尽4槽taking-item后调用`sub_36133`；AI分支不改actor方向且不提前写action_done。

`sub_2B483`共享物品效果已恢复23项状态数组：HP正负分支各自保留严格第二次RNG短路，毒值正负公式、HP/MP/体力/上限夹取和13项能力signed相加均按机器码执行；add_morality与add_attack_twice只进入显示数组却不写角色字段的BUG保留。真实item19在seed1下得到HP100→200、hurt40→0、poison50→0、体力30→100、MP10→100，三次RNG终态662824084。typed结果锁定battle重绘、`(70,18,148,20*n+30)`效果面板、等待输入及调用者固定9次tick等待。

现代已恢复两个typed handler计划、AI mode0共享物品状态与面板/等待参数、AI mode1暗器状态、两种来源扣减及携带槽移除状态核心，但未实际执行逐格移动、共享效果面板像素/present/input/tick、暗器effect/damage render/sample/present/wait、`sub_34C47`攻击回退和外层action_done continuation，故两个handler与`sub_3598C`仍保持`pending_implementation`。

`sub_361AC`请求医疗与`sub_36209`请求解毒实际共享同一函数体；后者仅建立相同栈帧后跳入前者。actor行动值严格大于0时先调用`sub_3650E(actor, mode0, value0)`，零或负值跳过；随后无条件从请求目标槽恢复x/y并调用`sub_34C47`自动攻击。typed计划保留move→automatic_attack同步边界、两项零参数和请求目标恢复；实际移动、攻击及外层完成仍待接线。

`sub_36210` AI医疗与`sub_363AC` AI解毒使用各自ability signed除15加1为射程；首轮目标图命中即执行动作，超距且行动值严格正时调用`sub_3650E(actor,mode1,value=range)`，移动后恢复同一目标再建图。零/负行动值虽然不移动，仍执行第二次建图。第二次仍超距时仅当`2*actor.attack`严格大于`2*wrapped_allied_total/allied_count`才自动攻击，否则休息。完整callee汇编确认IDIV余数和医疗分支两次`sub_3F50B`返回值均无行为效果。typed计划已恢复以上边界；实际移动、医疗/解毒、攻击/休息呈现及外层完成仍待接线。

`sub_3650E`已恢复完整目的格状态逻辑：mode2在本回合可入射程时按range向下找轴向层，mode3无轴向限制，其他分支从目标周围按上、右、左、下找同轴可达格、任意可达格或逐轴退向actor。目的格须连续通过当前图和重建actor movement图两次严格`path<128`检查，随后按原tie-break标最短路。battle4固定mode0/1 `(26,25)`、mode2 `(23,26)`、mode3 `(25,24)`。`advance_ai_movement`已实际逐格提交path255、occupancy、坐标、方向、sprite、体力和行动值，并恢复mode0/3行动值停止、mode1距离停止和mode2轴向停止；每步render/present/40 tick仍只返回typed参数。

`sub_36AF7`通用玩家光标选择已恢复为typed状态：入口按mode建立movement或targeting图，方向优先级下、右、左、上；相邻path不大于上限即可移动，path超限但occupancy非空仍可悬停；movement确认严格要求`0<path<=limit`，targeting允许`0<=path<=limit`，Escape清path上限并写取消结果。`sub_36A98`以actor行动值启动mode0选择，取消返回-1，否则复制选择期路径图、标最短路并逐格移动。battle4覆盖movement悬停占用格但拒绝确认、确认`(26,25)`、targeting确认占用格和首步状态；实际键盘、每轮战场重画、present及40 tick仍未接入。

`sub_3B387..sub_3C2AC`战后进度状态已恢复：敌方满HP/MP、体力100并清内伤/中毒；胜利把WAR word7总经验均分给存活side0；队伍至少补最大HP/5，死亡者体力至少10；word13及其80%分别加角色、练功、制造经验并unsigned封顶60000。等级提升保留30项机器阈值、资质分档成长RNG与技能条件RNG；练功保留需求系数、18项角色写入、武功学习/加100；制造保留五配方标记、反复`bounded(5)`、已有产物随机1..3与新槽固定1、材料槽压缩。提示框、present与按键等待仍为typed事件，四函数保持`pending_implementation`。

`sub_3C563`回合异常状态更新保留`hurt>0`优先分支、poison的HP/体力/hidden门槛、两次有符号除法，以及HP/体力仅严格负值夹1；`sub_3C672`对0..25槽（含当前活动数之外）仅在目标hidden严格等于1时清word11/12，现代仅对负值及大于25 target采用不读取数组外的安全边界，两者状态核心标`implemented_pending_review`。`sub_3C6D3`已恢复side横移220、面板/头像/名称NUL对齐、HP hurt色、最大HP poison色及MP类型色；非法MP类型复用poison色的寄存器残值BUG被保留。`BattleRenderer`已按`sub_2CEBF`实际绘制圆角半透明面板、离散白边、HDGRP头像、原Big5标签和数值，battle4固定队员面板叠加整帧的独立oracle与C++ FNV64均为`0x630a82d57e1d8715`；BattleSession已接玩家动作菜单调用点，剩余调用点及显式present/等待仍未接入。
