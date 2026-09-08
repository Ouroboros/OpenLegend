# 577-function catalog 最终覆盖证明

状态：`converged_no_new_differences`

真值仅来自当前只读原始资产、headless IDA 机器输出、owner-specific closure、现代 C++ 与测试。`function-catalog.tsv` 只提供物理函数集合；最终逐行分类写在 `module-function-ownership.tsv`，本文件说明每类分类的机器依据和覆盖边界。

## 1. 全集与最终计数

- 原始 `Z.DAT` SHA256：`0034f836def2b287e62b0902f08cab22b61b397f24d44d6038274f1750a007ce`。
- 原始 `Z.COM` SHA256：`9de4c8002f92759bd2771a4984ff02599121e6c66fed1eb0d7e905a327e2dac1`。
- catalog 共 577 个物理函数：`Z.COM=3`、`Z.DAT=574`。
- 游戏自有/宿主适配代码共 294 个：12 个 `assembly_exact`、282 个 `platform_adapted`。
- Watcom/CRT/Miles 外部链接代码共 283 个，全部为 `external_boundary`：`external_crt=91`、`external_miles=192`。
- `unresolved`、`pending_assignment`、`manual_reviewed`、`mechanical_catalog_only` 与非 high-confidence 行均为 0。
- 七份 closure 共 349 个 owner 行，对应 284 个物理函数；其中 51 个物理地址跨模块出现，但每个 closure owner 仍独立关闭，不从主模块、callee、caller或同址其他owner传播状态。
- 284 个 closure-backed 物理函数中，262 个位于游戏代码域；其物理终态为12个 `assembly_exact` 与250个 `platform_adapted`。另22个位于外部库域，owner-specific closure只证明游戏依赖合同；物理函数行按外部链接边界归为 `external_boundary`。
- closure外的32个游戏函数由第2、3节独立覆盖；其合法域机器合同逐项对应现代实现，DOS硬件、宿主呈现、文件系统、本地日期及非法索引安全拒绝均明确归入 `platform_adapted`。

最终12个 `assembly_exact` 游戏地址为：`0x2E29C`、`0x2E2D7`、`0x2E2F5`、`0x2E46B`、`0x2E571`、`0x2F34C`、`0x2F39C`、`0x2F9B5`、`0x2FEDF`、`0x2FF87`、`0x30B45`、`0x3D612`。其余282个游戏函数均有明确宿主或安全边界，归为 `platform_adapted`。

模块逐行计数：

| module | functions |
| --- | ---: |
| `app` | 12 |
| `audio` | 13 |
| `battle` | 82 |
| `external_crt` | 91 |
| `external_miles` | 192 |
| `input` | 1 |
| `model` | 4 |
| `persistence` | 6 |
| `random` | 2 |
| `render` | 34 |
| `resource` | 8 |
| `scene` | 83 |
| `time` | 2 |
| `ui` | 22 |
| `world` | 25 |

总和严格为577。

## 2. `Z.COM` 启动链

`Z.COM` 是独立DOS启动器，不是 `Z.DAT` 的CRT。正式IDA报告与完整反汇编固定三个函数：

| function | bytes | instructions | blocks | raw SHA256 | owner/status | machine contract |
| --- | ---: | ---: | ---: | --- | --- | --- |
| `start 0x10100..0x1015F` | 95 | 20 | 2 | `31199d9d040a2c2f7c8e0be0715af186bd5fe928f43addac5070b109e8ea9597` | `app/platform_adapted` | 缩减DOS内存块，执行 `LOGO.SCR`，切换文字模式并打印提示，等待后执行 `z.DAT`，最后DOS退出。 |
| `sub_1015F 0x1015F..0x101A7` | 72 | 20 | 1 | `00fd94b7205be9ccfec2b06a5d65a8b19f46d76a7917c2451b3377346a23d005` | `app/platform_adapted` | 构造DOS EXEC参数块，保存/恢复SS:SP并以 `INT 21h/4B00h` 启动子程序。 |
| `sub_101A7 0x101A7..0x101C4` | 29 | 12 | 4 | `0b77f516075f5370c9a6c635d9aa1342b209b4eab5bec1b279218272a4263213` | `app/platform_adapted` | 读取BIOS tick，等待54 tick或键盘已有按键。 |

现代程序把两次DOS EXEC、BIOS/DOS中断、文字模式和忙等合并为单一SDL进程的启动资源、呈现与输入生命周期；`LegacyStartupResources`、`LegacyGameRuntime`及SDL入口保留LOGO→游戏、资源失败、呈现成功后推进和退出清理顺序。该差异只属于宿主进程模型，不扩张游戏行为。

## 3. closure外的29个 `Z.DAT` 游戏函数

