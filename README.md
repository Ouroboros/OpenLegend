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
- **B9 已完成**：577项函数catalog全部分类，7张closure表349行对应284个物理函数，全部收敛且pending/unverified为0；Linux/Windows core与app矩阵、Sanitizer、SDL smoke、全量资产、Golden、IDA与原文件哈希门均已通过。

B0–B9执行计划已经关闭。最终验收见[`research/evidence/b9-final-acceptance.md`](research/evidence/b9-final-acceptance.md)，函数分母与分类见[`research/evidence/function-catalog-coverage.md`](research/evidence/function-catalog-coverage.md)。原DOS程序动态运行oracle因当前环境缺少可执行宿主而继续如实登记为`blocked_runtime_oracle`；它不冒充现代CTest或独立资产oracle，也不掩盖未登记的产品差异。

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
- 140条`WAR.STA`记录、26个战场、92套`FIGHT`资源共4,992帧、26槽参战者状态，以及覆盖战斗入口、行动、AI、伤害、状态面板、结算/升级/练功/制造的76项独立Golden。

对应汇编证据见 [`research/evidence/resource-loader-1to1.md`](research/evidence/resource-loader-1to1.md)、[`research/evidence/render-1to1.md`](research/evidence/render-1to1.md)、[`research/evidence/model-persistence-1to1.md`](research/evidence/model-persistence-1to1.md)、[`research/evidence/input-time-random-audio-1to1.md`](research/evidence/input-time-random-audio-1to1.md)、[`research/evidence/title-menu-new-game-1to1.md`](research/evidence/title-menu-new-game-1to1.md)、[`research/evidence/world-map-1to1.md`](research/evidence/world-map-1to1.md)、[`research/evidence/scene-event-dialogue-1to1.md`](research/evidence/scene-event-dialogue-1to1.md) 和 [`research/evidence/battle-1to1.md`](research/evidence/battle-1to1.md)。

## 原版数据目录

仓库不包含、也不会分发原版游戏文件。BUILD可用`--data-dir <目录>`指定每台机器上的原版数据目录；相对路径以仓库根目录为基准。未传参数时为兼容旧布局，默认使用仓库父目录。也可设置环境变量`OPENLEGEND_GAME_DATA_ROOT`，命令行参数优先。

例如仓库与数据目录并列时：

```text
E:\Game\OpenLegend\
├── OpenLegend\       # 本仓库
└── data\             # 原版数据目录
    ├── Z.COM
    ├── Z.DAT
    ├── RANGER.GRP
    ├── MMAP.IDX
    ├── MMAP.GRP
    ├── FONT3.E16
    ├── FONT3.C16
    └── ...            # 其余原版资源
```

对应BUILD命令为`build.bat app --data-dir "E:\Game\OpenLegend\data"`。该路径通过CMake测试配置在CTest运行时传入，不会作为机器路径编译进UT；修改路径后BUILD会自动重新配置现有缓存。原版文件只读使用；构建、测试和生成物只写入仓库内已忽略目录。

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

## 开发环境与BUILD手册

所有配置、编译和测试必须从仓库根目录通过平台包装器执行：Linux/WSL使用`./build.sh`，Windows使用`build.bat`。不要直接调用`build.py`、CMake、CTest、Ninja或编译器；根目录[`build.py`](build.py)是唯一BUILD编排实现，两个包装器只准备平台环境并转发参数。

### 1. 新环境准备

共同要求：

- Python 3；
- Clang 23 C/C++20工具链；
- 首次BUILD可访问Python package index，以及toml++、libADLMIDI、SDL GitHub release压缩包；
- 完整`app`测试所需的原版数据，至少应先确认目录中存在`Z.COM`和`Z.DAT`。

BUILD会把固定版本的CMake 3.31.10与Ninja 1.13.0安装到仓库内已忽略的`.tools/`，不会修改系统工具链。首次成功后会复用本地工具和Ninja cache。

Linux/WSL包装器默认使用`clang-23`和`clang++-23`；如环境已经提供其他Clang 23命令，可在调用前设置`CC`与`CXX`。Windows包装器当前使用：

```text
LLVM        D:\Dev\Compiler\LLVM\x64\bin
Python      D:\Dev\Python\python.exe；不存在时回退到PATH中的python
```

Windows路径和原版数据目录可以包含Unicode字符，但命令行中应始终加引号。

### 2. 原版数据目录

BUILD数据目录的优先级为：

1. `--data-dir PATH`；
2. 环境变量`OPENLEGEND_GAME_DATA_ROOT`；
3. 仓库父目录。

BUILD命令中的相对路径固定以仓库根目录为基准，不受调用者原工作目录影响。切换数据目录后，现有CMake cache会自动重新配置；路径只在CTest运行时传入，不会编译进UT。原版资产只读使用。

常见布局：

```text
workspace/
├── OpenLegend/       # 仓库
└── data/             # 原版数据
    ├── Z.COM
    ├── Z.DAT
    └── ...
```

对应命令：

```bash
./build.sh app --data-dir ../data
```

```bat
build.bat app --data-dir "..\data"
```

### 3. BUILD参数

