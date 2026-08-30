# 世界地图工作包

状态：`implemented_pending_review`；B6 完成结论已撤销

## 1. 有限范围

当前专项报告锁定 `research/inventory/world-map-closure.tsv` 的 34 个函数。该集合是现有 B6 报告范围，不自动等于 Z_DAT 全部 world owner；最终 REVIEW 前还必须与 `module-function-ownership.tsv` 的人工 owner 过滤结果对账。

## 2. 已有实现

五层世界数据、128×128 cache、移动、碰撞、入口、陆地/船、idle、天气、深度绘制和同步 scene request 已有 C++ 与测试。它们统一视为 `implemented_pending_review`，不继承 `assembly_exact`。

## 3. 已知回归

- SDL 左右键曾读取错误 translated-key 索引，已修正并证明此前平台整链 REVIEW 不完整；
- 用户报告人物移动后消失，尚未取得包含模式、坐标、frame 和深度结果的运行日志；
- 因上述缺陷，B6 保持打开。

## 4. 实现阶段队列

当前先完成基础日志和诊断框架。业务实现可以在 B0–B9 功能完成阶段继续；最终统一 REVIEW 时，按 closure 顺序从机器码入口重新恢复语义，不继承本文叙述。

## 5. 最终 REVIEW 停止线

34 行及 ownership 过滤后补入的全部 world 函数必须从 `pending_* / implemented_pending_review` 变为有证据的关闭状态；每行均需实现映射、函数证据、验证和 `converged_no_new_differences`。人物消失必须得到复现、解释或修正，不能以资源帧非空代替运行链证据。
