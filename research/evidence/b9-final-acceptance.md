# B9 最终全集成验收

状态：`converged_no_new_differences`

本文件记录B0–B9统一最终REVIEW完成后的全集成门；早期模块矩阵不替代本轮结果。

## 1. Linux/Windows BUILD矩阵

所有配置、编译和CTest均只经根目录`./build.sh`或`build.bat`进入，未直接调用底层构建工具，未传入并发参数。

| platform | target/config | result | managed process |
| --- | --- | --- | --- |
| Linux | core Debug | 13/13 | `proc_5b7e` |
| Linux | core Release | 13/13 | `proc_5b7e` |
| Linux | app Debug | 14/14，含SDL smoke | `proc_5b7e` |
| Linux | app Release | 14/14，含SDL smoke | `proc_5b7e` |
| Linux | app Debug ASan+UBSan | 14/14，含SDL smoke，无sanitizer finding | `proc_5b7e` |
| Windows | core Debug | 13/13 | `proc_a54f` |
| Windows | core Release | 13/13 | `proc_a54f` |
| Windows | app Debug | 14/14，含SDL smoke | `proc_a54f` |
| Windows | app Release | 14/14，含SDL smoke | `proc_546f` |

Windows app Release第一次执行时，新增的正式7表validator发现4个battle closure仍使用旧`converged_after_fix`标签；产品测试与smoke本身已通过。将这4行规范化为`converged_no_new_differences`并让正式validator覆盖全部349行后，仅重跑受影响配置，最终14/14。该失败及修复只涉及证据状态机，不改变C++行为。

Linux Debug/Release/ASan三次smoke分别产生不同PID日志，如`...-610789.log`、`...-611014.log`、`...-611518.log`；Windows Debug/Release分别产生`...-34348.log`、`...-17092.log`/重跑`...-21620.log`。命名均符合`openlegend-YYYY-MM-DD_HH-MM-SS-{PID}.log`，无跨进程共享追加。

## 2. Golden与全量资产

受管进程`proc_6e00`对五份独立Python oracle各生成两次，先逐字节比较两份临时输出，再把临时输出反向比较正式tracked文件；全部一致：

| golden | SHA256 |
| --- | --- |
| `title-menu-new-game-goldens.json` | `a4708f93c6793c653e10adf3a0e2c3f24555371ed1c755fa652532e7f673ba54` |
| `world-map-goldens.json` | `53f2238ac967ac538e6b466a44a25b46c90facf2db8e6a5632e07c7535e8bd8e` |
| `scene-goldens.json` | `41258dd5f705488da5580b141d83e90f60034123913a9ef02f67a889d30456e8` |
| `battle-goldens.json` | `f7ae2c8969587a5e5f6e34af9c6d73b4f910149d28493b86c2f0bf0b9ed66b4e` |
| `battle-player-status-golden.json` | `d6d9c58f61afba15cb327da007c6e4cbd1ddc00744897aff70226fe5a8144416` |

最终资产分母独立核对为：

- 普通累计索引包：118对IDX/GRP；
- sentinel包：84对SDX/SMP加26对WDX/WMP，共110对；
- 世界数据：`EARTH.002`、`SURFACE.002`、`BUILDING.002`、`BUILDX.002`、`BUILDY.002`五层；
- 场景：`S1.IDX`严格100个记录；
- 对话：`TALK.IDX`严格2,977项；
- 战斗：140条`WAR.STA`记录、26个`WARFLD`项、92个FIGHT包/4,992帧。

受管进程`proc_42bd`只读复核五个关键原始文件SHA256，均未改变：

| asset | SHA256 |
| --- | --- |
| `Z.COM` | `9de4c8002f92759bd2771a4984ff02599121e6c66fed1eb0d7e905a327e2dac1` |
| `Z.DAT` | `0034f836def2b287e62b0902f08cab22b61b397f24d44d6038274f1750a007ce` |
| `WAR.STA` | `98e3f66912c5ba4a0be00aaeff3462eb8c99f4d591d92a754930070dde9649b6` |
| `WARFLD.IDX` | `b5fdbb1775206c1de72bc6f5e7e9b45cfa126f2a2cd0d1a4384f4a00801ee7be` |
| `WARFLD.GRP` | `58cc936f758bd2ca23b0b6e818e14adf76845e822f03a9910c01b9987215c9bf` |

## 3. IDA、catalog与closure门

- fresh IDA资产门：26个closure外基础helper、最后3个共享游戏函数及282个自动识别库函数全部与冻结机器身份一致；无xref的`0x41748`由专项报告恢复109 bytes/28 instructions并以AIL字符串与实现边界确认为第192个Miles函数。
- 正式catalog：577行，`game_logic=294`、`library_or_platform=283`；`assembly_exact=12`、`platform_adapted=282`、`external_boundary=283`；`external_crt=91`、`external_miles=192`。
- 正式closure：7表349行，对应284个物理函数；全部`converged_no_new_differences`，pending/unverified为0。
- `build_reverse_inventory.py`修正为：终态closure不得重新生成待办、空末列不得产生行尾tab、已有人工作名优先于旧report标签。重建后六份report-driven closure保持既有人工语义名。
- 受管进程`proc_b8ed`最终输出：`reverse framework valid: 577 catalog/ownership rows, 349 closure rows/284 physical functions`，并机械锁定closure来源`game_logic=262`、`library_or_platform=22`；七表均为`0 pending or unverified`。

逐函数分类和库边界证明见`research/evidence/function-catalog-coverage.md`。

## 4. 原程序动态差分边界

当前环境仍没有可执行的原DOS程序运行oracle；该项继续登记为`blocked_runtime_oracle`。独立Python原资产oracle、fresh IDA机器合同、现代CTest和SDL smoke不伪称为原程序动态输出。当前所有可静态证明、可由原资产独立派生或可在现代宿主执行的门均已通过；没有未登记的产品差异。
