# OpenLegend 执行 GOAL

版本：v6
当前阶段：B0–B9 统一最终汇编→C++ REVIEW
当前有效进度：`closure=329/349`，`unique_any=273/284`，`unique_all=264/284`
当前任务指针：input-font `research/inventory/input-font-closure.tsv` `audit_order=33` `sub_3271E`
下一任务指针：input-font `research/inventory/input-font-closure.tsv` `audit_order=34` `sub_32E59`

## 0. 唯一正确性真值

**一切以当前原版 `Z.COM` / `Z.DAT` 的机器码和完整汇编为准；原版汇编是本项目唯一正确的行为真值。**

- IDA 反编译伪码只用于导航，不能覆盖实际指令、寄存器、标志位、栈参数和基本块顺序。
- 研究文档、字段命名、现有测试、现代 C++、开源端口、编辑器、攻略和玩法常识都只是辅助证据。
- 上述任何内容与原版汇编冲突时，无条件服从原版汇编，并同步修正实现、测试、文档和 GOAL 进度。
- 原始资源字节用于证明数据可达性和输入域，但资源表现的最终解释仍由读取它的原版汇编决定。
- 没有汇编或原程序输出证据的行为不得标记为 `assembly_exact`，不得据此宣称 1:1 完成。

### 0.1 强制反复 REVIEW 收敛门禁

每个函数、handler、事件 opcode、平台适配调用链和紧耦合行为单元都必须执行以下闭环，任何一步不得省略：

```text
锁定机器码物理范围、调用者、参数/寄存器/标志位、共享状态和全部出口
  -> 暂不参考现有 C++，从汇编独立记录基本块、分支、位宽、读写和调用顺序
  -> 从汇编独立派生正常、边界、错误、哨兵、截断、回绕和原 BUG 测试
  -> 再逐基本块把本轮汇编推导与现有 C++、测试和证据对照
  -> 发现差异时最小修正实现、测试、证据和本 GOAL 状态
  -> 修正后废弃本轮 REVIEW 结论，从函数入口重新开始
  -> 新一轮仍暂不参考现有 C++，重新执行完整汇编→C++推导与对照
  -> 重复上述过程，直到一轮完整汇编→C++ REVIEW 不再产生任何新差异或未决项
```

硬性要求：

- REVIEW 的方向始终只有“汇编→C++”：每轮先从机器指令独立推导，再与 C++ 对照；不执行“C++→汇编”REVIEW，不以现代实现为起点反向寻找汇编依据。
- REVIEW 次数不设上限，以结果收敛为唯一停止条件；“再看一次”或固定轮数不构成完成。
- 每次发现并修正差异后，上一轮 REVIEW 结论立即作废；必须从函数入口重新独立推导，不能只复查修改过的基本块。
- 编译通过、单元测试通过、真实资产可运行、已有 golden 和现有 C++ 看似合理均不能替代反复汇编→C++ REVIEW。
- 平台边界同样必须审计完整链路；核心函数正确但 SDL 键码、显示、音频或会话接线错误，仍视为该行为单元未完成。
- 条件方向、符号/零扩展、16/32 位截断、整数回绕、RNG 消费、阻塞/呈现时序、状态副作用和所有提前出口必须逐项追溯。
- 只有最后一轮完整 REVIEW 零新增差异、全部差异已落入实现/测试/证据且全部验证通过，才允许标记 `assembly_exact` 或关闭工作包。
- 后续发现真实运行缺陷时，必须立即撤销相关阶段的完成结论，重新打开并从汇编入口执行整轮收敛，不得仅做症状补丁。

实现阶段与最终 REVIEW 阶段允许分离：可以先完成 B0–B9 的全部 C++ 功能实现，再按锁定的函数/handler workpack 统一 REVIEW。分离期间状态必须精确记录：

- `pending_mapping`：尚未确认现有 C++ 是否覆盖；
- `pending_implementation`：范围已确认但实现未完成；
- `implemented_pending_review`：已有实现，但尚未取得最终汇编一致性结论；
- `assembly_exact` / `platform_adapted`：仅在最终 REVIEW 收敛后使用。

因此实现完成、测试通过和可运行都只能推进到 `implemented_pending_review`；最终 REVIEW 前不得把函数、模块或阶段写成“完成”。

### 0.2 构建验证频率

- 普通函数、handler、测试、日志、工具和小切片只执行与改动匹配的 Linux 定向构建/测试；不得为每个切片机械重复 Windows 构建。
- Windows `core/app × Debug/Release` 全矩阵仅在一个完整大模块实现/REVIEW 工作包收口时执行，例如 B7 整体完成、B8 整体完成，以及 B9 最终全集成验收。
- 大模块关闭前的 Windows 全矩阵结果属于该模块验收证据；更早小切片曾通过 Windows 不能替代模块关闭时的最终矩阵。
- B9 总目标关闭仍必须满足第 9 节完整 Linux/Windows、sanitizer、smoke、资产只读和 IDA 审计门禁。

### 0.3 阶段性 TG 汇报（强制格式）

每达到一个已经验证、可独立回退并完成提交与推送的阶段性边界，主 Agent 必须调用：

```bash
python3 /mnt/d/Dev/Source/Project/stockkit/scripts/tg_notify.py "CONTENT"
```

`CONTENT` 固定为恰好五段。必须逐字使用下面的段首与顺序，不得改标签、重排、增减或合并段落；相邻段落之间恰好保留一个真实空行：

```text
OpenLegend <模块或阶段>：<功能或工作包>已完成。

实现：<高层业务结果>。

兼容：<关键兼容行为与调用方回收；没有新增兼容事项时写“保持既有兼容边界。”>。

验证：<本轮实际执行的验证门>全部通过。

进度：<整体进度>；下一项<下一工作包或明确动作>。
```

硬性要求：

- 汇报只包含阶段/工作包名称、高层完成概述、关键兼容结果、实际验证和整体进度/下一步；不得罗列函数地址、helper、字段、flag、基本块、测试边界、workpack SHA256 或 commit ID。
- shell 参数必须使用普通 ASCII 双引号包住一整个多行字符串，并在双引号内直接写真实换行和真实空行；禁止使用单引号、`$"..."`、`$'...'`、字面量 `\n`、转义拼接或单行长句。
- 未到大模块边界时只报告本轮实际执行的 Linux/定向验证，不得暗示 Windows、sanitizer、smoke 或全矩阵已经运行。
- 五段之外不得增加标题、问候、总结、风险或备注；确有阻塞时把阻塞事实压缩写入“兼容”段。
- TG 命令失败时必须记录错误并明确告知用户，不得声称已经发送或送达。
- Context 压缩或自动 Goal continuation 不得中断该规则；恢复工作时必须先从本节重新确认最近一个已提交、已推送且尚未汇报的阶段边界。

### 0.4 每轮逆向后强制重读规则

- 每完成一轮逆向（物理函数机器码独立推导、C++ 映射/实现、证据登记和本轮验证收口）后，在开始下一轮逆向前，主 Agent 必须重新完整读取仓库根目录 `AGENTS.md` 与 `/root/.pi/agent/APPEND_SYSTEM.md`。
- 不得凭记忆、摘要或先前读取结果代替本轮重读；必须实际调用文件读取工具并读完整内容。
- Context 压缩、自动 Goal continuation、阶段提交、push 或 TG 汇报均不得跳过、延后或合并该重读门禁。
- 重读发现的新约束立即应用于后续工作；如与当前执行方式冲突，先停止冲突操作并按最新文件内容修正。

### 0.5 编译与测试强制使用 BUILD 脚本

- 所有配置、编译和测试必须调用仓库根目录现有 BUILD 脚本：Linux/WSL 使用 `./build.sh`，Windows 使用 `build.bat`。
- 禁止自行拼接或直接调用 `cmake`、`ctest`、`ninja`、编译器及等价底层构建命令；单 target、单测试、短命令或历史命令均不构成例外。
- 仅当现有 BUILD 脚本确实无法表达任务所需的明确特殊构建/测试需求时，才允许例外；执行前必须先向用户说明脚本缺口、直接命令、影响范围和恢复方式。
- 独立 oracle、逆向框架 validator 和资产审计不属于编译/测试，可继续通过后台进程管理器单独运行。

## 1. 总目标

以现代 C++20/CMake/SDL3 为实现载体，对《金庸群侠传》DOS 版进行**可观察行为 1:1 还原**。这不是依据玩法重新设计的“现代重实现”，也不是只保证可玩或大致兼容的复刻。

