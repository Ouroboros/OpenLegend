# 场景行走与战斗的宿主额外等待

## 原版证据

本轮直接核对当前 `Z.DAT`，SHA256：
`0034f836def2b287e62b0902f08cab22b61b397f24d44d6038274f1750a007ce`。

`main`、`sub_28E40`、`sub_29819`、`sub_299A1`、`sub_3271E`、
`sub_33599`、`sub_37355`、`sub_3DB83` 共八个函数的完整汇编字节，
回减绝对地址的 `0x20000` 加载差后，与原文件对应切片逐字节一致。
代码对象的原文件偏移为机器地址减 `0x6600`。
临时核对工具与结果：`tmp/tick-machine-audit.py`、`tmp/tick-machine-audit.log`。

| 原版位置 | 时序合同 |
| --- | --- |
| `main @ 0x20D35`，`0x20E1D` / `0x20F7B` | 大地图循环开始捕获 BIOS tick；循环末仅在捕获值仍相等时等待。 |
| `sub_28E40`，`0x28F66` / `0x292B0` | 场景循环同样在开始捕获、末尾等待；普通呈现后的续行不另等一 tick。 |
| `sub_29819 / sub_299A1` | 单次移动没有 delay 或 tick 自旋；唯一直接调用是栈探测。 |
| `sub_3271E`，`0x329B9` → `0x329F9` | actor 呈现之后同步进入 AI，中间没有 BIOS 等待。 |
| `sub_33599`，`0x33683` / `0x33688` | AI 前置呈现后调用 `delay(300)`，等八次 tick 变化。 |
| `sub_37355`，`0x3753E` / `0x37540` | 移动每格呈现后调用 `delay(40)`，等两次 tick 变化。 |
| `sub_3271E`，`0x327E3` / `0x32A3D` | 整轮开始捕获 tick；轮末若已经变化，不再追加等待。 |

BIOS 周期仍为 `65536 / 1193182` 秒。`sub_3DB83` 对非负参数执行
`argument / 40 + 1` 次“不相等”观察；40、100、300不是毫秒值，
分别对应2、3、8次 tick 变化。不能为加快战斗而删除这些原版延时。

## 现代差异与修正范围

过去 SDL 每次 `advance → render → finish_presented_tick` 后一律等待 tick。
场景的普通呈现还要在下次 `advance` 才确认，并立即返回，导致常规行走
需要两个宿主循环。低于一个 tick 的渲染耗时下，其单步节拍因此减半。

普通场景循环的呈现现在在实际呈现完成后立即确认；循环尾仍由宿主等待。
不把事件脚本的显式等待当作普通行走呈现，也不改变淡入淡出或 BIOS 频率。

战斗通过 `needs_immediate_frame` 区分待继续的无等待阶段与稳定等待阶段。
待呈现的动作/面板以及 AI 分派可以在同一 BIOS tick 继续；呈现完成后进入的
`*_wait` 仍按原计数等待。轮末以本轮捕获 tick 判断是否还需等待。
输入等待和淡入淡出仍保持宿主限速，避免无输入时忙循环。
SDL 继续逐帧呈现、轮询事件并同步键态清除，不合并或跳过原版画面序列。

## 回归合同与验证边界

- `title_menu_test.cpp`：隔离出口与自动事件后，在客栈原有可通行格之间，
  连续十三次 `advance → render → finish_presented_tick` 各走一步；
  不插入仅用于确认呈现的额外逻辑 tick。
- 同文件：战斗 actor 呈现与菜单首次呈现无需额外 tick；进入菜单输入等待后
  恢复宿主限速。
- `battle_data_test.cpp`：actor → AI 在同一 tick 续行；AI 前置仍等待八次变化，
  移动仍等待两次变化，同一个 tick 重复调用不能减少剩余等待。

这些回归检查现代运输是否实现机器恢复出的时序，不以现代结果反推原版。
本轮没有 DOS 动态帧时间实测；静态等待边界与宿主回归不等于实际玩家测速。

Windows 统一 BUILD 验收：

- `build.bat app --config Debug --tests --data-dir E:\Game\OpenLegend\data`：120/120通过。
- `build.bat app --config Release --sanitizers --tests --data-dir E:\Game\OpenLegend\data`：120/120通过。
- 日志：`tmp/tick-debug.log`、`tmp/tick-asan.log`。
