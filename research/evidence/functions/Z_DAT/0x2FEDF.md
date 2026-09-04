# 函数证据：`sub_2FEDF` `0x2FEDF..0x2FF87`

状态：`assembly_exact`；最终 REVIEW：`converged_no_new_differences`

来源：当前 `Z.DAT` 机器码与原始文件字节；headless IDA完整导出；当前 `KDEF.IDX/GRP`、`RANGER.GRP`；`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`。

## 1. 机器身份与caller

- 函数物理范围 `0x2FEDF..0x2FF87`，168 bytes，57条指令。
- loaded SHA256 `f6c7a9a16175300e0e8196f1441d562ae7634600f209f435d5a5fea7ee8dd35c`；原始文件 `Z.DAT[0x298DF:0x29987]` SHA256 `7f93592d687779b8025d6aab685f141fe7a33ba565f227aa22e39d2b72971fa6`。
- 5个差异字节全部是同一库存物品ID基址在五处读取中的raw加加载基址 `0x20000` 重定位；规范化后与loaded逐字节相同，未解释差异为零。
- 唯一物理代码caller是 `sub_2C319:0x2CAA7` 的opcode50；`0x55684`只是函数地址表数据引用。
- caller逆序压入false offset、true offset和五个item ID，七项均从KDEF sign-extend，调用后清28字节栈，先把PC增加8 words，再叠加callee返回的signed offset。
- 入口 `sub_3ED1E(20)` 仅为Watcom栈探测合同。

## 2. 不看C++的五项存在条件恢复

1. 初始化五个独立found位为0和库存槽索引为0。
2. 对固定200个库存槽逐槽扫描；每槽按4字节步长定位，并依次五次读取该槽的第一个word物品ID，与五个signed item入参分别精确比较。
3. 任一比较命中只把对应found位置1；不清回0，不因命中、空槽、重复参数或任一缺失而停止。库存count即第二个word从未读取。
4. 因而count为0或负数仍命中；item `-1`也可命中；五个查询参数重复时，同一库存槽可同时置五个found位。
5. 扫描完全部200槽后，仅当五个found位全为1时返回第六参数true offset，否则返回第七参数false offset。函数不修改状态、不显示UI、不present且不等待输入。

## 3. 首轮差异与从入口重审

首轮汇编→C++ REVIEW确认只按物品ID判断、忽略count、五项全命中条件和caller宽度/offset合同，但发现C++以 `all = all && inventory_contains_id(...)` 逐参数处理：首个缺失会跳过剩余参数扫描；全部存在时则按参数依次执行五轮200槽扫描，而不是机器的逐槽五次比较。

最小修正改为五个独立found位、外层固定200槽、内层固定五参数，并保留每槽五次独立ID读取。随后废弃首轮结论，从入口重新逐条复核57条指令、5项重定位、槽步长、五次读取/比较顺序、found保持、count未读、完整200槽、重复/-1参数、offset选择及caller PC，零新增差异。固定长度库存模型无需边界适配，归类 `assembly_exact`。

## 4. 全KDEF与状态oracle

当前1,018条KDEF只有script676 PC0一次opcode50，参数为物品138、139、140、141、142，true offset6、false offset0。按little-endian `<IIhhhhhhh>` 编码完整参数流SHA256为 `8e3d5db592bf2544c322c1a43af196237d6b5de14481fcd12d7af95204e1744f`。

- 当前RANGER基线五种物品均缺失，五个found位全0，选择false offset0。
- 真实script676测试把五种物品均以count0放入库存，仍选择true路径并进入对话2482；移除物品142后选择false路径并进入对话2481。
- 合成测试覆盖五个不同ID且count0、最后一项缺失、五个重复参数由单个count-32768槽同时满足，以及五个 `-1` 参数由单个槽满足。

双次独立生成字节一致；正式 `scene-goldens.json` SHA256 `0d0bce29ae7fc1cfc6d08c660e2df310835adc5b0b6b9596b874d843dfef011a`。

## 5. 验证与结论

- Linux app Debug根构建：14/14 tests通过（`proc_212d`）。
- 原程序动态差分：`blocked_runtime_oracle`。

```text
closure_status = assembly_exact
final_review = converged_no_new_differences
remaining =
```
