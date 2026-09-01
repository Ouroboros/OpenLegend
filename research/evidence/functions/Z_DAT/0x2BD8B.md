# 函数证据：`sub_2BD8B` `0x2BD8B..0x2C0BB`

状态：`implemented_pending_review`

该函数是装备/修炼共用的角色资格门禁。角色与物品均按signed word读取。内力属性在角色或物品任一方为2时通配，否则必须相等；`only_suitable_role!=-1`时必须等于角色ID。内力、攻击、轻功、用毒、医疗、解毒、拳、剑、刀、特殊、暗器十一项均要求角色值不低于物品门槛。

资质门槛为非负时要求`role.iq>=need_iq`；为负时要求`role.iq<=abs(need_iq)`。修炼物品另有两条后处理：物品记录ID为78或93且角色sexual为1时判不适合；若角色十个已学武功槽中已存在该物品的`magic_id`，最终结果强制恢复为适合，即覆盖此前门槛失败。

现代映射为`battle::role_meets_item_requirements`，由`LegacyGameRuntime::handle_menu_item_result`的装备和修炼分支共同调用。装备失败显示“此人不適合配備此物品”；修炼在十武功上限、完整门禁及挥剑自宫确认之后写入绑定。单测覆盖正/负资质边界、MP类型2通配、物品78/93性别门禁、已学武功最后覆盖此前失败条件的机器顺序，以及装备/修炼关联写集。

本切片通过Linux/Windows `core/app × Debug/Release`及Linux app ASan+UBSan门禁。当前仅为`implemented_pending_review`；最终双向逐基本块REVIEW为`not_started`。
