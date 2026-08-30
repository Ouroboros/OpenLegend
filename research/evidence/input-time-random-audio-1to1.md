# B4 输入、时间、随机与音频汇编合同

状态：assembly-reviewed / asset-inventoried
真值：当前 `Z.DAT` 机器码、当前 77 个 WAV 与 24 个 XMI 字节、Miles 3.03 包装器内嵌诊断字符串

## 1. 证据范围

- `0x20BC0..0x20C31`：嵌入式 IRQ1 键盘处理体；
- `0x3CDE3..0x3CDFE`：安装到中断 9 的保存寄存器/切换 DS/trampoline/`iret`；
- `sub_3CDFF @ 0x3CDFF..0x3CF18`：键盘中断、字体和音频初始化；
- `sub_3CF19 @ 0x3CF19..0x3CF44`：恢复原中断 9；
- `sub_20C32 @ 0x20C32..0x20C44`：清空并阻塞等待 last-key；
- `sub_3DB83 @ 0x3DB83..0x3DBBE`：BIOS tick delay；
- `sub_3D612/sub_3F987/sub_3F98D/sub_3F9B0`：有界随机包装、RNG 状态、next、seed；
- `sub_3DD66..sub_3E2E2`：游戏侧 Miles music/sample 包装器；
- `sub_41232..sub_43239`：Miles API 包装器与内嵌函数名；
- `research/ida/reports/Z_DAT.b4_runtime_xrefs.txt`：上述机器码、数据引用和调用引用；
- `DIG.INI` / `MDI.INI`：Miles 3.03、SBPRO.DIG、SBPRO2.MDI；
- 当前根目录 `ATK00.WAV..ATK23.WAV`、`E00.WAV..E52.WAV`、`GAME01.XMI..GAME24.XMI`。

IDA 9.2 仅通过 `idat.exe -A` headless 运行。IRQ 源块和 trampoline 在导出时临时强制解码；命令结束后恢复跟踪中的 IDB，只提交脚本、报告和成功日志。

## 2. IRQ1 与键态

### 2.1 翻译表

`byte_51B16` 的 84 个有效初始字节是：

```text
00 1b 31 32 33 34 35 36 37 38 39 30 2d 3d 08 09
51 57 45 52 54 59 55 49 4f 50 5b 5d 0d 82 41 53
44 46 47 48 4a 4b 4c 3b 27 60 83 5c 5a 58 43 56
42 4e 4d 2c 2e 2f 84 2a 85 20 86 c9 ca cb cc cd
ce cf d0 d1 d2 87 88 9d 9e 9f 2d 9a 9b 9c 2b 97
98 99 96 89
```

它把 PC/AT set-1 make code 映射到原版内部 key code：字母为大写 ASCII；`Esc=0x1B`、`Enter=0x0D`、`Backspace=0x08`、`Tab=0x09`；方向小键盘为 `End=0x97, Down=0x98, PgDn=0x99, Left=0x9A, Center=0x9B, Right=0x9C, Home=0x9D, Up=0x9E, PgUp=0x9F`；F1–F10 为 `0xC9..0xD2`。

表后不是独立安全数组：`last raw scan @ 0x51B6A`、`last key @ 0x51B6B`、键态基址 `0x51B6D` 与表地址连续。处理器以 `scan & 0x7F` 直接索引 `byte_51B16`，所以不支持的 `0x54..0x7F` 会读到相邻可变字节。现代模型保留这一地址别名，而不是先做安全范围裁剪。

### 2.2 make/break 状态机

IRQ 体严格按以下顺序执行：

1. `in al, 0x60`，保存 raw scan；
2. `translated = memory[(raw & 0x7F)]`；
3. make (`raw < 0x80`)：
   - 若 `state[translated] == 0`，先写 `last_key=translated`，再把 state 加 1；
   - 无条件再把 state 加 2；
   - 8-bit add 溢出时连续减 2，因此值稳定在 `254/255` 而不回绕；
4. break (`raw >= 0x80`)：清零对应 state，并清零全局 last-key；
5. 严格执行端口 `0x61` acknowledge、向 PIC `0x20` 写 EOI、清 busy、`sti`、返回。

因此首次 make 得到 state `3`，typematic make 依次得到 `5,7,...`。测试必须覆盖首次边沿、重复、break、`&1` 消费、`254/255` 饱和以及不支持 scan 的别名行为。

机器码没有过滤 set-1 的 `E0/E1` 前缀。SDL 适配器必须把右 Ctrl/Alt、keypad Enter/`/`、独立导航键、GUI 键、Print Screen 和 Pause 重新展开为原始 multi-byte 序列后再逐字节送入上述状态机。例如独立 Up 为 `E0 48 / E0 C8`，Print Screen 为 `E0 2A E0 37 / E0 B7 E0 AA`，Pause 为 `E1 1D 45 E1 9D C5` 且无 break。由于 `E0/E1 >= 0x80`，前缀自身会按 break 路径经越界别名清零动态 state 并清 last-key；不得只保留序列末字节。F11/F12、SysRq、非 US `\\` 和 keypad `=` 的单字节码同样必须经过别名内存，而不是因超出 84-byte 表而预先丢弃。

