# 函数证据：`sub_2CEBF` `0x2CEBF..0x2D1CD`

状态：`implemented_pending_review`

当宽高均大于10时，机器码先用十一段矩形构成削去五级角的圆角区域，并调用 `sub_3D8AD` 以 palette0 和目标像素各取 `1/8` 后经 RGB4 最近色表写回；随后用十二段颜色255矩形画白色边线。宽或高不大于10时不绘制。

现代 `SceneSession::blend_panel/draw_panel_border/draw_panel` 使用相同十一段混色坐标、十二段边线坐标和严格 `>10` 门；对话正文框在头像框之前绘制。独立 style0/1/2/4 framebuffer hashes 覆盖四种实际资产布局。最终双向 REVIEW 尚未执行。