### 3.1 26个基础primitive/wrapper

fresh IDA以一个新临时数据库从只读 `Z.DAT` 重建以下26个函数；逐函数导出完整指令、CFG、caller、callee、字符串和loaded字节哈希。合法域逐块正向映射到 `IndexedFramebuffer`、RLE/字体/面板renderer、inventory模型、`DataRoot`/存档I/O与 `SceneSession` 日期；反向检查现代分支后未发现新差异。DOS模式、VGA端口、裸地址越界、文件句柄/路径和宿主本地日期均按 `platform_adapted` 隔离。

| range | bytes | instructions | blocks | loaded SHA256 | module | status |
| --- | ---: | ---: | ---: | --- | --- | --- |
| `0x20000..0x20016` | 22 | 16 | 2 | `12786e4f234399b33ed2e9ca5ee213e90e1cea6c4c1906590b3f9e04f87a71b2` | `render` | `platform_adapted` |
| `0x20016..0x20039` | 35 | 19 | 1 | `8d01c4d6e16294bf2fde375782089932e1bd08a472b0bf04a40aa302bdcf53fb` | `render` | `platform_adapted` |
| `0x20039..0x2005B` | 34 | 19 | 1 | `629284cf3204779c0d275f418ae165557b17f90ed9f52d5150ff88a18d9be934` | `render` | `platform_adapted` |
| `0x2005B..0x20087` | 44 | 23 | 1 | `cd789f17617bdd51caa408dbeb5044b3dcf0bc8e7c223c6c77a284ed0e534ea0` | `render` | `platform_adapted` |
| `0x20087..0x200BD` | 54 | 36 | 5 | `989b34e81fcd2356510c04681e0d7f4e39f17571c4576f224c4f1b0bd4257350` | `render` | `platform_adapted` |
| `0x2010A..0x20145` | 59 | 28 | 3 | `af107cc8d6bea26bddf1db44a9be31dc9a8c47e8abd280a4922fac405c795a7c` | `render` | `platform_adapted` |
| `0x20354..0x2050A` | 438 | 167 | 46 | `7e5924437cc7512637f94de28eac4369112eaebd2791f350c60c37333eb3d150` | `render` | `platform_adapted` |
| `0x2050A..0x20615` | 267 | 87 | 5 | `572a782a7df4dfb3f578b096c713befc326e60c8a629db22b0fd3dd2ecaf842b` | `render` | `platform_adapted` |
| `0x2094A..0x20B22` | 472 | 176 | 46 | `f9f9f0f27a4b5f26bba6701a6c292cb4c71d8a1ba4d0ba8b9234d8f9ad26f0f5` | `render` | `platform_adapted` |
| `0x20B22..0x20BB5` | 147 | 73 | 19 | `9a5e3b22e098455a16f119b53ecd16d2e0276e156f4f1c82879c54be253778dc` | `render` | `platform_adapted` |
| `0x2A74C..0x2A7E8` | 156 | 43 | 2 | `76fdcf45808a37d6e654d8e7a200e3f4ace9d2b3e7998208a2493e5973b765a7` | `ui` | `platform_adapted` |
| `0x2A7E8..0x2A86C` | 132 | 40 | 2 | `22fa50b9d377842a190a0d3f0b418a4cfb2dec75faf865d62e344a5b1f82479e` | `ui` | `platform_adapted` |
| `0x2B227..0x2B275` | 78 | 16 | 4 | `577a170d737a2d200d39a694428a1be793ec5f0386fc59764df7857c8c20e526` | `model` | `platform_adapted` |
| `0x3D1E5..0x3D27A` | 149 | 61 | 8 | `cf7672e4d3509da84960ef3da4c6f5daab909faa076a2747c43094d0ad277872` | `render` | `platform_adapted` |
| `0x3D68A..0x3D6D1` | 71 | 21 | 4 | `3b5acf5c8b2b2f8484f6af65a0b88be6a61764abfea9127813255f53c80d1094` | `render` | `platform_adapted` |
| `0x3D832..0x3D88A` | 88 | 30 | 4 | `634bcec80de147bb08378fef367d48d773c24bcafc51362ec1048d2dd2379fd0` | `render` | `platform_adapted` |
| `0x3D8AD..0x3D8D8` | 43 | 11 | 1 | `42bd13179931ef0b5ef4e7c268cc7eb901ad78a6d8b6f124b92a058aeaa6ce05` | `render` | `platform_adapted` |
| `0x3D8FF..0x3D922` | 35 | 10 | 1 | `ad8114766a52de3a5ab08de5a7f1bf7a01d45ca07776c7763fa10d976d66235c` | `render` | `platform_adapted` |
| `0x3D922..0x3D939` | 23 | 6 | 1 | `5b411f946320778b7aa2de3662368a1afe9e41d3c9b5a5b5c50dc5930b6dd6f5` | `render` | `platform_adapted` |
| `0x3DA40..0x3DA66` | 38 | 9 | 1 | `75bc2501a072923590be8c083bfbae4f2f72e12242d3a43d899585900d1b7a20` | `persistence` | `platform_adapted` |
| `0x3DA66..0x3DABF` | 89 | 23 | 3 | `9e0fd49bfdff7d446397bf78a6a0e0d8cc91dd4ac11074a9080daa43ceb00223` | `resource` | `platform_adapted` |
| `0x3DABF..0x3DAE3` | 36 | 9 | 1 | `345edc2339a03461b1a18f0efa79456e959c0e7a72a993ea7c02d562a57d4f88` | `resource` | `platform_adapted` |
| `0x3DAE3..0x3DB07` | 36 | 9 | 1 | `1ecb4874872946a2c112452246698acac2a5955d091d050b4b6523e51badf1f9` | `persistence` | `platform_adapted` |
| `0x3DB07..0x3DB2B` | 36 | 9 | 1 | `96f0b3c9d2a4301eb1644c381d6ef9cf7b754cae2dae144e0543999248923371` | `resource` | `platform_adapted` |
| `0x3DB2B..0x3DB47` | 28 | 7 | 1 | `56bfac9695724bf11139a31abc0630d8d806203fc1e1274f5862bd751eb79ad0` | `resource` | `platform_adapted` |
| `0x3DB47..0x3DB83` | 60 | 18 | 1 | `9dfa6dc01f6a8ea5f75bac55117cf1731d43a3daa9e5eaa1bc7e03e1566324fa` | `time` | `platform_adapted` |

