# 函数证据：`sub_2FAB7` `0x2FAB7..0x2FBC0`

状态：`platform_adapted`；最终 REVIEW：`converged_no_new_differences`

来源：当前 `Z.DAT` 机器码与原始文件字节；headless IDA完整导出；当前 `KDEF.IDX/GRP`、`RANGER.GRP`；`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`。

## 1. 机器身份与caller

- 函数物理范围 `0x2FAB7..0x2FBC0`，265 bytes，67条指令。
- loaded SHA256 `2e6e643921f8d589ec36fd4ebea954e63592421455a879e477db99cd968134fc`；原始文件 `Z.DAT[0x294B7:0x295C0]` SHA256 `351ee1ecd229a5c25c70c3d682ae30dd380c3032a0ce81186f6aecc0b1b24b84`。
- 15个差异字节全部是角色轻功、角色名、格式串、文本缓冲区、framebuffer及呈现状态地址raw加加载基址 `0x20000` 的重定位；规范化后与loaded逐字节相同，未解释差异为零。
- 唯一物理代码caller是 `sub_2C319:0x2C9FD` 的opcode45；`0x55670`只是函数地址表数据引用。
- caller依次读取KDEF `argument(1)`和`argument(2)`，均sign-extend；按cdecl逆序先压delta再压role并调用。共享退出块清理8字节栈，把PC固定增加3 words；返回值不使用。
- 入口 `sub_3ED1E(40)` 仅为Watcom栈探测合同。

## 2. 不看C++的轻功写入恢复

1. 以 `role_id*182` 定位角色记录，sign-extend并保存word44轻功原值。
2. 对该word执行低16位add；结果先按signed `<0`钳到0，再按signed `>100`钳到100。
3. sign-extend最终值并减去原signed值，得到32位实际gain。
4. gain小于等于0时直接返回0，不显示提示，也不重绘场景。
5. gain严格大于0时显示实际gain；提示数字不必等于传入delta。例如原值-32768、delta-1先回绕32767再钳100，提示32868；原值-1、delta0钳0并提示1。
6. 可见与不可见两路均固定返回0。

## 3. 原Big5轻功提示与时序

可见路径使用含末尾NUL的15字节原格式串 `25 73 20 BB B4 A5 5C BC 57 A5 5B 20 25 64 00`，即 `%s 輕功增加 %d`。机器从角色记录byte8开始按C串读取名字并格式化实际gain。

令角色名字节数为N、`A=N+10`，机器绘制：

- 圆角混色面板 `(150-(4A+24),40,8A+68,27)`；
- 文字位置 `(160-(4A+24),45)`，颜色5/7；
- A不包含末尾十进制gain的位数。

面板直接叠加在caller已有framebuffer上，随后present、等待任意非零翻译键，再重绘并present裸场景后返回。委托panel、Big5文字、present、按键和裸场景绘制closure不由本行传播关闭。

## 4. 首轮差异与从入口重审

首轮汇编→C++ REVIEW确认word44、16位回绕、signed钳位、actual-gain条件和PC一致，但发现提示UI仍不一致：

- 原C++给opcode45构造ASCII `role <id> +<gain>`，没有使用角色原名和原Big5 `輕功增加`；
- 默认notice使用固定样式，机器使用按角色名字节长度变化的圆角面板、颜色5/7并冻结caller framebuffer。

最小修正只为opcode45新增原Big5中缀，复用已经按同型机器函数验证的角色属性动态面板与冻结底图运输；opcode47现有提示不变。随后废弃首轮结论，从入口重新逐条复核67条指令、15项重定位、word44 add/clamp、signed gain、无提示分支、格式串、角色C串、十进制实际gain、名称长度几何、颜色、present/等待/恢复、返回0和caller PC，零新增差异。

机器对非法role索引和损坏无NUL名字执行越界访问；现代仅在该资产未使用域内做记录边界保护。宿主以非阻塞notice/response运输替代函数内同步等待，归类 `platform_adapted`。

## 5. 全KDEF、文本与像素oracle

当前1,018条KDEF只有3次opcode45。按little-endian `<IIhh>` 编码 `(script,PC,role_id,delta)` 的完整参数流SHA256为 `6f4b605be4790b2b6b9da4ffa8f57a41adce87dd59248efc5cdfa28291518bf3`：

- 位置为 `(484,179,53,30)`、`(536,200,49,20)`、`(581,40,49,20)`；全部role合法。
- 按当前RANGER基线单次执行，script484从38增至68，角色53提示30，帧FNV-1a64 `0xca05da16dedc9e31`；script536与581均从30增至50，角色49提示20，帧FNV-1a64 `0xafb2996c4455f825`。
- 三帧共同面板为 `(70,40,180,27)`、文字位置 `(80,45)`，颜色5/7；script581原文字节为 `B5 EA A6 CB 20 BB B4 A5 5C BC 57 A5 5B 20 32 30 00`。
- 合成边界固定：32767+1回绕负后钳0且无提示；-32768-1回绕正后钳100并提示32868，帧 `0x4b693c807a3f6ee2`；-1+0钳0并提示1，帧 `0x112ab8f9e491ec4d`。两帧串联SHA256 `cb35e133337fd40c1560d9bfc197a18250a2af872cebb4b467b0a21cad83bd32`。
- 非法role `-1`由现代边界保护稳定拒绝，不写角色、不显示提示。
- 重复render固定同帧且不消费RNG；确认后固定scene present。

双次独立生成字节一致；正式 `scene-goldens.json` SHA256 `dc26493b0a0c521f5f56be29203f8bf9e7d4c1fc6ac1f3828d29c353140da334`。

## 6. 验证与结论

- Linux app Debug根构建：14/14 tests通过（`proc_101a`）。
- 原程序动态差分：`blocked_runtime_oracle`。

```text
closure_status = platform_adapted
final_review = converged_no_new_differences
remaining =
```
