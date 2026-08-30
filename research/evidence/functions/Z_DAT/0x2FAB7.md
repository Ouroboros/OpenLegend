# 函数证据：`sub_2FAB7` `0x2FAB7..0x2FBC0`

状态：`implemented_pending_review`

KDEF opcode45。角色 speed 做 int16 加法后 clamp `[0,100]`；仅最终值比旧值大时显示增加 notice。C++ case45 使用 `clamped_add` 与 positive-delta notice。真实 script581 把 role49 speed90 加20，结果100。Linux Debug scene 通过；最终双向 REVIEW 尚未执行。