关键独立边界：

- `0x20000`执行BIOS mode 3；`0x20016`绑定后备缓冲后执行mode 13h；现代SDL生命周期不伪造BIOS寄存器。
- `0x20039/0x2005B/0x20087`分别提交64,000 index字节、清完整VGA页、提交768个RGB6 DAC字节；现代只在host显示边界展开RGBA。
- `0x2010A/0x20354/0x2050A/0x2094A/0x20B22`的合法像素域由 `render-1to1.md` 的独立资产与像素门覆盖；裸地址越界和畸形高度改为安全拒绝。
- `0x2A74C/0x2A7E8`只组合固定箭头矩形；共享尾仍按各入口独立分类。
- `0x2B227`从指定slot逐项左移到199，并无条件把缓存word写为`-1,0`；现代额外拒绝越界slot。
- `0x3DA40`是创建/截断写打开；`0x3DA66`拼接数据根路径后以只读binary打开并在失败时回退原参数；`0x3DABF/0x3DAE3/0x3DB07/0x3DB2B`依次承接read/write/seek/close。现代文件对象、路径隔离和稳定错误传播只改变不安全DOS失败域。
- `0x3DB47`把DOS日期结构中的年、月、日写给三个输出；现代 `current_local_date()`保持同一业务字段，宿主时区/系统调用属于平台边界。

### 3.2 三个较大共享游戏函数

第二个全新IDA数据库从同一只读 `Z.DAT` 独立导出三函数，未继承caller closure状态：

| range | bytes | instructions | blocks | branches | calls | callers | loaded SHA256 | status |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `0x2B483..0x2BD8B` | 2312 | 474 | 85 | 47 | 21 | 4 | `275f0bc18a222c7e03976826e89183474b53b6ba28bab1be2fb7f908c26a8e76` | `platform_adapted` |
| `0x2BD8B..0x2C0BB` | 816 | 163 | 45 | 26 | 2 | 2 | `a2731bbad8e4a63dbcc968315d0666dc6a6918688e2d53604d6dcc45274159a1` | `platform_adapted` |
| `0x3CBE3..0x3CC97` | 180 | 59 | 13 | 6 | 2 | 3 | `d1f0d4ccab020338589a26ae9ce571dfbb7cc637d53052e2dd25f2058296e4eb` | `platform_adapted` |

`sub_2B483`六个RNG callsite按正/负HP与负poison条件执行，随后按world/scene/battle上下文重绘、列出23项非零效果并present。现代 `apply_role_item_effect`逐字段保留signed word回绕、夹值、RNG顺序、morality/attack_twice只显示不写回BUG、attack-with-poison写回和effect计数；具体显示/等待/延迟/来源扣除仍由各caller continuation承接。非法role/item索引与present失败安全拒绝属于平台适配。

