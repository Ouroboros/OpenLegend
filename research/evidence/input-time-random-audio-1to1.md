# B4 输入、时间、随机与音频汇编合同

状态：最终双向REVIEW已收敛；RNG核心为`assembly_exact`，输入/计时/音频及宿主生命周期为`platform_adapted`
真值：当前 `Z.DAT` 机器码、当前 77 个 WAV 与 24 个 XMI 字节、Miles 3.03 包装器内嵌诊断字符串

## 1. 证据范围

- `0x20BC0..0x20C31`：嵌入式 IRQ1 键盘处理体；
- `0x3CDE3..0x3CDFE`：安装到中断 9 的保存寄存器/切换 DS/trampoline/`iret`；
- `sub_3CDFF @ 0x3CDFF..0x3CF18`：键盘中断、字体和音频初始化；
- `sub_3CF19 @ 0x3CF19..0x3CF44`：恢复原中断 9；
- `sub_20C32 @ 0x20C32..0x20C44`：清空并阻塞等待 last-key；
- `sub_3DB83 @ 0x3DB83..0x3DBBF`：BIOS tick delay，函数级最终证据见`research/evidence/functions/Z_DAT/0x3DB83.md`；
- `sub_3D612/sub_3F987/sub_3F98D/sub_3F9B0`：有界随机包装、RNG 状态、next、seed；
- `sub_3DD66..sub_3E2E2`：游戏侧 Miles music/sample 包装器；
- `sub_41232..sub_43239`：Miles API 包装器与内嵌函数名；
- `research/ida/reports/Z_DAT.b4_runtime_xrefs.txt`：上述机器码、数据引用和调用引用；
- `DIG.INI` / `MDI.INI`：Miles 3.03、SBPRO.DIG、SBPRO2.MDI；
- 当前根目录 `ATK00.WAV..ATK23.WAV`、`E00.WAV..E52.WAV`、`GAME01.XMI..GAME24.XMI`。

IDA 9.2 仅通过 `idat.exe -A` headless 运行。IRQ 源块和 trampoline 在导出时临时强制解码；命令结束后恢复跟踪中的 IDB，只提交脚本、报告和成功日志。

## 2. IRQ1 与键态

### 2.1 翻译表

`byte_51B16` 的 84 个有效初始字节是：

```text
00 1b 31 32 33 34 35 36 37 38 39 30 2d 3d 08 09
51 57 45 52 54 59 55 49 4f 50 5b 5d 0d 82 41 53
44 46 47 48 4a 4b 4c 3b 27 60 83 5c 5a 58 43 56
42 4e 4d 2c 2e 2f 84 2a 85 20 86 c9 ca cb cc cd
ce cf d0 d1 d2 87 88 9d 9e 9f 2d 9a 9b 9c 2b 97
98 99 96 89
```

它把 PC/AT set-1 make code 映射到原版内部 key code：字母为大写 ASCII；`Esc=0x1B`、`Enter=0x0D`、`Backspace=0x08`、`Tab=0x09`；方向小键盘为 `End=0x97, Down=0x98, PgDn=0x99, Left=0x9A, Center=0x9B, Right=0x9C, Home=0x9D, Up=0x9E, PgUp=0x9F`；F1–F10 为 `0xC9..0xD2`。

表后不是独立安全数组：`last raw scan @ 0x51B6A`、`last key @ 0x51B6B`、键态基址 `0x51B6D` 与表地址连续。处理器以 `scan & 0x7F` 直接索引 `byte_51B16`，所以不支持的 `0x54..0x7F` 会读到相邻可变字节。现代模型保留这一地址别名，而不是先做安全范围裁剪。

### 2.2 make/break 状态机

IRQ 体严格按以下顺序执行：

1. `in al, 0x60`，保存 raw scan；
2. `translated = memory[(raw & 0x7F)]`；
3. make (`raw < 0x80`)：
   - 若 `state[translated] == 0`，先写 `last_key=translated`，再把 state 加 1；
   - 无条件再把 state 加 2；
   - 8-bit add 溢出时连续减 2，因此值稳定在 `254/255` 而不回绕；
4. break (`raw >= 0x80`)：清零对应 state，并清零全局 last-key；
5. 严格执行端口 `0x61` acknowledge、向 PIC `0x20` 写 EOI、清 busy、`sti`、返回。

因此首次 make 得到 state `3`，typematic make 依次得到 `5,7,...`。测试必须覆盖首次边沿、重复、break、`&1` 消费、`254/255` 饱和以及不支持 scan 的别名行为。

机器码没有过滤 set-1 的 `E0/E1` 前缀。SDL 适配器必须把右 Ctrl/Alt、keypad Enter/`/`、独立导航键、GUI 键、Print Screen 和 Pause 重新展开为原始 multi-byte 序列后再逐字节送入上述状态机。例如独立 Up 为 `E0 48 / E0 C8`，Print Screen 为 `E0 2A E0 37 / E0 B7 E0 AA`，Pause 为 `E1 1D 45 E1 9D C5` 且无 break。由于 `E0/E1 >= 0x80`，前缀自身会按 break 路径经越界别名清零动态 state 并清 last-key；不得只保留序列末字节。F11/F12、SysRq、非 US `\\` 和 keypad `=` 的单字节码同样必须经过别名内存，而不是因超出 84-byte 表而预先丢弃。

`sub_20C32` 先把 last-key 清零，再自旋到 IRQ 写入非零值；菜单代码还会直接读取、比较和清除此字节。现代输入层必须同时暴露 last-key 和 256 项 byte state，不能只产生一次性高层 action。

### 2.3 scene主循环实时键态分派

`sub_28E40 @ 0x28E40..0x29391` 的稳定循环每tick最多消费一个输入动作，固定优先级为`left -> up -> down -> right -> interact -> main_ui -> weather_disable -> idle_update`。四方向各检查两个别名并按完整非零byte判定；Enter/Space/Insert同样接受任意非零byte。Escape主菜单与L天气关闭只检查bit 0，所以状态2不会触发，状态3会触发并在消费后变为2。

四方向在delegated movement之前清空所选方向的两个别名。interaction、UI和weather的顺序相反：分别在`sub_29C36`、`sub_212C0`或天气状态写0完成后，才清确认组三byte、Escape bit 0或L bit 0。高优先动作不改任何低优先键态；低优先键只有在后续tick仍处于对应live状态时才可触发，期间keyup会清除它。

首轮最终对照发现SDL/runtime曾把三类非方向keydown锁存到跨tick布尔量并过早回收键态，导致已keyup的低优先动作仍会补触发。现改为每tick直接从`LegacyKeyboard`采样：方向先选并在movement前清别名；其余三类只记录当tick获胜动作，reset token延迟到同步场景continuation返回后、公共scene-tail present前应用。修正后从函数入口重审全部306条指令与68个基本块无新增差异。独立9组向量SHA256为`33247c06e5fc4480904d29eead2f7efde47fd0a40c97b01209882fbf09b77abb`，正式`scene-goldens.json` SHA256为`6deadaf03021c564c167ff47d4ac49ed7b8fda6f0b443b2a691f349b5569bec3`。

### 2.4 主菜单进入物品页的旧键清理

`sub_2A0D9 @ 0x2A0D9..0x2A10F`是54字节单基本块包装器。唯一caller `sub_212C0:0x21433`在主菜单selection严格等于2时进入；caller已经清last-key和Enter/Space/Insert三项键态，包装器在栈探测后再次无条件清last-key，随后才依次调用物品reset、初始draw/present和selection loop。包装器内不读取last-key，selector返回值也被caller忽略。