| 参数 | 含义与默认值 |
| --- | --- |
| `core` | 只构建无SDL应用的核心目标；未写目标时的默认值。 |
| `app` | 构建完整SDL应用；默认不运行CTest。 |
| `sdl` | `app`的兼容别名；新命令使用`app`。 |
| `--config Debug\|Release` | 普通BUILD默认`Debug`；启用Sanitizer且未指定配置时默认`Release`。 |
| `--data-dir PATH` | 原版数据目录；相对路径以仓库根目录为基准。 |
| `--jobs N` | 编译并发数；默认逻辑CPU数，可由`OPENLEGEND_BUILD_JOBS`设置。通常无需手工传入。 |
| `--test-jobs N` | 使用`--tests`时的测试并发数；默认逻辑CPU数，可由`OPENLEGEND_TEST_JOBS`设置。通常无需手工传入。 |
| `--configure-only` | 只生成或刷新配置，不编译、不测试。 |
| `--tests` | 编译后运行CTest，并在BUILD前验证原版数据目录；默认不运行。 |
| `--skip-tests` | 明确跳过CTest和BUILD前的数据目录身份检查；这是默认行为。 |
| `--sanitizers` | Linux启用ASan+UBSan；Windows启用LLVM动态ASan。 |

### 4. 日常BUILD

Linux/WSL：

```bash
./build.sh core --data-dir ../data
./build.sh app --data-dir ../data
./build.sh app --config Release --data-dir ../data
./build.sh app --config Debug --sanitizers --data-dir ../data
```

Windows：

```bat
build.bat core --data-dir "E:\Game\OpenLegend\data"
build.bat app --data-dir "E:\Game\OpenLegend\data"
build.bat app --config Release --data-dir "E:\Game\OpenLegend\data"
build.bat app --config Release --sanitizers --data-dir "E:\Game\OpenLegend\data"
```

日常BUILD默认只编译；需要运行CTest时显式添加`--tests`。Windows Sanitizer只支持Release；`--config Debug --sanitizers`会被BUILD明确拒绝。Sanitizer BUILD会把LLVM 23的`clang_rt.asan_dynamic-x86_64.dll`部署到应用和每个测试EXE旁。普通Debug/Release使用静态CRT且不会携带ASan DLL。

### 5. 完整验收矩阵

阶段关闭或跨平台基础设施变更后，至少执行：

```bash
./build.sh core --config Debug --data-dir ../data --tests
./build.sh core --config Release --data-dir ../data --tests
./build.sh app --config Debug --data-dir ../data --tests
./build.sh app --config Release --data-dir ../data --tests
./build.sh app --config Debug --sanitizers --data-dir ../data --tests
```

```bat
build.bat core --config Debug --data-dir "E:\Game\OpenLegend\data" --tests
build.bat core --config Release --data-dir "E:\Game\OpenLegend\data" --tests
build.bat app --config Debug --data-dir "E:\Game\OpenLegend\data" --tests
build.bat app --config Release --data-dir "E:\Game\OpenLegend\data" --tests
build.bat app --config Release --sanitizers --data-dir "E:\Game\OpenLegend\data" --tests
```

当前CTest注册中，core为119项，app为120项并包含SDL smoke。Windows render、scene与battle测试保持默认1MB PE栈，不允许用链接栈选项掩盖大型fixture。

### 6. 缓存、产物与重配置

普通与Sanitizer缓存隔离：

```text
build/<platform>-<core|app>/
build/<platform>-<core|app>-asan/
```

生成器固定为**Ninja Multi-Config**。BUILD会在生成器、编译器、Ninja路径、Sanitizer状态或数据目录变化时自动重配或重建失效缓存。需要无条件重新配置时：

```bash
OPENLEGEND_RECONFIGURE=1 ./build.sh app --data-dir ../data
```

```bat
set "OPENLEGEND_RECONFIGURE=1" && build.bat app --data-dir "E:\Game\OpenLegend\data"
```

应用BUILD完成后会输出可直接启动的`openlegend`或`openlegend.exe`绝对路径。用户配置文件`openlegend.toml`位于可执行文件旁；BUILD重置缓存时会保留已有配置内容。

### 7. 常见问题

- `Game data directory was not found`：检查`--data-dir`；BUILD相对路径以仓库根目录为基准。当前所有启用测试的target（包括`core`）都会先执行`Z.COM`/`Z.DAT`身份检查。
- `missing required file(s): Z.COM, Z.DAT`：传入的不是原版游戏根目录，或资产不完整。
- Windows提示ASan不支持Debug CRT：改用`--config Release --sanitizers`。
- 更换Clang、Ninja或数据目录后出现旧缓存信息：先让BUILD自动重配；仍需强制刷新时设置`OPENLEGEND_RECONFIGURE=1`。
- 首次BUILD无法下载CMake/Ninja或第三方源码：确认Python package index和GitHub release下载可达；不要改为直接调用系统CMake绕过统一入口。
- 命令提示`unrecognized arguments: --app`：`app`是位置参数，应写成`build.sh app`或`build.bat app`。

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
- [B9最终全集成验收](research/evidence/b9-final-acceptance.md)
- [577项函数分母与分类](research/evidence/function-catalog-coverage.md)
- [原程序架构](research/architecture/program-architecture.md)
- [现代代码所有权与依赖](research/architecture/rewrite-architecture.md)
- [研究索引](research/README.md)

## 许可说明

本仓库不授予原版游戏数据、文字、美术、音乐或可执行文件的再分发权。使用者必须自行合法取得原版数据。

XMI 合成使用固定的 libADLMIDI v1.6.1；该第三方库按 GNU LGPL v3 授权，构建时从上游源码压缩包取得并保留其独立许可条件。OpenLegend 不修改或再分发其源码快照。
