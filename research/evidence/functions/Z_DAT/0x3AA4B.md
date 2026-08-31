# 函数证据：`sub_3AA4B` `0x3AA4B..0x3AA85`

状态：`pending_implementation`

已映射状态边界：`BattleSetup::enable_automatic_mode`、`BattleSetup::automatic_enabled`。

函数顺序固定为：调用`sub_3AA85`重绘战场、以当前framebuffer执行一次present、把自动标志写1，最后以当前actor slot调用AI入口`sub_33599`。自动标志的写入不能提前到重绘或present之前。

`BattleSession`现把玩家确认自动动作后的状态拆为独立`automatic_present`相位：该相位实际重绘战场且自动flag仍为0，只有对应present完成回调到达后才写flag=1并切到同actor的AI相位。AI selector/handler尚未在该相位内同步执行，因此函数仍保持 `pending_implementation`。
