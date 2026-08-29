# OpenLegend

OpenLegend 是《金庸群侠传》DOS 版的现代 C++20 还原工程。

本项目的目标不是“玩法相近”或“能够通关”的现代复刻，而是让现代实现的可观察行为与当前原版一致。

> **唯一真值：一切以当前原版 `Z.COM` / `Z.DAT` 的机器码和完整汇编为准。** 反编译伪码、研究文档、测试、开源端口和当前 C++ 实现都只是辅助证据；发生冲突时必须修正它们，而不是修改原版行为。

## 当前状态

- **B0 已完成**：C++20/CMake 工程、同步模式协调、SDL3 窗口与 indexed framebuffer 上传。
- **B1 已完成**：普通 IDX、MMAP 特例、SDX/WDX 哨兵、RLE、字体、调色板和世界层读取。
- **B2 进行中**：按原版汇编还原 framebuffer、RLE 四向裁剪、ASCII/Big5 字形与基础呈现。
- **B3–B9 未完成**：模型存档、输入音频、标题菜单、世界、场景、战斗与完整集成。

“能够启动”“能够探索”或“一场战斗可运行”只属于中间里程碑，不代表 1:1 还原完成。完整验收条件见 [`goal/execution-plan.md`](goal/execution-plan.md)。

## 已验证的原版数据合同

当前真实资产测试覆盖：

- 118 对 `.IDX/.GRP`；
- 84 对 `SDX/SMP` 与 26 对 `WDX/WMP`；
- 12,927 个普通非空 RLE 帧；
- 65,087 个哨兵索引非空 RLE 帧；
- `MMAP` 前 3,731 个有效索引；
- `FONT3.E16`、`FONT3.C16` 与 `MMAP.COL`；
- 五个 `480×480×int16le` 世界层。

对应汇编证据见 [`research/evidence/resource-loader-1to1.md`](research/evidence/resource-loader-1to1.md)。

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

## 构建

要求：

- Python 3；
- GCC、Clang 或 MSVC 的 C++20 工具链；
- 首次构建时可访问 Python package index 和 SDL GitHub release 压缩包。

构建脚本会把固定版本的 CMake 3.31.10 与 Ninja 1.13.0 安装到仓库内已忽略的 `.tools/`，不会修改系统工具链。

### Linux / WSL

```bash
./build.sh core                 # 只构建核心库并运行测试
./build.sh sdl                  # 构建 SDL3 应用并运行全部测试
./build.sh sdl --config Release
```

### Windows

```bat
build.bat core
build.bat sdl
build.bat sdl --config Release
```

可选参数：

```text
--jobs N
--configure-only
--skip-tests
```

默认构建目录为：

```text
build/<platform>-<core|sdl>-<debug|release>/
```

## 工程结构

```text
include/openlegend/   公共 C++ 接口
src/                  app、compat、resource、render 与平台实现
tests/                单元、真实资产与集成测试
research/             架构、汇编证据、IDA 脚本/报告/数据库
goal/                 1:1 执行计划与阶段验收真值
tools/                项目自包含构建入口
```

核心模块不暴露 SDL、DOS 或 VGA 宿主类型。SDL3 仅负责窗口、输入、时钟、音频设备和最终纹理上传；核心画面真值始终是：

```text
320×200×8-bit indexed framebuffer + 256×RGB6 palette
```

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
