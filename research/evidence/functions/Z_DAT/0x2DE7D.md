# Z_DAT:0x2DE7D sub_2DE7D

状态：`platform_adapted / converged_no_new_differences`

## 1. 物理边界与二进制身份

- 地址范围：`0x2DE7D..0x2DF0E`，共145字节、37条指令。
- IDA加载字节SHA256：`bbaddd1162733a447cb95055c3e87a400815b84532ef2d089424440ae6feaed6`。
- 原始`Z.DAT`文件偏移`0x2787D`的145字节SHA256：`17f05acd776abbc7b1add52c71897b59e31f4c167d7c0d4642d9201daa6763cb`。
- 两者共有9个差异字节，全部是9个线性地址operand由DOS raw地址重定位为`raw+0x20000`；其他136字节完全一致。
- 唯一物理caller：`sub_2C319:0x2C57B`，即事件解释器opcode9。

caller先有符号载入假偏移`word[PC+2]`，再载入真偏移`word[PC+1]`；helper返回后进入与opcode5共用的尾部，回收8字节、固定`PC += 3`并加EAX，因此最终位置为`old_pc + 3 + selected_offset`。

## 2. 汇编独立合同

入口严格执行：

1. `sub_3ED1E(0x24)`建立Watcom栈边界，并把全局last-key `byte_51B6B`清零。
2. `sub_3EF4A`把地址`0x58860`的23字节NUL结尾Big5文本复制到scratch buffer `byte_C07C4`。原始字节为`ac4fa75fad6ea844a55ba44aa15da2e7a1fea2dca15e00`，SHA256为`e04a8043ef8f9f1bde2d94ed6635eb775b881187eb9722666a70626bcc53d4a6`，解码为`是否要求加入（Ｙ／Ｎ）`。
3. `sub_2CEBF`在当前framebuffer绘制`(61,40,187,27)`圆角混色panel：source index0、border255、style4。
4. `sub_3D832`在`(71,45)`绘制上述文本，颜色word `0x0705`即阴影5、前景7。
5. `sub_3D6D1`把当前问题framebuffer呈现一次。
6. `sub_20C32`再次清last-key并阻塞到任意非零翻译键。
7. 取得键后无条件调用`sub_2D653`，先重绘裸场景再呈现一次；该调用发生在真假选择之前，Y与非Y路径都必经。
8. 最后只比较last-key是否等于大写ASCII `Y` (`0x59`)；恰为Y返回真偏移，任何其他非零键立即返回假偏移。没有Y/N过滤循环，分支后没有额外重绘或present。

本轮从机器入口导出全部callee及短callee函数体；782字节panel primitive `sub_2CEBF`使用其已收敛closure证据。`sub_3D6D1`所属UI closure继续独立待审，不能由本helper提前关闭。

## 3. 全资产调用域

独立解析原始`KDEF.IDX/KDEF.GRP`，opcode9共81次。按`<II2h>`序列化`(script_id, PC, true_offset, false_offset)`后的完整参数流SHA256为`8be8acd438f85e423576e905d78fbbb4f4c2aba1daf78ff85330b14a217c018c`；首条`(10,101,1,0)`，末条`(999,5,6,0)`。

- 真偏移分布：0×4、1×14、6×49、7×1、11×10、16×2、20×1。
- 假偏移分布：0×77、42×2、47×2。
- 共8种offset pair，全部非负；四条反向布局为scripts304/306的`(0,47)`及scripts307/308的`(0,42)`，没有两侧同时非零的资产调用。

完整统计写入`research/evidence/scene-goldens.json:kdef.dialogue_vectors.question_prompts.opcode_9_asset_domain`；当前oracle SHA256为`5b9f4b009e5cba99ad9476d066a2d1a9d172a006336e9f5a85fa558bb8f303ec`。

## 4. 单向汇编→C++ REVIEW

机器合同固定后，才对照现代实现：

- `SceneSession` opcode9保存两个有符号偏移、先固定推进PC 3 words，再发出携带精确23字节原文的`SceneQuestion::join`。
- `draw_overlay`使用相同panel、文字坐标和颜色；独立scene70问题帧FNV-1a64为`0xbea93863a81cd9e0`。
- `LegacyGameRuntime::handle_key`只把大写`Y`映射为`yes`，所有其他translated key映射为`no`，与机器任意非零键后严格比较`0x59`一致。
- 对join问题，第一次`resume`只选择offset并返回`SceneStepKind::present`；`draw_overlay`对present不叠加UI，生成裸场景。宿主完成呈现后，`conditional_after_present`才把选择的offset加到已推进3 words的PC。
- battle/rest问题不进入该额外present continuation，保持各自机器合同。
- 既有synthetic `(true=0,false=3)`的Y路径固定`question→bare present→item211`；新增非Y路径固定同一bare present后按false offset直接结束，且item211不变。

机器使用全局scratch/framebuffer并在函数内同步阻塞；现代由`SceneSession`持有文字与indexed framebuffer、由宿主帧循环恢复两次同步边界，归类`platform_adapted`。合法域的原文、frame、按键接受、present数量/顺序和PC结果一致；首轮完整helper、唯一caller、所有callee边界与81条资产复核未发现产品实现差异，结论为`converged_no_new_differences`。原程序动态执行继续记录`blocked_runtime_oracle`。

## 5. closure隔离与验证

本行只关闭`scene-event-closure.tsv`中的`sub_2DE7D`职责；同址`input-font-closure.tsv`行及`sub_3D6D1` UI closure仍按自身审计顺序保持pending，不从本helper传播关闭。

- 独立oracle二次生成字节一致：`proc_1aa0`。
- `./build.sh app --config Debug`：`proc_0622`通过，14/14 tests通过。
- order32静态门：`proc_8030`通过；同时固定145字节机器身份、9处重定位、唯一caller、完整callee调用顺序、81条资产、两路present与closure隔离。
- `python3 research/tools/validate_reverse_framework.py`：`proc_4126`通过。
