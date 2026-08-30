# 场景、事件与对话工作包

状态：`implemented_pending_review`；基础框架和诊断日志已完成，按有限 handler 组继续业务实现

## 1. 有限范围

当前专项报告锁定 `research/inventory/scene-event-closure.tsv` 的 96 个函数，包括场景会话、六层绘制、事件入口、KDEF 解释器、事件 helper、battle 入口和公共绘制/RNG helper。最终范围仍需与全局 ownership 人工对账。

## 2. 已有实现

场景资源、六层绘制、移动/碰撞、跳转/出口、TALK、基础 KDEF switch、app 同步链、天气和部分角色/物品副作用已有 C++ 与测试。所有行统一视为 `pending_mapping` 或 `implemented_pending_review`，不得继承旧完成状态。

opcode 0/13/14 的呈现/淡入淡出已提交；opcode 25 的 `sub_2ED8D` 视口平移、逐帧呈现和 2 BIOS tick 等待已实现并登记为 `implemented_pending_review`。其余 handler 继续按机器码和真实脚本分组补齐。

## 3. 实现阶段与最终 REVIEW

允许先按完整 KDEF helper/handler 组补齐 B7 实现，再统一 REVIEW。最终 REVIEW 必须按机器码入口而非 opcode 命中顺序执行，逐项登记参数宽度、PC 推进/改写、同帧继续、跨帧让出、阻塞条件、状态副作用、异常出口和原 BUG，并反查全部 C++ 行为。

## 4. 停止线

96 行和后续 ownership 对账补入项全部关闭；任何 `pending_mapping`、`pending_implementation`、`implemented_pending_review` 或 remaining 非空都阻止 B7 完成。
