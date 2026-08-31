# 函数证据：`sub_34AEC` `0x34AEC..0x34C47`

状态：`pending_implementation`

映射：`BattleSetup::ai_escape_plan`。

函数以actor坐标建立movement距离图，严格按x外层、y内层扫描64×64格；只接受路径值恰等于actor round value的格。每个候选对所有不同side combatant累计曼哈顿距离，不跳过隐藏或死亡槽；仅在距离和严格大于当前最大值时替换，因此同分保留早扫描格。最大值大于0时写目的坐标并调用逐格移动。参数`a6==0`时移动后调用休息handler，`a6!=0`时用于物品前重定位且不休息。

真实field2 synthetic五槽向量中，source `(10,20)`、round value 3、敌方 `(13,23)/(14,24)`时首个最大格为`(7,20)`，距离和20。现代已生成目的格与`rest_after_move`计划；实际逐格render/present/tick移动和条件休息尚未由BattleSession执行，故保持 `pending_implementation`。Linux Debug 14/14和独立oracle通过；最终仍需完整汇编↔C++ REVIEW。
