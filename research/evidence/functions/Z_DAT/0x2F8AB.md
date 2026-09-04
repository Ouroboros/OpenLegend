# 函数证据：`sub_2F8AB` `0x2F8AB..0x2F8D1`

状态：`platform_adapted`；最终 REVIEW：`converged_no_new_differences`

来源：当前 `Z.DAT` 机器码与原始文件字节；headless IDA完整导出；当前 `KDEF.IDX/GRP`；相邻四项玩家基础图片表。

## 1. 机器身份与caller

- 函数物理范围 `0x2F8AB..0x2F8D1`，38 bytes，9条指令。
- loaded SHA256 `c60ab276fead7fcf344e441b1f7cf96766bd66b6a40d733c3a5503cc9c3017b4`；原始文件 `Z.DAT[0x292AB:0x292D1]` SHA256 `a5bfebf0ada8be8dc64039415acea5a678df52dea53be5a8039e8b8e1b49865b`。
- 三个差异字节分别来自朝向变量、四项图片表和当前玩家图片变量raw加加载基址 `0x20000` 的三项重定位；规范化后与loaded逐字节相同，未解释差异为零。
- 唯一物理代码caller是 `sub_2C319:0x2C93C..0x2C94F` 的opcode40；`0x5565C`只是函数地址表数据引用。
- caller把唯一KDEF参数sign-extend后传入，忽略返回0，经共享尾块清理4字节栈并把PC增加2 words。
- 入口 `sub_3ED1E(4)` 仅为Watcom栈探测合同。

## 2. 不看C++的朝向合同

1. 把参数低16位无条件写到当前场景朝向变量。
2. 随即从该word sign-extend为32位索引。
3. 读取相邻四项word表 `[5002,5016,5030,5044]` 中的对应项，覆盖当前玩家图片word。
4. 因而合法方向0/1/2/3分别立刻得到基础图片5002/5016/5030/5044，既有步行动画帧或临时图片被基础图片覆盖。
5. 返回0；不present、不等待输入、不播放音频。

## 3. 汇编→C++ REVIEW

C++ case40清除玩家图片override，把合法参数写入同序 `SceneDirection`，把步行帧offset归零，使 `player_frame()`立即返回同一四项基础图片；随后同步现代snapshot中的场景朝向镜像并把PC增加2。合法0..3域的运行状态、图片和后续存档结果与机器一致，本轮没有产品代码差异。

机器对非法signed索引直接在四项表前后读取；现代把小于0钳到0、大于3钳到3，归类 `platform_adapted`。机器运行全局与现代snapshot镜像的所有权差异也是既有平台适配。随后从入口重新逐条复核9条指令、三项重定位、参数低16位写入、signed表索引、当前图片覆盖、return0和caller，零新增差异。

## 4. 全KDEF与方向oracle

当前1,018条KDEF共有12次opcode40。按little-endian `<IIh>` 编码 `(script,PC,direction)` 的完整参数流SHA256为 `ebe741b0fba8a9a93cc06ed0b0ede392ba5f2f054df0f9cc94c8dbe17bb2165b`；方向0/1/2/3出现次数分别为2/2/1/7，全部真实参数合法，全部位置写入正式oracle。

- 真实script235 PC33方向3固定朝向3、步行offset0和玩家图片5044。
- 合成四向分别固定5002/5016/5030/5044，并同步snapshot朝向镜像。
- 合成非法-1固定现代朝向0/图片5002，非法4固定朝向3/图片5044。
- 双次独立生成字节一致；正式 `scene-goldens.json` SHA256 `f27c4720724a5c56a18a9a3e1eb8dab03a5bd66908ea720c4ec0c7e0d865b3b1`。

## 5. 验证与结论

- Linux app Debug根构建：14/14 tests通过（`proc_c644`）。
- 原程序动态差分：`blocked_runtime_oracle`。

```text
closure_status = platform_adapted
final_review = converged_no_new_differences
remaining =
```
