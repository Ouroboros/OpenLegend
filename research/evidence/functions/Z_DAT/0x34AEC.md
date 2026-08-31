# 函数证据：`sub_34AEC` `0x34AEC..0x34C47`

状态：`implemented_pending_review`

映射：`BattleSetup::ai_escape_plan`与`BattleSession`逃跑/物品重定位continuation。

函数以actor坐标建立movement距离图，严格按x外层、y内层扫描64×64格；只接受路径值恰等于actor round value的格。每个候选对所有不同side combatant累计曼哈顿距离，不跳过隐藏或死亡槽；仅在距离和严格大于当前最大值时替换，因此同分保留早扫描格。最大值大于0时写目的坐标并调用逐格移动。参数`a6==0`时移动后调用休息handler，`a6!=0`时用于物品前重定位且不休息。

真实field2 synthetic五槽向量中，source `(10,20)`、round value 3、敌方 `(13,23)/(14,24)`时首个最大格为`(7,20)`，距离和20。`BattleSession`现对参数0的逃跑执行mode0逐格移动、每格render/present与两次BIOS tick变化，移动后调用休息并由外层写action_done；真实battle2从`(30,24)`移动三格至`(31,22)`并推进下一actor，同时验证逃跑局部动作11不写word10。参数1的物品重定位也走同一移动相位，但移动后不休息并恢复物品typed计划。Linux/Windows Debug完整BUILD、全部测试、独立oracle与逆向框架门禁通过；本函数推进为`implemented_pending_review`，仍待最终完整汇编↔C++ REVIEW。
