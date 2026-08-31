# 函数证据：`sub_38DAC` `0x38DAC..0x39188`

状态：`implemented_pending_review`

实现映射：`BattleSetup::begin_magic_selection`、`BattleSetup::apply_magic_selection`、`BattleSession::begin_player_attack`、`handle_player_magic_selection_key`、`render_player_magic_selection`。

函数先扫描10个武功槽；仅magic id>0且当前MP≥该武功need_mp时标为可用，再统计可用数。cursor从0开始，语义是可用项ordinal；右键在末项回0，左键在0回末项。确认键组清三个input flag后再次扫描10槽，把ordinal映射为实际magic slot并写 `word_E6ED6`；取消清flag并把out flag写1。

每轮先调用 `sub_3AA85` 重画战场，再以 `(20,10,90,17*learned_count+10)` 调 `sub_2CEBF` 绘制圆角面板。普通名称颜色为`0x2321`，选中名称重绘颜色为`0x6663`；名称按1..5个Big5字分别从x=57/49/41/33/25开始，y=`17*ordinal+15`。普通名称循环只扫描slot `< learned_count`，而可用mask和确认映射扫描全部10槽；这是稀疏槽位下的原显示BUG，不能把learned_count替换为10或压缩后的available_count。选中名称仍按实际槽单独绘制。

固定状态：magic slots `[6,0,5,0,7,0,0,0,0,0]`、MP6、need MP分别6/4/7，learned3、available slots `[0,2]`，初始状态hash `0xc254d2cd83d7da76`；next→next→previous后cursor1，确认得到实际slot2；取消out flag1。

`BattleSession`现从动作菜单的攻击项进入独立武功选择相位，实际重画战场和圆角菜单、读取角色槽映射的原Big5武功名、按左右键回绕、接受Enter/Space/keypad Insert确认、Escape取消并返回原动作ordinal。稀疏slots `[5,0,6,...]` 的Session回归锁定初始菜单FNV64 `0x909332be9671b27c`，cursor1选中实际slot2时为`0x6977ba7a0c3172a6`；日志锁定ready/cursor/cancel/selected四个关键边界。Linux Debug完整BUILD 14/14。

无已学武功但MP达到动作菜单1000哨兵的非正常状态在现代层安全拒绝，不进入原版零可用项的未定义菜单循环；该差异留待最终安全边界REVIEW。当前实现仍须进行最终汇编↔C++双向REVIEW，因此仅标 `implemented_pending_review`。
