# B8 战斗 1:1 证据

状态：`pending_implementation`；最终汇编↔C++ REVIEW 尚未开始。

## 1. 物理范围与闭包

`sub_31C75 @ 0x31C75` 是 scene 与五轮试炼调用的 battle 入口。其后连续 battle 实现区间截止 `sub_3C6D3 @ 0x3C6D3..0x3CBE3`；`research/ida/reports/Z_DAT.b8_battle_xrefs.txt` 由当前 `Z_DAT.i64` 和 `idat.exe -A` headless 生成，枚举81个 FUNCTION 记录，报告规范为 LF，SHA256 为 `179b85c68ad87d03f175f7b22ff9af7ffbae68aed758eeaa0f0fe692ab67d488`。

`research/inventory/battle-closure.tsv` 以该报告为机械真值，当前30项为 `pending_mapping`、22项为 `pending_implementation`、29项为 `implemented_pending_review`。battle 区间调用到的 resource/render/input/time/random/audio 入口是共享 owner 边界，不随递归调用图吞入 battle closure。

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

`research/tools/generate_b8_battle_goldens.py` 只读取原版字节，不链接 OpenLegend C++；双生成逐字节一致。正式 `research/evidence/battle-goldens.json` SHA256 为 `d36198d26a7be34aae0be0671d76035cc0c902920547acbb84f99fb133851cca`。

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

`BattleSetup` 已实现26槽完整初值、固定/预置队伍、host-neutral cursor/0·1·2选择状态、按当前 count 取坐标追加、敌方建立、sprite word 和后写 occupancy 覆盖。真实 battle0 固定手选顺序、battle4 固定队伍、battle93 slot9→slot11覆盖及全140条记录均通过 Linux Debug 14/14。

因此 `sub_3265C/sub_3B1E6` 已推进为 `implemented_pending_review`；`sub_31EB9` 仍缺原像素选择框、present 与 input flag 清除基本块，继续保持 `pending_implementation`。

## 7. 回合排序、玩家菜单与胜负核心

`sub_3271E/sub_32A51/sub_32B78/sub_32E59/sub_3B238` 已从入口到返回完整恢复。`BattleSetup` 现实现：

- `int16(role.speed + 两件已装备物品的 add_speed word53)` signed降序，等值不交换；
- 交换时按原顺序复制 word0..7/9..13、逐槽写 occupancy、最后重算两槽 word8；
- 每轮 word6=`max(0,effective_speed/15-role.hurt/40)`，signed division toward zero；
- hp<=0且未 hidden 的槽清 occupancy并写 word5=1；无队伍为 raw1/`戰鬥失敗`，无敌方为 raw2/`戰鬥勝利`，双方皆空由 raw2覆盖。

真实 Big5 菜单固定为「移動、攻擊、用毒、解毒、醫療、物品、等待、狀態、休息、自動」，可用门槛和 ordinal cursor 见 `0x32E59.md`。`sub_32A51/sub_32B78` 已为 `implemented_pending_review`；顶层 render/turn dispatch/tick wait、十项动作、结果 panel/按键/战后提交尚未实现，所以 `sub_3271E/sub_32E59/sub_3B238` 保持 `pending_implementation`。

## 8. 战场路径图与最短路回溯

`sub_36E06..sub_37245` 的十个 path 单元已映射为 `BattlePathing`：

- movement 图把 upper layer非0、occupancy非-1或 ground tile 命中9段 IDA 常量的格写555，其余写254；targeting 图只检查 upper layer；source 随后强制写0；
- 原255槽环形队列保留 `(0,-1)` sentinel、distance `%128` 和上→右→左→下扩展顺序，没有替换为无界 queue；
- 回溯从 target 开始写250，每层用 `(distance+127)%128` 并按同一方向顺序选择首个前驱；消费标记255由后续移动单元使用；
- `sub_37070` 的坐标比较允许64，现代仅保留线性 index仍在0..4095的别名，index>=4096安全拒绝；不可达回溯返回false而不进入原死循环。

独立 oracle 固定 battle0/93 空 occupancy、单格占位、target距离14/22、回溯前后完整 FNV-1a 与首步 `(31,20)/(33,29)`；Linux Debug 14/14。十项均为 `implemented_pending_review`。