1:1 还原范围包括：

- 启动链、标题、世界、场景、菜单、战斗、保存、读取和退出的调用顺序；
- 当前原版资产可达的全部游戏自有逻辑和状态迁移；
- 原始资源编号、偶数精灵编号、记录布局、字符串字节和未知字段；
- 16/32 位有符号与无符号运算、截断、溢出、随机数流和原逻辑 BUG；
- 输入按下/持续/释放、同 tick 同步请求、阻塞等待和 BIOS tick 量化语义；
- `320×200×8-bit indexed framebuffer`、RGB6 调色板、裁剪、覆盖顺序和逐像素结果；
- 原存档文件的读取、写入、未知字节保留和未修改状态逐字节 round-trip；
- 错误返回、提前退出、异常资源和原程序实际依赖的 CRT/Miles 行为合同。

现代模块拆分只能改变源码所有权和宿主平台接入方式，不能改变上述行为。任何无法做到 1:1 的平台差异必须形成显式偏差记录，并在用户明确批准前阻止“完整还原”结论。

## 2. 当前固定决定

- **完整机器指令、原始文件字节和可重复原程序输出是行为真值。** IDA 伪码、现有开源端口、玩法常识和当前 C++ 测试都不能单独证明 1:1。
- C++20、CMake、模块静态库；B0 已固定 GCC/Clang/MSVC 可构建边界。
- SDL3 只用于宿主窗口、事件、音频设备和最终纹理上传，不拥有游戏语义；BIOS tick 量化属于核心。
- 核心像素真值为 `320×200×8-bit indexed framebuffer + 256×RGB6 palette`；现代显示层必须逐像素展开为宿主 RGBA 纹理并做 nearest-neighbor 整数缩放，不能把 DOS 索引字节直接交给现代窗口系统，也不能反向污染核心像素真值。
- 原始游戏数据保持只读；测试生成物只写入 OpenLegend 构建/测试目录。
- 资源解析采用显式小端读取；合法原始数据必须逐字节等价，新增边界保护不得改变合法输入行为。
- 原 Big5、legacy ID、16/32 位整数、整数除法、溢出、异常行为和原 BUG 必须显式表示。
- 跨模块请求由 `app` 在原调用点同步消费，不使用会改变时序的异步事件总线。
- 一个可变状态只有一个所有者；状态拆分后仍必须保持原程序交叉写入顺序。
- 存档只运输 snapshot，但导入/导出顺序和物理字节必须与原版一致。
- CRT、DOS、VGA 和 Miles 不要求复制其通用内部实现，但原游戏实际调用到的行为合同必须还原。
- IDA 必须继续使用 `idat.exe -A` headless；不启动 IDA GUI。
- 未经用户明确批准，不修复原逻辑 BUG、不改变数值平衡、不改善 AI、不重排 UI 流程、不替换资源语义。

## 3. 总体执行方法

先完成一次全局架构恢复，然后严格按模块循环：

```text
模块所有权与可达路径清单
  -> 对当前工作包逐函数/逐基本块定点逆向
  -> 写格式、整数、状态、副作用和时序规格
  -> 建立汇编向量、真实资产、逐字节/逐像素 golden
  -> 现代 C++ 实现合法域行为
  -> 暂不参考现有 C++，从汇编重新独立推导后逐基本块对照 C++ 的所有分支与调用点
  -> 有差异则修正、废弃本轮结论，并从函数入口重新执行完整汇编→C++ REVIEW
  -> 重复直到一轮完整汇编→C++ REVIEW 零新增差异
  -> 原程序差分或等价 oracle 验证
  -> 清零当前模块 unresolved 后关闭工作包
```

架构阶段不要求预先理解全部函数；但进入某模块后，该模块当前资产可达的游戏自有函数、分支、副作用和出口必须全部闭环，不能以“已经可玩”代替 1:1 完成。

## 4. 阶段 A：架构恢复

状态：**完成**

产物：

- `../research/architecture/program-architecture.md`
- `../research/architecture/rewrite-architecture.md`
- `../research/ida/` 下的 headless 数据库、脚本、日志和报告

完成条件：

- [x] 启动链和主程序边界已确认；
- [x] 资源、会话、世界、场景、战斗、UI、渲染、输入、音频、存档等大模块已识别；
- [x] 可说明初始化、普通世界、场景、战斗、存读档和退出的模块协作；
- [x] 平台替换边界与核心兼容行为已分离；
- [x] 现代 target、依赖方向、状态所有权和切环规则已确定；
- [x] 未决项均已归入对应模块，不再阻塞工程结构。

停止线：满足以上条件后立即结束全局调研。事件 opcode、战斗公式、完整键码表和未知记录字段不延长阶段 A，但必须在对应 B 阶段全部闭环后才能宣称 1:1 完成。

## 5. 阶段 B：逐模块 1:1 还原计划

用户已通过当前 Goal 明确授权执行 B0–B9。

状态重置：B0–B7 既有 C++、测试、golden 和证据不删除，但不继承完成结论。所有行为单元必须进入 `research/inventory` 和唯一模块 work package；已有实现先标 `implemented_pending_review`，未映射或未实现项分别标 `pending_mapping` / `pending_implementation`。允许先完成 B0–B9 功能实现，再统一按第 0.1 节 REVIEW。

### B0 · 工程与顶层骨架

范围：`compat + platform_sdl3 + app`

交付：

- CMake/C++20 工程和模块 target；
- 假平台端口与 `ModeCoordinator` 单元测试；
- SDL3 窗口、事件循环和空 indexed framebuffer 上传；
- 初始化、逻辑 tick、同步模式结果和逆序销毁骨架。

验收：无游戏数据也能运行空窗口测试；假模块验证调用顺序，不伪装业务模块已完成。

### B1 · 资源与格式基础

范围：`resource`

交付：

- `.IDX/.GRP`、`SDX/WDX`、RLE 帧、地图裸层、调色板和字体视图；
- 统一边界错误和只读数据根目录；
- 当前已识别资源的全量扫描测试。

验收：当前数据目录全部 118 对 IDX/GRP、全部 SDX/WDX、所有当前资产可达 RLE 帧和五个世界层逐项通过；每种索引规则、空项、零尾、异常偏移和截断均与原读取合同一致。

### B2 · 像素、字体与基础呈现

范围：`render`

交付：

- 320×200 indexed framebuffer；
- RLE 精灵裁剪、矩形、清屏、ASCII/Big5 字体和调色板；
- SDL3 通过受测兼容转换层执行 indexed/RGB6 → RGBA8 展开和 nearest-neighbor 整数缩放；
- 固定资源的 framebuffer/palette golden hash。

验收：所有绘制原语、四向裁剪、全部当前资产帧、ASCII/Big5 字形、地图深度顺序和调色板更新逐像素符合原算法；RGB6 端点和任意 palette index 的 RGBA8 展开有 golden，任意窗口视口仅采用居中整数倍 nearest-neighbor 缩放，平台转换不得改变核心缓冲。

### B3 · 游戏模型与物理存档

范围：`model + persistence` 的物理格式部分

交付：

- `RANGER.GRP` 六段受检视图和首批规范化类型；
- 基线、工作副本、三槽文件读取；
- 未知字节 lossless 保存；
- `GameSnapshot` 导入/导出边界。

验收：已有 R/S/D 和基线/工作副本全部可读；未修改状态逐字节 round-trip；所有会被当前游戏代码读取或写入的字段均有语义与所有者，未知但不可达字段保留原字节。

### B4 · 输入、时间、随机与音频

状态：**已有实现，`implemented_pending_review`**
证据：`../research/evidence/input-time-random-audio-1to1.md`、`../research/ida/reports/Z_DAT.b4_runtime_xrefs.txt`

范围：`input + time + random + audio + platform_sdl3 audio/event adapter`

交付：

- 按下/持续/释放和逻辑动作映射；
- BIOS tick 等价逻辑时钟和 LCG；
- WAV 播放；
- XMI 解码/合成后端及游戏级播放状态。

验收：完整键码与输入边沿、BIOS tick 量化、所有 RNG 调用点和当前 WAV/XMI 资产的播放/停止/切换顺序均有原程序向量；宿主输出差异必须有偏差审计，不能以“能播放”代替原时序。

### B5 · 标题、菜单与新建/读取流程

状态：**已有实现，`implemented_pending_review`**

