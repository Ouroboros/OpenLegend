# 函数证据：`sub_38DAC` `0x38DAC..0x39188`

状态：`pending_implementation`

已映射状态边界：`BattleSetup::begin_magic_selection`、`BattleSetup::apply_magic_selection`。

调用者 `sub_37734 @ 0x377A0..0x377B4` 依次压入out flag、已学武功数和actor slot。函数先扫描10个武功槽；仅magic id>0且当前MP≥该武功need_mp时标为可用，再统计可用数。cursor从0开始，语义是可用项ordinal；右键在末项回0，左键在0回末项。确认键组清三个input flag后再次扫描10槽，把ordinal映射为实际magic slot并写 `word_E6ED6`；取消清flag并把out flag写1。

面板宽度为 `17*learned_count+10`。普通名称绘制循环只扫描slot `< learned_count`，而可用mask和确认映射扫描全部10槽；这是稀疏槽位下的原显示BUG，不能把learned_count替换为10或压缩后的available_count。选中名称仍按实际槽单独绘制。

固定状态：magic slots `[6,0,5,0,7,0,0,0,0,0]`、MP6、need MP分别6/4/7，learned3、available slots `[0,2]`，初始状态hash `0xc254d2cd83d7da76`；next→next→previous后cursor1，确认得到实际slot2；取消out flag1。host-neutral选择状态已实现并由Linux Debug 14/14验证；panel、Big5名称居中/颜色、选中框、present与input flag continuation尚未接入，因此不能标为完整实现。
