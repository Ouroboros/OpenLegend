# 函数证据：`sub_2FBC0` `0x2FBC0..0x2FC9D`

状态：`implemented_pending_review`

KDEF opcode46。maximum_mp 做原生 int16 加法，不 clamp；随后 mp 直接赋新 maximum_mp。notice 条件/数值比较的是 `new_mp-old_mp>0`，不要求角色在队伍中。

旧 C++ clamp999并给当前mp加delta，错误保留亏损。现专用 case46 精确赋值。真实 script581：role49 maximum_mp900、mp100，加300后两者均1200（当时尚未加入队伍），并产生1100增量 notice。Linux Debug scene 通过；最终双向 REVIEW 尚未执行。
