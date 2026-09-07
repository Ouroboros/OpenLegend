# B4 输入、时间、随机与音频汇编合同

状态：最终双向REVIEW已收敛；RNG核心为`assembly_exact`，输入/计时/音频及宿主生命周期为`platform_adapted`
真值：当前 `Z.DAT` 机器码、当前 77 个 WAV 与 24 个 XMI 字节、Miles 3.03 包装器内嵌诊断字符串

## 1. 证据范围

- `0x20BC0..0x20C31`：嵌入式 IRQ1 键盘处理体；
- `0x3CDE3..0x3CDFE`：安装到中断 9 的保存寄存器/切换 DS/trampoline/`iret`；
- `sub_3CDFF @ 0x3CDFF..0x3CF18`：键盘中断、字体和音频初始化；
- `sub_3CF19 @ 0x3CF19..0x3CF44`：恢复原中断 9；
- `sub_20C32 @ 0x20C32..0x20C44`：清空并阻塞等待 last-key；
- `sub_3DB83 @ 0x3DB83..0x3DBBE`：BIOS tick delay；
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

机器码执行有符号 `idiv 40`，然后加 1：

```text
wait_count = trunc_toward_zero(argument / 40) + 1
```

仅当 `wait_count > 0` 时，每轮捕获一个 tick 值并等待它变化。必须保留：

- `0..39 -> 1 tick`；
- `40..79 -> 2 ticks`；
- `-39..-1 -> 1 tick`；
- `-79..-40 -> 0 ticks`；
- 不按毫秒重新解释参数，不修正约 18.2 Hz 与除数 40 的历史失配。

### 3.3 opcode27图片动画的tick边界

`sub_2F053 @ 0x2F053..0x2F107`为180字节、59条指令、14个CFG块、6次call、5个条件分支、4个无条件跳转、9项HIGHLOW重定位且无本地`RET`。raw/loaded SHA256分别为`7bfb0b81ff77dc3bc3544b6e692b4b177e21c58e7f5007367bb090a2b9675bdb`与`cc5c53f5a16319098f5bfa8813a847dc50a72dcda23eb377d54f792d7af5556d`；唯一物理caller为opcode27 dispatch，另有`0x55628`地址表引用。两个正常出口都跳入前一函数`0x2F04A..0x2F053`共享尾，其SHA256为`f7d57baaeaca8f29028fb0af30bc53418bee7a398b4188a9c960681e3dd7e2b5`。

本input/tick owner不读键盘状态。每个有效图片帧都严格按以下顺序运行：先保存`*dword_544D8`，再写玩家图片低16位或调用事件修改callee写三个图片word，然后完整重绘场景，调用`sub_3DB83(50)`等待2个BIOS ticks，最后仍自旋直到当前tick不等于帧首保存值。正常单调时钟中2-tick helper已经满足末尾条件，因此没有固定第三tick；零帧路径完全不读tick。循环值按signed 32-bit执行`+2`和`<=end`，只有图片写入截低16位。

现代`SceneSession::advance_picture_animation_frame`先写图片状态再返回`present(wait_ticks=2)`；`LegacyGameRuntime::render`只有成功绘制该状态后才把present effect标记为已显示，`advance_scene_effect`在首个宿主tick仅把2减为1，第二个tick才恢复解释器并生成下一图片或继续已提前推进4 words的PC。新增真实opcode27宿主回归固定未render不能推进、5002/5004/5006每帧恰好两tick及末帧后续PC；既有独立Golden覆盖43次真实资产调用、579帧、event `-1`/非`-1`分支、`-2`别名、32767边界、零帧和奇数跨度。三次Golden逐字节一致，正式`scene-goldens.json` SHA256为`8f78b137f17fc1d614eb84b0afc7201512d67a0d0ffb509397f79958a3ff3bba`。

本轮只独立关闭`input-font-closure.tsv audit_order=27`的tick引用/宿主时序owner；同址scene owner此前已独立关闭，`sub_3DB83`完整time owner、两个scene callee、编译器栈探测及共享尾owner均不传播状态。入口59条指令、全部14块、唯一caller、两个分支和全部出口最终重审无产品差异。

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
