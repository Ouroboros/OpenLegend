# 函数证据：`sub_2ED8D` `0x2ED8D..0x2F053`

状态：`platform_adapted`；最终 REVIEW：`converged_no_new_differences`

来源：当前 `Z.DAT` 机器码与原始文件字节；headless IDA完整导出；当前 `KDEF.IDX/GRP`、场景70真实资产；`research/ida/reports/Z_DAT.b7_scene_xrefs.txt`。

## 1. 机器身份、jump table与caller

- 函数物理范围 `0x2ED8D..0x2F053`，710 bytes，196条指令。
- loaded SHA256 `375ae5b8f3c979beb22a2c80d43f331d8cfb6951c928cde82b7b2357362882ea`；原始文件 `Z.DAT[0x2878D:0x28A53]` SHA256 `b7bf97f0d91c23af1d63785de5836df67fc5b4d1d827478c3dd3cfdd97f7b8ff`。
- 函数内33个差异字节组成33个32位绝对地址重定位；每项均满足loaded=`raw+0x20000`，未解释差异为零。
- 四项jump table位于函数前的 `0x2ED7D..0x2ED8D`，不在710字节函数范围内；loaded bytes为 `13ee0200a2ee020031ef0200c0ef0200`，SHA256 `cd12f6f1c43b910a63b53d6d7868b4d0e1b0c371b6f88d630c41f6b1f23e1514`。四个目标依次为 `0x2EE13/0x2EEA2/0x2EF31/0x2EFC0`。
- jump table原始SHA256 `29f9c4e47517b8c261ed3aaae2167f535c5b5d9558cc6e01817aa3c1eef3f213`，四个raw值 `0xEE13/0xEEA2/0xEF31/0xEFC0` 均各自加 `0x20000` 成为loaded目标。
- 唯一物理代码caller是 `sub_2C319:0x2C6E4..0x2C713` 的opcode25 dispatch；`0x55620` 只是函数地址表数据引用。
- caller按逆序压入 `target_y,target_x,source_y,source_x` 四个sign-extended KDEF words，调用后清理16字节栈并把事件PC增加5 words；callee固定返回1，caller不读取返回值。

## 2. 不看C++的完整汇编恢复

callee先在32位有符号域计算：

```text
step_x = target_x < source_x ? -1 : +1
step_y = target_y < source_y ? -1 : +1
(+,+) -> jump-table case0
(+,-) -> case1
(-,+) -> case2
(-,-) -> case3
```

四种case均先完整执行x循环，再完整执行y循环；每轴从source开始，以 `+1` 或 `-1` 前进，在等于target时停止，因此target坐标本身永远不生成帧。source==target时该轴循环零次，虽然step按机器比较选为 `+1`。两轴都相等时函数不绘制、不等待，直接返回1。

每个迭代严格执行：

1. 32位计算 `coordinate-11`；
2. 只把低16位写入 `word_D2958`（x）或 `word_D2956`（y）；
3. 把该word按signed int16比较：小于0写0，大于36写36；
4. 只更新当前轴，另一轴保持上帧值；不写玩家场景坐标；
5. 调 `sub_2D653` 完整场景绘制；
6. 调 `sub_3DB83(50)`；机器整数合同为 `trunc_toward_zero(50/40)+1=2` BIOS ticks；
7. 最后递增或递减循环coordinate。

这意味着视口公式不是直接的32位 `clamp(coordinate-11,0,36)`，而是：

```text
clamp(signed_int16(low16(coordinate-11)), 0, 36)
```

例如 `coordinate=-32768` 时，`-32779` 截为word `32757`，最终视口原点为36；`coordinate=-32757` 时结果word为 `-32768`，最终为0。

## 3. 差异、最小修正与重新REVIEW

首轮单向汇编→C++ REVIEW发现 `SceneSession::advance_pan_frame` 在C++ `int` 域直接钳位 `coordinate-11`。普通资产坐标结果一致，但合法int16输入 `-32768` 在机器中先回绕为32757后钳到36，旧C++却钳到0。

最小修正复用既有 `wrapping_add(int16,-11)`，先恢复机器word结果，再转换为 `int` 做 `0..36` 钳位；x/y两轴使用同一合同。修正后废弃首轮结论，从 `0x2ED8D` 入口重新复核196条指令、函数外jump table、四象限、全部循环边界、word位宽、两项callee顺序、返回值及caller PC，未发现新增差异。

