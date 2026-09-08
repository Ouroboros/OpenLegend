# 函数证据：`sub_38DAC` `0x38DAC..0x39188`

状态：`platform_adapted`

映射：`BattleSetup::begin_magic_selection`、`BattleSetup::apply_magic_selection`、`BattleSession::begin_player_attack`、`handle_player_magic_selection_key`、`render_player_magic_selection`。

## 机器身份、入口与owner边界

- input-font Order37使用独立临时IDB从入口导出完整988 bytes、259条指令、70个CFG基本块、52个函数体跳转、45处32位fixup和8次direct call；70块按`15+9+13+12+20+1`六段唯一覆盖，unassigned=0。
- raw SHA256为`57ae8494ab7b83a2e4871a11b0cf63c5384b4ab426d4c09a4c7bfdf9cfac6f21`，loaded SHA256为`4458a76d92ee84dddd18b94410dea5a757202b911bddd20fcc7e18abd4bde181`。45处loaded值均为raw值`+0x20000`，逐处逆归一化后完整988字节与只读`Z.DAT`一致。
- direct call依次为`0x38DB1→sub_3ED1E`栈探测、`0x38E70→sub_3AA85`战场重画、`0x38E99→sub_2CEBF`圆角面板、`0x38F27/0x38FF4→sub_3EF4A`普通/选中Big5武功名、`0x38FA0/0x3906F→sub_3D832`普通/选中名字绘制、`0x39082→sub_3D6D1`present；函数零直接RNG消费。
- 唯一caller为`sub_37734:0x377AF`，依次压入actor、正武功数和取消out-word地址；忽略返回值。caller在正武功数恰为1时不调用本函数而直接写共享slot0，保留物理槽0 BUG；取消out-word恰为1时caller返回`-1`。
- 本函数以本地`0x39187 retn`退出。caller、8个callee、普通战场renderer、键盘状态owner及共享武功slot均保持独立，不随本行closure传播。

## 完整机器合同

1. `0x38DD0..0x38E61`扫描全部10个物理武功槽。可用谓词严格为signed magic id大于0且actor当前signed MP大于等于该magic的signed `need_mp`；available count和mask按物理槽顺序形成，cursor初值为可用项ordinal 0。
2. 每轮严格先调用战场重画，再绘制`(20,10,90,17*learned_count+10)`圆角面板。普通名称只扫描物理槽`slot < learned_count`，但仍按available ordinal计算y，因此稀疏槽会漏画后置可用武功；这是原显示BUG，不能把上限替换为10或压缩后的available count。
3. 名称长度按Big5双字节字符数1..5选择x=`57/49/41/33/25`，普通颜色`0x2321`、选中颜色`0x6663`，y=`17*ordinal+15`。选中名称通过扫描全部10槽把cursor ordinal解析成实际slot后单独重绘，故被普通循环漏画的稀疏槽在选中时仍会显示。
4. 完成整帧绘制并调用`0x39082→sub_3D6D1`后才扫描输入状态。键盘state base为`0x51B6D`；`0x3908A`读取`byte_51C05 = base+0x98`即Down，`0x390BA`读取`byte_51C0B = base+0x9E`即Up。该函数不读取Right `0x9C`或Left `0x9A`。
5. 同轮优先级严格为Down→Up→Enter/Space/keypad Insert→Escape。Down只清`byte_51C05`，cursor为末项时回0否则加1；Up只清`byte_51C0B`，cursor为0时写`available_count-1`否则减1。两者都必须回到整帧重画/present后才再次扫描；低优先状态保留。
6. 三种确认状态地址依次为`0x51B7A/0x51B8D/0x51C03 = base+0x0D/0x20/0x96`。任一命中即同时清三者，再扫描全部10槽，以可用项ordinal映射实际物理slot并写共享`word_E6ED6`；取消out-word保持0并从本地RET退出。确认不清Escape。
7. Escape状态为`0x51B88 = base+0x1B`；命中只清自身，把取消out-word写1并退出，不写共享武功slot。
8. 入口不清上述状态。因此首个菜单帧present之前已置位的状态必须在present成功后参与扫描；每次present最多消费一组，余下状态留到后续轮。
9. available count为0时，原函数仍进入菜单：Up可产生cursor -1，确认可落入无效slot0，某些输入序列不终止。现代对零available及非法actor/magic记录安全拒绝，替代未定义访问或非终止，归类平台适配。