已有证据：标题/三槽逐像素 golden、CFONT 姓名输入、17 次 RNG 初始属性/BABERUTH、六项菜单、双页状态/物品 UI、隔离目录存读失败合同、`GameState` 会话所有者、SDL 主循环和 DOS indexed→现代 RGBA 显示兼容层均已接入。参数0/1医疗解毒目标选择、过滤后的施术者列表、状态公式、结果/无合格提示和任意键边界已覆盖世界及场景菜单；参数2“查看状态”选择框与双页角色状态由同一精确renderer覆盖世界、场景和战斗背景。医疗、解毒、状态入口及双页函数均为`implemented_pending_review`；共享选择器参数3..5与参数6完整离队业务仍为`pending_implementation`。最新Linux/Windows app Debug均14/14通过，Linux app ASan+UBSan 14/14通过；普通Linux Debug cache已恢复并复验，独立B5 golden及五项原版资产门禁通过。

范围：`ui + app + persistence`

交付：

- 标题、选择、文本框、角色状态/物品基础 UI；
- 新游戏从基线创建工作状态；
- 三槽扫描和加载；
- 模态 UI 的同步结果合同。

验收：启动器、标题和全部当前可达菜单分支逐状态闭环；新游戏、三槽扫描/加载、取消、错误和返回路径与原版顺序一致，并得到逐字段一致 `GameState`。

### B6 · 世界地图

状态：**完成；`world-map-closure.tsv` 34/34均已完成最终汇编→C++ REVIEW**

已有证据：`../research/evidence/world-map-1to1.md`、`../research/evidence/world-map-goldens.json`、`../research/ida/reports/Z_DAT.b6_world_xrefs.txt`。五层资源与缓存、移动/碰撞/船只、场景入口与返回续行、待机/天气/50次移动衰减、深度队列、indexed绘制、SDL按键与最终呈现链均已从当前机器码入口反复复核并收敛。原版左右键映射、人物移动后保留渲染节点、船体双格、缓存边界、跨scene世界瞬态及slot-index中毒BUG均有定向回归；B6 oracle双次幂等并与仓库基线一致，逆向框架为0项world待审，Linux app Debug及Windows core/app × Debug/Release模块关闭矩阵通过。动态原程序差分统一登记为`blocked_runtime_oracle`。

范围：`world`

交付：

- 五层 480×480 数据、128×128 缓存、32×32 视口；
- 菱形投影、玩家移动、碰撞、入口和周期更新；
- `WorldStepResult`。

验收：世界全部五层语义、缓存边界、投影、移动、碰撞、入口、周期更新和随机遇敌可达分支闭环；固定输入 trace 的坐标、请求、RNG 消费和 indexed frame 与原版一致。

### B7 · 场景、事件与对话

范围：`scene`

当前状态：`scene-event-closure.tsv` 100/100已完成统一最终汇编→C++ REVIEW；最终`audit_order=6`确认正常退出与内部跳转按当前场景号顺序写回完整地图和事件记录，载入进度分支跳过即将废弃的旧场景工作副本；现代全场景快照即时持有两个span属于平台适配。双oracle、静态/closure/逆向框架、Linux app Debug 14/14及Windows `core/app × Debug/Release`模块关闭矩阵全部通过；Windows Debug scene测试专用栈按实际fixture规模提升至64MiB。

交付：

- 6×64×64 场景层和事件区；
- `TALK.GRP` XOR/Big5；
- 事件执行器按实际使用指令逐批闭环；
- 返回世界、菜单和战斗请求。

验收：100 个场景数据和 2,977 条 TALK 记录可读；当前 KDEF/场景资产可达的全部事件指令、跳转、副作用、对话、战斗和返回路径闭环，不以单一示例场景代替完成。

### B8 · 战斗

范围：`battle`

交付：

- FIGHT 资源、战斗建立、角色临时态、输入、动作、AI、绘制和结果；
- 战后状态提交；
- `Victory / Defeat` battle出口；AI `Escape`为回合内动作11，不是battle级第三出口。

