# Persistence and compatibility integration work package

## 状态

- 实现映射：14/14个B9边界均已有C++承接点。
- 最终单向汇编→C++ REVIEW：5/14终态（`sub_25D0E`至`sub_26B5E`均为`platform_adapted`），9/14仍为`implemented_pending_review/not_started`。
- 当前work package：`B9-WP01`，按阶段终审、验证、提交，不等待14/14一次性收口。

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

## 当前终审队列

1. 已终态阶段：`sub_25D0E/sub_25F87/sub_26208/sub_265AB/sub_26B5E`。系统/槽位页、S/D/R运输、新游戏scene70入口、属性页淡黑、scene共享idle/键态清理及黑world淡入均完成零新增差异轮。
2. 前序待登记/复核：`main/sub_20FAF/sub_24A02/sub_24C8D/sub_25AB7`仍按TSV保持`implemented_pending_review`，不得以旧口头结论替代v6证据。
3. 后续顺序：`sub_2711A/sub_2EB49/sub_30C3D/sub_31241`。
4. 现代日志：SDL3没有当前进程PID API；SDL入口在Windows调用`GetCurrentProcessId()`，Linux/macOS调用`getpid()`，每次启动写`PREFIX-YYYY-MM-DD_HH-MM-SS-{PID}.log`。
5. 每个函数从入口独立恢复机器语义，再单向逐块对照C++；发现差异立即废弃当轮并从入口重启。禁止以C++→汇编或双向REVIEW作为终态证据。

## 验证

- fixed-record读写、缺失/截断失败、完整snapshot往返。
- title/system/scene三类调用者的load目的会话。
- wait帧present前磁盘不变，present后完整写入。
- 序章入口坐标、方向、图号、view、fade/script顺序和首对话。
- 每个进程独立日志路径的确定性格式与相同时间不同PID区分。
- Linux/Windows core/app × Debug/Release完整矩阵、Linux app ASan+UBSan、双平台SDL smoke、资产只读和正式IDB未修改门禁均已完成。

## Closure统计与下一停点

- `persistence-closure.tsv`：14/14实现映射（100%）；5/14最终关闭（35.7%），9/14待终审（64.3%）。
- 五个唯一地址已传播到persistence 5行、input/font 3行和UI 4行，共12条closure：全局由42增至54，即54/349（15.5%）。
- 唯一地址口径：43/284全部重复行均终态（15.1%）；另有4个地址仍为跨表部分终态，未计入43。
- 下一停点：本阶段五函数精确提交并push；随后按TSV状态与`audit_order`继续，不回扫已终态且依赖未变化的行。
