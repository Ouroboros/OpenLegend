# 函数证据：`sub_2DE7D` `0x2DE7D..0x2DF0E`

状态：`implemented_pending_review`

opcode9 清当前键，复制原 Big5“是否要求加入（Ｙ／Ｎ）”，在 `(61,40,187,27)` 绘混色圆角 panel、于 `(71,45)` 以颜色 `0x0705` 绘字，present 后等待任意键。与另外两类问题不同，本函数在取得键值后无条件调用 `sub_2D653` 重绘并 present 裸场景，之后才按仅大写 `Y` 选择 true/false offset。现代 `conditional_after_present` 保留该独立同步边界。

独立 scene70 frame hash `0xbea93863a81cd9e0`；synthetic opcode9 固定 question→bare present→branch。最终 REVIEW 仍需审计委托 panel/text/present/key helper。
