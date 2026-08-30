# 函数证据：`sub_2FC9D` `0x2FC9D..0x2FDA6`

状态：`implemented_pending_review`

KDEF opcode47。角色 attack 先做 16-bit `add word` 回绕，再以 signed 值 clamp `[0,100]`；仅最终值比旧值大时显示增加 notice。C++ case47 使用显式回绕的共享 `clamped_add` 与 positive-delta notice；真实 script581 把 role49 attack90 加30，结果100，共享回绕边界由 script673 的正溢出反例固定。Linux Debug scene 通过；最终双向 REVIEW 尚未执行。
