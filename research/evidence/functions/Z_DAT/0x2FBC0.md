# 函数证据：`sub_2FBC0` `0x2FBC0..0x2FC9D`

状态：`platform_adapted`；最终 REVIEW：`converged_no_new_differences`

来源：当前 `Z.DAT` 机器码与原始文件字节；headless IDA完整导出；当前 `KDEF.IDX/GRP`、`RANGER.GRP`；`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`。

## 1. 机器身份与caller

- 函数物理范围 `0x2FBC0..0x2FC9D`，221 bytes，61条指令。
- loaded SHA256 `f27830de48bd29e0795ef8a44aeb8c24df025b6691f039e2eb780e29cb8508e2`；原始文件 `Z.DAT[0x295C0:0x2969D]` SHA256 `e259d06002c9fd180dd662c19cfc1484da5d916e29e77aa5002da13f5d5b0436`。
- 12个差异字节全部是角色当前/最大内力、角色名、格式串、文本缓冲区、framebuffer及呈现状态地址raw加加载基址 `0x20000` 的重定位；规范化后与loaded逐字节相同，未解释差异为零。
- 唯一物理代码caller是 `sub_2C319:0x2CA17` 的opcode46；`0x55674`只是函数地址表数据引用。
- caller从KDEF依次sign-extend delta和role，按cdecl逆序先压delta再压role并调用。共享退出块清理8字节栈，把PC固定增加3 words；返回值不使用。
- 入口 `sub_3ED1E(40)` 仅为Watcom栈探测合同。

## 2. 不看C++的内力写入恢复

1. 以 `role_id*182` 定位角色记录，先sign-extend保存word26当前内力原值。
2. 把delta的低16位加到word27最大内力；结果按word自然回绕，不做任何上下限钳位。
3. 读取新最大内力的低16位，并原样覆盖word26当前内力；旧当前内力不会按delta增减，也不会保留亏损。
4. sign-extend新最大内力并减去旧当前内力，得到32位gain；比较基准不是旧最大内力。
5. gain小于等于0时直接返回0；gain严格大于0时显示该实际差值。角色是否已在队伍中不参与判断。
6. 因此delta为0仍可能提示；负delta仍会先覆盖当前内力；例如最大内力-32768、当前内力-32768、delta-1回绕得到32767并提示65535。
7. 可见与不可见两路均固定返回0。

## 3. 原Big5内力提示与时序

可见路径使用含末尾NUL的15字节原格式串 `25 73 20 A4 BA A4 4F BC 57 A5 5B 20 25 64 00`，即 `%s 內力增加 %d`。机器从角色记录byte8开始按C串读取名字并格式化实际gain。

机器单独以角色名计算长度。令角色名字节数为N、`A=N+10`，绘制：

- 圆角混色面板 `(150-(4A+24),40,8A+68,27)`；
- 文字位置 `(160-(4A+24),45)`，颜色5/7；
- 面板宽度不随末尾十进制gain位数变化。

面板直接叠加在caller已有framebuffer上，随后present、等待任意非零翻译键，再重绘并present裸场景后返回。委托panel、Big5文字、present、按键和裸场景绘制closure不由本行传播关闭。

## 4. 首轮差异与从入口重审

首轮汇编→C++ REVIEW确认word26/27、最大内力回绕后同时赋给当前内力、以旧当前内力计算gain、不检查队伍、返回0与PC均一致，但发现提示UI仍不一致：

- 原C++给opcode46构造ASCII `role <id> +<gain>`，没有使用角色原名和原Big5 `內力增加`；
- 默认notice没有采用机器按角色名字节长度生成的圆角面板、颜色5/7和冻结caller framebuffer。

最小修正只为opcode46新增原Big5中缀并复用已验证的角色属性动态面板；同一C++分支内尚未终审的opcode48提示保持不变。随后废弃首轮结论，从入口重新逐条复核61条指令、12项重定位、word26/27读取与写序、低16位回绕、signed gain、无提示分支、非队伍角色、格式串、名称几何、颜色、present/等待/恢复、返回0和caller PC，零新增差异。

机器对非法role索引和损坏无NUL名字执行越界访问；现代仅在该资产未使用域内做记录边界保护。宿主以非阻塞notice/response运输替代函数内同步等待，归类 `platform_adapted`。

## 5. 全KDEF、文本与像素oracle

当前1,018条KDEF只有2次opcode46。按little-endian `<IIhh>` 编码 `(script,PC,role_id,delta)` 的完整参数流SHA256为 `dd5b145847acd0ee65388c77df66ba1348d07770baf0b210fc774082bd58f759`：

- 位置为 `(536,191,49,300)`、`(581,31,49,300)`；role合法且不要求在队伍中。
- 按当前RANGER基线隔离单次执行，两处均由旧当前内力13、旧最大内力0得到新值300和gain287；原文字节为 `B5 EA A6 CB 20 A4 BA A4 4F BC 57 A5 5B 20 32 38 37 00`，帧FNV-1a64均为 `0x79d846f4c3c31cb6`。
- 角色49面板为 `(70,40,180,27)`、文字位置 `(80,45)`，颜色5/7。
- 合成边界固定：最大32767、当前-32768、delta1回绕为-32768，gain0且无提示；最大-32768、当前-32768、delta-1回绕为32767并提示65535，帧 `0x4624b5c6c4ca6d16`；最大100、当前0、delta0仍提示100，帧 `0xbbff4e602ca8260c`；最大100、当前100、delta-10写90且无提示。
- 两个可见合成帧串联SHA256 `7953d2c3690e41895d91aef79f6596dda9e216017499f3208894130ecd9af3c7`。
- 真实script581全事件测试另以最大900、当前100夹具固定新值1200、实际gain1100及原Big5提示；非法role `-1`由现代边界保护稳定拒绝。

双次独立生成字节一致；正式 `scene-goldens.json` SHA256 `b4883eece2a2537e0ec07193960b347fd6b1e8f3ccace636ffd55e5376eba467`。

## 6. 验证与结论

- Linux app Debug根构建：14/14 tests通过（`proc_46d8`）。
- 原程序动态差分：`blocked_runtime_oracle`。

```text
closure_status = platform_adapted
final_review = converged_no_new_differences
remaining =
```