现代`GameMenuController`在selection 2确认时切到items并把page/row/column归零；`LegacyGameRuntime`按dispatch前保存的main screen回报`confirmation_group`。SDL在同步回调返回后清三个确认键态并无条件`clear_last_key()`，然后才轮询下一事件。该不可重入边界合并了机器caller与包装器两次相邻清零，且中间现代代码不读取last-key，input owner无可观察差异。初始物品reset/draw/present/select及world/scene物品画面仍是同址UI与delegated callee的独立owner，不从本项传播closure。

机器合同SHA256为`59bb83d28586b8871cf9446349b0d1cd1840488e990a88e1f14c0e061cf4b389`；Order19关闭时的`title-menu-new-game-goldens.json`经三次一致生成后SHA256为`257e36bc9078c4c57f14b06c5d87c9b6130efabedd6e7e43ec6a5fa45307bc7b`。

### 2.5 物品选择只接受Enter与Space

`sub_2A86C @ 0x2A86C..0x2B227`为2491 bytes、548条指令、137个基本块、77个条件分支、35个无条件跳转（34个本地、1个共享尾出口）和47次call。167项重定位均为`+0x20000`；raw/loaded SHA256分别为`7494f3fde6bf4003b02830c6fb99e3899e2332bb3ccf94367f4eaf254347ae9f`与`813c4e3369973a4ed502e1c2193d4574d8f64340aadc0623769a07d092b4fd98`。函数没有本地`RET`，最终跳到归属`sub_29D2D`的共享清理尾`0x2A0D1..0x2A0D9`，本项不传播共享尾closure。

选择循环精确接受Enter`0x0D`、Space`0x20`、Escape`0x1B`、Down/PageDown/Left/Right/Up/PageUp`0x98/0x99/0x9A/0x9C/0x9E/0x9F`。keypad Insert`0x96`沿小于Down但不等于Enter/Space的失败路径继续等待，**不确认物品**。方向与翻页按5列、3行及page 0..37边界更新；确认索引为`5 * (page + row) + column`，并严格要求映射item的`show_introduction == 1`。

首轮对照发现world/scene的`GameMenuController`和battle的`BattleSession`都错误接受Insert确认物品。现仅在这两个物品选择上下文把确认集合收窄为Enter/Space，其他菜单和战斗动作的Insert确认不变。修正后从入口重新覆盖548条指令和全部137块，14次last-key写0、选择循环读取、两次严格大写Y读取、两个caller及全部出口零新增input owner差异。

机器合同SHA256为`32629e0bed2b0284822887666be436db69ac8e37f75dcb3770533ca7e40ab4c1`，20组状态向量SHA256为`f80f863c607a8d9aea5b9a678e7a9bb644608cc6c57d838d014479df2f49f09a`。两份临时Golden与正式第三次生成逐字节一致；当前`title-menu-new-game-goldens.json` SHA256为`33f8bd403715655e0a49b3f287e0c4ff9c2d8ca2cf0a2b9239f8c6a96e469d04`。同址UI绘制、物品资格/效果、库存、事件与共享尾owner继续独立审计。

### 2.6 战斗问句必须先呈现再接受任意键

`sub_2DD77 @ 0x2DD77..0x2DE03`为140字节、36条指令、3个基本块、6次call、9项重定位和2个本地`RET`。raw/loaded SHA256分别为`224f32fa14859850cd390ce56d0e8e6a15b4e15a3c392cad7c08ddad40953cb9`与`029ba93c5d02ac74cde1c9cd81457e1b7a2d5d03fa5dd57ab9bb1148b2982e50`。唯一物理caller `sub_2C319:0x2C4DE`按signed word传入真假offset，并固定执行`old_pc + 3 + returned_offset`。

机器先清last-key，格式化“是否與之過招（Ｙ／Ｎ）”，在当前底图绘制`(61,40,187,27)`面板和`(71,45)`阴影5/前景7文字，再于`0x2DDE3` present。随后`sub_20C32`再次清last-key并等待第一个非零翻译键；`0x2DDF0`只把大写`Y`判真，其他任意非零键立即判假，按键后不额外重绘或present。

input owner首轮对照发现SDL一帧会排空多个事件，现代问句生成后可能在首帧尚未present时被同批后续keydown回答，违反机器`present -> clear/wait`顺序。现仅为scene与world-event问句增加present门：问句产生时关闭，`finish_presented_tick`确认成功present后打开，回答前立即关闭。零翻译键仍在入口忽略，大写Y/其他非零键分支不变；其他菜单和输入不受影响。

修正后从入口重新覆盖全部36条指令、全部3块、唯一caller、两个出口、43条资产调用及两条宿主路由，零新增差异。宿主回归固定问句生成后及render后但present前均拒绝回答，present后keypad Insert作为任意非Y键立即走假分支。两次独立Golden与正式第三次生成逐字节一致，Order22关闭时`scene-goldens.json` SHA256为`2dd944ea065671509134cd989aaf0a8cb3b706bcd45827ba196fb298d01c7544`；同址scene绘制/脚本owner保持独立已关闭，不由本项传播closure。

### 2.7 加入问句回答后必须先恢复裸场景

`sub_2DE7D @ 0x2DE7D..0x2DF0E`为145字节、37条指令、3个基本块、7次call、9项HIGHLOW重定位和2个本地`RET`。raw/loaded SHA256分别为`17f05acd776abbc7b1add52c71897b59e31f4c167d7c0d4642d9201daa6763cb`与`bbaddd1162733a447cb95055c3e87a400815b84532ef2d089424440ae6feaed6`；唯一物理caller为`sub_2C319:0x2C57B`，另有跳转表数据引用`0x555E0`，没有外部内部入口。

机器清last-key，复制23字节`是否要求加入（Ｙ／Ｎ）`，在当前底图绘制`(61,40,187,27)`面板和`(71,45)`阴影5/前景7文字并present；`sub_20C32`再次清键并等待第一个非零翻译键。取得键后Y/非Y均无条件调用`sub_2D653`重绘并present裸场景，最后才严格比较大写`Y`并返回真/假signed offset。caller共享尾固定执行`old_pc+3+selected_offset`。等待helper SHA256为`d1f87751e3589507ed555b84cc4a8e5523937fd67282944b568b34e6eeaa3a55`；裸场景present helper SHA256为`51d1df23db48b1b1489112a5356a8138c387a8667fe8050ab726de579b87fc36`。

现代沿用Order22已发布的共同问句present门，因此opcode9首帧在成功present前同样不接键。join专用`conditional_after_present`在回答时只缓存signed offset并返回裸场景present；宿主成功present后才恢复解释器、把offset加到已推进3 words的PC。机器的Y比较发生在裸场景present后，现代不可观察的选择缓存形成于其前，但该present链不读取选择或last-key，脚本状态和副作用也保持阻塞，故可观察顺序一致。

宿主回归串联opcode5/opcode9，固定新join问句重新关闭present门、render后但present前拒绝回答、present后keypad Insert按任意非Y键进入裸场景present、该present完成前继续拒绝Y、完成后才结束脚本。完整入口、所有分支/出口、唯一caller、两个短helper、81条资产调用和scene/world-event两条宿主路由复核未发现新增产品差异。三次Golden逐字节一致，Order23关闭时`scene-goldens.json` SHA256为`e03f90d696b38adc17e9917f70a361e78acaf6bbbb7334c272287f726fa1ce61`；同址scene owner已独立关闭，`sub_2D653`/`sub_3D6D1` owner状态不传播。

### 2.8 住宿问句回答后立即分支

`sub_2E155 @ 0x2E155..0x2E1E8`为147字节、37条指令、3个基本块、6次call、10项HIGHLOW重定位和2个本地`RET`。raw/loaded SHA256分别为`9d4b0c7d1e4ef096ee12bb3e010ca7181a9fb561b9c94eadd27d4edb78a4a5c3`与`fb29e361de2fee971ffaf9d5c210d25e90f8b91b3060d4d98852278934f79cd3`；唯一物理caller为`sub_2C319:0x2C5A7`，另有跳转表数据引用`0x555E8`，没有外部内部入口。

