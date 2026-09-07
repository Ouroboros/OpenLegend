# 函数证据：`sub_3B6BE` `0x3B6BE..0x3BA85`

状态：`platform_adapted / converged_no_new_differences`

映射：`BattleSetup::apply_battle_level_up`与`BattleSession`战后升级消息preview/present/input/commit continuation。

## 1. 物理边界与二进制身份

- fresh临时IDA库：`tmp/Z_DAT-b8-order76.i64`；导出：`tmp/b8-order76-machine-all.txt`。
- 地址范围`0x3B6BE..0x3BA85`，967 bytes、201条指令、54个IDA flow block、26个条件分支、6个无条件跳转。
- 68处四字节重定位全部满足loaded=raw+`0x20000`；逆变换后967 bytes逐字节匹配。
- raw SHA256：`88a20006a2348013356818dea6ff5d72f1ad289aabf2ee57413919a2893aa02d`。
- loaded SHA256：`9c6e1e546d6860fa3139e7782c57708d63deb4c9265fb44b4f6008aeb8bb4db4`。
- 16次direct call依次为`0x3ED1E,0x3AA85,0x3EF4A,0x2CEBF,0x3D832,0x3D6D1,0x20C32`及九次`0x3D612`。
- 唯一入口xref为`sub_3B387:0x3B655`；函数无本地RET。`0x3B6EE,0x3BA71,0x3BA80`三个出口均进入独立共享尾`0x39A3E..0x39A45`，其字节为`83c4045f5e5bc3`（`add esp,4; pop edi; pop esi; pop ebx; ret`）。

## 2. 汇编独立合同

1. 入口以旧等级signed low16索引30项unsigned经验阈值：`0,50,150,300,500,750,1050,1400,1800,2250,2750,3850,5050,6350,7750,9250,10850,12550,14350,16750,18250,21400,24700,28150,31750,35500,39400,43450,47650,52000`。当前经验低于旧等级阈值时直接退出，不显示、不写角色且不消费RNG；满足时扫描至29，不在首个失败处停止，保留最后一个满足项的`level+1`。
2. 唯一caller先拒绝`side != 0`，随后把同一side作为第二参数，因此真实可达调用恒传0并显示升级消息。函数本身只在参数低字非0时抑制消息；该抑制域仍由独立向量固定。
3. 未抑制时，任何升级状态/RNG提交之前固定执行战场重画→Big5 `%s 升級了`格式化→`(100,30,120,27)`框→`(107,35,0x0705,16)`文字→present→清旧last-key并等待新非零键。消息结束后才开始第一次RNG。
4. 首次RNG上界由旧IQ signed比较分桶：`IQ<30 → 2`、`30..49 → 3`、`50..69 → 4`、`70..89 → 5`、`>=90 → 6`；结果加1为成长值。等级一次性增加跨越阈值数，按word low16写回。
5. 第二次RNG固定为`bounded(3)`。最大生命增加`low16((increased_life zero-extended + roll) * 3 * levels_gained)`，与旧word相加后先回绕，再按signed `>999`封顶；当前生命随后补满，内伤/中毒清0、体力写100。最大内力增加`(9-growth)*4*levels_gained`，同样先word回绕再按signed `>999`封顶并补满当前内力。
6. 攻击、速度、防御各增加`growth*levels_gained`并先word回绕。医疗、用毒、解毒、拳、剑、刀按此顺序，仅当各自旧/当前signed值严格`>20`时各消费一次`bounded(3)`并加回；暗器无条件最后消费一次`bounded(3)`。抗毒与特殊能力不读写。因此升级最少消费3次、最多9次RNG。
7. 最后依次对攻击、速度、防御、医疗、用毒、解毒、暗器、拳、剑、刀执行signed `>100`封顶。回绕成负数的word不会被999/100上限捕获；不能替换成32位饱和运算。

## 3. 汇编→C++ REVIEW

- `BattleSetup::apply_battle_level_up`的30项阈值全扫描、unsigned经验比较、signed IQ五档、等级跨度、HP/MP公式、word回绕、signed上限、技能严格`>20`门及九个RNG调用顺序逐基本块一致。
- 直接产品向量锁定seed1、等级1、经验150、IQ90、生命成长2与技能`21/20/22/23/24/25/26`时等级`1→3`、成长3、最大生命`100→118`、最大内力`80→128`及最终RNG `2633739833`。
- `BattleSession`用角色与RNG副本建立preview；升级frame present前及零键等待期间共享角色/RNG保持不变，收到新非零键后才一次提交副本。Session场景的role1为IQ60且六个条件技能均0，所以只消费3次并提交RNG `662824084`；这与上述8次直接向量是两个独立合法路径。
- 完整对照未发现产品差异。现代非法role/slot拒绝、负等级和等级>=30安全拒绝，以及把DOS同步阻塞拆成preview/present/input/commit宿主phase，均归类`platform_adapted`；合法caller域机器语义不变。
- 栈探测、renderer、格式化、框/文字、present/input、bounded RNG、caller、练功/制造owner及共享尾均保持独立closure，不传播本owner状态。

## 4. 回归与独立Golden

- C++回归新增：当前阈值前1点不升级且零RNG；等级0阈值0与等级29阈值52000；IQ `29/30/49/50/69/70/89/90`八个边界；六项技能20/21门、3/8/9次RNG顺序；negative increased-life；HP/MP和十项能力word回绕后signed封顶；抗毒/特殊能力不变；非零抑制参数；Session present前/零键共享状态不变及新非零键后提交。
- `battle_level_up_machine`含15个独立向量并逐字段记录before/after、消息序列、每次RNG名称/上界/结果和最终state；向量SHA256：`9c35ab92bd1521ce8237f95a2e9a21ba355bb9ba2ec472dc5576d0c44380d1fd`。
- 两次临时生成逐字节一致，历史69个顶层键逐值不变；第三次正式生成一致。70键正式Golden SHA256：`d08d6315e3f10e808d5b8ece0a06b45b1896cc7baf16c314616f885cd8ce9d17`。
- Linux `./build.sh app --config Debug`退出码0，14/14 tests passed。
- 原程序动态执行仍登记`blocked_runtime_oracle`；独立资产oracle不冒充原程序运行输出。