## 4. 全KDEF与独立oracle

当前1,018条KDEF共有52次opcode25。按little-endian `<IIhhhh>` 编码 `(script,PC,source_x,source_y,target_x,target_y)` 的完整参数流SHA256为 `70c2620ff731c246ddfe4481c4261a560b2aa32c6331e6c161723a83b000c116`：

- 首项 `(30,0,41,31,34,31)`；末项 `(936,34,21,26,33,26)`；
- 四参数最小值 `[17,14,17,14]`，最大值 `[48,54,48,54]`；当前资产域不触发16位极小负值回绕，但完整机器输入域仍由实现和合成测试保留；
- 52次调用合计460个present帧；最多16帧的是 `(script534,PC115,17,28,24,19)`；
- `research/tools/generate_b7_scene_goldens.py` 独立按 `clamp(int16(coordinate-11),0,36)` 生成结果，并固定 `-32768→32757→36`、`-32757→-32768→0`、`47→36`、`48→36` 四个边界向量；
- script30从 `(41,31)` 到 `(34,31)`，固定7帧x原点 `30..24`，framebuffer FNV-1a64依次为 `0x9838f6a2b37ad75d`、`0xa58b51e27d8f5fe3`、`0x6a876603fd1dce87`、`0x3d2c25f9165bd6b4`、`0x8248a9b81ee91c88`、`0x201c90b91aa11963`、`0x2a895d743d76c127`；终点23不绘制，随后到dialogue86；
- script225覆盖x/y递增与递减、x-before-y、0/36钳位、终点不含及两段pan之间独立opcode0 present；
- 合成opcode25 `(-32768,-32768)->(-32767,-32767)` 固定两帧：先x原点36且y保持18，再y原点36；第三次resume直接结束，玩家 `(44,29)` 不变；
- oracle双次生成逐字节一致；正式 `scene-goldens.json` SHA256 `f868c36d730fa52c7f49e2d7eacf7dcb2b437e4827669a5a90bb87ca4ed12031`。

## 5. 汇编→C++与C++→汇编映射

```text
opcode25四个sign-extended参数       -> run_event case25 / PanState
四项jump table与step符号           -> PanState step_x/step_y
四个重复x/y循环                    -> advance_pan_frame x-first / else-if y-second
word写入后的signed 0..36钳位       -> wrapping_add(int16,-11) + std::clamp
word_D2958 / word_D2956             -> view_origin_x_ / view_origin_y_
sub_2D653逐帧绘制                  -> present step后SceneSession::render_map
sub_3DB83(50)                       -> SceneStepResult::wait_ticks=2
callee同步返回后caller PC+5         -> pan结束后继续同一run_event PC
```

现代实现把原函数内同步的“绘制→BIOS等待→下一帧”拆为scene `present` 结果与app tick gate；每次resume前不能执行下一坐标或后续事件指令，可观察顺序与机器一致。PC在现代state建立时先增加5，但pan未结束前该PC不被执行且不对外暴露；pan结束后续指令时点与caller返回后增加5等价。

## 6. 平台适配与委托边界

归类 `platform_adapted`，不是 `assembly_exact`：现代帧呈现、宿主时钟等待及渲染失败处理替代DOS同步callee调用，但合法域坐标、像素、帧数、等待ticks与后续PC完全一致。

本closure只关闭 `sub_2ED8D` 的场景平移动画职责，不传播callee：`sub_2D653` 已由scene owner独立收口；`sub_3DB83` 的runtime/time职责已收口，但同址 `input-font-closure.tsv audit_order=39` 仍独立待审；`sub_3ED1E` 是编译器栈探测合同。函数外jump table作为本函数dispatch证据纳入，但不改动前一函数closure。

## 7. 验证与结论

- Linux app Debug根构建：14/14 tests通过（`proc_9837`）。
- 静态证据覆盖710字节函数、33项重定位、16字节jump table四项重定位、唯一caller、52次真实资产参数流、460帧、极值word回绕及scene/runtime映射。
- 原程序动态差分：`blocked_runtime_oracle`。

```text
closure_status = platform_adapted
final_review = converged_no_new_differences
remaining =
```