机器清last-key，复制23字节`是否住宿過夜（Ｙ／Ｎ）`，绘制与前两类问句相同的panel/文字并present。present后`0x2E1C9`先显式清键，紧邻的`sub_20C32`入口再清一次并等待首个非零翻译键；两次清零间没有读取、call或其他可观察操作。最后仅大写`Y`选择真signed offset，其他非零键选择假offset；caller共享尾执行`old_pc+3+selected_offset`。回答后不调用裸场景重绘或任何额外present。

现代共同问句present门同样覆盖rest：问句首帧成功present前拒绝同批keydown，present后大写Y为yes、其他translated非零键为no。rest不进入join专用`conditional_after_present`，selected offset立即应用，后续脚本输出直接成为pending。机器相邻两次pre-wait清零在现代不可重入事件边界合并为一次，没有中间观察点。

宿主回归把opcode5/9/11串联，固定join恢复present后rest问句重新关门、render后但present前拒绝Y、present后keypad Insert按任意非Y键直接结束脚本且不产生present。最终入口/出口、唯一caller、等待helper、7条资产和两条宿主路由复核零新增差异。三次Golden逐字节一致，Order24关闭时`scene-goldens.json` SHA256为`42c4008cf235d4fb1894dd39bb3a6db677b0fd01310a07d4fce37d381fa3895c`；同址scene owner和delegated UI/render owner状态均不传播。

### 2.9 死亡菜单每次显示只接收一个键

`sub_2E659 @ 0x2E659..0x2EB49`为1264字节、330条指令、28个FlowChart行、42 calls、15个条件分支、8个无条件跳转、79项HIGHLOW重定位和零本地`RET`。raw/loaded SHA256分别为`832743bec15c92ed76da1680bce82e80485330dba141da442ba5634fb5b936d5`与`e9350b3775deca2ee7ec049f92d20f411f1f1a1a6c3b33f493bcaaec85ca9826`；两个物理caller为opcode15 dispatch和五轮试炼非胜利出口，另有`0x555F8`表引用，无外部内部入口。

input机器合同固定每轮先present菜单，再读取一个last-key byte。Down `0x98`、Up `0x9E`、Enter `0x0D`、Space `0x20`及keypad Enter `0x96`按原分派处理，其他值保持selection；无论方向键、未知键还是确认键，下一次读取前都先重绘并present。前三项清屏present后经共享epilogue返回0-based读档槽。selection3先present确认框并经两次相邻清键等待非零键，仅uppercase `Y`退出；其他键重建selection3后重新present。

首轮现代对照发现宿主会把同一SDL事件批内的多个death-menu按键连续送入`SceneSession`，中间frame尚未present。预修复构建`proc_6a1a`直接证明render后、actual present前的Down已改变选择。最小修正增加death-menu专用present门，每个新菜单结果关闭、`finish_presented_tick`打开、消费一个键前再次关闭；问题、读档菜单和底层菜单业务不变。

修正后从入口重新审计全部330条指令，没有第二处差异。宿主回归覆盖首帧前、render-before-present、未知键、连续方向键、确认框、lowercase y、keypad Enter与uppercase Y；最终Linux app Debug `proc_c46f`通过14/14。三次Golden逐字节一致，正式`scene-goldens.json` SHA256为`8f78b137f17fc1d614eb84b0afc7201512d67a0d0ffb509397f79958a3ff3bba`；同址scene、共享退出及delegated owner状态不传播。

## 3. 时间边界

### 3.1 tick 来源与主循环

`dword_544D8` 初始指向 BIOS Data Area `0x046C` 的 32-bit tick。主循环和场景/战斗循环在一帧开始保存 tick，帧尾自旋直到当前值不同。行为合同是“观察到一次 tick 值变化”，不是以宿主 delta-time 累加和补跑多帧。

现代高精度时钟用 PIT 基准 `1,193,182 / 65,536 Hz` 生成兼容 tick，并在 BIOS 日界值 `0x1800B0` 回绕。等待逻辑只比较相等/不等，因此必须自然跨过回绕边界。

`main @ 0x20D35` 在同一 tick 按 left→up→down→right→Esc menu→idle 只执行一个分支。四个方向命中时不清 Esc 对应 state 的低位；只有实际进入菜单后才执行 `state[Esc] &= 0xFE`。SDL 主循环因此在世界态延迟 Esc keydown，把 `keyboard.edge(0x1B)` 与四方向键态同时交给 runtime；runtime 回报实际打开菜单后才调用 `consume_edge(0x1B)`。方向与 Esc 同按时先移动，若 Esc 在方向释放前 keyup，则不会事后补开菜单。

每个 world tick 完成重绘/呈现后，原入口把全局计数 `(counter+1)%5` 写回；余数为1时调用 `sub_3CBE3`，将 RGB6 palette entries 224..231 和244..252 各自右旋一格并立即提交 DAC。现代 `finish_presented_tick()` 只在宿主 present 成功后推进 `LegacyGameRuntime` 持有的全局相位并更新当前 world palette，使刚呈现帧仍使用旋转前 palette，下一帧才使用新顺序。进入 scene 时把同一相位传入 `SceneSession`，仅在原外层 scene present continuation 完成后推进并回写 runtime；普通对话/菜单等待宿主帧不推进，相位跨世界/场景往返持续。

### 3.2 `sub_3DB83`

input-font Order39从独立临时IDB固定`0x3DB83..0x3DBBF`为60 bytes、24条指令、6个CFG块、2个条件分支、1个无条件跳转、2处HIGHLOW重定位、1次栈探测call、37个callsite/16个caller owner和唯一RET。raw/loaded SHA256分别为`6aabd39558b1d8b637a2d2985b8c75ff9e47d686fd3e645473b490726cade438`与`0f5f1b33803d017e02d8f1704c13de5db083011da429dbc7876f9c575bbd87fd`；两处fixup回减`0x20000`后完整还原raw切片。`dword_544D8`原始初值为BIOS Data Area `0x046C`。

机器码执行signed `IDIV 40`并加1：

```text
wait_count = trunc_toward_zero(argument / 40) + 1
```

仅当`wait_count > 0`时，每轮捕获一个32-bit tick并等到任意不等值；跳过多个tick或`0x1800AF→0`跨日都只完成一次等待。必须保留：

- `0..39 -> 1 tick`；
- `40..79 -> 2 ticks`；
- `-39..-1 -> 1 tick`；
- `-79..-40 -> 0 ticks`；
- 37个实际参数的分布`1×1,17×3,30×1,40×1,50×14,100×11,300×2,340×2,500×1,2000×1`；
- 不按毫秒重新解释参数，不修正约18.2 Hz与除数40的历史失配。

逐块对照`legacy_delay_tick_count`、`wait_for_tick_change`、`wait_for_next_tick`、`legacy_delay`及caller已独立关闭的Session tick continuation，未发现合法域产品差异。steady-clock BIOS频率模拟、相等时宿主yield和同步自旋到可恢复phase的搬运归类平台适配；全部caller忽略机器EAX残值。独立signed/tick trace SHA256为`fe6d0d5c3ea65501da444006026713f1392e9ebca73408770103e6fc6900e3f6`；测试补齐全部实际参数、INT32极值、nonpositive零读取、重复值/跳tick/跨日精确读取序列。函数级完整证据见`research/evidence/functions/Z_DAT/0x3DB83.md`。最终`./build.sh app --config Debug`进程`proc_0a6c`通过Linux app Debug 14/14。

