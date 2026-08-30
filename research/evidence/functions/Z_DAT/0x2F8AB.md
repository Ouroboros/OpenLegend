# 函数证据：`sub_2F8AB` `0x2F8AB..0x2F8D1`

状态：`implemented_pending_review`

KDEF opcode40。机器码把参数写入朝向全局，再从四项基础图片表复制对应当前玩家图片。C++ `SceneSession::run_event(case40)` 清除图片 override、设置方向、把步行动画 offset 归零；`player_frame()` 随即返回同一方向基础图片，`commit_header` 同步朝向。

当前 KDEF 中 opcode40 共12处且参数均为0..3。真实 script235 已固定 direction3、walk offset0 和 player frame5044，独立 B7 oracle 同步输出该向量；C++ clamp 只保护当前脚本不可达的越界参数。Linux Debug `openlegend.scene` 通过；最终双向 REVIEW 尚未执行。
