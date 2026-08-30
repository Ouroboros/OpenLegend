# 函数证据：`sub_3CBE3` `0x3CBE3..0x3CC97`

状态：`implemented_pending_review`

机器码先复制 palette entry 231 的3个 RGB6 字节到 entry 224，再令 `j=231..225` 把 entry `j-1` 复制到 `j`，等价于把 entries 224..231 右旋一格。随后复制 entry 252 到 entry 244，再令 `j=252..245` 做同样复制，等价于把 entries 244..252 右旋一格；最后把完整 256×RGB6 palette 交给 `sub_3D939` 提交。

`word_5450A` 不是本函数内部状态；`main`、scene loop 与其他直接调用者在成功呈现后执行 `(counter+1)%5`，仅余数1时调用本函数。该相位是跨 world/scene 的程序级全局，场景跳转不清零。

现代像素变换分别由 `WorldSession::cycle_palette` 与 `SceneSession::cycle_palette` 执行；相位由 `LegacyGameRuntime` 持有并在 scene 构造/continuation 间传递。独立 MMAP.COL oracle 固定首次/第五次右旋后 FNV-1a64 `0x898e23463574ae76`、第六次右旋后 `0x6055f0cfd75adaa6`，并固定 world phase 4 进入 scene、一个外层 tick 后为0、返回 world 仍为0。其他直接调用者仍按各自 inventory 等待映射和最终 REVIEW。