## 汇编到现代实现逐块对照

- `BattleSetup::begin_magic_selection`保留10槽signed谓词、available mask、learned/available双计数和ordinal cursor；`apply_magic_selection`保持下一/上一回绕、确认、取消、全部10槽重新解析及实际slot结果。
- `BattleSession`从攻击动作进入独立选择相位，调用battle renderer后叠加原圆角面板和Big5名称；固定稀疏槽像素证明普通名称漏画与选中名称补画均保留。确认同步共享武功slot，取消返回原动作ordinal；caller恰一项时直接选物理slot0的遗留BUG保持在`begin_player_attack`。
- battle Order55首轮曾发现入口及方向更新后可跳过必经present，增加了一次presentation门；但当时把`0x51C05/0x51C0B`误称Right/Left，且把门前键按下作为translated event直接丢弃。input-font Order37按地址减state base重建合同后废弃该旧输入结论。
- Order37首轮独立对照发现两项合法域产品差异：现代错误接受Right/Left而非Down/Up；present门降为0后不扫描持久键态，导致入口前已置位状态、同轮优先级和低优先状态保留均无法表达。
- 最小修正把魔法菜单绑定改为Down/Up，并纳入宿主持久键态边界：入口及每次方向变化仍需成功present；随后按Down→Up→确认组→Escape扫描，每帧最多消费一组，只请求清命中的单键或确认组。Right/Left不触发魔法菜单，低优先状态不清也不另建队列。
- 修正后从`0x38DAC`入口重新覆盖全部259条指令、70/70基本块、52个函数体跳转、45处fixup、8次call、唯一caller、本地RET及所有合法输入出口，零剩余合法域产品差异。

## 判别性回归与验证

- 独立资产oracle锁定93条真实magic记录，资产SHA256为`8bdc630705dafacb594b6a59231c4ff7823ae93e3c4f955767dd7d81324b1b76`；名称字符数分布为`2:3, 3:20, 4:58, 5:12`，`need_mp`域为0..10。
- 真实资产固定slots `[6,0,5,0,25,0,0,0,0,0]`、MP3得到learned3、available slots `[0,2]`，完整状态hash `0xc254d2cd83d7da76`；稀疏显示hash `0x9eeb370071c9a0af`，Down→Up→确认→Escape多状态优先级hash `0x7398c6fcaccb922c`并保留未消费Escape，零available异常hash `0x47d47c419ce4142b`。
- Session固定cursor0菜单FNV64 `0x909332be9671b27c`、cursor1/实际slot2菜单`0x6977ba7a0c3172a6`。回归锁定Right/Left无效、直接Down/Up的present门、首帧持久状态采样、Down/Up/确认/Escape优先级、每轮单组消费、Escape单独取消与确认不清Escape。
- 两次独立生成逐字节一致后执行第三次正式生成；三份Golden逐字节一致，SHA256均为`cae592a562fd3022c47ac2167c8f0d7eda302101e83eab2e2d5a9a463dc42799`。
- 修正首轮测试失败为`tests/unit/battle/battle_data_test.cpp:7599: CHECK failed: !session.take_clear_confirmation_states_request()`；根因为测试未先排空更早相位遗留的clear request，不是产品时序差异。修正测试隔离后，Linux app Debug通过`100% tests passed, 0 tests failed out of 14`。

最终归类：`platform_adapted / converged_no_new_differences`。
