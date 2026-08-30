# 函数证据：`sub_2D1CD` `0x2D1CD..0x2D372`

状态：`implemented_pending_review`

该函数只执行圆角区域的十一段 `sub_3D8AD` 混色，不画白边。`sub_2CC21` 以头像框 `(x,y,60,62)` 调用后，才在 `(x+2,y+59)` 绘制 HDGRP sprite。

现代 `SceneSession::blend_panel` 在对话头像路径中先执行相同混色，再调用 `draw_portrait`；没有提前画边线。独立 oracle 的 style0/1/4 帧覆盖左上、右下和右上头像框。最终双向 REVIEW 尚未执行。
