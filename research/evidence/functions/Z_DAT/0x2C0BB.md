# 函数证据：`sub_2C0BB` `0x2C0BB..0x2C175`

状态：`implemented_pending_review`

机器码以 scene metadata 偏移2处的 NUL 结尾名称计算字节长度 `n`，在 `(150-4n,10,8n+20,27)` 绘制原暗化圆角面板，并在 `(160-4n,15)` 以颜色 `0x0705` 画名称。随后呈现、等待任意键、重绘裸场景并再次呈现后返回；名称长度按原字节流而不是 Big5 字符数计算。

现代 `SceneSession::draw_overlay/resume` 已改用共享原面板算法和动态字节长度，并以 `scene_title -> present -> auto_event_check` continuation 保留按键后的裸场景呈现。独立 scene70 oracle 固定8字节名称、面板 `(118,10,84,27)`、文字 `(128,15)` 与 framebuffer FNV1a64 `0xc5a8777e049759f2`。最终双向 REVIEW 尚未执行。
