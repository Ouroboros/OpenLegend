# 函数证据：`sub_2FEBF` `0x2FEBF..0x2FEDF`

状态：`platform_adapted`；最终 REVIEW：`converged_no_new_differences`

来源：当前 `Z.DAT` 机器码与原始文件字节；headless IDA完整导出；当前 `KDEF.IDX/GRP`、`RANGER.GRP`；`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`。

## 1. 机器身份与caller

- 函数物理范围 `0x2FEBF..0x2FEDF`，32 bytes，7条指令。
- loaded SHA256 `7b772c2065e5650a348822f899d17079cfdda05bb71bab0cff47f8c8c3d65391`；原始文件 `Z.DAT[0x298BF:0x298DF]` SHA256 `6487d07f923d062b09cb7aff07d658751f25c1e0eaa1fdd334a2eac01c373210`。
- 唯一差异字节是角色word40地址raw加加载基址 `0x20000` 的重定位；规范化后与loaded逐字节相同，未解释差异为零。
- 唯一物理代码caller是 `sub_2C319:0x2CA65` 的opcode49；`0x55680`只是函数地址表数据引用。
- caller从KDEF依次sign-extend value和role，按cdecl逆序先压value再压role并调用。共享退出块清理8字节栈，把PC固定增加3 words；返回值不使用。
- 入口 `sub_3ED1E(4)` 仅为Watcom栈探测合同。

## 2. 不看C++的写入恢复

1. 直接以32位role入参乘182定位角色记录，不读取任何角色旧值。
2. 从32位value入参取低16位，直接覆盖角色word40内力属性；没有范围判断、钳位、算术或条件分支。
3. 不读取队伍、不显示UI、不present、不等待输入，也不调用栈探测之外的任何helper。
4. 固定返回0。

## 3. 汇编→C++ REVIEW

首轮及从入口复核均确认C++ opcode49直接把signed int16脚本参数写入角色 `mp_type`，PC增加3，且无额外可见行为；没有产品代码差异。机器对非法role执行越界写，现代实现仅在该资产未使用域内增加记录边界保护，因此归类 `platform_adapted`。

从入口重新逐条复核7条指令、1项重定位、参数扩展、角色步长、word40低16位覆盖、无旧值读取、无UI、返回0和caller栈/PC，零新增差异。

## 4. 全KDEF与状态oracle

当前1,018条KDEF共有3次opcode49。按little-endian `<IIhh>` 编码 `(script,PC,role_id,value)` 的完整参数流SHA256为 `7e110f5178ae6bdd0edf6d87c37ffdbc74d3a428d99240a0e43db831b794cc84`：

- 位置为 `(484,188,53,2)`、`(536,207,49,2)`、`(581,47,49,2)`；全部role合法。
- 当前RANGER基线中角色53旧值0、角色49旧值1；三次均直接覆盖为2。
- 真实script581全事件测试固定角色49由0写为2；合成测试覆盖32767、-32768和0的原样写入，以及非法role `-1`的现代稳定拒绝。

双次独立生成字节一致；正式 `scene-goldens.json` SHA256 `e5b048b02768d3f54454d81a6fdc0cf0dbe97724323623f4829f59af8b79f7de`。

## 5. 验证与结论

- Linux app Debug根构建：14/14 tests通过（`proc_0468`）。
- 原程序动态差分：`blocked_runtime_oracle`。

```text
closure_status = platform_adapted
final_review = converged_no_new_differences
remaining =
```
