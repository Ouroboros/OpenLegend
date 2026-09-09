# 世界精灵解析缓存：CPU 修复证据

## 修复前 Windows PROFILE

- 基线：`5868358c4786e375d4d78e37c4c6e17b992022c2`，Windows Release＋sanitizers。
- 用户实际运行的 `openlegend.exe`，PID 7812；标题主菜单与 LOAD 后空闲各一份 WPR CPU ETL，保存匹配的 EXE/PDB。
- 本地原始记录：`tmp/cpu-profile-13028-4444/`。使用 Windows Toolkit `xperf` 导出目标进程的函数和 butterfly 调用链，并加载匹配游戏 PDB 及微软 `ntdll` 符号。
- 两份 ETL 均无丢失事件、无丢失缓冲区；完整 trace 时长分别约 40.214 秒、40.033 秒，包含 WPR 停止时的收尾区间，不能将完整 trace 当作严格 20 秒。
- 游戏内目标进程共有 13,125 个带栈的 Profile 样本。包含子调用的命中：`WorldSession::render` 11,978（91.26%），`SpriteFrameView::parse` 6,849（52.18%），`SdlRuntimePlatform::present` 482（3.67%），`AudioMixer::render` 304（2.32%）。包含子调用的比例不能相加，也不是整机 CPU 使用率。
- 函数加权 self CPU 热点是 `ntdll!RtlpUnwindPrologue`（34.83%）和 `ntdll!RtlpLookupFunctionEntryForStackWalks`（18.22%）。调用链与 C++ 对应到精灵解析/销毁中的临时数组分配、释放，经 ASan 进入 Windows 栈回溯。
- 先前 Linux 无窗口采样不是上述 Windows 现象的根因依据，不用于本次结论。

## 原版行为边界

唯一行为真值仍是原版 Z.DAT 和原始资产；本次不新增行为 Golden。

- `sub_3D6E0` 加载 GRP 并建立 IDX 指针表；`sub_3D643` 将精灵编号除以 2，查指针后调用 `sub_20354`。
- `sub_20354 @ 0x20354..0x2050A` 的完整汇编为 167 条指令、0 次函数调用，直接读取 RLE 并以 `rep movsb` 写入帧缓冲，不建立现代解析容器。
- `sub_2558B` 保持地面、覆盖层、深度列表中的建筑/人物/船、天气的覆盖顺序。
- 原始指令见 `research/ida/reports/Z_DAT.targets.txt`、`Z_DAT.b6_world_xrefs.txt`；资源和绘制合同见 `resource-loader-1to1.md`、`world-map-1to1.md`。

## 现代实现变化

- 仅缓存 `WorldSession` 中 MMAP 精灵的解析结果，按 archive entry 编号索引，首次使用时解析一次；错误解析结果也保留，后续仍返回失败。
- 原始 GRP 本来就驻留内存，本次没有增加磁盘缓存、磁盘读取或全游戏预加载。
- 缓存保存行/run 元数据，像素 span 继续引用不可变的原始 GRP。GRP 使用共享所有权，保证复制世界会话并销毁原会话后，缓存 span 仍有效。
- 缓存随会话生命周期释放；保持原渲染函数、裁剪、索引映射、调色板和覆盖顺序。未缓存最终画面，天气/人物动画和调色板继续更新。
- 未修改 18.2Hz 节拍、输入、音频、SDL 呈现或 sanitizer 配置。

## 验证

- 新增重复渲染、热缓存会话复制后原对象销毁、移动后源对象销毁的回归；像素期望使用既有独立原资产 Oracle 的初始帧值 `0x0604155353F95194`，不以缓存实现生成新期望。
- 原有移动、深度顺序、人物帧、天气与调色板回归继续执行。
- Windows `build.bat app --config Debug --tests --data-dir E:\Game\OpenLegend\data`：120/120 通过。
- Windows `build.bat app --config Release --sanitizers --tests --data-dir E:\Game\OpenLegend\data`：120/120 通过。
- 日志：`tmp/sprite-cache-debug.log`、`tmp/sprite-cache-asan.log`。
- 修复后的用户现场 CPU 百分比和缓存内存增量尚未测量；不承诺低于 0.1%，不将回归通过当作性能改善幅度的实测结果。
