# B5 标题、菜单、新游戏与基础 UI 汇编合同

状态：B5/UI统一最终汇编→C++ REVIEW为39/39，全部owner关闭；原资产Golden已冻结。
真值：当前 `Z.DAT` 机器码、当前 `title.idx/title.grp/title.big/CFONT/RANGER.GRP` 字节

input-font Order38确认原版唯一启动链固定加载`FONT3.E16/FONT3.C16`，并废弃此前由非启动链字体生成的姓名及菜单像素证明。后续UI owner继续由同一只读资产oracle追加证明；B9最终两次独立生成逐字节一致并反向匹配正式`research/evidence/title-menu-new-game-goldens.json`。物品贴图基址纠正后再次独立生成并逐字节反向匹配；系统/槽位菜单宽度纠正后，Oracle直接从原始push指令提取几何并再次反向匹配，当前SHA256=`964d53d96a9124f9a548f18b1799926868d1c96045f3abf57c70fc131b21ce0b`。旧文件中仅由C++运行期输出手填的`runtime_ui_regression_fnv1a64`不是独立oracle，已移出正式Golden，运行期哈希只保留为测试回归锁。

## 1. 证据范围

- `main @ 0x20D35..0x20FA9`：启动资源、标题循环与游戏会话循环；
- `sub_20FAF @ 0x20FAF..0x212BF`：标题、新游戏、读取与退出；
- `sub_212C0 @ 0x212C0..0x21495`：六项游戏菜单分派；
- `sub_21496/sub_21AC0`：医疗与解毒；
- `sub_22066/sub_22090/sub_22A59`：队员选择与状态面板；
- `sub_25BBA/sub_25D0E/sub_25F87`：离队、系统菜单与三槽菜单；
- `sub_26208 @ 0x26208..0x265AA`：三槽读取与运行时状态导入；
- `sub_269AB @ 0x269AB..0x26A91`：游戏菜单绘制；
- `sub_26B5E @ 0x26B5E..0x27119`：新游戏基线复制、会话初始化与序章入口；
- `sub_2711A @ 0x2711A..0x27A25`：姓名完成后的初始属性生成；
- `sub_27A26/sub_2841A/sub_287CA/sub_28975`：姓名输入、绘制、候选与退格；
- `sub_2A0D9/sub_2A10F/sub_2A186/sub_2A86C`：物品基础 UI；
- `sub_3D643 @ 0x3D643..0x3D689`：按 `legacy_id / 2` 取帧并绘制 RLE；
- `research/ida/reports/Z_DAT.b5_ui_xrefs.txt`：上述完整机器码、调用引用、数据引用与静态表；
- `research/evidence/title-menu-new-game-goldens.json`：独立 Python oracle 生成的标题像素、姓名输入像素与角色随机向量。

IDA 9.2 只通过 `idat.exe -A` headless 运行。最终报告按仓库 LF 规则规范化后为 704,601 字节、10,421 行，SHA256 `df04245790f8a3161c37fb9af64376a6e3f56c6cd5701a33bc31d526e68468f6`。导出结束后以 Git 中的字节恢复 `research/ida/databases/Z_DAT.i64`；数据库不属于 B5 改动。

## 2. 标题状态机

### 2.1 资源与像素

标题流程直接读取：

- `title.idx`：36 字节，9 个普通累计尾；
- `title.grp`：8,179 字节，9 个 RLE 帧；
- `title.big`：64,000 字节，完整 `320×200` indexed 背景；
- `mmap.col`：768 字节，256 个 RGB6 调色板项。

`title.big` 先逐字节成为背景。主标题始终在 `(117,137)` 绘制 legacy id `0`，然后按选择项 `s=0..2` 在 `(117, 137 + 20*s)` 绘制 id `2 + 2*s`。读取子菜单先在 `(117,137)` 绘制 id `8`，再按槽位 `s=0..2` 绘制 id `10 + 2*s`。选择槽位后，原版先把 `(115,135,135,65)` 清零，再在 `(120,160)` 绘制 id `16` 的“请稍候”帧。

像素覆盖顺序不可交换。独立 RLE oracle 的 FNV-1a 64-bit 结果：

```text
main 0 = 86690e3b3b68fe20
main 1 = 01a9df9f3a147c7c
main 2 = 7e4c70629a4c00d4
load 0 = c6475eb2b76d4457
load 1 = 7d49db972e535393
load 2 = b76c74065386ab63
wait   = 7333253ca7400de6
```