`sub_37355` 另以每次一个同步边界实现逐格核心：旧 path=255、occupancy 搬移、x/y、方向、sprite、条件体力 DEC、行动值 DEC依机器顺序写入；destination、Manhattan range、aligned range和行动值耗尽停止规则也已映射。连续左移两格固定 direction2、sprite5110、physical power 1→0、round 5→4→3。该函数仍缺每格视图更新、render/present与40 tick等待，故保持 `pending_implementation`。

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

`sub_3AA85` 已恢复为严格两次local-x外层/local-y内层的32×32命令计划：第一pass绘制WARFLD layer0；第二pass依次加入path overlay、主/副cursor、非0且非15000的layer1、normal或三种调色高亮角色、effect以及五种damage文字。path overlay与主cursor同受range严格大于0保护，secondary cursor由独立flag控制。普通sprite锚点为`145+18*(x-y), -81+9*(x+y)`；overlay左移18，damage再按offset上移。独立oracle以真实battle4资产和非对称view/cursor生成1,157条命令，哈希`0xb9f8a428699b3712`，C++逐字段复算一致；零range向量不产生cursor命令。typed计划尚未执行成RLE/overlay/highlight/font indexed像素，故原绘制函数仍为 `pending_implementation`。

## 20. AI六个候选selector

`sub_33C4D/sub_33E93/sub_340D9` 已恢复低HP、已中毒和低MP候选。三者均先保留自身能力的strict门槛，再按side使用队伍200库存或敌方4个随身物品，首个命中即返回。中毒selector保留原字段不一致：队伍解毒物品检查item word56为负，敌方随身物品检查word47为负。仍无物品时，低HP/中毒selector可写请求医疗8或请求解毒9并指定首个符合能力门槛的未隐藏同伴。

`sub_341F6/sub_343DA` 已恢复医疗/解毒目标选择：按combatant槽序扫描同side未隐藏目标，请求动作优先；其后分别按HP或poison四档短路。固定HP24/100和poison35均以seed1消费三次`bounded(10)`得到`[8,8,3]`、终态662824084，再选择slot1；早档命中不得消费后续RNG。

`sub_34550` 已恢复攻势selector。双方`HP+attack`按int16逐次回绕累计且不跳过隐藏槽；危险条件成立时选择缺HP或中毒值最大的同伴。否则固定先执行用毒的`bounded(50)`，条件成立才继续`bounded(150)`；随后按side使用不同暗器强度和RNG门槛。无暗器后，体力严格大于10且当前MP不少于十槽非零武功的最小need_mp才返回攻击2。原函数对攻击2只返回而不写word10，动作3/4/5/10才写入。六个selector均为 `implemented_pending_review`。

## 21. AI入口typed同步合同

`sub_33599`已拆成三个严格边界：`begin_ai_turn`在任何绘制前按actor side对全部combatant的`HP+attack`进行int16回绕累计，并要求后续依次重绘、present、等待300 tick；`choose_ai_turn_action`在该等待后按低HP、中毒、低MP、医疗队友、解毒队友、逃跑、攻势selector顺序执行；`finish_ai_turn`仅在对应动作handler返回后写combatant word7为1。

低HP四档RNG阈值为3/5/7/9，中毒入口无条件消费一次`bounded(10)`并与`poison/10`比较，低MP四档为2/4/6/8；医疗和解毒能力20/40/60分别比较4/6/8，80最终无RNG兜底；逃跑先比较5，再按HP的1/4和1/5档比较6/8。保留原顺序BUG：体力<10先选等待7，但低HP selector若被调用后返回0，会把等待清零并继续后续决策。固定seed1清零等待向量为`[8,8,13]`、终态662824084；seed10逃跑向量为`[3,4]`、终态1849040536。

动作0/7共享休息handler，其余1..6、8..11逐项映射移动、攻击、用毒、解毒、医疗、物品、请求医疗、请求解毒、暗器和逃跑。当前现代代码只生成typed handler并提供handler后最终写入；render/present/300 tick和11个实际handler尚未由BattleSession同步执行，故`sub_33599`保持 `pending_implementation`。
