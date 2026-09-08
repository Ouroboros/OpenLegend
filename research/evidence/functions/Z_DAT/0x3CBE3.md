# 函数证据：`sub_3CBE3` `0x3CBE3..0x3CC97`

状态：`platform_adapted`

机器码先复制 palette entry 231 的3个 RGB6 字节到 entry 224，再令 `j=231..225` 把 entry `j-1` 复制到 `j`，等价于把 entries 224..231 右旋一格。随后复制 entry 252 到 entry 244，再令 `j=252..245` 做同样复制，等价于把 entries 244..252 右旋一格；最后把完整 256×RGB6 palette 交给 `sub_3D939` 提交。

`word_5450A` 不是本函数内部状态；`main`、scene loop 与其他直接调用者在成功呈现后执行 `(counter+1)%5`，仅余数1时调用本函数。该相位是跨 world/scene 的程序级全局，场景跳转不清零。

现代像素变换分别由 `WorldSession::cycle_palette` 与 `SceneSession::cycle_palette` 执行；相位由 `LegacyGameRuntime` 持有并在 scene 构造/continuation 间传递。独立 MMAP.COL oracle 固定首次/第五次右旋后 FNV-1a64 `0x898e23463574ae76`、第六次右旋后 `0x6055f0cfd75adaa6`，并固定 world phase 4 进入 scene、一个外层 tick 后为0、返回 world 仍为0。

## 最终独立REVIEW

fresh IDA从只读原始`Z.DAT`重建`0x3CBE3..0x3CC97`：180 bytes、59条指令、13个基本块、6条分支、2个call、单一`RETN`，loaded SHA256=`d1f0d4ccab020338589a26ae9ce571dfbb7cc637d53052e2dd25f2058296e4eb`。三个caller严格为`0x20E64`（main/world）、`0x26A51`（scene）和`0x290FF`（scene continuation）；每个caller的5-tick相位与本callee独立审计。

从入口逐指令正向确认两段保存末项、descending复制和完整palette submit；`std::rotate(first,last-1,last)`在两个session中与机器右旋一格完全等价。反向检查runtime时序确认render只预览本次palette，重复render幂等，且只有成功present后的`finish_presented_tick()`才推进全局相位并提交session palette；present失败不提前改变状态。

合法RGB6资产域未发现新差异。VGA DAC提交、无超时轮询与host present成功边界属于`platform_adapted`。全入口重审状态为`converged_no_new_differences`。
