# B5 标题、菜单、新游戏与基础 UI 汇编合同

状态：assembly-reviewed / golden-generated / implementation-pending
真值：当前 `Z.DAT` 机器码、当前 `title.idx/title.grp/title.big/CFONT/RANGER.GRP` 字节

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

系统菜单内嵌于代码 `0x20D17`，三个标签是“读档 / 存档 / 离开”。读档和存档都进入 `sub_25F87` 的“一 / 二 / 三”三槽菜单；参数 0 调 `sub_26208`，参数 1 调 `sub_265AB`。槽菜单 `Esc` 同步返回系统菜单，不执行 I/O。系统菜单“离开”先显示“真要离开游戏（Ｙ／Ｎ）”，仅大写 `Y=0x59` 退出；其他键原样返回系统菜单。

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

非空姓名Enter先清`(158,141,50,17)`并重绘最终姓名、present，再阻塞30 ticks才进入属性生成；空姓名Enter无效。现代用present-gated continuation承接该顺序。详细入口终审见`0x27A26.md`、`0x2841A.md`、`0x287CA.md`、`0x28975.md`。

## 6. 状态与物品基础 UI

状态入口 `sub_22066` 调队员选择器，再以角色 ID 调 `sub_22A59`。共享选择器和状态页都先按`word_C0BF2`分支重画世界、场景或战斗背景；参数2使用“要查閱誰的狀態”圆角标题框、62像素宽队伍列表、NUL居中姓名和当前项颜色。状态函数连续显示两页并各调用一次同步按键等待：第一页为姓名/头像、等级、生命、内力、体力、经验/升级，以及攻击、防御、轻功、医疗、用毒、解毒、拳掌、御剑、耍刀、特殊兵器、暗器；任意键进入第二页，显示两件装备、修炼物/经验和最多十项武功/等级；第二次任意键才返回主菜单。

现代实现由`BattleRenderer`单一拥有参数0/1/2/6选择框、医疗/解毒能力列表与两页状态像素；`BattleSession`与`LegacyGameRuntime`分别提供战斗及世界/场景背景。参数0以`%3d/%3d`显示生命，参数1以`%3d`显示中毒，参数2保留纯姓名状态选择；`GameMenuController`保存过滤后的party slot、嵌套施术者/目标状态和`status_page=0/1`两次同步返回合同。全部值直接读取当前RANGER的`int16`/raw bits，不拥有或复制第二份角色状态。旧`BasicUiRenderer`简化状态页已删除。世界整帧回归固定医疗、解毒、状态选择与结果，场景回归确认框/面板之外逐像素保持场景背景。

物品入口 `sub_2A0D9` 依次调用 reset/draw/select。从 header 的200格物品/数量对保留连续库存顺序，`GameMenuController`与`BasicUiRenderer::render_items`实现同一5×3网格、左右列回绕、上下行/滚页、PageUp/PageDown、Escape、三确认键、上下箭头、secondary name、简介及数量。共享 `sub_2D501` 四边框 primitive 映射到 `IndexedFramebuffer::outline_rectangle`。选择后外层按机器顺序承接装备、修炼、消耗品或事件分支，并由共享角色选择器与物品门禁完成状态写集；`sub_2A10F`和`sub_2A186`均为`implemented_pending_review`，最终双向逐基本块REVIEW仍为`not_started`。

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
- 标题主菜单、三槽和等待帧使用独立 Python RLE oracle；姓名输入另以当前`title.big/FONT.X16/FONT.C16/CFONT`固定初始、组合、候选第一页、负页安全适配、无候选、英数A、单ASCII残影和接受帧八项hash；世界菜单参数2选择框、双页精确状态和物品帧锁定 framebuffer FNV-1a 回归值，战斗双页状态另由独立原资产oracle复算，场景调用验证面板外背景逐像素不变；现代提交路径仍为 `indexed8 + RGB6 -> RGBA8 -> SDL nearest integer viewport`；
- 本次菜单医疗/解毒切片的Linux/Windows app Debug均14/14通过，Linux app ASan+UBSan 14/14通过并已恢复普通Debug cache；独立B5 golden与tracked证据一致，原始`Z.COM/Z.DAT/WAR.STA/WARFLD.IDX/WARFLD.GRP`与阶段前hash合同一致，`research/ida/databases/Z_DAT.i64`无工作树修改。