当前状态：81项closure已全部完成最终汇编→C++ REVIEW；当前Linux/Windows `core/app × Debug/Release`模块关闭八项矩阵全部通过（core各13/13、app各14/14）。order18 `sub_34C47`机器身份为1044 bytes、248条指令、42条分支、73处重定位、6个caller和10次direct call；packed magic槽、magic先于目标的RNG顺序、七项特殊加成末项覆盖、unsigned熟练度/100、三次targeting判定、mode0/1/2、signed行动值短路、automatic flag1、休息与统一action_done尾均已逐块审计。首轮修正了两个stale-target差异：selector未写时复用合法旧目标，以及移动后nearest未写时保留旧目标并落入rest；非法线性索引继续由现代安全拒绝。order19 `sub_3505B`机器身份为223 bytes、63条指令、9条分支、6处重定位、2个caller和8次direct call；signed morality高低门槛、IQ门槛、每段`bounded(10)<7`、实际0至2次RNG、命中策略未写目标不回退及无RNG nearest尾均已逐块审计。order20/21最高与最低攻击selector分别固定为109/112 bytes、32/32条指令、5/5条分支、7/7处重定位；均按signed全槽扫描、same-side/任意非零hidden过滤、无HP过滤、strict比较与first-tie写目标。最高攻击best0且非正值保留stale目标；最低攻击best1000，负值可选而等于/高于1000保留stale目标。order22专长selector固定为347 bytes、91条指令、19条分支和19处重定位；同方用毒触发扫描不排除hidden/HP0，敌方解毒与医疗共享signed best，独立flag BUG使detox达到20仍跳过medicine后调用最低攻击，只有medicine达到20才保留专长目标。order23最近selector固定为156 bytes、39条指令、5条分支和12处重定位；原版逐合格候选重建相同actor-source targeting图，按signed best1000 strict最短选择，不读HP、同距保留早槽，blocked555仍可选，无候选保留stale word11。order24用毒handler固定为497 bytes、115条指令、8条分支、40处重定位、1个caller和10次direct call；signed用毒射程、round零/正/负方向、mode3同目标复检、无作用abs调用、actor attack fallback及入口冻结allied total/count均已审计。首轮修正误用target attack，第二轮修正宿主resume时重算总值，第三轮入口重审零新增差异。order25用毒目标固定为104 bytes、32条指令、3条分支、2处重定位、1个caller和4次direct call；signed IQ>60、roll<7、最高攻击-1 fallback、BX截断返回及0/1次RNG均已审计。首轮修正把局部EBX=-1误作actor word12清零的差异，入口重审零新增差异；无目标plan与完整Session均验证旧word12保留。order26用毒最高攻击selector固定为168 bytes、46条指令、8条分支、11处重定位、1个caller和1次栈探测call；signed全槽扫描、异方、hidden恰0、不读HP、poison<95、anti-poison<actor use-poison、best0 strict最高攻击、同值首槽、非正攻击返回-1并保留word12均已审计。dead同值首槽、negative hidden和signed负poison/anti等号向量补齐后，首轮入口REVIEW零产品差异。order27用毒首目标selector固定为244 bytes、56条指令、8条分支、18处重定位、1个caller和2次direct call；每合格候选重复建actor-source图但始终查询全局`word_E6EE0`陈旧槽，best1000 signed strict使距离<1000时只选首个合格目标，无候选或距离>=1000保留word12。首轮宿主REVIEW修正Session以actor旧word12替代独立全局scratch的差异，按AI选择、攻击初选/重选、成功用毒及暗器写点维护Session镜像；修正后入口重审零新增差异。order28 AI物品wrapper固定为40 bytes、14条指令、0分支/重定位、1个caller和3次direct call；严格忽略`sub_34AEC(actor,1)`返回并无条件调用`sub_3598C(actor,0)`，自身无RNG/状态写入，第二callee EAX仅透传且caller不读。现代有目的时mode0/value0移动后resume使用，无目的时直接使用，完整Session保持效果面板、不读取输入、九tick延后来源提交及外层action_done；入口REVIEW零产品差异，callee不传播closure。order29 AI暗器handler固定为353 bytes、84条指令、3条分支、30处重定位、1个caller和7次direct call；一次目标策略后无条件复制word11到独立scratch，signed hidden_weapon/15+1射程、signed路径比较、首检、round正值mode1移动、零/负值跳移动但仍同目标二检及失败后不消费暗器的自动攻击回退均已审计。负hidden_weapon和round边界、stale目标及完整Session三路径补齐后，入口REVIEW零产品差异，callee不传播closure。order30 AI物品/暗器执行器固定为1959 bytes、440条指令、47条分支、125处重定位、两个正常入口、三个外部owner共享尾跳入及17次direct call；mode0效果面板后不读键而固定九tick再扣来源，mode1队伍动画/消费取当前AI物品但HP/毒payload取陈旧玩家确认槽，随机后add_hp先截signed word，完整EFT后才提交状态，damage非零写kind1且`sub_38910(0)`固定前四帧闪烁。order53 caller交叉审计纠正先前把固定suppress0误实现成固定kind0的回归；修正后从order30入口重审全部440条指令、47个分支、125处fixup、17次call及出口，零剩余差异；delegated callee及共享尾caller不传播closure。order31敌方携带物品删除固定为121 bytes、28条指令、2个跳转、8处重定位、唯一caller和唯一栈探测call；signed slot0..3分别同步左移3/2/1/0组item ID与数量，最后无条件清第4槽，返回role字节偏移但caller不读。现代单次role查找等价，四槽与非法actor/role/高槽回归补齐后首轮入口REVIEW零产品差异。order32请求医疗handler固定为93 bytes、22条指令、1个signed分支、6处重定位、唯一直接caller、一个外部owner共享跳入及3次direct call；signed行动值正数才以mode0/value0移动并忽略返回，零/负跳过，汇合后重读请求目标当前坐标并无条件自动攻击，返回由caller忽略。目标坐标重读、signed三边界及非法域回归补齐后，两轮入口REVIEW均零产品差异。order33请求解毒wrapper严格为7 bytes、2条指令、1个short jump、零重定位/call/local RET；push16后跳入`0x361B1`形成与order32相同栈形，最终由共享RET直接回唯一动作9caller。动作9正/零/负行动值、移动后坐标重读与非法域回归补齐后，两轮wrapper入口REVIEW零产品差异，且不反向复制order32主体closure。order34 AI医疗handler固定为412 bytes、97条指令、6个分支、33处重定位、唯一caller和9次direct call；signed medicine/15向零截断加1、两次signed距离检查、仅正行动值mode1/range移动、移动后目标重读、两个纯abs返回丢弃，以及live actor攻击对入口冻结wrapped己方平均的strict攻击/休息门均已审计。首轮修正移动continuation后重扫队友attack/HP的差异，计划现携带入口prelude总值/人数；区分向量锁定冻结平均300而非错误重算2300。修正后入口重审零新增差异。order35 AI解毒handler固定为354 bytes、79条指令、7个分支、28处重定位、唯一caller和7次direct call；signed detoxification/15向零截断加1、两次signed距离检查、仅正行动值mode1/range移动、移动后目标重读及相同冻结平均回退均已审计。该owner没有医疗入口的两个abs call，解毒、攻击和休息三路全部尾跳外部`loc_39A3E`回收参数与寄存器。动作4专属signed/移动/目标重读/冻结/wrapped/非法域回归补齐后，首轮入口REVIEW零产品差异。order36 AI移动handler固定为1418 bytes、311条指令、57个分支、80处重定位、8个正常caller、一个外部共享尾入口和14次direct call；signed目标距离减行动值的本回合预判、mode2条件轴向exact layer、mode3非轴向layer、x外/y内strict Manhattan first-tie、generic上右左下两轮候选、逐候选重建、x优先/y退向actor、source短出口、双signed path<128检查及最短路/逐格delegation均已审计。新增mode2超出本回合转generic、非同轴第二轮、全占用退回source、降层终止后二检失败四组原资产区分向量；首轮入口REVIEW零产品差异。order37 玩家移动wrapper固定为95 bytes、32条指令、1个JNZ、1处重定位、唯一caller和4次direct call；signed actor/行动值、mode0光标调用、32位local仅比较low word恰等于1、取消不标路/不移动返回-1、确认调用最短路及`sub_37355(actor,0,0,0)`后返回0，以及caller紧随的IMUL覆盖EAX均已审计。新增完整Session取消后坐标、行动值与体力不变回归，入口REVIEW零产品差异。order38 通用玩家光标handler固定为783 bytes、157条指令、35个分支、72处重定位、六个正常caller和7次direct call；mode0/1图、下右左上优先级、signed 64列线性别名、occupancy悬停例外、Escape及mode0/1确认条件均已审计。首轮发现首键前少一次present的产品差异，现以入口计数2和每次未结束输入后计数1修正；新增两次早期Escape与source拒绝确认后立即方向均被忽略的Session回归，修正后入口重审零剩余差异。order39 移动寻路图wrapper固定为121 bytes、20条指令、1个循环分支、12处重定位、八个callsite和3次direct call；movement初始化后按signed `y*64+x`强制source为0，队列严格初始化为读0/写2/distance0、slot0 sentinel与slot1 source，至少一次调用flood step并在EAX为0时回环，返回值由全部caller忽略。合法caller域首轮完整入口REVIEW零产品差异，额外私有队列清零不可观察，非法source安全拒绝归类平台适配；新增battle0/93 source occupancy向量锁定source归零及完整图hash。order40 目标寻路图wrapper同为121 bytes、20条指令、1个循环分支、12处重定位和3次direct call，中和三条CALL后与order39其余116字节完全相同，唯一业务差异是调用`sub_36FF9` targeting初始化；十五个callsite均在任何读取或控制流使用前覆盖EAX。真实battle0/93 upper-layer阻挡source向量锁定初始化后source归零及完整图hash，合法caller域首轮完整入口REVIEW零产品差异，非法source安全拒绝归类平台适配。order41 movement阻挡图初始化固定为257 bytes、72条指令、17个跳转、8处重定位、唯一caller和1次20-byte栈探测call；先把64×64 path全部写0，再按x外/y内和signed `y*64+x`扫描，upper layer非0、occupancy任一非-1或signed ground命中九段原资产闭区间时写555，否则写254。正常EAX63由唯一caller忽略，外部共享尾只回收ABI状态且不传播owner closure；现代省略不可观察预清零、改独立格线性扫描及void返回均归类平台适配，首轮完整入口REVIEW零产品差异。order42 targeting阻挡图初始化固定为119 bytes、39条指令、10个跳转、4处重定位、唯一caller和1次8-byte栈探测call；同样预清零并按x外/y内与signed `y*64+x`扫描，但只读upper layer，零写254、任一非零写555，完全不读occupancy或ground。正常EAX8190由唯一caller忽略，自有`POP EBX/RET`退出；现代同类中间态、独立格扫描顺序及void返回均归类平台适配，battle0/93非空occupancy与battle89 ground-only tile区分向量通过，首轮完整入口REVIEW零产品差异。order43 flood step固定为246 bytes、76条指令、11个跳转、7处重定位、两个wrapper caller和7次direct call；每step先出队，signed y<0时distance按signed `%128`递增、先重入sentinel再出队，连续sentinel返回-1；普通节点按上右左下及signed坐标`0..64`扫描，path恰254时先入队再写当前distance，完成后返回0。两个wrapper均按EAX零回环，7-byte共享尾保持EAX；现代内联全部step、私有化共享queue/index/distance和void返回不可观察，真正index>=4096安全拒绝归类平台适配，x64 in-storage别名保留。中心十字六步trace、battle0/93一二层距离和battle0 x64别名hash通过，首轮完整入口REVIEW零产品差异。order44 queue dequeue固定为72 bytes、21条指令、0跳转、2处重定位、两个同owner callsite和1次8-byte栈探测call；第三参数signed读下标在x读、y读和更新前各重读一次，分别从255-word x/y数组写出后对`index+1`执行signed `idiv255`，余数回写且商由EAX返回。真实caller三指针互异并只检查y，合法254→0的商1及其他商0均忽略；现代交错coordinate、私有index、一次读取与省略商返回不可观察。253→254→0 signed极值trace和battle0/93回溯前457/822个可达格通过，首轮完整入口REVIEW零产品差异。order45 queue enqueue固定为73 bytes、18条指令、0跳转、4处重定位、两个同owner callsite和1次12-byte栈探测call；一次signed读取write index后把x/y低16位依次写入同一slot，无full guard，随后signed `idiv255`回写余数并返回商。sentinel与候选caller均忽略EAX；现代交错coordinate、私有index与一次复合写不可观察。253→254→0 signed极值写trace及真实图457/822个可达格通过，首轮完整入口REVIEW零产品差异。order46 path write固定为39 bytes、10条指令、0跳转、1处重定位、四个callsite和1次4-byte栈探测call；signed `2*(y*64+x)`得到字节偏移，写value低16位并以EAX返回偏移。四caller分别写distance、250、250与255；三个忽略或覆盖EAX，首个mark caller残留高字在合法域为0且后续只消费low word。现代直接int16写、存储内x64别名一致，负坐标和真正index>=4096安全拒绝归类平台适配；线性别名/越界向量及真实distance/marked/consume hash通过，首轮完整入口REVIEW零产品差异。order47 path read固定为39 bytes、9条指令、0跳转、2处重定位、四个callsite和1次4-byte栈探测call；signed `y*64+x`读取word，先写scratch原始位型再`cwde`返回。四caller均使用EAX判254、保存distance、判distance或判250；scratch仅有本函数写xref且无读xref，现代省略不可观察。signed返回/别名/越界向量与真实path hash通过，首轮完整入口REVIEW零产品差异。order48 shortest-path marking固定为272 bytes、75条指令、10跳转、10处重定位、两个caller和5次direct call；target先标250，signed `(d+127) IDIV128`余数降层，上右左下首个匹配前驱与source均标250，完成跳外部共享尾。合法BFS链最多4095步且两个caller均在path<128域进入、忽略EAX；现代bool/4096上限等价，无前驱false替代机器无界下移回环。正常/tie/0→127/source相等/malformed及battle0/93 marked hash通过，首轮完整入口REVIEW零产品差异。order49 per-step movement固定为991 bytes、215条指令、38个函数体跳转、61处重定位、2个caller和11次direct call；每格严格按旧path255、旧occupancy -1、新occupancy actor、x/y、direction、sprite、条件体力DEC、round DEC写入，随后更新视图、render、present并等待参数40，再按player destination或AI mode0..3的destination/行动值/Manhattan/同轴规则判停。合法caller域首轮完整入口REVIEW零产品差异；无250邻格的checked终止替代机器无界重试或伪destination停止，非法slot/role/坐标/mode安全拒绝归类平台适配。三十三个owner修正或对照后均从入口重审零新增差异。order50 attack core固定为3690 bytes、837条指令、106个函数体跳转、226处重定位、34次direct call和2个caller；完整锁定slot0 BUG、unsigned profile、四种area、缓存双击、FIGHT/EFT/damage/sprite/present提交、RNG升级、MP/体力与16位回绕。首轮发现原`E6EC2`无战斗初始化清零而现代每场重置的跨战差异，现由runtime持有进程期scratch并透传Session/Setup；区分回归及修正后完整入口重审通过。order51 FIGHT animation固定为684 bytes、174条指令、13个函数体跳转、35处重定位、9次direct call和4个caller；完整锁定FIGHT/双bank先预载、阈值只启动loaded sample、同帧bank1先于bank2、19帧与signed零帧边界。首轮修正阈值处错误load+start；fresh xref再修正support清槽、Session丢失跨战`E6ED6`及AI同步过晚差异，现由runtime持有进程期槽并在玩家攻击入口/选择及AI plan建立时同步。slot0 sample7/slot2 sample8跨Session、AI移动前写点与菜单取消回归通过，修正后完整入口重审通过。order52 throwing effect animation固定为198 bytes、56条指令、4个函数体跳转、9处重定位、9次direct call和3个caller；锁定sample13/effect双bank先预载、bank1启动、100参数等待期间零render/present、其后仅启动bank2，以及effect0/2/30逐帧时间线。首轮修正默认play导致的load/start错序及prelude重绘caller帧两项差异；玩家、AI直接和AI移动后三路径音频action、caller-frame不变、EFT/damage hash及RNG/状态边界回归通过，修正后入口重审零剩余差异。order53 damage animation固定为120 bytes、31条指令、5个函数体跳转、7处重定位、4次direct call和7个caller；锁定suppress只观察低16位、十帧phase0..9、normal前4帧flash、suppressed全灭、render→present→phase++→delay1及最终kind清零。首轮修正AI暗器非零damage被清kind0及present确认后phase延迟递增两项差异，并从受影响order30入口重审全部440条指令；玩家支持/暗器与AI直接/移动暗器逐帧状态、pixel、RNG及延后消费回归通过，零剩余差异。order53关闭时独立原资产golden三生成一致，SHA256为`a6e7c3adccb1cedd5c74296b305cba84da73dfb784906e0a28b6208974e46b1b`，Linux app Debug 14/14通过。order54 line attack固定为1043 bytes、248条指令、30个函数体跳转、69处重定位、5次direct call、唯一caller和外部共享尾；完整锁定低字方向0/1/2/3、signed range、越界继续、同方skip、空格/敌方effect1、敌方固定kind1 HP伤害及distance/damage写回。首轮发现机器`range=32767`因BX回绕不返回而现代有限执行，现仅安全拒绝该畸形值；15条真实line magic range均为1..7，四方向、越界后重新入界、友军/两敌人继续、HP0 hidden1、非法/zero/max range及双击复用首轮方向/缓存范围回归通过。修正后从入口重审248条指令及全部出口零剩余合法域差异；最新独立golden三生成逐字节一致SHA256为`e7c4b24495a6ddab76b449e11473d31dc982705a5bf9d37774a20fc17b9ea276`，Linux app Debug 14/14通过。order55 magic selection menu固定为988 bytes、259条指令、52个函数体跳转、45处重定位、8次direct call、唯一caller和本地RET；完整锁定signed十槽availability、ordinal cursor、普通名称稀疏漏画、选中名称十槽解析、Big5布局、右→左→三确认→Escape优先级与每次输入前重画/present。首轮发现Session入口及方向后可跳过必经present，现以一次presentation门修正；零available异常域安全拒绝。修正后从入口重审全部指令、分支、call、caller及出口零剩余合法域差异；最新独立golden三生成逐字节一致SHA256为`4c923e05b6739b8dcc097d1183517ca7f0ef6fcff58118cdde8dd20226f2141c`，Linux app Debug 14/14通过。order56 HP damage kernel固定为1124 bytes、283条指令、27个跳转、56处重定位、5次direct call、9个caller、无本地RET并跳7-byte外部共享尾；完整锁定knowledge过滤、unsigned熟练度与MP affordability scale、主/fallback公式及RNG顺序、signed/unsigned距离缩放、counter/HP strict underkill/hurt/poison写回与16位阈值。首轮完整对照零产品差异；新增scale9/scale1、knowledge排除、负fallback跳过、hurt/poison阈值、counter回绕和非法域回归后入口重审仍零差异，最终归类`platform_adapted / converged_no_new_differences`。最新独立golden三生成逐字节一致SHA256为`ec9ff1431f453f5fa8d2a1ec73ee87dd8c5febe63fe71a6d1d12957a4cd0c86a`，Linux app Debug 14/14通过。order57 MP damage kernel固定为394 bytes、96条指令、3个条件跳转、22处重定位、6次direct call、唯一caller和本地RET；完整锁定unsigned熟练度/100、五次RNG调用及第三次bound<=1时不推进、actor current/max MP的16位写回和signed夹取、target两次16位扣减与signed <=0清零、signed32旧减新返回及caller低16位保存。首轮完整对照零产品差异；新增add_mp0/3/4、负add_mp、maximum999、current/max/target回绕、exact/under zero、seed2和共享role别名回归后入口重审仍零差异，最终归类`platform_adapted / converged_no_new_differences`。最新独立golden三生成逐字节一致SHA256为`fb4d4ec7a49114ca5361904e3f5fff1bff2f32371ba675b85f22709184f1a636`，Linux app Debug 14/14通过。order58 poison-range wrapper固定为111 bytes、36条指令、1个条件分支、2处重定位、3次direct call、唯一caller和两个本地RET；完整锁定signed actor→role→use_poison查找、向零`/15+1`射程、mode1 selector四参数、local低字恰1取消返回-1、其余值调用用毒动作并返回0，以及caller忽略EAX后检查action-done。首轮完整对照零产品差异；新增signed极值、负除法、14/15与89/90阶梯和非法actor/role回归后入口重审仍零差异，最终归类`platform_adapted / converged_no_new_differences`。最新独立golden三生成逐字节一致SHA256为`30863e17bdd3fa340acfb74b55ea425a33a0c1e7b3146c25ec8c928371a43b71`，Linux app Debug 14/14通过。order59 poison action handler固定为608 bytes、148条指令、16个条件分支、6个无条件跳转、42处重定位、7次direct call、2个正常caller和1个本地RET；完整锁定同格保向、strict abs方向与水平tie、4096格effect清理、空/友/敌分派、毒值返回低字、固定effect30/kind2动画、全combatant sprite刷新、action_done及counter/体力16位回绕提交。机器upper-y边界重复检查x且空格短暂负索引读取side，现代safe reject归类平台适配；首轮合法域零产品差异，方向、边界、负damage、sprite、回绕和非法actor回归补齐后入口重审仍零差异。Golden三生成一致SHA256为`45fd92b983c702d907797f6d45d70b47287033b2c486ab5e8545a64ae4a9619e`，向量SHA256为`30c5af17b805884d0d3f1588046fac237ae8f7403d8c12999ac1f8da8be19243`，Linux app Debug 14/14通过。order60 poison value kernel固定为218 bytes、52条指令、5个signed条件分支、9处重定位、唯一caller和1次栈探测call；完整锁定signed毒术减抗毒除4向零截断、初始0/99夹值、strict容量限制、目标poison 16位写回与末次高低夹值，以及现存poison大于99时可返回负amount而非实际delta。首轮完整对照零产品差异；signed极值、poison 99/100/32767/-32768/-1与same-role alias回归通过，非法slot/role安全拒绝归类平台适配。Golden三生成一致SHA256为`96406b67eecdda8e638ba945800dd15eab2ef4819a79c388dc756fe7ec38516a`，向量SHA256为`fec49732797f1f546f4e3cbcf234d6640e6c4d04a1edb4938d8ea8699e995477`，Linux app Debug 14/14通过。order61 detox-range wrapper固定为111 bytes、36条指令、3个基本块、1个条件分支、2处重定位、3次direct call、唯一caller和两个本地RET；完整锁定signed actor→role→detoxification查找、向零`/15+1`射程、mode1 selector四参数、local低字恰1取消返回-1、其余值调用解毒动作并返回0，以及caller忽略EAX后检查action-done。完整入口对照零产品差异；新增signed极值、负除法、14/15与89/90阶梯和非法actor/role回归通过，最终归类`platform_adapted / converged_no_new_differences`。最新独立Golden三生成逐字节一致SHA256为`86bf13f29fa1f1cb6f0a26c85010d73a65757390026d1a89da5401fed6050ff3`，向量SHA256为`e91e030c5e87ada68a90af4c50f221dc8f73c2579a6531bcf97e3f0933bb0d0f`。order62 detox action handler固定为533 bytes、132条指令、15个条件分支、7个无条件跳转、35处重定位、7次direct call、2个入口xref且无本地RET；完整锁定同格保向、strict abs方向与横向tie、4096格effect清理、空/友/敌分派、解毒值返回低字、固定effect36/kind3/suppress1动画、全combatant sprite刷新及尾跳外部`0x399FE`共享行动提交。机器重复检查x上界而漏掉y上界，并在空格短暂负槽读取side；现代安全拒绝或避读归类平台适配。首轮合法域完整对照零产品差异；同格/四方向/tie、x/y边界含y64、非法actor/occupancy、无kernel路径不消费RNG、sprite及counter/体力回绕回归通过。最新独立Golden三生成逐字节一致SHA256为`1d77356fefa8e722b6682fa129f9515cb14caea72accece5a0dc2b3405632afe`，向量SHA256为`1a876ce56e8bd4e7d96e3c66b147165a5227518aebbc56a29a6e136c8f143cd9`。order63 detox value kernel固定为229 bytes、54条指令、13个基本块、6个signed条件分支、10处重定位、3次direct call、2个入口xref和1个本地RET；完整锁定signed detoxification/3、两次无条件bounded10顺序、BX 0..99夹值、strict poison阈值、signed target cap、poison word减法、负值/strict大于100末级夹值及sign-extended返回。完整合法域对照零产品差异；strict门槛、seed2 target cap、signed极值、负poison返回、poison100/101/32767、same-role alias和非法索引不消费RNG回归通过，非法slot/role安全拒绝归类平台适配。最新独立Golden三生成逐字节一致SHA256为`4b5f54db596da762efb3a2f763f358b0f6c2e135ee1b97ffbbdc6a664fe08ea3`，向量SHA256为`d7abe430906b406543b643d76ba19ea840790d7bbff7d20d4742708e99efc3ea`。order64 medicine target wrapper固定为111 bytes、36条指令、3个基本块、1个条件分支、2处重定位、3次direct call、唯一caller和2个本地RET；完整锁定signed medicine/15向零截断加1、mode1 selector四参数、local低字恰1取消返回-1、其他值调用医疗动作并返回0，以及caller忽略EAX后检查action-done。完整入口对照零产品差异；新增signed极值、负除法、14/15与89/90阶梯和非法actor/role回归通过，非法索引安全拒绝及宿主continuation归类平台适配。最新独立Golden三生成逐字节一致SHA256为`ef5c52444a0c3a0cd6643d8ffd4a42fa0955f53e4f48fee47e76b47b6721429a`，向量SHA256为`3ebada112102eafaa27d00f9fc365f7dc2c4cb185609a0f764c158480149d039`。order65 medicine action handler固定为533 bytes、132条指令、28个IDA flow block、15个条件分支、7个无条件跳转、35处重定位、7次direct call、两个入口xref且无本地RET；完整锁定同格保向与strict abs方向/tie、4,096格effect清理、空/友/敌分派、医疗返回低字、固定effect0/kind4/suppress1动画、全slot sprite刷新及外部共享行动提交尾。完整合法域对照零产品差异；边界重复检查x、空格短暂负槽读取side及非法actor/occupancy/role由现代安全拒绝或避读并归类平台适配，同格/四方向/tie、边界含y64、RNG不消费、sprite和counter/体力回绕回归通过。最新独立Golden三生成逐字节一致SHA256为`ebfc177795f318f87b3e6535a9325a131b48348376e82b1ccafd9edaab37caaa`，向量SHA256为`1958639fc75bb68858498f3eeaf860f0589ed6fd170095ba86a59141e7329b9a`。order66 medicine value kernel固定为400 bytes、103条指令、25个基本块、12个条件分支、3个无条件跳转、20处重定位、2次direct call、两个入口xref和本地RET；完整锁定signed体力<50早退、负medicine local夹值、四档hurt比例、一次bounded5、strict原medicine门槛、signed HP cap、HP/hurt低字回绕写、体力减2与完整signed32返回。合法域完整对照零产品差异；体力49/50、全部hurt分界、负medicine、signed极值、负治疗与EAX=-65535/AX=1、same-role alias及非法role RNG不推进回归通过，非法role/slot提前拒绝归类平台适配。最新独立Golden三生成逐字节一致SHA256为`0d3a179c201f2d65dd29181e2db715c818b57be1f05e3724cf1d338745d08067`，向量SHA256为`f758d1f4dae67a61868a007a13336a7c94e1c11ed21332904c6f7b26b5b1f472`。order67 player item selection wrapper固定为111 bytes、33条指令、5个基本块、2个条件分支、2处重定位、5次direct call、唯一caller和本地RET；完整锁定filter4按slot列type3/type4且忽略数量、初始5×3网格present、signed actor role selector、AX低字4委托暗器目标、低字1写action-done及caller忽略EAX。首轮发现现代可在初始或导航frame present前读取同批下一键，现以物品presentation门修正；首帧早键、Escape不完成、重入和每次导航present回归通过，修正后入口重审零剩余合法域差异。最新独立Golden三生成逐字节一致SHA256为`bbafb835c53f40faaa872d1052ceff7b63930ac2949420e43723003af41ceccd`，向量SHA256为`9657c3c342df26368b3e944dcff5923946ea64026a02bdab8fb0d61ceea270a5`。order68 throwing weapon action固定为1433 bytes、340条指令、62个基本块、31个条件分支、11个无条件跳转、85处重定位、13次direct call、唯一caller和本地RET；完整锁定signed射程、水平tie含同格方向2、空/友/敌effect分派、EFT后重读payload、四档hurt、随机基数低字回绕、HP/hurt/poison写回及damage后库存/action/sprite提交。首轮修正同格保向、缺失随机基数int16回绕及玩家状态早于EFT提交三项差异；修正后入口重审零剩余合法域差异。最新独立Golden三生成一致SHA256为`cc94651bde1b784f51f2b9173cf1b8e0c0ec07330cba4c674734878a11614c8a`，向量SHA256为`4de5134d6363b727de38a09d0b2cc7cc335dd0cf70831c0e9d498e6a663decd1`。order69 rest actor action固定为371 bytes、83条指令、11个基本块、5个条件分支、1个无条件跳转、24处重定位、5次direct call、两个caller和唯一RET；完整锁定action-done先写、signed speed/10与round相等分支、一次必经bounded(3)、体力低字回绕及strict上界100、signed阈值30、共享bound的HP后MP RNG顺序、bound1两次调用不消费状态、HP/MP低字回绕与strict maximum。首轮完整入口对照零产品差异；negative speed、体力/HP/MP极值回绕、共享role及非法索引回归通过。最新独立Golden三生成一致SHA256为`2fe0a9e0eb676fa9df0bad9c735df4935caeeca90e039d117bb22b7a8cf20a23`，向量SHA256为`ae98253e95a49d58796c472bda36e0a8d4d0b3f74965034e74a5cff4448c8ac8`。order70 defer turn to end固定为52 bytes、20条指令、4个基本块、1个条件分支、1个无条件跳转、1处重定位、2次direct call、唯一caller和唯一RET；完整锁定signed低字slot/count循环、严格相邻交换到尾、已在尾slot零交换原样返回，以及caller忽略EAX后检查原slot并在等待重绘后同索引续行。首轮完整入口对照零产品差异；首/中/尾/单参战者、非法slot状态不变和Session不消费RNG回归通过。最新独立Golden三生成一致SHA256为`4ca46b6fd020bb7b537956d6646541e51cbc60e526b752ba12d804f538718d4c`，向量SHA256为`c12dfc863630909d2a1b7b6edf08e5f688a525d7cddf061cf8c882612d5f5eab`。order71 enable automatic mode固定为58 bytes、13条指令、单一基本块、0分支、3处重定位、4次direct call、唯一caller和唯一RET；完整锁定战场render、flag0 present、flag1写入、same-actor AI入口及caller忽略透传EAX。首轮完整入口对照零产品差异；present前advance、render后回调前flag/RNG/actor不变、present后flag1但AI未消费RNG与同actor prelude回归通过。最新独立Golden三生成一致SHA256为`eea9c95ac51769a9ab16c3aea8867a3319cdc8a7330dad4fed01954c95b94174`，向量SHA256为`4a37e849a035425b8ae019458b0f2edcebce0cddeb6115136726ab022c14c20e`。order72 two-pass battle renderer固定为1889 bytes、527条指令、58个基本块、40个分支、116处重定位、19次direct call、26个call site和唯一RET；锁定x外/y内两pass、path/主副cursor、object、combatant/highlight、effect与五类damage顺序。首轮发现并修正renderer入口错误清屏、damage遗漏`%3d`宽度及battle sprite错误拒绝奇数ID三项合法域差异；真实battle13连续视角四个旧像素保留、battle4整帧/status、alternate/zero-range、负HP/非法mode、kind1..6和偶奇同帧回归通过。修正后从入口重审全部527条指令零剩余差异；Golden三生成一致SHA256为`8dafbc86668a6bc17180df43950b57c0c3f53a4a0dab9c451b46c2e86f1e5efb`，order72独立关闭。order73 combatant sprite lookup固定为82 bytes、20条指令、单块零分支、5处重定位、1次栈探测call、13个call site与9个caller owner；锁定signed低字公式、常量0/5106及全部caller写AX至word8。首轮发现空槽固定5098丢失role=-1对Ranger header inventory slot155 item ID的合法别名，现按动态值计算并从受影响`sub_31EB9`入口重审零剩余差异；四套stock资产、合法值7和signed极值回归通过，Golden三生成一致SHA256为`03ed0086cd490bc790a1e26e342207c2f7aa6ae9dc725c46e3c62a78dcb0fde2`，order73独立关闭。order74 battle outcome gate固定为335 bytes、84条指令、23块、14分支、23处重定位、8次direct call、唯一caller与唯一RET；锁定signed HP死亡隐藏、任意非零side、双方同时为空时victory覆盖、Big5结果框、present后清旧last-key等待新非零键、结算后caller清理/轮末状态/tick等待顺序。首轮合法域零产品差异，negative HP/side、already-hidden death、both-dead和present/input回归通过；Golden三生成一致SHA256为`49f87783bb154a386bced732f9c4bb61a62ee89582246180cae616d15eefcec9`且历史67键不变，order74独立关闭。order75 battle settlement固定为823 bytes、194条指令、44块、29分支、61处重定位、10次direct call、唯一caller与唯一RET；锁定exact side1敌方恢复、所有非side1存活者进入signed均分分母、仅side0存活者领取共享经验、side0 signed HP/体力下限、全slot经验提交、负reward的32位左移后unsigned除10、三个word先low16回绕后unsigned 60000封顶，以及经验消息后等级/练功/制造调用顺序。首轮发现runtime将raw get-exp以`!=0`压成bool，现最小修正为机器的`==1`并从入口重审全部194条指令零剩余合法word域差异；side=-1分母、negative reward、回绕/封顶及synthetic get-exp9回归通过。Golden三生成一致SHA256为`a343bfe7d579a8b5de486a63d5654ea3851ade75ea19f0826b94bf7313179c41`且历史68键不变，Linux app Debug 14/14通过，order75独立关闭。order76 battle level up固定为967 bytes、201条指令、54块、32分支、68处重定位、16次direct call、唯一caller且无本地RET；锁定30项unsigned经验阈值全扫描、提示先于RNG/字段提交、signed IQ五档、3..9次RNG顺序、六项技能严格`>20`门、暗器无条件消费及word回绕后signed 999/100封顶。完整入口对照未发现合法caller域产品差异；非法索引/等级安全拒绝和宿主preview/present/input/commit分相归类平台适配。阈值、IQ八边界、技能20/21、固定8次/完整9次RNG、负生命成长、回绕/封顶、抑制参数和延迟提交回归通过；独立向量SHA256为`9c35ab92bd1521ce8237f95a2e9a21ba355bb9ba2ec472dc5576d0c44380d1fd`，Golden三生成一致SHA256为`d08d6315e3f10e808d5b8ece0a06b45b1896cc7baf16c314616f885cd8ce9d17`且历史69键不变，Linux app Debug 14/14通过，order76独立关闭。order77 battle practice固定为2087 bytes、415条指令、94块、51分支、143处重定位、15次direct call、唯一caller且无本地RET；锁定signed IQ需求系数、首匹配槽需求、两次32位回绕乘法、成功提示先于字段提交、word回绕后signed字段边界、全十槽武功提交、每槽提示确认时序及空槽只写ID保留等级。首轮修正现代只更新首个同ID槽与signed需求乘法溢出两项差异，并让Session按提示逐槽延迟提交；修正后入口重审零新增合法域差异。重复槽、898/899、满级阻止插槽、空槽旧等级、magic-id0、需求回绕和Session两提示回归通过；独立向量SHA256为`9570d8c53815699a00bd40c42f8a3bd77bf5432adadf93762be56b952138d016`，Golden三生成一致SHA256为`1d071ae2dcb673d02e0079ae35dcecb3633f053ddd0fa60bd653be15a2c4f31d`且历史70键不变，Linux app Debug 14/14通过，order77独立关闭。order78 battle crafting固定为695 bytes、171条指令、33块、22分支、35处重定位、12次direct call、唯一caller且无本地RET；锁定signed IQ制造需求、unsigned经验、首材料槽、五配方signed资格、拒绝采样RNG、提示先于库存、已有产物`bounded(3)+1`、新槽旧count+1、材料删除及满库存提示后零提交。完整入口对照未发现合法caller域产品差异；首材料槽、抑制RNG、新槽旧count、满库存、删除左移、negative requirement/signed材料、数量回绕和同槽别名回归通过。独立向量SHA256为`8a3918fd77b1c109714d70b3b0c2a51eb8e01c8ae3d22560f3221beaa698c50f`，Golden三生成一致SHA256为`c9963790de069ce61cd899976114187d608561644e75e70b7ab8ae2dc65736b4`且历史71键不变，Linux app Debug 14/14通过，order78独立关闭。order79 round status damage固定为271 bytes、58条指令、13块、9分支、19处重定位、1次栈探测call、唯一caller和本地RET；锁定signed hurt绕过poison-path门、poison存活/体力/可见资格、先hurt/20后poison/10的两次HP word写回、严格负值夹1及重复role逐槽累积。完整入口对照未发现合法caller域产品差异；严格零、负商、两次回绕、非法role和正常/战果Session轮末时序回归通过。独立向量SHA256为`fda21d81b051395e7b2cac5d012d0181658ca5aaf4588d6aebf337861df37eda`，Golden三生成一致SHA256为`158e6c36bc46f9f1052934502b47a13272112310b345e360b87bd613489418c4`且历史72键不变，Linux app Debug 14/14通过，order79独立关闭。order80 hidden target cleanup固定为97 bytes、24条指令、8块、4跳转、7处重定位、1次栈探测call、唯一caller和本地RET；锁定活动源槽升序、攻击目标先于用毒目标、0..25固定目标槽含inactive槽、hidden严格等于1才清-1及inactive源槽不扫描。合法caller域完整入口对照零产品差异；无效目标安全保留且不执行原数组外读取归类平台适配。多源、同目标/自目标、hidden 0/1/2/负值、slot0/25、inactive target/source、无效role、越界target及hidden-skip/outcome Session回归通过。独立向量SHA256为`f596fa71372d63877c7acbd25eb4afce8a877d59b327144858399669ea06e084`，Golden三生成一致SHA256为`2c2cd200fd7a5558f11b8bc515f80c3b69aef2d3fffacedf4351d05292ad3734`且历史73键不变，Linux app Debug 14/14通过，order80独立关闭。order81 status panel固定为1296 bytes、338条指令、39块、31分支、87处重定位、22次direct call、三个入口xref及12条外部共享尾入口；锁定side恰零/非零布局、名称scratch byte1..8首NUL、signed `%3d`、hurt/poison阈值、非法MP type复用poison颜色、两个玩家完整重绘/缓存轮询和AI prelude/present/300等待时序。完整入口对照零合法域产品差异，左右独立面板FNV64为`0x87ff9c43d54b7bca`/`0x0f803235628e69e5`；向量SHA256为`f59c9af65abcb5cbe93ffe902aae397b70449608b3390f7220c3e0f3224cf6b0`，Golden三生成一致SHA256为`07de27463eb6b7639caf4e9256bff9b93f4b9de8093aa4a007c86313cd6dd199`且历史74键不变，Linux/Windows模块关闭八项矩阵通过，order81独立关闭，B8 81/81完成。