### 3.3 opcode27图片动画的tick边界

`sub_2F053 @ 0x2F053..0x2F107`为180字节、59条指令、14个CFG块、6次call、5个条件分支、4个无条件跳转、9项HIGHLOW重定位且无本地`RET`。raw/loaded SHA256分别为`7bfb0b81ff77dc3bc3544b6e692b4b177e21c58e7f5007367bb090a2b9675bdb`与`cc5c53f5a16319098f5bfa8813a847dc50a72dcda23eb377d54f792d7af5556d`；唯一物理caller为opcode27 dispatch，另有`0x55628`地址表引用。两个正常出口都跳入前一函数`0x2F04A..0x2F053`共享尾，其SHA256为`f7d57baaeaca8f29028fb0af30bc53418bee7a398b4188a9c960681e3dd7e2b5`。

本input/tick owner不读键盘状态。每个有效图片帧都严格按以下顺序运行：先保存`*dword_544D8`，再写玩家图片低16位或调用事件修改callee写三个图片word，然后完整重绘场景，调用`sub_3DB83(50)`等待2个BIOS ticks，最后仍自旋直到当前tick不等于帧首保存值。正常单调时钟中2-tick helper已经满足末尾条件，因此没有固定第三tick；零帧路径完全不读tick。循环值按signed 32-bit执行`+2`和`<=end`，只有图片写入截低16位。

现代`SceneSession::advance_picture_animation_frame`先写图片状态再返回`present(wait_ticks=2)`；`LegacyGameRuntime::render`只有成功绘制该状态后才把present effect标记为已显示，`advance_scene_effect`在首个宿主tick仅把2减为1，第二个tick才恢复解释器并生成下一图片或继续已提前推进4 words的PC。新增真实opcode27宿主回归固定未render不能推进、5002/5004/5006每帧恰好两tick及末帧后续PC；既有独立Golden覆盖43次真实资产调用、579帧、event `-1`/非`-1`分支、`-2`别名、32767边界、零帧和奇数跨度。三次Golden逐字节一致，正式`scene-goldens.json` SHA256为`8f78b137f17fc1d614eb84b0afc7201512d67a0d0ffb509397f79958a3ff3bba`。

本轮只独立关闭`input-font-closure.tsv audit_order=27`的tick引用/宿主时序owner；同址scene owner此前已独立关闭，`sub_3DB83`完整time owner、两个scene callee、编译器栈探测及共享尾owner均不传播状态。入口59条指令、全部14块、唯一caller、两个分支和全部出口最终重审无产品差异。

### 3.4 opcode44双图片动画的tick边界

`sub_2F9F2 @ 0x2F9F2..0x2FAB7`为197字节、71条指令、12个CFG块、5次call、4个条件分支、3个无条件跳转、6项HIGHLOW重定位和1个本地`RET`。raw/loaded SHA256分别为`e294edd4540be60f0979db3fe6c9c7154a08c342a0dba3d0c6718fe8eecca882`与`d9e2c1cd2614ef204601d60d2724eab9897956fa3c010ab62c6d74079cb197b5`；唯一物理caller是opcode44，另有`0x5566C`地址表引用。caller传入六个sign-extended word，callee完全不读`second_end`，本地返回尾SHA256为`bbe999c7b5b6b5ce26bc05b647d0bccb2d042eb83e9797d27928d655fbcab3dd`，caller清理24字节并把PC推进7 words。

循环只由signed 32-bit `first_counter<=first_end`控制；first/second图片和独立counter每帧都加2，图片仅在写word时截低16位。每帧先捕获tick，严格按first后second写两个目标：event精确等于`-1`才写玩家图片，其他值交给事件callee。两个目标同为玩家或同一事件时，second在render前覆盖first，因此只显示second；两张图片都在完整场景render调用前先执行寄存器`+2`，但渲染读取已经写入内存的旧值。随后`sub_3DB83(50)`等待2 ticks并确认tick已离开帧首值。`first_start>first_end`时零写入、零render、零tick等待。

现代opcode44只把前五个参数构造为`DualPictureAnimationState`，两个控制bool默认false；因此两个`-1`都调用`set_animated_picture`且second覆盖first，第六参数自然无读。`advance_dual_picture_animation_frame`以C++ `int`执行first终点判断和两图片`+2`，每帧返回`present(wait_ticks=2)`；宿主成功render后才开始两tick倒计时，第二tick才恢复下一帧或已推进7 words的PC。真实opcode27/44串联回归固定5016/5018/5020第二序列、无视`second_end=-30000`、未render不推进和每帧恰好两tick。

独立Golden覆盖6次真实opcode44调用、script534的18个双事件逐像素帧及start>end、双玩家覆盖、32767后32位退出和奇数终点四组合成边界；参数流SHA256为`3261a421b7f4bc68691ba6c590451cefd773d308a22a43e59a66930470a88f7a`。三次生成逐字节一致，正式`scene-goldens.json` SHA256为`8f78b137f17fc1d614eb84b0afc7201512d67a0d0ffb509397f79958a3ff3bba`。本轮只独立关闭`input-font-closure.tsv audit_order=28`，同址scene、四个callee和本地返回尾均保持owner隔离；最终入口重审无产品差异。

### 3.5 opcode57三雕像动画的tick边界

`sub_301D1 @ 0x301D1..0x302E0`为271字节、90条指令、11个CFG块、8次call、5个条件分支、零无条件跳转、11项HIGHLOW重定位和1个本地`RET`。raw/loaded SHA256分别为`a564d127f7e97066229213af75c152c3b54d326ae7ffe779a8801b52a5764a13`与`bc69b6a118c46a189cbb0b0c4450647230f3f1330fe0a439477c06fe60a73179`；唯一物理caller为opcode57 `sub_2C319:0x2CB0D`，另有`0x556A0`地址表引用。返回0被忽略，caller经公共块把PC推进1 word；本地返回尾SHA256为`d187e6ec96b89e29cf0712d92b3bd72c4fda595d7c6817db2666e557ba741057`。

机器分两阶段。阶段0以signed 32-bit计数执行7664..7674含端点、步长2，共6帧，每帧写玩家图片低16位。阶段1把计数清零，执行0..56含端点、步长2，共29帧；每帧先按signed int16判断当前玩家图片`<7688`，满足时写`counter+7676`，故counter=12写到7688后冻结；再严格按event2、event3、event4顺序把current/end/begin三字段分别写为`counter+7690/7748/7806`，其余八个事件字段参数和场景选择参数均为`-2`保持。两阶段每帧均先捕获BIOS tick，完成图片写入后render、`sub_3DB83(50)`等待2 ticks、确认tick已离开帧首值，最后才把32位计数加2。总计35帧，最终玩家7688，三事件图片7746/7804/7862。

现代`ThreeStatueAnimationState`以C++ `int`保存phase/value；case57建立默认`phase=0,value=7664`并先把PC推进1，但动画状态优先于脚本继续执行。`advance_three_statue_animation_frame`保持6+29帧、signed玩家冻结条件、event2→3→4写序与三字段同步；内部计数提前加2只保存下一帧状态，本帧可见图片不变。每帧返回`present(wait_ticks=2)`；宿主未成功render前不推进，首次成功呈现只消耗第一tick，第二次成功呈现后才恢复下一帧，等价于同步render/delay/tick自旋。

