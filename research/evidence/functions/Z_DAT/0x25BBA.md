# 函数证据：`sub_25BBA` `0x25BBA..0x25D0E`

状态：`implemented_pending_review`

该函数承接世界/场景主菜单的“要求誰離隊”。它调用`sub_22090(6)`取得连续队伍前缀中的槽位；取消时直接返回。若所选角色ID为0，原版先重绘并呈现1帧裸背景，再绘制/呈现“抱歉！沒有你遊戲進行不下去”并等待任意键，不修改队伍或事件。

其余角色先在25项静态表`off_20CE5`中查找：

`1,2,9,16,17,25,28,29,35,36,37,38,44,45,47,48,49,51,53,54,58,59,61,63,76`

命中索引`i`后执行KDEF脚本`950+2*i`。真实脚本均保留原副作用：先经opcode1显示角色离队对话，再由opcode21压缩队伍槽，最后执行一个或多个opcode3修改对应场景事件；角色58脚本还保留opcode60条件分支、opcode7和opcode17。未命中表的角色不执行脚本。

现代映射位于`LegacyGameRuntime::handle_game_menu_result`、`begin_world_leave_event`和`handle_world_menu_event_result`。世界菜单构造`SceneSessionContext::world_event_overlay`，复用同一KDEF解释器和对话绘制，只跳过场景入口脚本/场景精灵加载，并把对话层叠加到已绘制世界帧；场景路径复用当前`SceneSession`。两条路径均先在选择器返回后重绘并呈现一次不含菜单的裸背景，再开始脚本。present/fade/wait/question continuation仍由运行时逐帧恢复。脚本返回后，世界与场景路径分别执行函数外层的64级fade-to-black、黑色调色板下重绘/呈现1帧背景、65级fade-from-black，再回到`sub_212C0`等价的原上下文主菜单循环；只有玩家随后按Escape才恢复世界/场景输入。这段边界不依赖脚本是否含fade opcode。

回归锁定槽1角色ID为0时仍按`<=0`截断连续队伍前缀、主角分支的裸背景present与提示不改快照，并以角色1验证脚本前裸背景present、世界背景对话、opcode21移除、scene0/event0的event_1写为951、外层fade期间输入阻断、黑屏重绘帧、130帧后返回世界主菜单及再按Escape恢复世界控制。本切片通过Linux/Windows `core/app × Debug/Release`及Linux app ASan+UBSan门禁。当前仅为`implemented_pending_review`；最终双向逐基本块REVIEW为`not_started`。
