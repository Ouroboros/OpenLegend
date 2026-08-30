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

## 6. C++ → LST/机器码反向追溯

- [ ] 每项可观察 C++ 行为均能回指具体指令或批准的平台例外；
- [ ] 没有为了安全/整洁新增合法域行为；
- [ ] owner/RAII 适配不改变原时序和结果。

## 7. 差异记录与重启 REVIEW

每次发现差异记录：原机器码、错误 C++、修正、测试、证据和 inventory 更新。修正后必须从函数入口重新执行第 2–6 节，不只复查差异附近。

## 8. 验证

- 定向 UT：
- 真实资产/固定状态：
- 集成：
- Linux/Windows/sanitizer：
- 原程序动态差分：`verified | blocked_runtime_oracle`

## 9. 最终结论

只有最后一轮完整正向和反向追溯零新增差异时填写：

```text
final_review = converged_no_new_differences
remaining =
```
