# 函数证据：`sub_2BD8B` `0x2BD8B..0x2C0BB`

状态：`platform_adapted`

该函数是装备/修炼共用的角色资格门禁。角色与物品均按signed word读取。内力属性在角色或物品任一方为2时通配，否则必须相等；`only_suitable_role!=-1`时必须等于角色ID。内力、攻击、轻功、用毒、医疗、解毒、拳、剑、刀、特殊、暗器十一项均要求角色值不低于物品门槛。

资质门槛为非负时要求`role.iq>=need_iq`；为负时要求`role.iq<=abs(need_iq)`。修炼物品另有两条后处理：物品记录ID为78或93且角色sexual为1时判不适合；若角色十个已学武功槽中已存在该物品的`magic_id`，最终结果强制恢复为适合，即覆盖此前门槛失败。

现代映射为`battle::role_meets_item_requirements`，由`LegacyGameRuntime::handle_menu_item_result`的装备和修炼分支共同调用。装备失败显示“此人不適合配備此物品”；修炼在十武功上限、完整门禁及挥剑自宫确认之后写入绑定。单测覆盖正/负资质边界、MP类型2通配、物品78/93性别门禁、已学武功最后覆盖此前失败条件的机器顺序，以及装备/修炼关联写集。

## 最终独立REVIEW

fresh IDA从只读原始`Z.DAT`重建`0x2BD8B..0x2C0BB`：816 bytes、163条指令、45个基本块、26条分支、2个call、单一`RETN`，loaded SHA256=`a2731bbad8e4a63dbcc968315d0666dc6a6918688e2d53604d6dcc45274159a1`。两个caller严格为`0x2AAC0/0x2AED7`，均属于共享菜单函数`sub_2A86C`；caller closure未传播到本入口。

从入口逐块正向对照确认MP type2通配、only-role、十一项signed下界、正/负IQ的相反比较、物品78/93性别门和十个magic槽扫描均一致。反向从`role_meets_item_requirements`每个现代分支回到机器基本块，确认magic ID命中位于最后且可把此前任意失败重新置为true；这不是短路优化。装备与修炼的消息、确认和绑定仍归caller continuation。

合法资产/状态域未发现新差异。现代对非法role/item索引的安全拒绝避免原版裸数组访问，故最终状态为`platform_adapted`而非`assembly_exact`。全入口重审状态为`converged_no_new_differences`。
