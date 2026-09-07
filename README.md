# OpenLegend

OpenLegend 是《金庸群侠传》DOS 版的现代 C++20 还原工程。

本项目的目标不是“玩法相近”或“能够通关”的现代复刻，而是让现代实现的可观察行为与当前原版一致。

> **唯一真值：一切以当前原版 `Z.COM` / `Z.DAT` 的机器码和完整汇编为准。** 反编译伪码、研究文档、测试、开源端口和当前 C++ 实现都只是辅助证据；发生冲突时必须修正它们，而不是修改原版行为。

## 当前状态

- **B0 已完成**：C++20/CMake 工程、同步模式协调、SDL3 窗口与 indexed framebuffer 上传。
- **B1 已完成**：普通 IDX、MMAP 特例、SDX/WDX 哨兵、RLE、字体、调色板和世界层读取。
- **B2 已完成**：framebuffer、RLE 四向裁剪、ASCII/Big5 字形、palette 淡变、shadow-mask 与地图深度顺序。
- **B3 已完成**：RANGER 六段、基线/工作副本/三槽 R/S/D、lossless snapshot 与逐字节回写。
- **B4 已完成**：IRQ1/set-1 键态、BIOS tick、原 LCG、Miles 命令顺序、8 槽 raw WAV 与 XMI/OPL3 音频。
- **B5 已完成**：标题/三槽逐像素流程、CFONT 姓名输入、初始属性、六项菜单、状态/物品基础 UI、隔离存读错误路径与 SDL 会话链。
- **B6 已完成**：五层世界、128×128 缓存、陆地/船移动、碰撞与入口、待机/天气周期和逐像素世界绘制。
- **B7 已完成**：场景、事件、对话与场景/世界往返；`scene-event` closure 为100/100。
- **B8 已完成**：战斗入口、选择、玩家动作、AI、动画、结算与战后提交；battle closure 为81/81。
- **B9 进行中**：统一最终汇编→C++ REVIEW当前为`closure=330/349`、`unique_any=273/284`、`unique_all=265/284`。剩余owner集中在`input-font`（33/39已关闭）与`ui`（26/39已关闭）。

“能够启动”“能够探索”或“一场战斗可运行”只属于中间里程碑，不代表 1:1 还原完成。完整验收条件见 [`goal/execution-plan.md`](goal/execution-plan.md)。

## 已验证的原版数据合同

当前真实资产测试覆盖：

- 118 对 `.IDX/.GRP`；
- 84 对 `SDX/SMP` 与 26 对 `WDX/WMP`；
- 12,927 个普通非空 RLE 帧；
- 65,087 个哨兵索引非空 RLE 帧；
- `MMAP` 前 3,731 个有效索引；
- `FONT3.E16`、`FONT3.C16` 与 `MMAP.COL`；
- 五个 `480×480×int16le` 世界层；
- RANGER 六段、100 个 S 场景状态、100 个 D 事件状态、基线/工作副本和三槽存档；
- 84-byte 键盘翻译表、tick 舍入/回绕和 4 组独立 RNG 向量；
- 24 个 `ATK*.WAV`、53 个 `E*.WAV` 与 24 个 `GAME*.XMI`，逐项长度、格式与 XMI PCM 生成；
- 9 个标题帧、主/读档/等待逐像素 hash、CFONT 候选、四组新游戏 RNG 向量、双页状态/物品渲染与三槽成功/损坏/写失败会话链；
- 五层 128×128 cache hash、固定陆地/船轨迹、场景 70/IQ 条件、初始世界与 300 tick 半透明天气 framebuffer；
- `ALLSIN` 100个六层场景、`ALLDEF` 100×200条十一字段事件、2,977条对话与1,018份KDEF脚本，以及场景输入、跳转、对话和事件opcode的独立Golden；
- 140条`WAR.STA`记录、26个战场、92套`FIGHT`资源共4,992帧、26槽参战者状态，以及覆盖战斗入口、行动、AI、伤害、状态面板、结算/升级/练功/制造的75项独立Golden。

对应汇编证据见 [`research/evidence/resource-loader-1to1.md`](research/evidence/resource-loader-1to1.md)、[`research/evidence/render-1to1.md`](research/evidence/render-1to1.md)、[`research/evidence/model-persistence-1to1.md`](research/evidence/model-persistence-1to1.md)、[`research/evidence/input-time-random-audio-1to1.md`](research/evidence/input-time-random-audio-1to1.md)、[`research/evidence/title-menu-new-game-1to1.md`](research/evidence/title-menu-new-game-1to1.md)、[`research/evidence/world-map-1to1.md`](research/evidence/world-map-1to1.md)、[`research/evidence/scene-event-dialogue-1to1.md`](research/evidence/scene-event-dialogue-1to1.md) 和 [`research/evidence/battle-1to1.md`](research/evidence/battle-1to1.md)。

## 原版数据目录

仓库不包含、也不会分发原版游戏文件。当前构建测试要求原版文件位于仓库父目录：

```text
金庸群侠传/
├── OpenLegend/       # 本仓库
├── Z.COM
├── Z.DAT
├── RANGER.GRP
├── MMAP.IDX
├── MMAP.GRP
├── FONT3.E16
├── FONT3.C16
└── ...               # 其余原版资源
```

原版文件只读使用；构建、测试和生成物只写入 `OpenLegend/build/` 或其他仓库内已忽略目录。

## 配置文件

示例见 [`config/openlegend.example.toml`](config/openlegend.example.toml)。把它复制到 `openlegend` 可执行文件同目录并命名为 `openlegend.toml`。当前实际生效字段如下：