独立Golden固定唯一真实script655 PC47、scene14事件2/3/4坐标与初始字、35个玩家/三事件/逐像素帧；调用流SHA256为`09d3a95aa4379601d167e041d3e98651e478fe12dde36e9118a11e0b0f51c700`，35帧trace SHA256为`8cf0a9274a8549457160fbb8ec526bf7d61592a1f5cd4b7a678da8043b7b1647`。三次生成逐字节一致，正式`scene-goldens.json` SHA256为`8f78b137f17fc1d614eb84b0afc7201512d67a0d0ffb509397f79958a3ff3bba`。scene14最小`57,-1`脚本宿主回归进一步逐帧固定未render不推进和两个成功呈现tick；原始script655路径仍由独立scene测试与Golden覆盖。scene owner此前独立关闭；本轮只关闭`input-font-closure.tsv audit_order=29`，事件、render、time、栈探测和本地返回尾owner不传播状态，最终入口重审无产品差异。

### 3.6 opcode62结局前置动画的tick边界

`sub_30B81 @ 0x30B81..0x30C38`为183字节、63条指令、10个CFG块、6次call、4个条件分支、1个无条件跳转、5项HIGHLOW重定位且无本地`RET`。raw/loaded SHA256分别为`a02fbf9c48bdf528c673f9c7e3917e42f58e6662f5a22abd5b0f5a3629a436ee`与`8bb744ce90e6740072386328c6f49a888098cc6e42612c14f8a811d0d247d592`；唯一物理caller为opcode62 `sub_2C319:0x2CBA7`，另有`0x556B4`地址表引用。caller压入六个sign-extended word，但callee完全不读`second_end`；末尾无栈回收、无PC推进、无返回，直接`call sub_30C3D`转交完整结局，五字节SHA256为`3c94ca4056d7f52660e0aae426bd783a03702f3e8903ef2d1d0008ce98192d53`。

函数在任何循环判定前先把玩家图片写为signed `-86`。循环只由sign-extended first_start与first_end的signed 32-bit闭区间控制，第一、第二图片值每帧各加2，第六参数不参与。每帧先捕获tick；first/second event各自只有精确等于`-1`时跳过，否则严格按first后second调用事件primitive，同步current/end/begin三图片，其他八个事件字段参数和场景选择参数为`-2`保持；因此同一事件由second覆盖first。两图片寄存器在render前加2，但本帧内存图片仍是旧值；随后render、`sub_3DB83(50)`等待2 ticks、确认tick离开帧首值，最后first控制计数才加2。start>end时零动画帧但仍隐藏玩家并转交结局。

现代case62先写`player_frame_override_=-86`，只把前五参数保存为`DualPictureAnimationState`，并置`skip_negative_events=true`、`quit_after=true`；`advance_dual_picture_animation_frame`保持first范围、两图片32位状态、精确`-1`跳过、first→second写序和两tickpresent。PC+7虽预先保存，但动画状态优先且结局不返回，机器无回收/无PC推进的中间值不可观察；最后一帧第二tick后直接`start_ending()`，等价于无返回调用相邻结局。

独立Golden固定唯一真实script1017 PC5、38帧8054..8128与8130..8204图片、76组事件参数和逐像素画面；调用/图片/事件参数/帧流SHA256分别为`21848ec618b479c85562cc0ae4602b6934a31f6d7324957eff94cf54686274e8`、`f4b057366e7eae4fa207716b3842548150b0d0943fd31e8acb3e38111a2b0430`、`bec4a864813a27c7d5695ce5de579b245777de7338fbd2d90dbd2fc48ee796a6`、`50b9832fc97d5f03be8847a59ba1197487ad15c73594d3d11405049d6a7404ef`。三组second_end反例完全相同；四组合成向量再覆盖`-1`跳过、同事件后写覆盖、start>end与32767后32位退出。三次生成逐字节一致，正式`scene-goldens.json` SHA256为`6f04fa857b4477525553b24f04e6c51b249e725757d2b697c2c7f0a8297ae878`。scene83最小opcode62宿主回归逐38帧固定未render不推进、两个成功呈现tick及末帧结局淡出。scene owner此前独立关闭；本轮只关闭`input-font-closure.tsv audit_order=30`，事件、render、time、栈探测、结局及caller后续物理字节owner均不传播状态，最终入口重审无产品差异。

### 3.7 opcode64商店每帧单键与清键边界

`sub_312A6 @ 0x312A6..0x3192E`为1672字节、473条指令、79个CFG块、34次call、40个条件分支、22个无条件跳转、68项HIGHLOW重定位且无本地`RET`。raw/loaded SHA256分别为`b5eb765deb1994347ca33820aaf9b78b75a29c8da36d712ef4d2a1fdda289597`与`a8ac857e29c681aa581f539506ce0055526b3b5cccad17619ec46deae4a22980`，68个loaded dword各减`0x20000`后与raw逐字节一致。唯一可执行caller为无参数opcode64 `sub_2C319:0x2CBCB`，`0x556BC`是地址表引用；返回`EAX=1`后跳外部共享尾，caller忽略返回并经公共块PC+1。共享尾`0x2D36A..0x2D372` SHA256为`008ac1116ed948abd324b4fcb16f6fb87d6e6c5256e8954e2b3d3673759ac057`，不属于本owner。

函数唯一last-key读取为`0x314DB mov bl,byte_51B6B`。`0x98`下与`0x9E`上按压缩后的可见商品索引回绕；`0x0D`、`0x20`、`0x96`确认；`0x1B`取消；数字`1..5`及其他值不触发选择或购买。下/上分支分别把last-key与对应方向flag清零；三确认键共享出口，清last-key与三项确认组flags；Esc清last-key与escape flag；未知值不清。只要没有确认或取消，机器都会重绘商品列表、调用present primitive，再回到唯一last-key读取；因此初始`last_key=0`也先完成一次可见商店帧，随后每个继续中的键都必须先呈现再读取下一键。

正常五项列表中下键`0→1`、`4→0`，上键`0→4`、`3→2`。全空列表的机器局部可见索引仍初始化0，下键得到1、上键得到-1，随后绘制会发生局部数组越界；现代将空列表显示索引稳定为-1，忽略上下导航但仍按对应translated组清键，分类为平台适配。直接确认空列表仍按机器选择物理slot0且不复核stock。

本轮入口REVIEW发现现代宿主此前可在商店成功呈现前、或同一SDL事件批次内连续消费多键。最小修正新增`scene_shop_presented_`：每次进入/继续shop时关闭，仅在`render`成功并完成presented tick后开启；消费一个键立即关闭。scene1/script938真实宿主回归证明预呈现按键不消费、数字/每个上下键后必须重呈现、首尾回绕、三确认键清理组、Esc清理组及空列表稳定化。12组合成机器向量覆盖`key=0`、数字、普通/边界上下、空列表上下、三确认和取消，向量SHA256为`3dedf37e87641f62bf399b79c1877cd6b656993fc98d4065db39e3013e017515`。两份临时Golden和正式第三次生成逐字节一致，正式`scene-goldens.json` SHA256为`bfc30f29b9f36fd15b808eb83dd070efc98a0ce19fbfac8594969f5053eb3343`。

商店五场景映射、压缩槽、面板、价格、购买时序、对话、事件关闭及库存primitive由scene和各callee owner独立关闭；本轮只关闭`input-font-closure.tsv audit_order=31`的last-key与宿主呈现职责，不传播共享尾或callee状态。修正后废弃旧结论并从入口重审79块/473条指令，未发现新增差异。

### 3.8 战斗队伍选择的flag优先级与逐呈现消费

`sub_31EB9 @ 0x31EB9..0x3265C`为1955字节、453条指令、86个IDA flow block、19次call、40个条件分支、25个无条件跳转、144项HIGHLOW重定位和1个本地`RET @ 0x3265B`。raw/loaded SHA256分别为`04dbdb0cdb56c248d69df22853e4a21aa8939152839f778631b51cd5602608ae`与`6743a9d962317bde209fd1a6c36b54a60678d374fa9ca5a90dfa6b9a934feb0f`，144个loaded dword各减`0x20000`后与raw逐字节一致；唯一caller为`sub_31C75:0x31CAF`。