### 2.2 输入与返回值

标题主菜单有三项：新游戏、读取、离开。内部选择从 0 开始，`Down=0x98` 与 `Up=0x9E` 均按 3 环绕。`Enter=0x0D`、`Space=0x20`、keypad Insert/0 `0x96` 都确认。主标题上的 `Esc=0x1B` 没有副作用。

确认“读取”进入独立三槽子状态；槽位同样按 3 环绕。子状态 `Esc` 返回主标题并保留主选择在“读取”。确认槽位返回 `slot+1`，调用者再减 1 传给 `sub_26208`。确认“新游戏”返回 0。确认“离开”按原顺序停止音乐、恢复 IRQ 并退出。

现代边界用强类型同步结果表示 `new_game/load(slot)/exit/cancel`，不得用真假值混淆取消、失败和槽 0。标题读档失败必须保留原 `GameState`，向 UI 返回明确错误；不得部分导入或退回伪造默认状态。

## 3. 游戏菜单与系统菜单

`sub_269AB` 的六个 5-byte Big5 标签位于 `0x545CA`：

```text
0 醫療  1 解毒  2 物品  3 狀態  4 離隊  5 系統
```

`word_C0BF2 == 0` 时显示 6 项；`word_C0BF2 == 1` 时仅显示前 4 项。选择按该项数环绕。分派严格为：

```text
0 -> sub_21496 医疗
1 -> sub_21AC0 解毒
2 -> sub_2A0D9 物品
3 -> sub_22066 状态
4 -> sub_25BBA 离队
5 -> sub_25D0E 系统
```

医疗与解毒先在连续队伍前缀中筛选对应能力`>=10`的施术者，再进入参数0/1共享目标选择器选择全部队员。目标Escape返回原施术者光标，外层Escape才返回主菜单；确认后party slot经队伍表映射为角色ID，分别调用共享医疗/解毒状态核心。结果框显示`%3d`变化量并等待任意非零键。没有合格施术者时保留已显示的主菜单底图，在其上叠加原提示框并同样等待任意非零键。

系统菜单内嵌于代码 `0x20D17`，三个标签是“读档 / 存档 / 离开”。三层框几何由机器调用立即数固定：主菜单`(20,18,42,132)`、系统菜单`(70,18,42,72)`、槽位菜单`(120,18,26,72)`。读档和存档都进入 `sub_25F87` 的“一 / 二 / 三”三槽菜单；参数 0 调 `sub_26208`，参数 1 调 `sub_265AB`。槽菜单 `Esc` 同步返回系统菜单，不执行 I/O。系统菜单“离开”先显示“真要离开游戏（Ｙ／Ｎ）”，仅大写 `Y=0x59` 退出；其他键原样返回系统菜单。

离队分支先选择队员。主角 ID 0 不可离队，并显示“抱歉！没有你游戏进行不下去”；其他角色映射到离队事件并返回。B5 只固化入口、选择和返回合同；角色事件执行仍属于 B7。

## 4. 新游戏基线与初始属性

`sub_26B5E` 先把只读基线 `RANGER.GRP/ALLSIN.GRP/ALLDEF.GRP` 复制到工作副本并导入；现代实现等价为 `load_baseline -> GameState::import_snapshot`，不得写原始基线文件。姓名与属性只修改内存中的主角记录。

`sub_2711A` 每次普通重掷严格消费 17 次 `sub_3D612`：

1. `mp_type = bounded(2)`；
2. `maximum_mp = bounded(20) + 21`；
3. attack、speed、defence、medicine、use_poison、detoxification、anti_poison、fist、sword、knife、unusual、hidden_weapon，依次各 `bounded(10)+21`；
4. `increased_life = bounded(5)+3`；
5. `maximum_hp = increased_life * 3 * level + 29`；
6. 先 `bucket=bounded(10)`：0..1 时 `iq=bounded(35)+30`，2..7 时 `iq=bounded(20)+60`，8..9 时 `iq=bounded(20)+75`；
7. `hp=maximum_hp`，`mp=maximum_mp`。

首轮进入页面即重掷。属性页等待任意 last-key；仅 `Y=0x59` 接受当前结果，任何其他键都立即再掷，并同时进入 8-byte 滚动缓冲。缓冲精确等于 `BABERUTH` 时，不消费 RNG，写入：`mp_type=2`、`maximum_mp=40`、十二项能力均 30、`increased_life=10`、`maximum_hp=50`、`iq=100`，然后同样等待 `Y` 接受。不得把界面上的“Y/N”改成只有 N 才重掷。

