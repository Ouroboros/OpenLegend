# B1 资源加载汇编合同

状态：assembly-reviewed  
真值：`Z.DAT` 原版汇编；伪码只作导航

## 1. 全文件读取 `sub_3CF45 @ 0x3CF45`

- 173-byte owner含53条指令、5个CFG块、3个条件分支、0个跳转、11个call及5处重定位；36个callsite分属24个owner，无外部跳入内部块。
- 两个cdecl参数依次为filename与destination，无capacity。机器先构造`global_prefix+filename`，但固定先以flag `0x200`打开裸filename，失败才打开fallback路径。
- 两次open均失败时，以原filename打印`no this file '%s'....\n`并同步等待uppercase Q；随后仍以handle `-1`落入公共I/O链，不存在安全早退。
- 公共链取得32-bit完整文件长度、seek到0、一次请求恰该长度的读取、关闭，忽略seek/read/close返回并返回长度查询值。20个IDX caller比较`-1`，1个archive caller消费`size/4`，其余15个忽略返回值。
- 现代`read_binary_file`一次分配和读取完整byte vector；`DataRoot::read`显式读取选定root下的relative路径。成功原资产域的长度/内容与机器一致；动态容量、完整读检查、稳定错误对象、绝对路径拒绝、显式数据根和省略无效handle Q后续均属`platform_adapted`。
- 独立机器合同SHA256=`827ce4caf36cda9a6c646d9b41c76afac221d79deb14cb99631f8f32d9b22368`；完整证据见`research/evidence/functions/Z_DAT/0x3CF45.md`。

## 2. IDX 指针表 `sub_3D6E0 @ 0x3D6E0`

普通 IDX：

- 调用 `sub_3CF45` 读取完整 IDX；
- 项数严格为 `file_size / 4`；
- `pointer[0] = grp_base`；
- 对 `i = count-1 .. 1`：`pointer[i] = grp_base + cumulative_end[i-1]`；
- 因此 IDX 的第 `i` 个 dword 是 entry `i` 的结束偏移，最后一个 dword 是 GRP EOF。

`mmap.idx` 特例：

- 按文件名前四字节 `mmap` 分支；
- 固定读取 `14924 = 3731 × 4` 字节；
- 固定项数 3,731；
- 文件后续零区不进入运行时指针表。

SDX/WDX：

- 使用普通 `size/4` 项数；
- 最后一个零值本身不参与 `pointer[i]` 计算；
- 倒数第二个累计偏移成为末帧起点，末帧结束位置由对应 SMP/WMP 文件 EOF 决定。

## 3. RLE 绘制 `sub_20354 @ 0x20354`

- 头：`width:u16le, height:u16le, xOffset:i16le, yOffset:i16le`；
- 实际左上角：`(x-xOffset, y-yOffset)`；
- 每行先读一个 payload 长度；
- payload 内重复 `skip:u8, count:u8, pixels[count]`；
- skip 相对前一 run 末尾；
- 原函数分别实现上、下、左、右裁剪并直接写 320 字节 stride framebuffer。

## 4. 当前资产验证

- 118 对 IDX/GRP 全部覆盖到对应 GRP EOF；
- 92 个 FIGHT 加 8 个公共图像包，共 12,927 个非空普通 RLE 帧通过；
- 84 对 SDX/SMP 与 26 对 WDX/WMP，共 65,087 个非空哨兵 RLE 帧通过；
- `MMAP` 有效项固定为 3,731；
- `FONT3.E16 = 128 × 16`，`FONT3.C16 = 13,973 × 32`；
- 五个世界层均为 `480 × 480 × int16le`；
- `MMAP.COL = 256 × RGB6`。

对应自动验证：`tests/unit/resource/resource_archive_test.cpp`。