输入循环在`0x3218F`只清last-key；每轮先重画场景与选择UI，并在`0x324C0`调用present，再按down `0x51C05`、up `0x51C0B`、Enter/Space/keypad Insert `0x51B7A/0x51B8D/0x51C03`顺序检查。down只清down，up只清up；三确认键任一命中都清整个确认组。每次命中均回到重画入口，因此一次成功present最多消费一个方向或确认组；并发flag严格按down > up > confirmation逐帧处理。方向循环、mandatory不切换、状态0/1切换、按当前combatant count取坐标、空确认重画及非空确认返回均由本函数完整机器合同锁定。

首轮现代对照发现party selection未使用battle key-state路径，可在成功呈现前改选并于同一SDL事件批次连续消费多键。最小修正将该phase纳入现有键状态通道，并在`BattleSession::finish_presented_tick`确认成功render后按机器优先级消费一个锁存组、发出对应精确清键请求；预呈现按键保留到首个成功present。真实battle0宿主回归覆盖未render不消费、并发down/up分两帧按优先级处理、三确认合并清理、mandatory稳定与非空确认退出。

独立`battle_setup_machine.party.input`的10组向量覆盖无输入、首尾回绕、多flag优先级、三确认清理、mandatory、选择切换、空确认与非空确认，向量SHA256为`ab7b7329846275c9fed00e71c4cf20142b7fbdf2a6838dd8f824b4e824851238`。两份临时Golden和正式第三次生成逐字节一致，正式75键`battle-goldens.json` SHA256为`1f063b707ad9924e872a2224596baf499baa21fffbe7093f6f3bc43c54349ac6`。同址battle owner、caller、紧邻`sub_3265C`和7个callee owner保持独立；本轮只关闭`input-font-closure.tsv audit_order=32`。发现差异后废弃旧结论并从入口重审86块/453条指令，未发现新增差异。

### 3.9 战斗回合槽入口确认态与首黑帧排序

`sub_3271E @ 0x3271E..0x32A51`由Order33独立临时IDB冻结为819字节、166条指令、47个基本块、23个条件分支、4个无条件跳转、13次call、69项HIGHLOW重定位且零本地RET。raw/loaded SHA256分别为`d8e0befe7bfaa838fb81780210ec2814c6f3898b005fc6f6342e4a64906997dc`与`03fb8619f4d161ca82d1c6b26f20c3b08f56b129855f65af27771a8fe24328b3`；唯一caller为`sub_31C75:0x31D39`，`0x327DD`条件出口使用共享`0x35407..0x3540E`。

每个slot入口依次检查Enter `0x51B7A`、Space `0x51B8D`、keypad Insert `0x51C03`的当前byte state。任一非零即在hidden检查前清三项并把automatic清0；按键若在slot边界前已释放则不取消automatic。hidden槽跳过绘制/动作但仍执行战果与隐藏目标清理；可见槽先定位次光标和相机，再render/present、清action/AI scratch，并仅在side恰0且automatic恰0时进入玩家菜单，否则进入AI。动作返回6抵消公共slot递增；全槽结束后轮末状态只执行一次，并以轮首保存tick无重绘等待变化。

首轮C++对照发现`initial_present`此前先绘制/present黑色indexed frame，回调后才排序和定位；机器严格为`sort -> slot0定位 -> render -> present`。全黑调色板虽遮蔽RGB，indexed framebuffer与setup状态仍可观察。最小修正把排序和定位前移到`begin_initial_battle()`，不改变淡入帧数。独立原资产Golden同时保留caller继承`view=0,0`帧，并新增回合入口`view=19,13`、1185命令、FNV64=`0x03446a8a41ef2ec6`的首黑帧区分向量；双生成和正式第三次生成一致，正式`battle-goldens.json` SHA256为`c8a083ec57902eec86d71a3086c365a6370d65fc8f49767e40f529e36e66ba7c`。Linux app Debug 14/14通过。

本轮只关闭`input-font-closure.tsv audit_order=33`；同址battle owner、caller、共享尾、排序/交换、玩家/AI动作、战果、hidden清理、轮末状态、render/present/fade及time owner均不传播。修正后从入口重审47块/166条指令及全部出口，未发现其他合法域差异。

### 3.10 战斗光标的持续键态、优先级与呈现门

`sub_36AF7 @ 0x36AF7..0x36E06`由input-font Order35独立临时IDB重新冻结为783字节、157条指令、38个CFG块、28个条件分支、7个无条件跳转、7次call、72项HIGHLOW重定位且零本地RET；raw/loaded SHA256分别为`880246a653a60137ff63b21d437f2722ffbfb14bb4a0dfa6f4b06d64d34215e9`与`cb0b440727ac96038276f0de9a9b139c820184b9dba7a5772b45c4b4800ed9de`。六个caller与唯一`0x36BA7 -> 0x3677E`共享尾出口分别保留owner边界。

入口只清Enter、Space、keypad Insert三个确认byte，保留方向和Escape；先完成两次`render -> present`才进行首次键态扫描。其后每次成功present最多按`down -> right -> left -> up -> Escape -> confirmation`消费一组。四方向分别接受两种翻译键码并在路径判断前成对清零；方向接受或拒绝、确认拒绝都会回到下一次present，Escape或接受确认则不额外present。signed `y*64+x`、`+64/+1/-1/-64`线性别名、path上限、occupancy悬停、movement `path>0`与targeting `path>=0`确认条件保持不变。

首轮只修正了首键前少一次present；随后完整SDL边界复查发现事件到达顺序仍会替代机器的固定多键优先级。该“仅一项差异且已收敛”结论已废弃。最终实现由`BattleSession`在成功present后扫描持续键态并请求清理一个状态组，`LegacyGameRuntime`转发，SDL持续采样八个方向别名、Escape与三确认键并成组清理。入口确认清理、预先按住、多键优先、每帧一组、Escape和拒绝确认回归均通过。修正后重新从入口覆盖完整157条指令、38块、全部分支/call/caller和共享尾，零新增合法域差异。

独立原资产Golden新增`cursor_selection.input_state_transport`合同；两份临时结果与第三次正式生成逐字节一致，正式`battle-goldens.json` SHA256为`11ed531110466abaff4fdd599f733385041093e6542eaa51f61bc0162ccf82bd`。本轮只关闭input-font order35；同址battle owner及caller、路径图、render/present与共享尾不传播closure。

### 3.11 玩家直线攻击方向的首帧门与固定优先级

`sub_37734 @ 0x37734..0x3859E`由input-font Order36使用独立临时IDB冻结为3690字节、837条指令、135个CFG块、81个条件分支、25个无条件跳转、34次direct call、226项HIGHLOW重定位和唯一`RETN @ 0x3859D`。raw/loaded SHA256分别为`e1e0b9a203500a28d37fbca2eba008c0d3cff9c507104c5ac1ab769a6808f833`与`b1c8388f04bc4660f3d13f0280000900a7e84beb4e5d562f2609487e8945b631`；226项loaded值逆减`0x20000`后逐字节还原原始`Z.DAT+0x31134`。玩家caller `0x33371`压`mode=0`，AI caller `0x34E52`压`mode=1`，二者均不观察返回值。

只有`mode==0 && area_type==1 && hit_index==0`进入方向输入。机器先绘制方向框并恰present一次，入口不清方向状态；随后无键只在`0x37C56..0x37D5B`忙等，不再调用render、present或delay。扫描固定为`down > right > left > up`，方向映射为`3/1/2/0`；对应状态对依次为`51C04/51C05`、`51C06/51C09`、`51C0A/51C07`、`51C0C/51C0B`。每个byte任意非零均有效，命中后只清对应双别名并保留低优先状态；没有Escape或确认分支。AI按目标差值自动定向，双击第二击复用首击方向，均不重新读取键态。