独立向量记录于 `title-menu-new-game-goldens.json`；seed 0、1、`0x12345678`、`0xFFFFFFFF` 均固定了全部字段、17 次消费后的 state 与当前 HP/MP。

## 5. 姓名输入

主角姓名的物理区域是 role 0 的 offset 8、10 字节；输入器先清零全部 10 字节，但可提交长度上限是 6 字节。英数占1字节，Big5占2字节。注音模式只在当前长度小于5时接受新组合，英数模式可写到6字节；不得统一成单一6-byte输入门。

初始模式为注音；`Ctrl(0x82)+Space(0x20)` 在注音/英数间切换并清组合。`0x545E8..0x54B67` 按translated key提供3-byte注音标签、128个`{type,value}`记录和英数资格/输出字符。type 1/2/3/4分别写初声、介音、韵母、声调，并在x=100/120/140/160、y=161以颜色`0x1715`绘制；声调写入后自动查找。英数模式只接受翻译表产生的`0..9`与`A..Z`。

`CFONT`查询严格令`i=initial*5+tone`，从文件开头16-bit little-endian累计边界读取`[word[i],word[i+1])`；用`(medial<<4)|final`寻找索引字节；命中后从下一字节连续扫描到首个`<0x40`，字节数除2形成候选。无匹配时在`(240,161)`以`0x0705`绘“沒有字”，present并等待任意一次非零key，再清组合。

候选底栏先清`(0,160,320,40)`，模式提示改用`0x1719`；最多8项，页内编号与Big5位于`(30*(i+1),180)`。第一页仅画后箭头，中间页画双箭头，末页画前箭头。机器原BUG会在20候选的第一页把前页flag也置1，Shift+comma令有符号页码变-1并读取候选起点前16字节；现代保留负页、分页与数字选择语义，但对原字体例程会越界的非法Big5确定性跳过，并在CFONT头边界停止继续前翻。

候选态普通短按Enter/Backspace不执行命令；Space前进并在末页回第一页，Escape退出候选，数字1..8按`page*8+digit`生成1-based选择。退格在普通输入态按韵母→介音→初声优先清组合，否则按1/2-byte姓名单元删除。仅有一个ASCII时原机长度减0却不清首字节，画面保留残字；现代用独立display buffer保留该残影而保持逻辑姓名为空。

非空姓名Enter先清`(158,141,50,17)`并重绘最终姓名、present，再调用`sub_3DB83(30)`；callee按`argument/40+1`只等待1次BIOS tick变化，随后立即进入属性生成。空姓名Enter无效。现代用present-gated continuation承接该顺序。详细入口终审见`0x27A26.md`、`0x2841A.md`、`0x287CA.md`、`0x28975.md`。

## 6. 状态与物品基础 UI

状态入口 `sub_22066` 调队员选择器，再以角色 ID 调 `sub_22A59`。共享选择器和状态页都先按`word_C0BF2`分支重画世界、场景或战斗背景；参数2使用“要查閱誰的狀態”圆角标题框、62像素宽队伍列表、NUL居中姓名和当前项颜色。状态函数连续显示两页并各调用一次同步按键等待：第一页为姓名/头像、等级、生命、内力、体力、经验/升级，以及攻击、防御、轻功、医疗、用毒、解毒、拳掌、御剑、耍刀、特殊兵器、暗器；任意键进入第二页，显示两件装备、修炼物/经验和最多十项武功/等级；第二次任意键才返回主菜单。

现代实现由`BattleRenderer`单一拥有参数0/1/2/6选择框、医疗/解毒能力列表与两页状态像素；`BattleSession`与`LegacyGameRuntime`分别提供战斗及世界/场景背景。参数0以`%3d/%3d`显示生命，参数1以`%3d`显示中毒，参数2保留纯姓名状态选择；`GameMenuController`保存过滤后的party slot、嵌套施术者/目标状态和`status_page=0/1`两次同步返回合同。全部值直接读取当前RANGER的`int16`/raw bits，不拥有或复制第二份角色状态。旧`BasicUiRenderer`简化状态页已删除。世界整帧回归固定医疗、解毒、状态选择与结果，场景回归确认框/面板之外逐像素保持场景背景。

