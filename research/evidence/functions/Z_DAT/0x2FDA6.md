# 函数证据：`sub_2FDA6` `0x2FDA6..0x2FEBF`

状态：`implemented_pending_review`

KDEF opcode48。maximum_hp 做原生 int16 加法，不 clamp；随后 hp 直接赋新 maximum_hp。notice 比较 `new_hp-old_hp>0`，且额外要求角色 ID 出现在六个队伍槽任一处；是否在队伍只影响 notice，不影响状态写入。

旧 C++ clamp999并给当前hp加delta，且非队员也 notice。现专用 case48。真实 script581 在 role49 尚未入队时把 maximum_hp900、hp100 加200，两者仍写1100但不由此 helper 出 notice；后续才 opcode10 入队。Linux Debug scene 通过；最终双向 REVIEW 尚未执行。
