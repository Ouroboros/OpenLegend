# 基础框架、runtime 与 platform_sdl3 工作包

状态：基础逆向框架 `ready`；runtime/platform 现有实现统一视为 `implemented_pending_review`

## 1. 当前范围

- 逆向 inventory 生成与 validator；
- 日志基础设施；
- SDL 事件翻译、held-key 接线、indexed frame 最终呈现与错误报告；
- app 同步模式/会话生命周期；
- Linux/Windows core/app 构建入口和 smoke；Windows 全矩阵频率遵守 GOAL 第 0.2 节。

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
3. `[x]` 将 validator 注册为 CTest；
4. `[x]` 完成线程安全文件日志、立即 flush、stderr/Windows debugger 失败回退和日志测试；
5. `[x]` 接入启动、错误、输入、模式切换、世界/场景移动、碰撞、入口/跳转、坐标、方向、动画帧、玩家绘制列表和呈现失败日志；
6. `[ ]` 建立 runtime/platform 专项 headless FUNCTION 报告和 closure；
7. `[x]` 日志切片 Linux app Debug 构建、13/13 CTest 与 SDL dummy smoke；
8. `[ ]` B7 完整模块收口时按 GOAL 第 0.2 节执行 Windows 全矩阵，不为本小切片重复执行。

## 4. 停止线

基础框架完成只表示可以继续实现业务模块，不表示 runtime/platform 已 `assembly_exact`。完整实现可继续标 `implemented_pending_review`，最终统一 REVIEW 时再逐函数关闭。
