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

原scene审计只关闭`scene-event-closure.tsv`中的`sub_2DE7D`职责，没有向同址input owner或`sub_3D6D1` UI owner传播状态。input owner随后按自身顺序在第6节独立关闭；`sub_3D6D1` UI closure仍保持自己的审计状态。

- 独立oracle二次生成字节一致：`proc_1aa0`。
- `./build.sh app --config Debug`：`proc_0622`通过，14/14 tests通过。
- order32静态门：`proc_8030`通过；同时固定145字节机器身份、9处重定位、唯一caller、完整callee调用顺序、81条资产、两路present与closure隔离。
- `python3 research/tools/validate_reverse_framework.py`：`proc_4126`通过。

## 6. input owner独立最终REVIEW（audit_order=23）

本轮为`input-font-closure.tsv`重新建立独立临时IDB并导出完整145字节、37条指令、3个基本块、7次call、9项HIGHLOW重定位、2个本地`RET`、唯一物理caller及跳转表数据引用；没有借用scene owner的旧导出作机器门。补充导出的caller共享尾逐指令证明：opcode9先有符号载入假偏移与真偏移，调用`sub_2DE7D`后执行`add esp,8; add ebx,3; add ebx,eax`，故最终PC为`old_pc+3+selected_offset`。

input合同从入口独立恢复为：入口清last-key；问题frame present；`sub_20C32`再次清键并等待第一个非零翻译键；任何键到达后都先调用`sub_2D653`重绘并present裸场景；最后才严格比较大写`Y`，Y返回真偏移、其他非零键返回假偏移。等待helper 19字节SHA256为`d1f87751e3589507ed555b84cc4a8e5523937fd67282944b568b34e6eeaa3a55`；裸场景present helper 37字节SHA256为`51d1df23db48b1b1489112a5356a8138c387a8667fe8050ab726de579b87fc36`。

机器合同冻结后才对照现代输入链。Order22已经发布的`scene_question_presented_`门同时覆盖opcode9，使问句生成和render后但实际present前的同批keydown均不能回答。join专用`conditional_after_present`先缓存selected offset并返回裸场景`present`；宿主成功present后才以`acknowledge`恢复并应用offset。机器在裸场景present后才读取last-key作比较，现代在keydown时先形成不可观察的response；裸场景render/present链不读取response或last-key，且脚本PC、状态副作用和后续输入在present完成前都保持阻塞，因此合法域可观察顺序一致。

宿主回归把opcode5和opcode9串联在同一初始脚本中：前一问句非Y响应后立即产生join问句并重新关闭present门；join问句render后但present前拒绝Y；present后keypad Insert作为任意非Y键进入裸场景present；该present完成前的Y仍被拒绝，完成后才推进到脚本结束。纯`SceneSession`回归继续分别固定Y/非Y两路均先得到裸场景present及最终副作用/偏移。

完整37条指令、3块、7 calls、9项重定位、2出口、唯一caller、等待/裸场景helper、81条资产调用及scene/world-event两条宿主路由对照后没有发现新的产品差异；本input owner归类`platform_adapted / converged_no_new_differences`。两份独立Golden与正式第三次生成逐字节一致，正式`scene-goldens.json` SHA256为`e03f90d696b38adc17e9917f70a361e78acaf6bbbb7334c272287f726fa1ce61`，且相对上一版只新增`kdef.dialogue_vectors.question_prompts.join_input_machine`。最终Linux app Debug构建`proc_7be4`通过14/14；首次构建`proc_fb60`只暴露新增测试误用了不同场景坐标的旧像素哈希，删除该越域测试假设后产品代码未修改。原程序动态执行继续登记`blocked_runtime_oracle`。
