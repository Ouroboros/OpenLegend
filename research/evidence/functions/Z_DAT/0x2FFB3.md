# 函数证据：`sub_2FFB3` `0x2FFB3..0x30035`

状态：`platform_adapted`；最终 REVIEW：`converged_no_new_differences`

来源：当前 `Z.DAT` 机器码与原始文件字节；headless IDA完整导出；当前 `KDEF.IDX/GRP`；当前原版字体、调色板与场景资产。

## 1. 机器身份与caller

- 函数物理范围 `0x2FFB3..0x30035`，130 bytes，34条指令。
- loaded SHA256 `15329fa96b74fda544a56fa47872ddd51dfca042554f5cf158982a4bf17a7951`；原始 `Z.DAT[0x299B3:0x29A35]` SHA256 `bb33a808a40cb484726ae69107c160b624e41fd6f6c0e0413a53d7405058d560`。
- 8个差异字节全部由8项绝对地址操作数的 `raw+0x20000` 搬移解释，归一化后逐字节等于loaded；目标依次为主角品德word、格式串、文字缓冲区、帧缓冲区三次、present状态和文字缓冲区第二次引用。
- 唯一代码caller是事件解释器opcode52；没有脚本参数，忽略callee返回0，经公共块把PC增加1 word。
- 入口 `sub_3ED1E(36)` 仅为Watcom栈探测合同。

## 2. 不看C++的品德面板恢复

1. `movsx`读取角色0记录word56，故输入是完整signed int16品德值。
2. 以原Big5格式串 `你現在的品德指數為%5d` 格式化到全局缓冲区；`%5d`是最小字段宽度，不截断负号或五位数。
3. 调用面板helper绘制 `(x=54,y=40,width=212,height=27)`，填充色255、暗化源色0、style4，目标为当前全局帧缓冲区。
4. 在 `(64,45)` 以16点字、颜色5/7绘制已格式化的NUL结尾文字。
5. 依机器顺序呈现当前帧、等待任意键、重绘并呈现场景，然后返回0；函数本身不写品德或其他游戏状态。

## 3. 汇编→C++ REVIEW

首轮及从入口复核确认：C++ opcode52读取角色0 `morality` signed word，生成完全相同的Big5前缀和 `%5d` ASCII值；style52在相同固定矩形绘制颜色5/7文字。事件PC增加1，确认后显式输出一次scene present，再回到stay，等价承载机器同步的present→任意键→场景恢复边界。

机器直接在调用方帧缓冲区上同步绘制和等待；跨平台实现将其运输为协作式notice/ack/present步骤，因此本行归类 `platform_adapted`。格式化、面板、字体、present、输入和场景绘制内部均保留各自closure职责，不向本行传播。

从入口重新逐条复核34条指令、8项搬移、signed读取、格式串、全部面板与文字参数、调用顺序、返回值和caller PC，零新增差异，产品代码无需修正。

## 4. KDEF与oracle

当前1,018条KDEF只有script825 PC0一次opcode52。按little-endian `<II>` 编码 `(script,PC)` 的完整调用流SHA256为 `b7191338c8ed84796ecd378ccb54c23288c34a90c771b53fc7c89eccfdad3f17`。

真实script825以品德7得到原字节 `a741b27ba662aabaab7ebc77abfcbcc6acb0202020203700`，最终frame FNV1a64 `0x1cc47112086c10e7`，确认后输出present、stay。新增signed word边界覆盖：

- `-32768`：文字尾部 `-32768`，frame `0xd6e8a2c917f2ada7`；
- `0`：四个前导空格后`0`，frame `0x1e6d8da21b3bb12f`；
- `32767`：五位数字无前导空格，frame `0x4bf7246d16058dad`。

独立oracle双次生成逐字节一致；正式 `scene-goldens.json` SHA256 `c9aff6f6aa02c7582eeea9a1f40bb50146e230728ab8404f3e48ac2d7c4158e9`。

## 5. 验证与结论

- Linux app Debug根构建：14/14 tests通过（`proc_1678`）。
- 原程序动态差分：`blocked_runtime_oracle`。

```text
closure_status = platform_adapted
final_review = converged_no_new_differences
remaining =
```