验收：92 个 FIGHT 包、所有当前可达战斗建立、行动、AI、伤害、状态、物品、胜负和逃跑分支闭环；整数公式和 RNG 消费按汇编验证，不以一场战斗可运行代替完成。

### B9 · 完整持久化与兼容整合

范围：全模块

当前状态：`B9-WP01`为唯一work package；14项有限closure均已映射为`implemented_pending_review`，实现映射口径100%、统一最终REVIEW口径0%。已修正新游戏scene70序章入口、初始最大生命漏乘3、编号槽I/O等待帧present门禁、系统菜单载入后的返回页和每进程独立日志；14项实现差异审计已覆盖，Linux/Windows八项矩阵、Linux app ASan+UBSan、双平台smoke、golden与原版资产只读门禁均已通过。提交B9切片后处理其余closure，再进入B0→B9统一最终反复汇编→C++ REVIEW。

交付：

- 所有状态所有者的 snapshot/import；
- 新游戏、世界、场景、战斗、菜单、保存、读取的集成链；
- 原存档兼容、错误路径、退出与资源清理；
- 每次启动独立的`PREFIX-YYYY-MM-DD_HH-MM-SS-{PID}.log`，不得追加到跨启动共享日志；
- 可发布构建和兼容性报告。

验收：启动到退出的全部当前资产可达主流程均有固定输入差分；状态、调用序列、RNG、存档字节、indexed framebuffer、调色板和音频命令序列与原版一致，偏差清单为空或每项均获用户明确批准。

