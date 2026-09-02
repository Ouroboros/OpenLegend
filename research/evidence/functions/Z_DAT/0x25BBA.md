# 函数证据：`sub_25BBA` `0x25BBA..0x25D0E`

状态：`platform_adapted`

## 审计范围

- 当前 `Z.DAT` 入口：`0x25BBA`
- 机器包：`tmp/startup-review/0x25BBA.txt`
- 唯一调用方：`sub_212C0 @ 0x21459`
- 现代承接：
  - `LegacyGameRuntime::handle_game_menu_result`
  - `LegacyGameRuntime::begin_world_leave_event`
  - `LegacyGameRuntime::handle_world_menu_event_result`
  - `SceneSession::begin_event` / `SceneSession::render_overlay`
- 本轮从函数入口独立恢复，再按基本块顺序单向核对汇编→C++；未以既有映射、测试或本文旧结论代替终审。

## 入口、选择与取消

`0x25BBA..0x25BE2` 先调用栈探针，复制 `off_20CE5` 的25个有符号word到栈，再以参数6调用共享队员选择器 `sub_22090`。`0x25BE5..0x25BE7` 对返回值执行有符号比较；槽位 `< 0` 时直接走尾声返回，不重绘、不淡变、不修改队伍。

现代主菜单先由已终审的共享选择器返回party slot；取消保持当前菜单上下文，等价承接该早退边界。

## 主角拒绝分支

`0x25BED..0x25BF9` 以返回槽位乘2，从 `word_C0B78` 读取16位有符号角色ID。角色ID为0时：

1. `0x25BFF..0x25C15` 根据 `word_C0BF2` 选择世界 `sub_2558B` 或场景 `sub_29D2D` 重绘；
2. `0x25C15..0x25C25` present一次裸背景；
3. `0x25C28..0x25C46` 在 `(40,40)` 绘制 `228×27`、颜色255、样式3的面板；
4. `0x25C49..0x25C63` 在 `(50,45)` 以字色 `0x0705`、16字节长度绘制字符串1797：“抱歉！沒有你遊戲進行不下去”；
5. `0x25C66..0x25C79` present提示帧并等待任意键；
6. 不修改队伍或事件，直接返回 `sub_212C0` 的主菜单循环；本分支没有外层淡变。

现代以 `leave_protagonist_notice_pending_` 和一次 `SceneEffectKind::present` 保留“先裸背景present、后提示”边界；`GameMenuNotice::leave_protagonist` 等待任意键后回主菜单，不修改快照。世界与场景分别复用相应背景renderer。

## 25项表与脚本派发

当前 `Z.DAT` 函数机器签名位于文件偏移 `0x1F5BA`，映像地址与文件偏移差为 `-0x6600`；因此 `off_20CE5 @ 0x20CE5` 的50个原始字节位于文件偏移 `0x1A6E5`：

`0100020009001000110019001c001d0023002400250026002c002d002f00300031003300350036003a003b003d003f004c00`

按little-endian int16解码为：

`1,2,9,16,17,25,28,29,35,36,37,38,44,45,47,48,49,51,53,54,58,59,61,63,76`

与现代 `kLeavePartyRoles` 逐项一致。

`0x25C83..0x25C98` 从索引0线性比较到24；命中时把索引写入EBP，并把EAX置25结束循环。`0x25CC3..0x25CD0` 计算 `950 + 2*index` 并调用 `sub_2C319`。当前25个偶数脚本 `950..998` 均由真实 `KDEF.IDX/KDEF.GRP` 承接；独立解码结果记录于 `tmp/leave-scripts.tsv`：每条脚本包含角色离队对话opcode1、压缩队伍opcode21和场景事件修改opcode3；脚本990还保留opcode60条件、opcode7及两个opcode17。

现代世界路径通过 `SceneSessionContext::world_event_overlay` 复用同一KDEF解释器并将对话覆盖在世界帧上；场景路径直接复用当前 `SceneSession`。两条路径均在脚本开始前先退出选择器层，重绘并present一次裸背景，对应 `0x25C9A..0x25CC0`。

## 未命中表的平台边界

机器循环没有初始化EBP。若选择的非零角色ID不在25项表中，`0x25CC3` 仍以调用者遗留的EBP计算脚本号并执行；行为取决于未初始化寄存器，不存在稳定的原版脚本语义。旧证据中“未命中不执行脚本”的表述错误，本轮已废弃。

对当前1018条 `KDEF` 以机器68项opcode宽度线性解码，所有脚本均合法；opcode10入队角色集合只比25项表多角色26。角色26只在脚本320的战斗事件内临时加入，并在该事件恢复正常场景控制前由opcode21移除；队员离队菜单不在脚本执行期间开放。因此当前原始资产的正常菜单可达域只会选择主角0或表内25名角色。现代对损坏/改造存档中的未列非零ID确定性拒绝，不复现未初始化EBP，登记为宿主安全的 `platform_adapted`。

## 脚本后尾链

脚本返回后机器执行：

1. `0x25CD3`：`sub_3CC97`，64级fade-to-black；
2. `0x25CD8..0x25CEE`：按world/scene上下文在黑色调色板下重绘；
3. `0x25CEE..0x25CFE`：present一次黑屏背景；
4. `0x25D01`：`sub_3CD17`，65级fade-from-black；
5. 返回 `sub_212C0` 的原上下文主菜单循环。

现代world/scene两条continuation都保留脚本最后可见帧作为淡黑起点，完成64级淡黑后在全黑palette下重绘并present一次背景，再执行65级淡入；淡入结束后恢复相应world/scene主菜单，而非直接恢复玩家移动。只有随后Escape才退出主菜单。

## 圆角primitive修正后的入口重审

后续审计 `sub_269AB/sub_2CEBF/sub_2050A` 发现共享主菜单、参数6离队选择框及主角拒绝提示均为style3圆角混色；现代旧直角框不一致，初次4/8+4/8修正又误作style4。读取 `unk_54294` 原始表后恢复palette0 3/8加目标5/8，旧可视结论两次废弃。再次从 `0x25BBA` 入口覆盖取消、主角拒绝、25项表、脚本分派、world/scene路径和淡变尾链，除该primitive像素修正外无新增差异；离队选择整帧现固定为 `0x298AF9814AE272C3`。

## 终审结论

入口到出口各基本块、16位有符号角色读取、取消与主角分支、25项表顺序、脚本号公式、world/scene绘制分流、present位置以及64/黑帧/65尾链均有现代承接。唯一有意差异是未列角色时以确定性拒绝替代机器未初始化EBP；该状态不属于当前原始资产正常菜单可达域，属于平台安全适配。

终态：`platform_adapted / converged_no_new_differences`。
