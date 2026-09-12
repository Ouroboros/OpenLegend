# OpenLegend 构建、测试、部署与启动

本文档说明 OpenLegend 的统一 BUILD 入口、原版数据目录、测试矩阵、产物部署和启动边界。

## 操作边界

以下操作彼此独立，不得混淆：

1. **BUILD**：配置、编译和链接；
2. **部署**：把 EXE、PDB 和所需运行库复制到目标目录；
3. **启动**：运行 `openlegend` 或 `openlegend.exe`。

项目内的 `build.sh` 与 `build.bat` 只负责 BUILD，不会启动游戏。这里的 `build.bat` 仅指本仓库根目录内的文件；仓库外同名的工作区脚本不是 BUILD 入口，可能包含复制、启动程序或其他本地动作。

## 统一 BUILD 入口

所有配置、编译和测试必须从仓库根目录通过平台包装器执行：

- Linux/WSL：`./build.sh`
- Windows：`build.bat`

不要直接调用 `build.py`、CMake、CTest、Ninja 或编译器。根目录 [`build.py`](build.py) 是唯一 BUILD 编排实现，两个包装器只准备平台环境并转发参数。

## 新环境准备

共同要求：

- Python 3；
- Clang 23 C/C++20 工具链；
- 首次 BUILD 可访问 Python package index，以及 toml++、libADLMIDI、SDL GitHub release 压缩包；
- 完整 `app` 测试所需的原版数据，至少应确认目录中存在 `Z.COM` 和 `Z.DAT`。

BUILD 会把固定版本的 CMake 3.31.10 与 Ninja 1.13.0 安装到仓库内已忽略的 `.tools/`，不会修改系统工具链。首次成功后会复用本地工具和 Ninja cache。

Linux/WSL 包装器默认使用 `clang-23` 和 `clang++-23`；如环境提供其他 Clang 23 命令，可在调用前设置 `CC` 与 `CXX`。

Windows 包装器当前使用：

```text
LLVM        D:\Dev\Compiler\LLVM\x64\bin
Python      D:\Dev\Python\python.exe；不存在时回退到 PATH 中的 python
```

Windows 路径和原版数据目录可以包含 Unicode 字符，但命令行中应始终加引号。

## 原版数据目录

仓库不包含原版游戏文件。BUILD 数据目录的优先级为：

1. `--data-dir PATH`；
2. 环境变量 `OPENLEGEND_GAME_DATA_ROOT`；
3. 仓库父目录。

BUILD 命令中的相对路径固定以仓库根目录为基准，不受调用者原工作目录影响。切换数据目录后，现有 CMake cache 会自动重新配置；路径只在 CTest 运行时传入，不会编译进 UT。原版资产只读使用。

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

## BUILD 参数

| 参数 | 含义与默认值 |
| --- | --- |
| `core` | 只构建无 SDL 应用的核心目标；未写目标时的默认值。 |
| `app` | 构建完整 SDL 应用；默认不运行 CTest。 |
| `sdl` | `app` 的兼容别名；新命令使用 `app`。 |
| `--config Debug\|Release` | 普通 BUILD 默认 `Debug`；启用 Sanitizer 且未指定配置时默认 `Release`。 |
| `--data-dir PATH` | 原版数据目录；相对路径以仓库根目录为基准。 |
| `--jobs N` | 编译并发数；默认逻辑 CPU 数，可由 `OPENLEGEND_BUILD_JOBS` 设置。 |
| `--test-jobs N` | 使用 `--tests` 时的测试并发数；默认逻辑 CPU 数，可由 `OPENLEGEND_TEST_JOBS` 设置。 |
| `--configure-only` | 只生成或刷新配置，不编译、不测试。 |
| `--tests` | 编译后运行 CTest，并在 BUILD 前验证原版数据目录。 |
| `--skip-tests` | 明确跳过 CTest 和 BUILD 前的数据目录身份检查；这是默认行为。 |
| `--sanitizers` | Linux 启用 ASan+UBSan；Windows 启用 LLVM 动态 ASan。 |

## 日常 BUILD

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

日常 BUILD 默认只编译；需要运行 CTest 时显式添加 `--tests`。Windows Sanitizer 只支持 Release；`--config Debug --sanitizers` 会被 BUILD 明确拒绝。Sanitizer BUILD 会把 LLVM 23 的 `clang_rt.asan_dynamic-x86_64.dll` 部署到应用和每个测试 EXE 旁。普通 Debug/Release 使用静态 CRT，不携带 ASan DLL。

## 完整验收矩阵

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

当前 CTest 注册中，core 为 119 项，app 为 120 项并包含 SDL smoke。Windows render、scene 与 battle 测试保持默认 1MB PE 栈，不允许用链接栈选项掩盖大型 fixture。

回归测试不得读取、修改或依赖玩家实际使用的 `data/SAVE1`。

## 缓存、产物与重配置

普通与 Sanitizer 缓存隔离：

```text
build/<platform>-<core|app>/
build/<platform>-<core|app>-asan/
```

生成器固定为 **Ninja Multi-Config**。BUILD 会在生成器、编译器、Ninja 路径、Sanitizer 状态或数据目录变化时自动重配或重建失效缓存。

需要无条件重新配置时：

```bash
OPENLEGEND_RECONFIGURE=1 ./build.sh app --data-dir ../data
```

```bat
set "OPENLEGEND_RECONFIGURE=1" && build.bat app --data-dir "E:\Game\OpenLegend\data"
```

应用 BUILD 完成后会输出 `openlegend` 或 `openlegend.exe` 的绝对路径。用户配置文件 `openlegend.toml` 位于可执行文件旁；BUILD 重置缓存时会保留已有配置内容。

## 部署

部署是独立于 BUILD 的显式操作。不要借助会自动启动游戏的外层脚本完成复制。

普通应用至少复制：

- `openlegend` 或 `openlegend.exe`；
- Windows 调试需要对应的 `openlegend.pdb`。

Windows ASan 应用还必须把同批 BUILD 生成的 `clang_rt.asan_dynamic-x86_64.dll` 复制到 `openlegend.exe` 同目录。EXE、PDB 和 ASan DLL 应来自同一个构建配置，不得混用旧产物。

`openlegend.toml` 应放在部署后的可执行文件同目录。相对的 `data_dir`、`save_dir` 和日志路径均从该目录解析。

部署完成不代表已经启动；除非明确要求，不应自动运行游戏。

## 常见问题

- `Game data directory was not found`：检查 `--data-dir`；BUILD 相对路径以仓库根目录为基准。所有启用测试的 target（包括 `core`）都会先执行 `Z.COM`/`Z.DAT` 身份检查。
- `missing required file(s): Z.COM, Z.DAT`：传入的不是原版游戏根目录，或资产不完整。
- Windows 提示 ASan 不支持 Debug CRT：改用 `--config Release --sanitizers`。
- 更换 Clang、Ninja 或数据目录后出现旧缓存信息：先让 BUILD 自动重配；仍需强制刷新时设置 `OPENLEGEND_RECONFIGURE=1`。
- 首次 BUILD 无法下载 CMake/Ninja 或第三方源码：确认 Python package index 和 GitHub release 下载可达；不要直接调用系统 CMake 绕过统一入口。
- 命令提示 `unrecognized arguments: --app`：`app` 是位置参数，应写成 `build.sh app` 或 `build.bat app`。
