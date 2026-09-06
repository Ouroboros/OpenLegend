# 函数证据：`sub_38DAC` `0x38DAC..0x39188`

状态：`platform_adapted`

映射：`BattleSetup::begin_magic_selection`、`BattleSetup::apply_magic_selection`、`BattleSession::begin_player_attack`、`handle_player_magic_selection_key`、`render_player_magic_selection`。

## 机器身份、入口与owner边界

- fresh临时IDB从入口导出完整988 bytes、259条指令、52个函数体跳转、45处32位fixup和8次direct call；raw SHA256为`57ae8494ab7b83a2e4871a11b0cf63c5384b4ab426d4c09a4c7bfdf9cfac6f21`，loaded SHA256为`4458a76d92ee84dddd18b94410dea5a757202b911bddd20fcc7e18abd4bde181`。45处loaded值均为raw值`+0x20000`，逐处逆归一化后完整988字节与只读`Z.DAT`一致。
- direct call依次为`0x38DB1→sub_3ED1E`栈探测、`0x38E70→sub_3AA85`战场重画、`0x38E99→sub_2CEBF`圆角面板、`0x38F27/0x38FF4→sub_3EF4A`普通/选中Big5武功名、`0x38FA0/0x3906F→sub_3D832`普通/选中名字绘制、`0x39082→sub_3D6D1`present；函数零直接RNG消费。
- 唯一caller为`sub_37734:0x377AF`，依次压入actor、正武功数和取消out-word地址；忽略返回值。caller在正武功数恰为1时不调用本函数而直接写共享slot0，保留物理槽0 BUG；取消out-word恰为1时caller返回`-1`。
- 本函数以本地`0x39187 retn`退出。caller、8个callee、普通战场renderer、输入flag owner及共享武功slot均保持独立，不随本行closure传播。

## 完整机器合同

1. `0x38DD0..0x38E61`扫描全部10个物理武功槽。可用谓词严格为signed magic id大于0且actor当前signed MP大于等于该magic的signed `need_mp`；available count和mask按物理槽顺序形成，cursor初值为可用项ordinal 0。
2. 每轮严格先调用战场重画，再绘制`(20,10,90,17*learned_count+10)`圆角面板。普通名称只扫描物理槽`slot < learned_count`，但仍按available ordinal计算y，因此稀疏槽会漏画后置可用武功；这是原显示BUG，不能把上限替换为10或压缩后的available count。
3. 名称长度按Big5双字节字符数1..5选择x=`57/49/41/33/25`，普通颜色`0x2321`、选中颜色`0x6663`，y=`17*ordinal+15`。选中名称通过扫描全部10槽把cursor ordinal解析成实际slot后单独重绘，故被普通循环漏画的稀疏槽在选中时仍会显示。
4. 完成整帧绘制和present后才扫描输入flag。优先级严格为右→左→Enter/Space/keypad Insert→Escape；机器flag是异步全局布尔状态，同轮多个flag只执行最先命中的分支，未命中flag保留到后续轮次。
5. 右键只清右flag；cursor为末项时回0，否则加1。左键只清左flag；cursor为0时写`available_count-1`，否则减1。两种方向都必须回到整帧重画/present后才再次扫描输入。
6. 三种确认flag命中任一后整体清零，再扫描全部10槽，以可用项ordinal映射实际物理slot并写共享`word_E6ED6`；取消out-word保持0并从本地RET退出。确认分支不清Escape等其他flag。
7. Escape只清自身flag，把取消out-word写1并退出；不写共享武功slot。
8. available count为0时，原函数仍进入菜单：左键可产生cursor -1，确认可落入无效slot0，某些输入序列不终止。全部真实caller由动作菜单正武功数门保证合法菜单调用；现代对零available及非法actor/magic记录安全拒绝，替代未定义访问或非终止，归类平台适配。

## 汇编到现代实现逐块对照

- `BattleSetup::begin_magic_selection`保留10槽signed谓词、available mask、learned/available双计数和ordinal cursor；`apply_magic_selection`保持左右回绕、三确认键、Escape、全部10槽重新解析及实际slot结果。
- `BattleSession`从攻击动作进入独立选择相位，调用battle renderer后叠加原圆角面板和Big5名称；固定稀疏槽像素证明普通名称漏画与选中名称补画均保留。确认同步共享武功slot，取消返回原动作ordinal；caller恰一项时直接选物理slot0的遗留BUG保持在`begin_player_attack`。
- 首轮逐基本块对照发现一项产品时序差异：现代入口及方向更新后可在对应菜单帧present前继续接受translated event，从而跳过机器必经的中间呈现。
- 最小修正只在`BattleSession`增加一次presentation门：入口及每次左右更新后计数置1，只有成功render后的`finish_presented_tick`确认present才降为0；门前translated event沿用已关闭cursor-selection owner的宿主适配而忽略。确认/取消清零计数，不建立跨后续owner的键队列。
- 修正后从`0x38DAC`入口重新覆盖全部259条指令、52个函数体跳转、45处fixup、8次call、唯一caller、本地RET及所有合法输入分支，零剩余合法域产品差异。

## 判别性回归与验证

- 独立资产oracle锁定93条真实magic记录，资产SHA256为`8bdc630705dafacb594b6a59231c4ff7823ae93e3c4f955767dd7d81324b1b76`；名称字符数分布为`2:3, 3:20, 4:58, 5:12`，`need_mp`域为0..10。
- 真实资产固定slots `[6,0,5,0,25,0,0,0,0,0]`、MP3（对应need_mp 3/2/10）得到learned3、available slots `[0,2]`，完整状态hash `0xc254d2cd83d7da76`；稀疏显示hash `0x9eeb370071c9a0af`，右→左→确认→Escape多flag优先级hash `0x7398c6fcaccb922c`并保留未消费Escape，零available异常hash `0x47d47c419ce4142b`。
- Session稀疏slots `[5,0,6,...]` 固定cursor0菜单FNV64 `0x909332be9671b27c`、cursor1/实际slot2菜单`0x6977ba7a0c3172a6`。回归明确锁定入口前置键忽略、每次方向后必须present、确认必须等待选中帧present及取消回原动作ordinal。
- 两次独立生成逐字节一致后更新正式Golden，第三次生成与正式文件逐字节一致；三份SHA256均为`4c923e05b6739b8dcc097d1183517ca7f0ef6fcff58118cdde8dd20226f2141c`。
- Linux Clang 23根入口`./build.sh app --config Debug`完成最终源码构建并通过`100% tests passed, 0 tests failed out of 14`。机器、static、closure与reverse-framework门共同验证本owner。

最终归类：`platform_adapted / converged_after_fix`。
