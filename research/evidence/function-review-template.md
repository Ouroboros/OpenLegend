# 函数/紧耦合组证据：`<name>` `<address-range>`

状态：`pending_mapping | pending_implementation | implemented_pending_review | assembly_exact | platform_adapted`

来源：当前 `Z.COM` / `Z.DAT` 机器码

函数物理范围：`<start>..<end>`

直接调用者：`<addresses>`

调用约定与参数：`<registers/stack/width/signedness>`

返回合同：`<all exits and caller use>`

## 1. 范围与非范围

- 本单元负责：
- 明确转交：
- 外部 CRT/DOS/Miles 合同：

## 2. 不看 C++ 的独立汇编恢复

逐基本块记录：

```text
block address | entry state | reads | writes | condition | calls | successor/return
```

必须列出全部物理出口、提前返回、循环回边和异常/损坏输入域。

## 3. 汇编独立测试向量

- 条件两侧与相等值；
- 零、正、负、最小/最大、哨兵；
- 16/32 位截断、符号/零扩展和回绕；
- RNG 调用次数；
- 同帧继续、跨帧让出、阻塞与呈现时点；
- 原 BUG；
- 真实资产或固定状态样本。

## 4. 现有 C++ 映射

只在第 2、3 节完成后填写：

```text
assembly block/address -> C++ symbol/branch
```

## 5. LST/机器码 → C++ 正向追溯

- [ ] 每个基本块均有实现、不可达证明或平台例外；
- [ ] 每个调用、重复调用和状态写入顺序一致；
- [ ] 每个出口一致。

## 6. C++、测试与证据对照

本节只能在第2、3节的机器恢复完成后填写；C++、测试、golden和旧文档不得作为机器语义的推导输入。

- [ ] 逐基本块将独立机器结论与C++分支对照；
- [ ] 测试覆盖条件边界、状态顺序、位宽和present时点；
- [ ] owner/RAII及安全保护仅登记为平台例外，不改变合法域时序和结果；
- [ ] 测试与文档只作佐证，不替代REVIEW。

## 7. 差异记录与重启 REVIEW

每次发现差异记录：原机器码、错误 C++、修正、测试、证据和 inventory 更新。修正后立即废弃该轮结论，并从函数入口重新执行第2–6节，不只复查差异附近。

## 8. 验证

- 定向 UT：
- 真实资产/固定状态：
- 集成：
- Linux/Windows/sanitizer：
- 原程序动态差分：`verified | blocked_runtime_oracle`

## 9. 最终结论

只有最后一轮从函数入口完成独立机器恢复并逐块单向对照C++、且零新增差异时填写：

```text
final_review = converged_no_new_differences
remaining =
```