## 6. 单模块开始条件

进入某模块前必须同时满足：

- 公共职责和依赖已在 `rewrite-architecture.md` 中存在；
- 已机械列出该模块的游戏自有函数、入口、调用点、数据、共享状态和当前资产可达性；
- 已区分游戏代码、CRT/编译器和第三方库合同；
- 至少准备汇编派生向量、真实资产/记录样本和差分策略；
- 明确核心 1:1 行为和纯宿主平台适配边界；
- 不需要修改上游模块依赖方向。

## 7. 单模块完成条件

- 模块函数/状态清单中的 `unresolved`、`hypothesis_only` 和未审计可达分支均为零；
- 每个游戏自有函数均已归属为 `assembly_exact` 或有证据的 `platform_adapted`；
- 公共接口只改变代码所有权，不改变原调用时点和副作用顺序；
- 单元测试覆盖正常、边界、错误、原 BUG、溢出和提前退出路径；
- 当前原版资产全量测试通过；
- 像素、字节、状态、RNG 和调用序列有 golden 或原程序差分证据；
- 已按第 0.1 节执行不限次数、每轮均从入口独立推导的汇编→C++逐基本块 REVIEW，最后一轮完整复核零新增差异；
- 实现按基本块复核，研究结论、实现和测试三者一致；
- 没有跨模块裸全局、SDL 类型泄漏、反向依赖或未批准行为改良；
- 剩余的不可达库内部细节有证明性排除，不能用 TODO 代替可达逻辑。