```toml
[paths]
data_dir = 'E:\Game\金庸群侠传'

[window]
width = 960
height = 600
maximized = false
```

- `--data-dir <目录>` 或 `--data-dir=<目录>` 的优先级高于 `[paths].data_dir`；
- 配置中的相对数据路径以可执行文件目录为基准，命令行相对路径以启动目录为基准；
- 正常退出会回写窗口宽高和最大化状态，同时保留 `[paths]` 与未知 TOML 表；
- 当前不暴露显示 FPS/世界移动插值字段；B4/B6 已按 BIOS tick 和原始 held-key 状态驱动世界，不增加宿主插值配置。

## 构建

要求：

- Python 3；
- Clang 23 C/C++20 工具链；
- 首次构建时可访问 Python package index，以及 toml++、libADLMIDI、SDL GitHub release 压缩包。

根目录 [`build.py`](build.py) 是唯一构建编排脚本，统一负责参数、配置、缓存目录、编译、测试、Sanitizer运行库和最终产物路径。`build.sh`与`build.bat`只准备各平台编译器/Python环境并原样转发参数。脚本会把固定版本的CMake 3.31.10与Ninja 1.13.0安装到仓库内已忽略的`.tools/`，不会修改系统工具链。

### Linux / WSL

```bash
./build.sh core                 # 只构建核心库并运行测试
./build.sh app                  # 构建 SDL3 应用并运行全部测试
./build.sh app --config Release
./build.sh app --config Debug --sanitizers  # ASan + UBSan
```

### Windows

Windows薄包装器准备固定LLVM/Python环境；CMake、CTest与Ninja仍由根`build.py`统一取得：

```text
Clang       D:\Dev\Compiler\LLVM\x64\bin
Python      D:\Dev\Python\python.exe，PATH中的python仅作回退
```

```bat
build.bat core
build.bat app
build.bat app --config Release
build.bat app --config Release --sanitizers
```

`build.bat`保持仓库根目录的Unicode长路径并原样转发参数；测试侧把UTF-8原版资源路径显式转换为Windows宽路径。Windows Sanitizer只支持Release，使用LLVM 23官方动态ASan runtime；构建脚本会在测试前把`clang_rt.asan_dynamic-x86_64.dll`复制到应用和每个测试EXE旁，因此产物可直接启动。普通Debug/Release继续使用静态CRT，且不会携带ASan DLL。

可选参数：

```text
--jobs N
--test-jobs N
--configure-only
--skip-tests
--sanitizers
```

普通与Sanitizer缓存始终隔离：

```text
build/<platform>-<core|app>/
build/<platform>-<core|app>-asan/
```

生成器固定为 **Ninja Multi-Config**，同一Sanitizer变体内的`Debug`与`Release`共用target构建目录；可用`--config`选择配置。构建脚本复用有效的Ninja cache，设置`OPENLEGEND_RECONFIGURE=1`可强制重新配置。完成应用构建后会输出可直接启动的`openlegend`/`openlegend.exe`绝对路径。

兼容旧命令时仍可使用 `sdl`，但它只作为 `app` 的别名：

```bash
./build.sh sdl
```

当前B9工作包最近一次匹配范围验收为Linux app Debug 14/14；完整B9关闭仍需按执行计划重新完成Linux/Windows矩阵、Sanitizer、smoke、资产只读和IDA审计，不能由既往阶段结果替代。

## 工程结构

```text
build.py              唯一构建编排脚本
config/               可复制的 openlegend.toml 示例
include/openlegend/   公共 C++ 接口
src/                  app、compat、resource、model、persistence、input、time、random、audio、render 与平台实现
tests/                单元、真实资产与集成测试
research/             架构、汇编证据、IDA 脚本/报告/数据库
goal/                 1:1 执行计划与阶段验收真值
```

核心模块不暴露 SDL、DOS 或 VGA 宿主类型。SDL3 仅负责窗口、宿主键事件、音频设备和最终纹理上传；BIOS tick、键态、RNG 和 mixer 均在核心。核心画面真值始终是：

```text
320×200×8-bit indexed framebuffer + 256×RGB6 palette
```

DOS 索引像素不能直接作为现代窗口像素提交。`openlegend_compat` 的受测显示转换层按 `palette[index]` 逐像素取 RGB6，并用 `(value << 2) | (value >> 4)` 展开到 RGBA8；SDL3 只上传这份现代纹理，以 nearest-neighbor 居中整数倍缩放并保留黑边。转换与缩放只发生在最终显示边界，绝不反写或替换上述核心像素真值。

## 研究与贡献约束

- 原版汇编是唯一正确性真值；合法数据上的新增保护不得改变原版行为。
- 不修复原逻辑 BUG，不改善 AI，不改变数值、流程、随机数消费或像素覆盖顺序。
- IDA 分析只能使用 `idat.exe -A` headless。
- 原版资源、可执行文件、存档、构建产物和用户环境输出不得提交。
- 每个已验证阶段必须精确暂存、使用 `$commit` Skill 提交并立即推送。
- 完整仓库规则见 [`AGENTS.md`](AGENTS.md)。

## 文档入口

- [执行 GOAL](goal/execution-plan.md)
- [原程序架构](research/architecture/program-architecture.md)
- [现代代码所有权与依赖](research/architecture/rewrite-architecture.md)
- [研究索引](research/README.md)

## 许可说明

本仓库不授予原版游戏数据、文字、美术、音乐或可执行文件的再分发权。使用者必须自行合法取得原版数据。

XMI 合成使用固定的 libADLMIDI v1.6.1；该第三方库按 GNU LGPL v3 授权，构建时从上游源码压缩包取得并保留其独立许可条件。OpenLegend 不修改或再分发其源码快照。
