# 函数证据：`sub_2FEBF` `0x2FEBF..0x2FEDF`

状态：`implemented_pending_review`

KDEF opcode49。把参数低16位直接写角色 word40 `mp_type`，无 clamp、无 UI、无其它副作用。C++ case49 直接 `set_word(mp_type, argument2)`。真实 script581 把 role49 mp_type 从0写2。Linux Debug scene 通过；最终双向 REVIEW 尚未执行。
