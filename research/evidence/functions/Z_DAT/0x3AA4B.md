# 函数证据：`sub_3AA4B` `0x3AA4B..0x3AA85`

状态：`platform_adapted / converged_no_new_differences`

映射：`BattleSetup::enable_automatic_mode`、`BattleSetup::automatic_enabled`与`BattleSession`自动动作continuation。

## 机器身份

fresh临时IDB固定范围为58 bytes、13条连续指令、1个IDA flow block、0分支、3处重定位、4次direct call、唯一caller及唯一`RETN @ 0x3AA84`。raw SHA256为`d43cf928ed39315528b674f667b9f711dadf2e259d7a20b23adf5a9a599b99e6`，loaded SHA256为`2609df1b59233d641c8c84fddd71754d3419c841317d51f99a3c0666fc19354a`；loaded在`0x3AA5C/0x3AA61/0x3AA70`的DWORD均比raw严格增加`0x20000`，归一化后58字节逐字节一致。

唯一入口xref为玩家动作9分派`sub_32E59:0x333D6`。direct call严格依次为栈探测`sub_3ED1E @ 0x3AA50`、战场renderer `sub_3AA85 @ 0x3AA55`、present `sub_3D6D1 @ 0x3AA65`及AI入口`sub_33599 @ 0x3AA7C`。

## 机器合同

函数无分支，顺序固定为：重绘战场；以当前framebuffer和palette执行一次present；将`word_556F2`自动标志写1；把caller参数按signed低字扩展为当前actor slot并调用AI入口。自动标志在render和present期间必须仍为0，到AI入口时必须已为1。

函数自身不消费RNG、不写actor状态。AI入口的EAX偶然透传至本函数RET；唯一caller在清理参数后立即重算actor记录地址，只读取该actor的`action_done`，完全不消费EAX。

`word_556F2`另外只有battle入口与轮准备写0；轮循环读取它来选择玩家或AI控制。因此本函数的写1时点既不能提前至首帧render/present之前，也不能延迟到AI入口之后。

## C++与宿主对照

`BattleSession`在玩家确认自动动作后进入独立`automatic_present`相位，机器可见的`BattleSetup::automatic_enabled()`仍为false；该相位只执行普通战场render。即使宿主在present完成前调用`advance`，相位、flag、RNG与actor状态均保持不变。只有对应frame实际present后的`finish_presented_tick`才调用`enable_automatic_mode()`并进入`ai_action`；下一次host advance以同一actor slot调用AI，保持机器同步调用的可观察顺序。

现代私有`player_automatic_action_`在确认时先记录caller continuation，但它不参与首帧绘制，也不改变机器可见自动flag；AI handler完成后它仅恢复外层caller等价的`action_done`检查。拆分render/present/AI为显式宿主phase是平台适配，不接受输入、不重排共享状态。

完整入口13条指令、全部call、flag写入、RET及唯一caller对照未发现合法域产品差异。renderer、present backend、AI handler与外层玩家dispatcher均为独立owner，不传播closure。

## 验证

回归锁定present前advance不得置flag或启动AI、render完成而present回调未到时flag/RNG不变、present后flag=1但AI尚未消费RNG，以及同actor进入AI prelude。独立向量SHA256为`4a37e849a035425b8ae019458b0f2edcebce0cddeb6115136726ab022c14c20e`。Golden从只读原资产三次生成逐字节一致，新增`battle_enable_automatic_machine`且历史64个顶层键逐值不变，正式SHA256为`eea9c95ac51769a9ab16c3aea8867a3319cdc8a7330dad4fed01954c95b94174`。Linux app Debug 14/14通过。