首轮现代对照发现`player_attack_direction`仍由单个SDL `KEYDOWN`立即分派，可在方向帧成功present前生效，且多键结果取决于事件到达顺序。此前同址battle owner“共享scale修正后零差异”的结论随之作废。最小修正把该phase纳入已有cursor持续键态传输，设置一帧成功present门；首帧回调按down/right/left/up顺序消费预按的一组，首帧无键时后续host advance直接扫描并复用已呈现方向框，不要求第二次方向present。命中后请求SDL调用`consume_world_direction`成对清理；入口不清方向，Escape和确认保持未消费，AI与第二击路径不变。

方向修正后的入口重审又发现现代在返回“熟练度升级”前已扣MP，而机器严格在升级框present及参数500等待之后才扣，并于此时重新读取selected slot的当前magic id与定义need MP。现拆分熟练度/RNG提交与MP提交；升级路径把MP提交延后到等待结束并按slot重新解析profile。独立HP/MP伤害callee每次命中也自行重读当前熟练度/need MP，所以首击升级可改变第二击伤害scale；外层只缓存几何/hurt/hit count。机器`0x378B0..0x378DA`还证明4096个effect word只在hit loop前清一次，当前C++本已如此，旧“每击清零”仅为证据文字错误。

全部修正后重新从入口覆盖837条指令和135块。12个半开地址阶段的块数为11、13、31、17、8、1、36、6、3、1、3、5，合计135且没有未分组块；34个call、两个caller、菜单取消和正常返回全部复核，未发现新增差异。回归锁定首帧前直接事件忽略、预按五态按down优先、首帧无键后right/left/up由host advance直接扫描且无第二次方向present、方向word写回、精确清理请求，以及升级present/等待期间MP保持、AI单击伤害完成后在等待中把定义need MP从5改为30并在等待结束时按重读值扣到MP0。方向判别向量SHA256为`c5e62579350f961039ec5a328798de022c31e8bcafb0f8f05f6ed9290902ecd9`；两份临时Golden与第三次正式生成逐字节一致，正式`battle-goldens.json` SHA256为`e23151ba9ed10c41131475cf0901cc24f3b5681b253ea56db01bd70ffe4dbf1b`；Linux app Debug通过14/14。

本轮只关闭`input-font-closure.tsv audit_order=36`。同址battle owner既有状态只作为交叉核对，两个caller、18个不同callee地址、目标/直线area、伤害、动画、renderer、present、delay、RNG及共享状态owner均不传播closure；原程序动态runtime oracle仍登记为`blocked_runtime_oracle`。

### 3.12 Big5字体索引、64槽缓存与启动字体源

`sub_3D27A @ 0x3D27A..0x3D34A`由input-font Order38使用独立临时IDB冻结为208 bytes、63条指令、8个CFG块、3个条件分支、1个无条件跳转、9处HIGHLOW重定位、4次direct call、2个caller和2个本地RET；raw/loaded SHA256分别为`2184f101736a6b043b9275525bd771cf193e0e14aa50bac9a94c73a8890a323d`与`7ae1394fcff40636b1abb15da18eb68a374e4e6617566b686c31b985bec5baf4`。两个caller都以零扩展byte组成`lead<<8|trail`，返回slot只立即用于读取缓存内32-byte字形。

机器按slot0..63升序首命中；hit不seek/read且不推进replacement。miss先写低16位tag，再按`(lead-0xA1)*157 + (trail<0xA1 ? trail-0x40 : trail-0x62)` seek/read 32 bytes，最后以`(slot+1)%64`推进并返回旧slot。当前`FONT3.C16`的13,973个合法编码全部映射为连续index0..13972，映射流SHA256=`4b3ce05fbebaf79ea5a5c7d5197aeb80e362e3a60e401e8745252139169aadaa`；命中、miss、回绕和驱逐trace SHA256=`c7f2602f1c04d455c15cfc3a29d8bb38dec57a28c5ae305bd38928dccc6d8936`。

首轮汇编→C++对照发现`Big5GlyphCache`核心算法一致，但Basic UI、scene和battle加载的字体资产与原版唯一启动链不符；旧零差异结论及相应像素Golden立即废弃。三条产品路径、四个独立生成器和fixture均最小修正为启动链固定的`FONT3.E16/FONT3.C16`。修正后从入口重新覆盖8/8块、4个跳转、4次call、两个caller和两个RET，合法域零剩余产品差异。畸形trail别名、负/EOF seek、短字体与I/O失败由现代显式拒绝；进程全局cache改为renderer/session局部cache且返回span替代slot，在只读合法字体和立即绘制caller下均不可观察，归类平台适配。

B5、B7、B8及battle-player-status分别执行两份临时生成和正式第三生成，每组三份逐字节一致；正式SHA256依次为`2543ef3cca3099a89dcfaaec6022c64936d5e3fdfb481f98bb7dd0c7cf23b186`、`41258dd5f705488da5580b141d83e90f60034123913a9ef02f67a889d30456e8`、`ab3b9a67ceec89430176a5469899e3e3ed318efc7d44a1a4947272115e77d6ab`和`d6d9c58f61afba15cb327da007c6e4cbd1ddc00744897aff70226fe5a8144416`。聚合render证据同时纠正`sub_20615`为Big5、`sub_20663`为ASCII，但两个callee仍按独立owner状态，不传播closure。完整证据见`research/evidence/functions/Z_DAT/0x3D27A.md`；原程序动态runtime oracle继续登记为`blocked_runtime_oracle`。

## 4. RNG

`sub_3F987` 返回全局 32-bit state；`sub_3F9B0(seed)` 原样覆盖；`sub_3F98D()` 为：

```text
state = (state * 0x41C64E6D + 0x3039) mod 2^32
result = (state >> 16) & 0x7FFF
```

`sub_3D5DE` 从 DOS time 的 second/hundredth 字节形成 `second * 100 + hundredth` 后 seed。独立 oracle 的前 10 项：

```text
seed 0          : 0,21468,9988,22117,3498,16927,16045,19741,12122,8410
seed 1          : 16838,5758,10113,17515,31051,5627,23010,7419,16212,4086
seed 5999       : 22023,21564,10621,8352,13846,27280,21825,23901,5153,23205
seed 0xFFFFFFFF : 15929,4409,9862,26718,8713,28226,9080,32063,8032,12734
```

游戏侧唯一有界包装 `sub_3D612(upper)` 先检查 `upper > 1 && upper <= 30000`；不满足时返回 0 且完全不消费 RNG，满足时只消费一次并返回 `next() % upper`。`next()` 已在 `0..32767`，机器码仍以 `cdq/idiv` 取得有符号余数。最终 headless call xref 报告列出该包装的全部直接业务调用点。

state 乘加必须显式按 `uint32_t` 回绕；不得替换成 `<random>`、改变消费点或共享一个隐式全局宿主 RNG。

## 5. Miles 音频合同

### 5.1 初始化

`sub_3DD66`：

- music 与 sound disabled flag 先清零，驱动安装失败时各自置 1；
- XMIDI 分配一个 sequence handle；
- digital preference 1 设为 `11025`，preference 8 和 7 设为 0；
- 分配严格 8 个 sample handle；
- `DIG.INI` 指定 Miles 3.03 `SBPRO.DIG`；`MDI.INI` 指定 `SBPRO2.MDI`。

现代实现保留 8 个逻辑 sample slot 和 11025 Hz 原始 sample 速率。SDL 只负责设备流；XMI 解码器使用独立后端，不进入游戏状态模型。

