# 函数证据：`sub_2FDA6` `0x2FDA6..0x2FEBF`

状态：`platform_adapted`；最终 REVIEW：`converged_no_new_differences`

来源：当前 `Z.DAT` 机器码与原始文件字节；headless IDA完整导出；当前 `KDEF.IDX/GRP`、`RANGER.GRP`；`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`。

## 1. 机器身份与caller

- 函数物理范围 `0x2FDA6..0x2FEBF`，281 bytes，78条指令。
- loaded SHA256 `d21e17ec2a15d273a7e806b1aa25dd70ddcfd1c453b837547a0e94ada0df8ef6`；原始文件 `Z.DAT[0x297A6:0x298BF]` SHA256 `2f692b666b34df41c96248a5c849c9a2d5a054dd4a3f570482cd15d8e56516fa`。
- 14个差异字节全部是角色生命、队伍槽、角色名、格式串、文本缓冲区、framebuffer及呈现状态地址raw加加载基址 `0x20000` 的重定位；规范化后与loaded逐字节相同，未解释差异为零。
- 唯一物理代码caller是 `sub_2C319:0x2CA4B` 的opcode48；`0x5567C`只是函数地址表数据引用。
- caller从KDEF依次sign-extend delta和role，按cdecl逆序先压delta再压role并调用。共享退出块清理8字节栈，把PC固定增加3 words；返回值不使用。
- 入口 `sub_3ED1E(44)` 仅为Watcom栈探测合同。

## 2. 不看C++的最大生命写入恢复

1. 以 `role_id*182` 定位角色记录，sign-extend并保存word17当前生命原值。
2. 把delta低16位加到word18最大生命，不做任何钳位；再把新最大生命原样覆盖word17当前生命。
3. 完整读取六个队伍槽，不因空槽或已经命中而停止；任一槽的signed角色ID与入参精确相等即保留found位。
4. sign-extend新当前生命并减去修改前当前生命，得到32位实际gain。
5. 只有gain严格大于0且found精确为1时显示实际gain。角色不在队伍只抑制提示，不影响最大生命和当前生命写入。
6. 可见与不可见两路均固定返回0。

## 3. 原Big5生命提示与时序

可见路径使用含末尾NUL的15字节原格式串 `25 73 20 A5 CD A9 52 BC 57 A5 5B 20 25 64 00`，即 `%s 生命增加 %d`。机器从角色记录byte8开始按C串读取名字并格式化实际gain。

机器单独以角色名计算长度。令角色名字节数为N、`A=N+10`，绘制：

- 圆角混色面板 `(150-(4A+24),40,8A+68,27)`；
- 文字位置 `(160-(4A+24),45)`，颜色5/7；
- 面板宽度不随末尾十进制gain位数变化。

面板直接叠加在caller已有framebuffer上，随后present、等待任意非零翻译键，再重绘并present裸场景后返回。委托panel、Big5文字、present、按键和裸场景绘制closure不由本行传播关闭。

## 4. 首轮差异与从入口重审

首轮汇编→C++ REVIEW确认word18低16位回绕、不钳位、覆盖word17、actual-gain条件、返回0和PC一致，但发现两项差异：

- 原C++给opcode48构造ASCII `role <id> +<gain>`，没有使用角色原名和原Big5 `生命增加`，也没有采用机器的动态面板、颜色和冻结caller framebuffer；
- 共用的队伍查询在首次命中时提前返回，而机器固定读取全部六槽。

最小修正给opcode48接入原Big5角色属性提示，并把队伍查询改为完整扫描后返回found。随后废弃首轮结论，从入口重新逐条复核78条指令、14项重定位、word17/18写入、六槽扫描、signed gain、队伍条件、格式串、角色C串、名称几何、颜色、present/等待/恢复、返回0和caller PC，零新增差异。

机器对非法role索引和损坏无NUL名字执行越界访问；现代仅在该资产未使用域内做记录边界保护。宿主以非阻塞notice/response运输替代函数内同步等待，归类 `platform_adapted`。

## 5. 全KDEF、文本与像素oracle

当前1,018条KDEF共有8次opcode48。按little-endian `<IIhh>` 编码 `(script,PC,role_id,delta)` 的完整参数流SHA256为 `4d64ef2844e5443472d446cd76426f0c3d5e5cfab53af6c3d952bfebaf1e908f`：

- 位置为script115的 `(PC54,role10,200)`、`(PC57,role11,200)`、`(PC60,role12,200)`、`(PC63,role13,200)`、`(PC66,role14,200)`、`(PC69,role15,200)`，以及 `(536,194,49,200)`、`(581,34,49,200)`；全部role合法。
- 按当前RANGER初始队伍 `[0,-1,-1,-1,-1,-1]` 隔离单次执行，八次均完成两项生命写入且均不提示。真实script581全事件测试另以角色49最大生命900、当前生命100夹具固定写为1100，并确认该角色稍后入队前本helper不显示生命提示。
- 合成边界固定：最大生命32767加1回绕到-32768，相对旧当前生命-32768的gain为0且不提示；最大生命-32768减1回绕到32767，gain65535并提示，帧 `0xa5ad1328bffe69df`；最大生命100、当前生命0、delta0仍覆盖为100、gain100并提示，帧 `0x831ad6f0707bcda1`；负gain不提示；非队员正gain仍写状态但不提示。两帧串联SHA256 `680e31fe85d9209b74750606984612d5f00041776173c5ff83f105642d72982b`。
- 六字节角色名使用面板 `(62,40,196,27)`和文字位置`(72,45)`，颜色5/7；非法role `-1`由现代边界保护稳定拒绝。

双次独立生成字节一致；正式 `scene-goldens.json` SHA256 `371a021c0b630638e06fed2dba7e6f9f69c6cbb847bb3681d8213e4c6c6743cf`。

## 6. 验证与结论

- Linux app Debug根构建：14/14 tests通过（`proc_c0aa`）。
- 原程序动态差分：`blocked_runtime_oracle`。

```text
closure_status = platform_adapted
final_review = converged_no_new_differences
remaining =
```
