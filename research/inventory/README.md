# 逆向 inventory 合同

本目录是 OpenLegend 函数范围、模块归属和 REVIEW 状态的机械真值。GOAL、证据文档、测试和聊天摘要不能覆盖这里尚未关闭的 `pending_*` / `implemented_pending_review`。

## 1. 分层

1. `function-catalog.tsv`
   - 由 `Z_COM.report.json`、`Z_DAT.report.json` 与带 `FUNCTION` 物理范围的专项 headless 报告取并集生成。
   - 只证明函数入口、末端、大小和导航调用关系，不证明业务语义。
2. `module-function-ownership.tsv`
   - 与 function catalog 一一对应。
   - 初始全部 `unresolved / pending_assignment`；只能由人工查看完整机器码和调用边界后更新。
3. `*-closure.tsv`
   - 锁定一个专项报告已经枚举出的有限函数集合和固定审计顺序。
   - 现有 C++ 不继承完成状态；必须先登记实现映射，再进入最终 REVIEW。
4. `research/modules/*.md`
   - 每个模块只允许一个当前 work package，记录范围/非范围、closure 集合、接口、状态所有权、实现队列、验证和未决项。
5. `research/evidence/*.md`
   - 每个函数或紧耦合组的物理范围、ABI、基本块、测试向量、双向追溯和平台例外。

## 2. 实现与 REVIEW 状态分离

closure 状态：

- `pending_mapping`：尚未确认现有 C++ 是否覆盖该函数；不得推断已实现或未实现。
- `pending_implementation`：物理范围和 owner 已确认，但 C++ 尚未覆盖完整合同。
- `implemented_pending_review`：已有 C++ 实现映射，可以继续实现其他函数；尚未完成最终汇编↔C++ 收敛，仍视为未验证。
- `assembly_exact`：完整有效域已完成最终双向逐基本块 REVIEW，最后一轮零新增差异。
- `platform_adapted`：完整原合同已 REVIEW，只有明确记录的宿主隔离不同。
- `cross_module_handoff`：当前模块只负责的请求/结果边界已 REVIEW，内部合同明确交给另一个 owner。
- `unreachable_current_assets`：当前资产不可达已由机器码和资产证明，但原分支仍保留。
- `external_boundary`：CRT、DOS、Miles 等外部实现不复制，游戏实际依赖合同已 REVIEW。

`pending_mapping`、`pending_implementation` 和 `implemented_pending_review` 都是未关闭状态。允许先完成全部 B0–B9 实现后统一 REVIEW，但在最终 REVIEW 前，函数、模块和阶段不得标为 `assembly_exact` 或“完成”。

## 3. `assembly_exact` 硬门

一行只有同时满足以下条件才能关闭：

- `target_owner` 已人工确认；
- `implementation_mapping` 指向实际 C++ 单元；
- `evidence` 指向存在的函数/紧耦合组证据；
- `verification` 记录汇编独立 UT、适用真实资产和集成门；
- `final_review = converged_no_new_differences`；
- `remaining` 为空。

最终 REVIEW 必须：

```text
不看 C++，从机器码入口独立恢复全部基本块/出口
→ 汇编到 C++ 正向逐块映射
→ C++ 到汇编反向逐行为定位
→ 差异同步修正代码、测试、证据和 inventory
→ 从函数入口重新执行完整正反向 REVIEW
→ 重复直到完整一轮零新增差异
```

## 4. 生成与验证

```bash
python3 research/tools/build_reverse_inventory.py
python3 research/tools/validate_reverse_framework.py
```

生成器只刷新机器生成字段并保留人工状态列。validator 会拒绝：

- function catalog 与 headless 报告并集不一致；
- ownership 缺项、重复地址或非法 owner/status；
- closure 与专项报告的函数集合/顺序不一致；
- `implemented_pending_review` 没有实现映射；
- 关闭行没有 evidence/verification/最终收敛；
- 关闭行仍声明 remaining；
- 伪造的空 closure 工作包。