## 8. 集成里程碑

以下只是可运行检查点，不是模块完成或 1:1 完成标准：

1. **M0**：现代窗口和空 indexed framebuffer；
2. **M1**：真实精灵、调色板和字体可呈现；
3. **M2**：标题、新游戏和读档可用；
4. **M3**：世界地图可探索；
5. **M4**：场景进入、对话和返回闭环；
6. **M5**：一个真实战斗闭环；
7. **M6**：保存/读取后状态和画面一致；
8. **M7**：全部当前资产可达路径完成原程序差分，才进入最终 1:1 评审。

## 9. 全项目 1:1 完成条件

- `Z.COM` 启动链和 `Z.DAT` 全部游戏自有函数均已归属，当前资产可达函数和分支全部闭环；
- 约 256 个 Watcom/CRT/Miles 函数均已证明为无关、不可达内部细节或被精确平台合同覆盖；
- 118 对 IDX/GRP、110 对 SDX/WDX、五个世界层、100 个场景、2,977 条 TALK、92 个 FIGHT 包和全部存档文件通过全量验证；
- 固定输入流程的状态、RNG、像素、调色板、保存字节和音频命令序列与原程序一致；
- 所有原逻辑 BUG 和异常路径已保留，所有平台偏差均有用户批准；
- Linux 与 Windows 构建、单元、真实资产、集成和原程序差分门禁全部通过；
- 所有游戏自有函数、事件 opcode 和平台调用链均按第 0.1 节完成最后一轮零新增差异的汇编→C++ REVIEW；
- 不存在以“可玩”“测试通过”或“现代实现更合理”为理由保留的未验证兼容缺口。

