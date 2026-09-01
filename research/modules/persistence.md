# Persistence and compatibility integration work package

## 状态

- 实现状态：`implemented_pending_review`（14/14个B9边界均已有C++映射；本轮已修正序章入口、初始最大生命公式、槽位I/O present门禁、系统载入返回页和独立会话日志）。
- 最终 REVIEW：`not_started`。
- 当前唯一work package：`B9-WP01`。

## 范围与非范围

- 有限机器码集合锁定为`research/inventory/persistence-closure.tsv`的14个边界，入口从程序主流程、标题新建/载入、运行态导入、工作场景重置、系统/场景槽位菜单、编号槽运输、序章入口、结局与共享shutdown闭合。
- 范围包含完整snapshot运输、baseline导入、新游戏序章入口、读写等待帧、调用方目的会话、退出/结局资源生命周期和每次进程独立日志。
- ranger/scene-map/scene-event字节模型由model owner持有；菜单像素与输入由ui/scene owner持有；B9只负责这些边界的调用顺序与状态交接。
- 独立日志是现代运维要求，不声称来自DOS机器码。

## 汇编与资产证据

- 机器码包：`tmp/b9-review/manifest.tsv`与`tmp/b9-review/0x*.txt`。
- 持久化xref：`research/ida/reports/Z_DAT.persistence_xrefs.txt`。
- 综合证据：`research/evidence/persistence-integration-1to1.md`。
- 字节运输证据：`research/evidence/model-persistence-1to1.md`。
- 等待文本原字节、框几何和present边界来自`sub_25F87`及其被调槽位函数的完整机器码。

## 接口与状态所有权

- `GameState`单一拥有完整`GameSnapshot`；persistence只运输固定记录，不持有第二份游戏状态。
- app单一拥有title/world/scene/battle/menu session生命周期及pending I/O continuation。
- UI只返回typed load/save/quit选择；真正磁盘I/O在app确认等待帧已present后执行。
- scene事件PC和序章首脚本属于`SceneSession`；新游戏入口只通过显式`SceneEntryOverride`传递机器状态。
- 所有状态owner以`research/inventory/module-state-ownership.tsv`为准，函数owner以`module-function-ownership.tsv`为准。

## 当前实现队列

1. `sub_20FAF/sub_24A02/sub_25AB7/sub_26208/sub_265AB`：现有fixed-size S/D/R slot运输、完整snapshot导入/导出及baseline reset已映射；仍为`implemented_pending_review`。
2. `sub_26B5E/sub_2711A`：name/17次属性RNG、`increased_life*3*level+29`最大生命及确认后直接进入scene70；显式入口固定`(19,20)`、方向1、图号6890、view`(8,9)`，fade后首脚本691与首对话2520；仍为`implemented_pending_review`。
3. `sub_25D0E/sub_25F87/sub_2EB49`：系统与场景槽位菜单已映射；编号槽确认先绘制“请稍候”并present，之后才执行I/O；系统菜单载入后保留系统页覆盖在已导入世界上；仍为`implemented_pending_review`。
4. `main/sub_24C8D/sub_30C3D/sub_31241`：标题/世界/场景/结局退出汇聚于runtime-owned资源生命周期；结局保留专属终端消息；仍为`implemented_pending_review`。
5. 现代日志：SDL3没有当前进程PID API；SDL入口在Windows调用`GetCurrentProcessId()`，Linux/macOS调用`getpid()`，每次启动写`PREFIX-YYYY-MM-DD_HH-MM-SS-{PID}.log`。
6. B9实现稳定后，对14项执行独立机器码恢复与不限次数双向逐基本块REVIEW；任何差异均从函数入口重来。

## 验证

- fixed-record读写、缺失/截断失败、完整snapshot往返。
- title/system/scene三类调用者的load目的会话。
- wait帧present前磁盘不变，present后完整写入。
- 序章入口坐标、方向、图号、view、fade/script顺序和首对话。
- 每个进程独立日志路径的确定性格式与相同时间不同PID区分。
- Linux/Windows core/app × Debug/Release完整矩阵、Linux app ASan+UBSan、双平台SDL smoke、资产只读和正式IDB未修改门禁均已完成。

## Closure统计与下一停点

- `persistence-closure.tsv`：0项`pending_mapping`、0项`pending_implementation`、14项`implemented_pending_review`，0项最终关闭；实现映射口径100%，最终REVIEW口径0%。
- 下一停点：精确提交B9实现切片；随后按计划处理其余closure，再从B0开始统一最终双向逐基本块REVIEW。