物品入口 `sub_2A0D9` 依次调用 reset/draw/select，机器从header的200格物品/数量对建立5×3图标网格，并处理左右列回绕、上下行/滚页、PageUp/PageDown、Escape、Enter/Space确认、上下箭头、secondary name、简介及数量；keypad Insert`0x96`不确认物品。共享 `sub_2D501` 四边框primitive已映射到 `IndexedFramebuffer::outline_rectangle`，world、scene与battle均已按原三面板、5×3 MMAP图标格、真实库存slot映射和详情几何接线。

`sub_2A0D9`的input-font owner已独立终审：wrapper在caller预清last-key及三确认键态后再次清last-key，现代把两次相邻写零合并到不可重入的SDL事件返回边界，下一事件前清理且中间无last-key读，零产品差异。UI Order26随后以新临时IDB独立重启同址wrapper REVIEW，首轮发现现代进入items后可在初始frame成功present前消费同批下一键，违反机器`reset → draw/present → selector`顺序。现由runtime items初始present门修正：进入items关闭，render不打开，只有`finish_presented_tick()`打开；修正后重审16/16条指令、1/1块、四次call、唯一caller和RET无第二处wrapper差异。新UI序列SHA256为`962ae13a3845694f656a57eddcd92543144c72da532fd5b5572eae5c1d9ed5bb`，原资产正式合同SHA256保持`59bb83d28586b8871cf9446349b0d1cd1840488e990a88e1f14c0e061cf4b389`。

UI Order27独立冻结`sub_2A10F`为119 bytes、34条指令、9/9 CFG块、6个条件分支、6处重定位、2次direct call、2个caller和唯一RET。机器先把200个输出word填`-1`，再完整扫描库存slot 0..199；signed负ID跳过，quantity不读，filter0收全部合法ID，filter4收type4/type3并集，并稳定保存原库存slot。首轮对照发现现代world/scene遇中间空槽即截断，且controller/renderer把逻辑网格index误作库存slot；现以固定200项slot映射最小修正，battle原实现无需修改。修正后从入口重审34/34条指令和全部分支无第二处合法域差异；稀疏槽、重复ID、count0、负count与slot199回归通过。Order28审计随后纠正了本项资产oracle把record id `+0`误作item type的错误；正确字段为byte `+82`/word 41，基线四项均为type3，因此filter4前缀为`[0,1,2,3,-1]`。产品原已使用word 41，不受该证据错误影响。修正后正式B5 Golden三生成一致SHA256=`dbd1721d185aebe764b12f1d8afc0e7992a0b12377a92b153379494929e37415`，合同/向量SHA256分别为`b821c125ab79c95c0cd6e9074e6a6b9d4c67cef202e40573fe402a79f24fdb6d`/`3fd01787c6d22cd4414b79ca7a947ee75769f49e12a01ad13703a56877fcb76b`。本项只关闭reset/filter owner；draw与selector保持独立。

UI Order28独立冻结`sub_2A186 @ 0x2A186..0x2A74C`为1478 bytes、379条指令、47个CFG块、25个条件分支、6个无条件跳转、94处重定位、27次direct call、3个caller和唯一RET。机器按context先重绘world/scene/battle背景，绘制三个固定面板，再用原始库存首个exact `-1`的`slot+1`作为箭头metric；5×3格通过Order27真实slot映射取MMAP图标，选中边框先于映射验证。详情严格使用item record `+0/+2/+22/+42/+76/+80/+82`，保留type/role条件居中、item 182坐标格式、role注记与signed quantity `>1`门。现代world/scene原8行文字列表和battle按过滤数画箭头等差异已最小修正；修正后从入口重审379/379条指令及全部块、分支、调用和出口无第二处合法域差异。独立原始资产oracle的五个整帧向量覆盖15项page0仍画down、item182、主/备用名称、strict role中心、quantity与空选择边框；battle另覆盖原始15槽但filter4仅1项。合同/向量SHA256为`e2512968dbf346d44b6856095c8f21ea87d0af23cd5626d8fdc40d629fc7f4b1`/`9ebe37fed73291d35b8254607d12e1d3c81310f0f654e6d9fc2e0e9857655598`；正式B5 Golden三生成一致SHA256=`dbd1721d185aebe764b12f1d8afc0e7992a0b12377a92b153379494929e37415`，统一Linux app Debug `proc_15a4`精确通过14/14。本项只关闭draw/present owner；reset、selector、callers及各callee均不传播closure。

