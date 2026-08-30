# 函数证据：`sub_2ED8D` `0x2ED8D..0x2F053`

状态：`implemented_pending_review`

来源：当前 `Z.DAT` 机器码；专项导出 `research/ida/reports/Z_DAT.b7_scene_xrefs.txt`

函数物理范围：`0x2ED8D..0x2F053`

直接调用者：`sub_2C319` 的 jump-table case 25（`0x2C6E4..0x2C713`）

调用约定与参数：caller 从 KDEF word stream 依次 `movsx` 并压入四个 32-bit 实参；callee 实际读取 `(source_x, source_y, target_x, target_y)`。脚本来源值均为有符号 int16，循环和比较在 32-bit 有符号域执行。

返回合同：全部正常路径和 default 路径均 `eax=1`；caller 不使用该值，随后把 KDEF PC 增加 5 words。

## 1. 范围与非范围

- 本单元负责：opcode 25 的视口平移路径、坐标截断、逐帧重绘顺序、endpoint-exclusive 循环和每帧 BIOS tick 等待。
- 明确转交：`sub_2D653` 的完整场景绘制属于 scene render 单元；`sub_3DB83` 的通用 tick-delay 合同属于 runtime/time 单元。
- 外部合同：`sub_3DB83(50)` 按原机器码计算 `trunc_toward_zero(50/40)+1 = 2` 个 BIOS tick，不解释为 50 ms。

## 2. 不看 C++ 的独立汇编恢复

```text
block/address          | reads                         | writes/calls                                      | condition/successor
2ED8D..2ED97            | frame size 0x14               | sub_3ED1E                                         | prologue
2ED9A..2EE0B            | source/target x,y             | x_step,y_step/quadrant 0..3                       | target < source => -1, else +1
2EE13..2EE9D (case 0)   | x: source<target; y:<         | D2958/D2956=clamp(coord-11,0,36); render; wait(50)| x loop first, then y
2EEA2..2EF2C (case 1)   | x: source<target; y:>         | same                                               | target excluded
2EF31..2EFBB (case 2)   | x: source>target; y:<         | same                                               | target excluded
2EFC0..2F04A (case 3)   | x: source>target; y:>         | same                                               | target excluded
2F04A..2F052             | —                             | eax=1; restore                                    | return
```

每个坐标迭代严格执行：

1. 计算 `coord - 11`；
2. 先按有符号 `< 0` 截为 0，再按 `> 36` 截为 36；
3. x 写 `word_D2958`，y 写 `word_D2956`；
4. 调 `sub_2D653` 完整重绘；
5. 调 `sub_3DB83(50)` 等待 2 个 BIOS tick；
6. 递增或递减 coord。

x 阶段始终先于 y 阶段；相等参数选择 `+1` 分支但循环零次；终点从不绘制。函数不写玩家场景坐标。

## 3. 汇编独立测试向量

- KDEF script 30 首指令：`25,41,31,34,31`。初始 view `(33,18)`；帧 origin 依次为 `(30..24,18)`，共 7 帧，然后同次事件继续到 dialogue 86。
- script 225：`25,44,54,48,53` 后经 dialogue/opcode 0，再执行 `25,48,53,44,54`；覆盖 x/y 的递增、递减、clamp 36、x-before-y 和终点不含。
- script 30 的 7 个 framebuffer FNV-1a64 由独立 Python RLE/scene oracle 生成并保存在 `research/evidence/scene-goldens.json` 的 `kdef.opcode_25_script_30`。
- wait 参数 50 固定为 2 tick；普通 opcode 0 present 保持 1 次呈现边界，测试防止两者混淆。

## 4. 现有 C++ 映射

```text
0x2C6E4..0x2C713       -> SceneSession::run_event case 25 / PC += 5
0x2ED9A..0x2EE0B       -> PanState step_x/step_y 初始化
四个 jump-table cases    -> SceneSession::advance_pan_frame 的 x-first/y-second 状态机
word_D2958/word_D2956  -> SceneSession::view_origin_x_/view_origin_y_
sub_2D653              -> SceneSession::render_map（由 app 在 present 边界同步调用）
sub_3DB83(50)          -> SceneStepResult::wait_ticks=2 + LegacyGameRuntime scene effect tick gate
```

## 5. LST/机器码 → C++ 正向追溯

- [ ] 最终 REVIEW 尚未执行；本轮只完成独立恢复、实现和定向验证。

## 6. C++ → LST/机器码反向追溯

- [ ] 最终 REVIEW 尚未执行；需在 B7 全实现后从函数入口重新检查。

## 7. 差异记录与重启 REVIEW

- 旧 C++ case 25 仅 `program_counter_ += 5`，完全跳过视口平移、逐帧 render 和 delay。
- 首次实现后复查 `sub_3DB83`，发现参数 50 应为 2 tick，而非单次主循环 tick；增加 typed `wait_ticks` 并修正 app effect gate。
- 测试最初把 script 225 中间 opcode 0 present 误认为第二次 pan 首帧；按真实 word stream 单独消费 opcode 0 后，正反向序列通过。
- 因发生差异，最终 REVIEW 必须从 `0x2ED8D` 入口重做，不能继承本轮局部结论。

## 8. 验证

- 定向 UT：`openlegend.scene` 通过；固定 origin、frame hash、方向、clamp、endpoint-exclusive 和 wait ticks。
- 集成：`openlegend.ui` 通过；app 编译并保持既有 scene effect 链。
- 独立 oracle：`research/tools/generate_b7_scene_goldens.py`，真实 KDEF/ALLSIN/ALLDEF/SDX070/SMP070。
- Linux/Windows/sanitizer：本小切片按计划只执行 Linux Debug 定向测试；B7 收口时执行完整矩阵。
- 原程序动态差分：`blocked_runtime_oracle`（本机无 DOSBox/dosemu）。

## 9. 最终结论

```text
final_review = not_started
remaining = B7 全实现后执行完整正向/反向 REVIEW，复核 sub_2D653 与 sub_3DB83 转交边界
```