`sub_2BD8B`逐项检查MP type2通配、only-role、11个signed技能门槛、正/负IQ门槛、物品78/93性别门，最后扫描10个武功槽；任一已学同magic ID会把先前失败结果重新置为适合。现代 `role_meets_item_requirements`保持该覆盖顺序；非法role/item索引安全拒绝属于平台适配。

`sub_3CBE3`严格把palette `224..231`及`244..252`各右旋一格，再调用palette submit。`WorldSession::cycle_palette`与`SceneSession::cycle_palette`使用等价的 `std::rotate(first,last-1,last)`；runtime在present前预览本次旋转，只在成功present后提交共享5-tick相位和session palette。最后一轮从三个机器入口重审全部基本块与出口，未发现新差异。

## 4. 283个 Watcom/CRT/Miles 外部函数

### 4.1 边界不是地址猜测

fresh IDA对 `0x3E33D` 起的链接区逐函数导出边界、完整bytes SHA256、字符串、直接caller和callee。边界由以下机器证据共同确定：

1. `0x3E2E2`是最后一个游戏音频控制函数；其后`0x3E33D`进入共享runtime/file helper。
2. LE唯一入口是`start @ 0x3F234`；其启动代码执行DOS/Watcom初始化，经`0x469C2`进入游戏 `main @ 0x20D35`。
3. AIL区起点`0x3FAD8`是统一时间戳日志helper；`0x3FB7E`引用`AIL_DEBUG`、`AIL_SYS_DEBUG`、`Audio Interface Library ... V3.03`和`AIL_startup()`。
4. 连续区内API字符串明确列出`AIL_call_driver`、timer、DIG、sample、MDI、sequence、XMIDI等；并包含DIG/MDI driver、XMIDI chunk和设备错误字符串。
5. `0x4DE0A`引用`Floating-point support not loaded\r\n`，明确重新进入Watcom浮点/标准运行库；其后直到最后函数均是CRT/DOS extender支持。

因此库区分段为：

| interval | classification | functions | fresh bytes | aggregate SHA256 |
| --- | --- | ---: | ---: | --- |
| `0x3E33D <= start < 0x3FAD8` | `external_crt` | 42 | 3964 | `e00cf45ab0b9ffe808c886ee0f60b09832b061d904fe8013d75f74cd2319be28` |
| `0x3FAD8 <= start < 0x4DE0A` | `external_miles` | 192 | 32035 + 109 recovered | fresh 191函数：`204c80da577314b72a62b4dbe4d0c70dcc6665a1ffcf559666ba3193d5e72b37` |
| `0x4DE0A <= start <= 0x51ADE` | `external_crt` | 49 | 5505 | `bf640eab3e00bdfb130a86a43e8dc297976757d45b02ea024ec88e2232139f70` |

fresh自动分析得到282个库函数。唯一额外catalog项`0x41748..0x417B5`因无正式function xref而未自动建函数，但B4专项机器报告恢复109 bytes/28 instructions，loaded SHA256=`8d5ae99a4c2bc396da10b51a3076b05b42003b0e476af80c2f90a614d5b20264`；其指令内直接引用字符串`AIL_set_sample_loop_count(0x%X,%d)\n`并尾调Miles sample loop实现，故明确属于 `external_miles`，不是地址推断。加回后库函数严格为283。

正式catalog显示52个库函数有低于`0x3E33D`的直接caller；专项报告另恢复`0x3E172 -> 0x41748`。这53个游戏可调用外部入口的参数、返回、状态与调用顺序由runtime/audio/render/resource/persistence closure及第3节wrappers覆盖。LE `start`另作为进程入口覆盖；其余229个函数没有游戏代码直接入口，只是这些外部入口的Watcom/Miles内部实现。现代工程不复制其DOS extender、实体声卡、debug script、heap/stdio内部，仅保留游戏实际观察到的精确边界合同。

## 5. 最终逐行门

`validate_reverse_framework.py`现在对577行执行以下硬门：

- key与正式IDA报告/专项恢复函数集合完全一致；
- owner不得为`unresolved`，confidence必须为`high`，assignment basis不得为空或`mechanical_catalog_only`；
- status只能是`assembly_exact`、`platform_adapted`、`external_boundary`或有证明的`unreachable_current_assets`；
- `0x3E33D`后的每一行按上述两个精确边界逐地址验证为`external_crt`或`external_miles`，边界以下不得冒充外部库；
- 最终计数必须严格为`assembly_exact=12`、`platform_adapted=282`、`external_boundary=283`，其中`external_crt=91`、`external_miles=192`；
- 七份closure仍按owner、固定顺序、evidence、verification和最终零差异分别验证。

以上使“577行存在”与“577项已完成语义分类”成为两个独立、均可机械失败的门禁。