`sub_20C32` 先把 last-key 清零，再自旋到 IRQ 写入非零值；菜单代码还会直接读取、比较和清除此字节。现代输入层必须同时暴露 last-key 和 256 项 byte state，不能只产生一次性高层 action。

## 3. 时间边界

### 3.1 tick 来源与主循环

`dword_544D8` 初始指向 BIOS Data Area `0x046C` 的 32-bit tick。主循环和场景/战斗循环在一帧开始保存 tick，帧尾自旋直到当前值不同。行为合同是“观察到一次 tick 值变化”，不是以宿主 delta-time 累加和补跑多帧。

现代高精度时钟用 PIT 基准 `1,193,182 / 65,536 Hz` 生成兼容 tick，并在 BIOS 日界值 `0x1800B0` 回绕。等待逻辑只比较相等/不等，因此必须自然跨过回绕边界。

`main @ 0x20D35` 在同一 tick 按 left→up→down→right→Esc menu→idle 只执行一个分支。四个方向命中时不清 Esc 对应 state 的低位；只有实际进入菜单后才执行 `state[Esc] &= 0xFE`。SDL 主循环因此在世界态延迟 Esc keydown，把 `keyboard.edge(0x1B)` 与四方向键态同时交给 runtime；runtime 回报实际打开菜单后才调用 `consume_edge(0x1B)`。方向与 Esc 同按时先移动，若 Esc 在方向释放前 keyup，则不会事后补开菜单。

每个 world tick 完成重绘/呈现后，原入口把全局计数 `(counter+1)%5` 写回；余数为1时调用 `sub_3CBE3`，将 RGB6 palette entries 224..231 和244..252 各自右旋一格并立即提交 DAC。现代 `finish_presented_tick()` 只在宿主 present 成功后推进 `LegacyGameRuntime` 持有的全局相位并更新当前 world palette，使刚呈现帧仍使用旋转前 palette，下一帧才使用新顺序。进入 scene 时把同一相位传入 `SceneSession`，仅在原外层 scene present continuation 完成后推进并回写 runtime；普通对话/菜单等待宿主帧不推进，相位跨世界/场景往返持续。

### 3.2 `sub_3DB83`

机器码执行有符号 `idiv 40`，然后加 1：

```text
wait_count = trunc_toward_zero(argument / 40) + 1
```

仅当 `wait_count > 0` 时，每轮捕获一个 tick 值并等待它变化。必须保留：

- `0..39 -> 1 tick`；
- `40..79 -> 2 ticks`；
- `-39..-1 -> 1 tick`；
- `-79..-40 -> 0 ticks`；
- 不按毫秒重新解释参数，不修正约 18.2 Hz 与除数 40 的历史失配。

## 4. RNG

`sub_3F987` 返回全局 32-bit state；`sub_3F9B0(seed)` 原样覆盖；`sub_3F98D()` 为：

```text
state = (state * 0x41C64E6D + 0x3039) mod 2^32
result = (state >> 16) & 0x7FFF
```

`sub_3D5DE` 从 DOS time 的 second/hundredth 字节形成 `second * 100 + hundredth` 后 seed。独立 oracle 的前 10 项：

```text
seed 0          : 0,21468,9988,22117,3498,16927,16045,19741,12122,8410
seed 1          : 16838,5758,10113,17515,31051,5627,23010,7419,16212,4086
seed 5999       : 22023,21564,10621,8352,13846,27280,21825,23901,5153,23205
seed 0xFFFFFFFF : 15929,4409,9862,26718,8713,28226,9080,32063,8032,12734
```

游戏侧唯一有界包装 `sub_3D612(upper)` 先检查 `upper > 1 && upper <= 30000`；不满足时返回 0 且完全不消费 RNG，满足时只消费一次并返回 `next() % upper`。`next()` 已在 `0..32767`，机器码仍以 `cdq/idiv` 取得有符号余数。最终 headless call xref 报告列出该包装的全部直接业务调用点。

state 乘加必须显式按 `uint32_t` 回绕；不得替换成 `<random>`、改变消费点或共享一个隐式全局宿主 RNG。

## 5. Miles 音频合同

### 5.1 初始化

`sub_3DD66`：

- music 与 sound disabled flag 先清零，驱动安装失败时各自置 1；
- XMIDI 分配一个 sequence handle；
- digital preference 1 设为 `11025`，preference 8 和 7 设为 0；
- 分配严格 8 个 sample handle；
- `DIG.INI` 指定 Miles 3.03 `SBPRO.DIG`；`MDI.INI` 指定 `SBPRO2.MDI`。

