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

- 独立owner固定为338 bytes、101条指令、16块、8条件分支、2跳转、14 calls、11 fixups及1个RET；13个callsite分属8个owner。raw/loaded SHA256=`874a191a383271c738bdfa17138e4818a8c01e240bdce4953b78de0a42666a34`/`964457918e555036189ddd327f398321f56c7d38501e3c22e851f5ed00c875c2`。
- 四参数为GRP filename、IDX filename、pointer start index、GRP destination。普通IDX由`sub_3CF45`读取完整内容，signed返回长度按4向0截断；逆序构造`pointer[start+i]=destination+cumulative_end[start+i-1]`，最后无条件写base pointer，再读取完整GRP并原样返回GRP长度。
- filename前四byte严格为lowercase `mmap`时走专用open/read/close路径，固定读取`14924=3731×4` bytes并固定项数3,731；原MMAP.IDX后5,000个零dword不进入pointer table。现代`PackedArchive`读取完整文件后严格裁剪该全零尾，得到相同3,731项。
- SDX/WDX走普通路径；末尾zero dword计入pointer数量但不作为offset，倒数第二个累计offset成为末帧起点，末帧到SMP/WMP EOF。现代`SentinelArchive`和battlefield offset vector显式形成同一末帧range。
- `0x3D82A..0x3D832`恢复尾另有`sub_3D950`两处外部jump进入；只共享epilogue，不传播owner closure。
- 独立只读oracle覆盖118对普通IDX/GRP及110对SDX/WDX，分别为18,193个有效普通项与428,105个sentinel项（65,087非空）；聚合原资产合同SHA256=`aa0f40cd18a01593961962f7523e07461a1915175fbd539e9ec751c187fc0250`。
- 现代非空/4-byte对齐、offset单调/range/EOF、完整读、动态ownership及稳定错误检查均为`platform_adapted`；canonical原资产entry边界和bytes零产品差异。结构化机器合同SHA256=`584a43c219142afdc01dffe1689198245e6897ddb77fb0fa3dbf7f46b52a87c2`；完整证据见`research/evidence/functions/Z_DAT/0x3D6E0.md`。

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
