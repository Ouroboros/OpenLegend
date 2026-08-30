# 函数证据：`sub_2FFB3` `0x2FFB3..0x30035`

状态：`implemented_pending_review`

KDEF opcode52。机器码以原 Big5 格式串 `你現在的品德指數為%5d` 格式化 role0 word56（morality），调用 `sub_2CEBF(54,40,212,27,255,0,...,4)` 按原11条暗化带与12条边线画圆角面板，再由 `sub_3D832(64,45,...,0x0705,16)` 绘制文字；呈现后阻塞等待任意键，最后重绘场景。

C++ case52 生成相同原始 Big5/ASCII 字节和 `%5d` 空格宽度，以 notice style52 保持同步按键边界；`draw_overlay` 使用共享原面板算法、相同坐标和颜色5/7，acknowledge 后回到无覆盖层场景。真实 script825、morality7 固定文本十六进制和 framebuffer FNV1a64 `0x1cc47112086c10e7`；独立 B7 oracle 从 scene70 原帧、MMAP.COL、FONT.X16/FONT.C16 计算同一结果。Linux Debug BUILD脚本门禁通过；最终双向 REVIEW 尚未执行。