现代实现保留 8 个逻辑 sample slot 和 11025 Hz 原始 sample 速率。SDL 只负责设备流；XMI 解码器使用独立后端，不进入游戏状态模型。

### 5.2 music

活跃路径 `sub_3E1B2(index)`：

1. `AIL_set_sequence_volume(sequence, 0, 2000)`；
2. `AIL_delay(1000)`；
3. `AIL_end_sequence` 并解锁前一 XMI；
4. 从 `GAME01.XMI..GAME24.XMI` 读取所选文件到固定缓冲；
5. XMIDI master volume 设 `127`；
6. `AIL_init_sequence(sequence, bytes, 0)`；
7. `AIL_start_sequence`；
8. `AIL_set_sequence_loop_count(sequence, 0)`，Miles 语义为无限循环；
9. 记录当前 music index。

`sub_3E23B` 是 `volume=127,duration=2000` 的 fade-in；`sub_3E25B` 是 `volume=0,duration=2000` 后只 delay 1000 的 fade-out。不可把 2000/1000 改成一个等待完整淡出的重新设计。

XMI 后端固定使用 libADLMIDI v1.6.1、内嵌 bank 0 `AIL (The Fat Man 2op set, default AIL)`、AIL volume model、一个 DOSBox OPL3 emulator；这是对当前 `SBPRO2.MDI`/AIL 边界的受控平台适配，不宣称 PCM 与实体声卡模拟逐采样相同。

### 5.3 sample

活跃 sample 路径把 WAV **整文件字节**作为 `AIL_set_sample_address(handle, pointer, file_size)` 的 unsigned mono 8-bit 数据，不解析/跳过 RIFF header；随后强制 playback rate `11025`。这是机器码和 Miles 包装器诊断字符串共同确认的历史行为，现代混音器也必须包含这 44 个 header 字节。

- bank 1：`ATK00.WAV..ATK23.WAV`，固定 slot 1，调用 `AIL_set_sample_volume(..., 200)`；
- bank 2：`E00.WAV..E52.WAV`，固定 slot 2，调用 `AIL_set_sample_volume(..., 400)`；
- 若目标 slot status 为 Miles `4`（playing），先 end 再覆盖；
- `AIL_init_sample` 重置默认 type/loop/pan；活跃路径不设置 loop count，因此一次播放；
- 未被游戏调用的 `sub_3E088` 另设 volume 100、loop count 0，保留在证据中但不伪装成活跃调用点。

`AIL_set_sample_volume` 内部 `sub_49120` 先保存原参数，随后 `sub_47DB8 @ 0x47DBF..0x47DDF` 把 sample field 明确裁剪到 `0..127` 再计算设备增益。因此现代 backend 保留传入的 legacy volume 200/400 作为命令证据，并在混音时按相同上限裁剪；不得在控制器层预先把两个值改写为 127。

## 6. 当前资产门禁

当前根目录严格包含：

- 24 个 `ATK*.WAV`；
- 53 个 `E*.WAV`；
- 24 个 `GAME*.XMI`；
- 合计 77 WAV + 24 XMI。

全部 WAV 当前为 mono unsigned 8-bit；74 个 header rate 为 11025，`E20/E25/E33` 为 11000，但原版仍强制 11025。全部 XMI 以 `FORM/XDIR/INFO` 开始。自动测试必须逐个读取全部 101 个资产、验证编号无缺口、格式边界与原始长度，并证明音乐解码器可从每个 XMI 内存块初始化。

## 7. 完成门禁

- IRQ 状态机与 84-byte 表的独立向量逐字节一致；
- tick 除法、等待次数和 `0x1800AF -> 0` 回绕由 fake clock 验证；
- RNG 至少覆盖上述四个 seed 的序列和最终 state；
- fake audio port 验证 music fade/delay/end/load/start/loop 顺序、sample stop-before-reuse、8 slot 和 legacy 参数；
- real mixer 验证每个 XMI 可加载并产生 PCM，全部 WAV 可按整文件 raw U8 mono 11025 运输；
- Linux/Windows `core/app × Debug/Release` 全矩阵通过；
- SDL、ADLMIDI、DOS 与 Miles 类型不进入 model/persistence/render 公共接口。

最终验证：Linux 与 Windows LLVM 的 `core/app × Debug/Release` 全部通过；core 6 项 CTest、app 7 项 CTest。独立 `OPENLEGEND_ENABLE_SANITIZERS=ON` 配置以 ASan+UBSan+LeakSanitizer 串行运行 6 项 core CTest 全部通过。首次把 sanitizer 全局施加到第三方时，只命中 libADLMIDI DOSBox OPL 的 `dbopl.cpp:1620` 空指针 `offsetof` 实现技巧；最终门禁仅 instrument OpenLegend targets，未屏蔽或跳过任何 OpenLegend 测试。
