# 函数证据：`sub_2FC9D` `0x2FC9D..0x2FDA6`

状态：`platform_adapted`；最终 REVIEW：`converged_no_new_differences`

来源：当前 `Z.DAT` 机器码与原始文件字节；headless IDA完整导出；当前 `KDEF.IDX/GRP`、`RANGER.GRP`；`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`。

## 1. 机器身份与caller

- 函数物理范围 `0x2FC9D..0x2FDA6`，265 bytes，67条指令。
- loaded SHA256 `64b9848a67e54ab0b245ab493a4936fb296010483695db3007d9f555c245b6f1`；原始文件 `Z.DAT[0x2969D:0x297A6]` SHA256 `cd3061643121bec910e991ff357d2c857ff29587c846fd5f061f1d3b373f19f8`。
- 15个差异字节全部是角色武力、角色名、格式串、文本缓冲区、framebuffer及呈现状态地址raw加加载基址 `0x20000` 的重定位；规范化后与loaded逐字节相同，未解释差异为零。
- 唯一物理代码caller是 `sub_2C319:0x2CA31` 的opcode47；`0x55678`只是函数地址表数据引用。
- caller从KDEF依次sign-extend delta和role，按cdecl逆序先压delta再压role并调用。共享退出块清理8字节栈，把PC固定增加3 words；返回值不使用。
- 入口 `sub_3ED1E(40)` 仅为Watcom栈探测合同。

## 2. 不看C++的武力写入恢复

1. 以 `role_id*182` 定位角色记录，sign-extend并保存word43武力原值。
2. 对该word执行低16位add；结果先按signed `<0`钳到0，再按signed `>100`钳到100。
3. sign-extend最终值并减去原signed值，得到32位实际gain。
4. gain小于等于0时直接返回0，不显示提示，也不重绘场景。
5. gain严格大于0时显示实际gain；因此提示数字不必等于delta。原值-32768、delta-1回绕后钳100并提示32868；原值-1、delta0钳0并提示1。
6. 可见与不可见两路均固定返回0。

## 3. 原Big5武力提示与时序

可见路径使用含末尾NUL的15字节原格式串 `25 73 20 AA 5A A4 4F BC 57 A5 5B 20 25 64 00`，即 `%s 武力增加 %d`。机器从角色记录byte8开始按C串读取名字并格式化实际gain。

机器单独以角色名计算长度。令角色名字节数为N、`A=N+10`，绘制：

- 圆角混色面板 `(150-(4A+24),40,8A+68,27)`；
- 文字位置 `(160-(4A+24),45)`，颜色5/7；
- 面板宽度不随末尾十进制gain位数变化。

面板直接叠加在caller已有framebuffer上，随后present、等待任意非零翻译键，再重绘并present裸场景后返回。委托panel、Big5文字、present、按键和裸场景绘制closure不由本行传播关闭。

## 4. 首轮差异与从入口重审

首轮汇编→C++ REVIEW确认word43、低16位回绕、signed钳位、actual-gain条件、返回0和PC一致，但发现提示UI仍不一致：

- 原C++给opcode47构造ASCII `role <id> +<gain>`，没有使用角色原名和原Big5 `武力增加`；
- 默认notice没有采用机器按角色名字节长度生成的圆角面板、颜色5/7和冻结caller framebuffer。

最小修正给opcode47新增原Big5中缀，并让同型opcode34/45/47共用已验证的角色属性提示构造与动态面板。随后废弃首轮结论，从入口重新逐条复核67条指令、15项重定位、word43 add/clamp、signed gain、无提示分支、格式串、角色C串、名称几何、颜色、present/等待/恢复、返回0和caller PC，零新增差异。

机器对非法role索引和损坏无NUL名字执行越界访问；现代仅在该资产未使用域内做记录边界保护。宿主以非阻塞notice/response运输替代函数内同步等待，归类 `platform_adapted`。

## 5. 全KDEF、文本与像素oracle

当前1,018条KDEF共有5次opcode47。按little-endian `<IIhh>` 编码 `(script,PC,role_id,delta)` 的完整参数流SHA256为 `bf1a04f156f7db9fa1c8ff13929f1b4b2e8d3623c7329ad38e50290b5466a8ab`：

- 位置为 `(284,182,35,5)`、`(363,462,38,10)`、`(484,182,53,10)`、`(536,197,49,30)`、`(581,37,49,30)`；全部role合法。
- 按当前RANGER基线隔离单次执行，五次均产生提示：角色35由53到58，帧 `0xeddab5104e45a1bc`；角色38由42到52，帧 `0xc9d2ed810ffb30fd`；角色53由24到34，帧 `0x07bb43ede04a56cc`；角色49两次均由14到44，帧 `0x0e0aee18584ddabe`。
- 六字节角色名使用面板 `(62,40,196,27)`和文字位置`(72,45)`；四字节角色名使用面板 `(70,40,180,27)`和文字位置`(80,45)`；颜色均为5/7。
- 合成边界固定：32767+1回绕负后钳0且无提示；-32768-1回绕正后钳100并提示32868，帧 `0x02f03b6e246a4973`；-1+0钳0并提示1，帧 `0x9e3c9a177d228048`。两帧串联SHA256 `0cb6b8fc6a6dccb0aed841ac8942bd31ce3951d8b1da4d57bbe103929070dc7f`。
- 真实script581全事件测试另以武力90夹具固定钳到100、实际gain10及原Big5提示；非法role `-1`由现代边界保护稳定拒绝。

双次独立生成字节一致；正式 `scene-goldens.json` SHA256 `49b5d863ec7123cbdf94be0106d3e81f7b63b446df19fca8aded0fcf661dcc62`。

## 6. 验证与结论

- Linux app Debug根构建：14/14 tests通过（`proc_36e5`）。
- 原程序动态差分：`blocked_runtime_oracle`。

```text
closure_status = platform_adapted
final_review = converged_no_new_differences
remaining =
```
