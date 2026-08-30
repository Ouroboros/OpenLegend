# 基础框架、runtime 与 platform_sdl3 工作包

状态：`framework_in_progress`；现有实现统一视为 `implemented_pending_review`

## 1. 当前范围

- 逆向 inventory 生成与 validator；
- 日志基础设施；
- SDL 事件翻译、held-key 接线、indexed frame 最终呈现与错误报告；
- app 同步模式/会话生命周期；
- Linux/Windows core/app 构建入口和 smoke。

不在本工作包恢复世界、场景、KDEF 或战斗业务语义。

## 2. 证据与 inventory

- `research/inventory/function-catalog.tsv`
- `research/inventory/module-function-ownership.tsv`
- `research/inventory/rewrite-module-map.tsv`
- `research/inventory/module-state-ownership.tsv`
- `research/inventory/module-dependencies.tsv`
- `research/ida/reports/Z_DAT.b4_runtime_xrefs.txt`
- `research/ida/reports/Z_DAT.input_callbacks.txt`

现有专项报告尚未形成覆盖完整 runtime/platform 根地址的 closure；因此本模块不得关闭。

## 3. 当前实现单元

1. `[x]` 机械 function catalog / ownership / closure 生成器；
2. `[x]` inventory validator 首版；
3. `[ ]` 将 validator 注册为 CTest；
4. `[ ]` 完成线程安全文件日志、失败回退和日志测试；
5. `[ ]` 接入启动、错误、输入、模式切换、移动结果和呈现失败日志；
6. `[ ]` 建立 runtime/platform 专项 headless FUNCTION 报告和 closure；
7. `[ ]` Linux/Windows Debug/Release 与 sanitizer 验证。

## 4. 停止线

基础框架完成只表示可以继续实现业务模块，不表示 runtime/platform 已 `assembly_exact`。完整实现可继续标 `implemented_pending_review`，最终统一 REVIEW 时再逐函数关闭。