`sub_2A86C`的input-font owner已独立确认物品只接受Enter/Space、不接受keypad Insert。UI Order29随后用新临时IDB从同一2491 bytes入口独立终审完整selector/use owner：机器在确认或Escape后必先重画并present裸world/scene/battle背景，再分派type0事件、type1装备、type2练功、type3消耗品或battle type4返回；每次物品导航、角色选择、换人/自宫询问、失败notice与效果面板也都在读下一键前present。首轮现代实现跳过裸context帧、允许同一SDL event batch越过中间屏，且world/scene在效果面板present前已扣库存；现以统一deferred item dispatch、逐阶段present门、battle `player_item_context_present`及effect-present后库存提交最小修正。装备双向解除、练功known/full/资格/自宫/经验写入、零效果不扣不等待、signed count删除与返回0/1/4均从入口重审一致。UI合同/15向量SHA256=`a322d31f3ee654f0425ef42899b4057396150339b2fa8ec674cc7dd6faffae97`/`5647f386cb37172ac2dd355f7e9a2df9520375b2114e6020dbeddb14b7beaeb4`；正式B5 Golden三生成一致SHA256=`5302224e809ecd01fca0ee0fceb4afe2d1fb84e153d9a241cd04a17eba100584`。本项不传播`sub_22090/sub_2BD8B/sub_2B483/sub_2B227/sub_2B288`、callers或共享尾closure。

## 7. 失败、所有权与阶段边界

- `model::GameState` 是唯一游戏状态所有者；UI 仅保留选择、候选和模态状态；
- persistence 只运输完整 snapshot，读失败不改变 `GameState`，写失败不报告成功；
- 原始资产只读，所有测试写入 `build/.../tests/generated/<Config>/`；文件系统权限可写不构成写原资产授权；
- SDL 只把宿主键和 indexed framebuffer 连接到核心；显示兼容层逐像素执行 `palette[index]` 与 RGB6→RGBA8 位扩展，再以 nearest-neighbor 居中整数倍上传，禁止把 DOS 索引字节直接当现代颜色，也禁止缩放反写核心缓冲；UI 核心不出现 SDL 类型；
- B5 完成条件是标题、新游戏默认状态、三槽读取/取消/错误、六项菜单、状态/物品基础 UI 及同步模态返回全部有 golden/单测和 Linux/Windows app 验证；世界移动与场景事件继续由 B6/B7 实现。

## 8. 现代实现与验证结果

- `app::LegacyGameRuntime` 是 B5 会话所有者：标题、新游戏、三槽 I/O、世界占位状态、游戏菜单和错误模态均由单一状态机协调；`model::GameState` 仍是唯一游戏状态所有者；
- 标题读档先显示原“请稍候”帧，再在下一逻辑步完整导入 snapshot；失败进入显式错误模态且测试逐字节确认原 `GameState` 不变；
- 存档测试只复制基线和 UI 资产到 `build/.../tests/generated/<Config>/b5-runtime/`，验证成功写出的 R/S/D snapshot 与内存状态完全相等，并通过删除隔离目录强制证明写失败不会伪报成功；
- 主角离队返回原 Big5 提示且不修改 snapshot；其他角色的离队事件副作用按汇编边界留给 B7 事件执行器；物品使用/装备/修炼副作用同样留给 B7；
- 标题主菜单、三槽和等待帧使用独立 Python RLE oracle；姓名输入另以当前`title.big/FONT3.E16/FONT3.C16/CFONT`固定初始、组合、候选第一页、负页安全适配、无候选、英数A、单ASCII残影和接受帧八项hash；世界菜单参数2选择框、双页精确状态和物品帧锁定 framebuffer FNV-1a 回归值，战斗双页状态另由独立原资产oracle复算，场景调用验证面板外背景逐像素不变；现代提交路径仍为 `indexed8 + RGB6 -> RGBA8 -> SDL nearest integer viewport`；
- 本次菜单医疗/解毒切片的Linux/Windows app Debug均14/14通过，Linux app ASan+UBSan 14/14通过并已恢复普通Debug cache；独立B5 golden与tracked证据一致，原始`Z.COM/Z.DAT/WAR.STA/WARFLD.IDX/WARFLD.GRP`与阶段前hash合同一致，`research/ida/databases/Z_DAT.i64`无工作树修改。