### 5.2 music

活跃路径 `sub_3E1B2(index)`：

1. `AIL_set_sequence_volume(sequence, 0, 2000)`；
2. `AIL_delay(1000)`；
3. `AIL_end_sequence` 并解锁前一 XMI；
4. 从 `GAME01.XMI..GAME24.XMI` 读取所选文件到固定缓冲；
5. XMIDI master volume 设 `127`；
6. `AIL_init_sequence(sequence, bytes, 0)`；
7. `AIL_start_sequence`；
8. `AIL_set_sequence_loop_count(sequence, 0)`，Miles 语义为无限循环；
9. 记录当前 music index。

`sub_3E23B` 是 `volume=127,duration=2000` 的 fade-in；`sub_3E25B` 是 `volume=0,duration=2000` 后只 delay 1000 的 fade-out。不可把 2000/1000 改成一个等待完整淡出的重新设计。

XMI 后端固定使用 libADLMIDI v1.6.1、内嵌 bank 0 `AIL (The Fat Man 2op set, default AIL)`、AIL volume model、一个 DOSBox OPL3 emulator；这是对当前 `SBPRO2.MDI`/AIL 边界的受控平台适配，不宣称 PCM 与实体声卡模拟逐采样相同。

### 5.3 sample

活跃 sample 路径把 WAV **整文件字节**作为 `AIL_set_sample_address(handle, pointer, file_size)` 的 unsigned mono 8-bit 数据，不解析/跳过 RIFF header；随后强制 playback rate `11025`。这是机器码和 Miles 包装器诊断字符串共同确认的历史行为，现代混音器也必须包含这 44 个 header 字节。

- bank 1：`ATK00.WAV..ATK23.WAV`，固定 slot 1，调用 `AIL_set_sample_volume(..., 200)`；
- bank 2：`E00.WAV..E52.WAV`，固定 slot 2，调用 `AIL_set_sample_volume(..., 400)`；
- 若目标 slot status 为 Miles `4`（playing），先 end 再覆盖；
- `AIL_init_sample` 重置默认 type/loop/pan；活跃路径不设置 loop count，因此一次播放；
- 未被游戏调用的 `sub_3E088` 另设 volume 100、loop count 0，保留在证据中但不伪装成活跃调用点。

`AIL_set_sample_volume` 内部 `sub_49120` 先保存原参数，随后 `sub_47DB8 @ 0x47DBF..0x47DDF` 把 sample field 明确裁剪到 `0..127` 再计算设备增益。因此现代 backend 保留传入的 legacy volume 200/400 作为命令证据，并在混音时按相同上限裁剪；不得在控制器层预先把两个值改写为 127。

## 6. 当前资产门禁

当前根目录严格包含：

- 24 个 `ATK*.WAV`；
- 53 个 `E*.WAV`；
- 24 个 `GAME*.XMI`；
- 合计 77 WAV + 24 XMI。

全部 WAV 当前为 mono unsigned 8-bit；74 个 header rate 为 11025，`E20/E25/E33` 为 11000，但原版仍强制 11025。全部 XMI 以 `FORM/XDIR/INFO` 开始。自动测试必须逐个读取全部 101 个资产、验证编号无缺口、格式边界与原始长度，并证明音乐解码器可从每个 XMI 内存块初始化。

## 7. 完成门禁

- IRQ 状态机与 84-byte 表的独立向量逐字节一致；
- tick 除法、等待次数和 `0x1800AF -> 0` 回绕由 fake clock 验证；
- RNG 至少覆盖上述四个 seed 的序列和最终 state；
- fake audio port 验证 music fade/delay/end/load/start/loop 顺序、sample stop-before-reuse、8 slot 和 legacy 参数；
- real mixer 验证每个 XMI 可加载并产生 PCM，全部 WAV 可按整文件 raw U8 mono 11025 运输；
- Linux/Windows `core/app × Debug/Release` 全矩阵通过；
- SDL、ADLMIDI、DOS 与 Miles 类型不进入 model/persistence/render 公共接口。

最终验证：Linux 与 Windows LLVM 的 `core/app × Debug/Release` 全部通过；core 6 项 CTest、app 7 项 CTest。独立 `OPENLEGEND_ENABLE_SANITIZERS=ON` 配置以 ASan+UBSan+LeakSanitizer 串行运行 6 项 core CTest 全部通过。首次把 sanitizer 全局施加到第三方时，只命中 libADLMIDI DOSBox OPL 的 `dbopl.cpp:1620` 空指针 `offsetof` 实现技巧；最终门禁仅 instrument OpenLegend targets，未屏蔽或跳过任何 OpenLegend 测试。

## 8. 最终双向逐基本块 REVIEW

本轮先仅依据`Z_DAT.b4_runtime_xrefs.txt`和机器指令重新锁定42项closure的物理范围、调用者、参数、全局读写、循环回边、提前出口及外部Miles合同，完成独立向量后才读取现代C++。逐地址结论如下：

- `0x20C32`：清last-key后等待IRQ写非零；现代异步事件循环保留同一last-key/byte-state合同，阻塞方式属于宿主适配。
- `0x20D35`：启动、同tick单分支优先级、present后调色板相位、tick等待及退出链全部有现代对应；SDL事件、窗口与设备生命周期属于宿主适配。
- `0x3CDFF/0x3CF19`：IRQ9、字体和Miles初始化/恢复由`LegacyKeyboard`、资源RAII、`AudioMixer`和SDL生命周期整体替代，所有游戏可见调用时点保留。
- `0x3DB83`：有符号`idiv 40`、加1、`<=0`提前出口和逐tick等待逐块对应`legacy_delay_tick_count/legacy_delay`；tick来源替换为PIT比例steady clock。
- `0x3DD57/0x3DD66/0x3DE4D/0x3DECB/0x3DF59/0x3DF90/0x3E088/0x3E172/0x3E1B2/0x3E23B/0x3E25B/0x3E288/0x3E2E2`：disabled门禁、stop-before-reuse、GAME/ATK/E文件索引、整文件WAV字节、11025Hz、slot1/2、volume200/400、loop0/1及fade/delay顺序均正向映射；反向审计`LegacyAudioController`没有无汇编来源的游戏状态写入。
- `0x3D5DE`：DOS second/hundredth形成`second*100+hundredth`后seed；宿主仅替换取时来源。
- `0x3D612/0x3F987/0x3F98D/0x3F9B0`：边界门禁、零消费提前出口、32位回绕LCG、15位结果和原样seed逐指令一致，无平台偏差。
- `0x3F9C0`：Watcom RNG运行时注册由显式`LegacyRandom`实例生命周期替代，算法状态不变。
- `0x41232/0x41383/0x413F8/0x4146D/0x4159C/0x41601/0x4166E/0x41748/0x417B5/0x42AF2/0x42BDE/0x42D0D/0x42DE7/0x42E5C/0x42EC9/0x43239/0x47DB8/0x49120`：均为Miles 3.03内部API/设备增益实现；游戏依赖的handle状态、loop、rate、volume裁剪和sequence状态合同由`LegacyAudioPort`覆盖，DOS驱动和实体声卡内部不复制。

正向汇编→C++复核覆盖上述全部分支、位宽、回绕、资源顺序与外部调用；反向C++→汇编复核覆盖`LegacyKeyboard`、`legacy_clock`、`LegacyRandom`、`LegacyAudioController`、`AudioMixer`、`SdlAudioDevice`及SDL主循环的每项游戏可见行为。最后一轮未发现新差异或未决项；原程序动态差分因本机无DOS运行器登记为`blocked_runtime_oracle`，不改变机器码静态结论。

```text
final_review = converged_no_new_differences
remaining =
```
