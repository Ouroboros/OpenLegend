# 函数证据：`sub_3AA4B` `0x3AA4B..0x3AA85`

状态：`pending_implementation`

已映射状态边界：`BattleSetup::enable_automatic_mode`、`BattleSetup::automatic_enabled`。

函数顺序固定为：调用`sub_3AA85`重绘战场、以当前framebuffer执行一次present、把自动标志写1，最后以当前actor slot调用AI入口`sub_33599`。自动标志的写入不能提前到重绘或present之前。

现代只恢复BattleSetup中的自动flag，并已恢复可供前置重绘使用的typed战场命令计划；实际framebuffer绘制、present和AI同步调用仍未接入BattleSession，因此保持 `pending_implementation`。