## 10. 计划维护规则

- `goal/execution-plan.md` 是阶段和当前队列真值；研究摘要不能覆盖它。
- 主 GOAL 必须持续保持干净、简洁；状态变化时替换旧状态，不得追加工作流水账、阶段完成日志或已由 evidence、inventory 和 Git 历史保存的过程记录。
- 架构文档只在模块边界或依赖被新证据推翻时修改。
- 详细地址、伪码和字段证据放在 `research/evidence`，不把 GOAL 变成无限变更日志。
- 当前模块只保留一个明确工作包；完成后再开启下一项。
- 测试通过不能单独证明原版行为；必须同时有反汇编、原始字节或真实资产证据。

## 11. 当前唯一队列

1. 完善基础框架：function catalog、module/state/dependency inventory、模块 work package、closure 状态机、validator、日志与诊断；框架通过 CTest 前停止业务逆向扩展；
2. 把 B0–B7 现有 C++ 映射到 inventory，统一标 `implemented_pending_review` 或更早状态，不继承旧完成结论；
3. 在框架约束下完成 B7 剩余事件实现、B8 战斗和 B9 全集成；实现通过测试后仍只标 `implemented_pending_review`；
4. 全部功能实现后，按锁定 workpack 对 B0–B9 逐函数/handler 执行第 0.1 节反复汇编→C++ REVIEW；每次差异修正后废弃本轮结论并从入口重启，直到一轮完整复核零新增差异；
5. 最终执行 Linux/Windows 全矩阵、sanitizer、smoke、资产只读、IDA 数据库和原程序差分/登记阻塞验收；
6. 只有全部 closure 关闭并满足第 9 节全部条件后才调用 Goal 完成。
