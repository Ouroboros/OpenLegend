# 函数证据：`sub_2C0BB` `0x2C0BB..0x2C175`

状态：`platform_adapted`

## 最终汇编→C++ REVIEW

当前 `Z.DAT` 函数范围为 `0x2C0BB..0x2C175`，共186字节、53条指令；B7报告 SHA256 为 `9e2310396c323ba7647fa6afec3ecf27f5081dc7ed9f2a0139430833c977d4a9`。本函数实际职责是普通scene进入及内部jump后的名称提示，不是退出提示。对照发现并修正标题前额外scene重绘后，旧结论作废；最后一轮从入口重审全部指令、直接callee、两个物理caller和84条scene metadata，零新增差异。

机器码从 `scene_metadata + scene_id*52 + 2` 复制NUL结尾名称到临时缓冲，并按字节而非Big5字符计算长度 `n`。它直接在caller已经绘制的scene framebuffer上，以 `(150-4n,10,8n+20,27)` 绘制暗化圆角面板，再于 `(160-4n,15)` 以颜色字 `0x0705` 绘制名称并present。

随后 `sub_20C32` 先清最后按键码，再阻塞到任意非零键；按键后调用 `sub_29D2D` 完整重绘无标题scene并再次present。函数自身返回第二次present的结果，两个caller均不使用该返回值。

## Caller时序

跨全部xrefs报告去重后只有 `sub_28E40` 内两个物理callsite：

- `0x28F4B`：普通scene载入、事件图扫描、首次scene render/present及淡入完成后显示名称；返回后在 `0x28F53` 调自动事件入口。
- `0x292A3`：内部jump载入目标scene、事件图扫描、scene render/present及淡入完成后显示目标名称；返回后在 `0x292AB` 调自动事件入口。

因此严格输出序列为 `scene_title -> present -> key -> bare_scene_present -> auto_event_check`；名称overlay前不得再次调用scene render。

## 现代映射与修正

- `SceneSession::show_scene_title` 从metadata名称槽复制原字节；`draw_overlay`按首个NUL的字节距离计算同一面板和文字几何。
- `LegacyGameRuntime` 在标题态把任意新translated key映射为acknowledge，等价于机器清旧键后等待任意非零键。
- `SceneSession::render` 现于标题态冻结进入前已有framebuffer；首次及重复标题render均从该底图恢复后只叠overlay。此前标题态会先完整重绘scene，特殊天气scene因此比机器提前消费两次RNG，已修正。
- 标题按键后返回 `present`，此时才完整重绘scene；该present回收后执行自动事件检查，与两个caller顺序一致。
- modern为窗口重绘保留冻结底图，且对非法scene ID稳定拒绝；这是平台适配，不声明机器非法内存域等价，分类为 `platform_adapted`。

## 资产域与验证

84条正式scene metadata名称均在10字节槽内NUL终止，长度分布为4字节13条、6字节47条、8字节24条，无空名、奇数长度或跨记录读取。scene70名称为8字节，面板 `(118,10,84,27)`、文字 `(128,15)`，framebuffer FNV1a64 保持 `0xc5a8777e049759f2`。

scene5天气回归验证：标题首次render与重复render均不推进RNG且frame字节幂等；按键后的无标题scene重绘恰消费两次 `bounded(7)`。独立oracle登记53条指令、两个caller、84条长度分布、任意键、冻结底图及 `0/0/2` RNG时点；双次字节一致，SHA256 为 `eac9b120e1560c3a9fe22d7a9cf1b5e7ee754a823f9d5cc377bd146af9fc492b`。Linux app Debug 14/14通过（`proc_244d`）。原程序动态差分因运行环境不可用记为 `blocked_runtime_oracle`，不替代静态机器码审计。
