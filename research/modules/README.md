# 模块 work package 合同

每个正在实施或 REVIEW 的模块只能有一个 work package。文件至少包含：

1. 状态：实现状态与 REVIEW 状态分开；
2. 范围与非范围：由 `inventory` 的地址集合锁定；
3. 汇编/资产证据：机器码范围、报告和原始文件；
4. 接口与状态所有权：只引用 inventory，不重复造 owner；
5. 当前实现单元：可先完成实现并标 `implemented_pending_review`；
6. 测试、真实资产和差分点；
7. closure 统计；
8. 未决项和精确下一停点。

允许先实现全部 B0–B9，再统一 REVIEW；但 work package 不得把“已有 C++”“测试通过”或“可运行”写成模块完成。只有 closure 全部关闭且最后一轮完整正反向 REVIEW 零新增差异，模块才能关闭。

函数/紧耦合组证据使用 `research/evidence/function-review-template.md`。
