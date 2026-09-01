# 函数证据：`sub_3C2AC` `0x3C2AC..0x3C563`

状态：`implemented_pending_review`

部分映射：`BattleSetup::apply_battle_crafting`。

制造需求为练功物品`need_make_item_experience*(7-IQ/15)`；需求字段必须严格大于0，角色制造经验以unsigned读取。先取库存中首个所需材料槽，再按五个配方顺序标记“产物ID不等于-1且材料数不少于需求”的可用项。有任一可用项后持续消费`bounded(5)`，直到抽中标记项，保留不可用配方导致的重复RNG消费。

消息抑制参数非0时，原函数在选出配方后直接返回且不制造。正常路径先显示消息；若产物库存已存在，数量增加`bounded(3)+1`，再扣材料；若不存在，写入首个ID=-1槽且数量仅加1，不消费数量RNG。材料数不大于0时调用原库存槽左移；成功后制造经验清0。库存已满且产物不存在时，已显示消息但不扣材料、不清经验。

seed1且仅配方0可用时，选择RNG序列严格为`3,3,3,0`，已有产物数量增加2，材料3→1、产物4→6，RNG最终状态4182499122。`BattleSession`在制造框前以共享RNG完成原反复`bounded(5)`配方选择但不改库存，执行原`(55,30,210,27)`制造框、`%s 製造出 %s`Big5文字、present和任意键等待；按键后才消费已有产物数量RNG并提交产物、材料与制造经验；真实battle4制造帧FNV64为`0xb980de17004d5b6c`。Linux Debug完整BUILD 14/14，因此推进为`implemented_pending_review`；最终完整双向REVIEW尚未开始。
