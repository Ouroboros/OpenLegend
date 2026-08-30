# B2 软件绘制汇编合同

状态：assembly-reviewed
真值：`Z.DAT` 原版机器码与完整汇编

## 1. Indexed framebuffer

- `sub_20039 @ 0x20039`：把 64,000 字节后备缓冲提交到 VGA `A000:0000`。
- `sub_2010A @ 0x2010A`：按 320 字节 stride 逐行填充矩形。
- 核心真值固定为 `320×200×8-bit indexed framebuffer`；DOS index 字节不可直接作为现代颜色提交，宿主 RGBA 只在最终显示兼容层生成。

## 2. RLE 精灵 `sub_20354 @ 0x20354`

- 帧头为 `width:u16le, height:u16le, xOffset:i16le, yOffset:i16le`。
- 左上角严格为 `(anchorX-xOffset, anchorY-yOffset)`。
- 每行先读一个 payload 长度，payload 内重复 `skip:u8, count:u8, pixels[count]`。
- 原汇编分别跳过完全不可见行、裁掉左侧 run 前缀、裁掉右侧 run 后缀并按 320 stride 写入。
- 当前 78,014 个非空帧全部在左上、右上、左下、右下四个 anchor 实际执行；独立 Python oracle 的四 framebuffer 组合 FNV-1a 为 `fce6bf593964e433`。

## 3. 字形写入

### ASCII `sub_20615 @ 0x20615`

- 8×16、每行 1 字节、从 bit 7 到 bit 0。
- 每个置位 bit 先在当前像素写 foreground，再在右侧像素写 right-shadow。
- 相邻置位 bit 会覆盖前一个 right-shadow；现代实现保留该逐字节覆盖顺序。

### Big5 `sub_20663 @ 0x20663`

- 16×16、每行 2 字节；每个置位 bit 使用与 ASCII 相同的两像素写法。

### 文本与缓存 `sub_3D1E5 @ 0x3D1E5`、`sub_3D27A @ 0x3D27A`

- ASCII 正常前进 8 像素。
- `_` 使用 `FONT3.E16` 第 32 个空白字形但只前进 4 像素。
- Big5 code 为 `lead<<8 | trail`，前进 16 像素。
- Big5 索引：`(lead-0xA1)*157 + (trail<0xA1 ? trail-0x40 : trail-0x62)`。
- `FONT3.C16` miss 时读取 32 字节到 64 槽环形缓存；hit 不推进替换槽。
- 128 个 ASCII 与 13,973 个 Big5 字形全部绘制；独立 oracle 的序列 FNV-1a 为 `6fa3df724d833333`。

## 4. 调色板

- `sub_20087 @ 0x20087` 按 256×RGB6 顺序写 VGA DAC。
- 核心保留 0..63 原值；显示兼容层使用 `(value<<2)|(value>>4)` 展开到 8-bit，不反写核心 palette。
- 对每个 framebuffer 字节严格执行 `color=palette[index]`，依次输出 `R8,G8,B8,255`；该转换位于 `compat`，与 SDL API 解耦并可独立单测。

## 5. 世界地图投影与深度

### 画面坐标 `sub_2558B @ 0x2558B`

对 32×32 缓存窗口，外层 X、内层 Y：

```text
dx = cacheX - (viewCacheX - 11)
dy = cacheY - (viewCacheY - 11)
screenX = 18*dx - 18*dy + 145
screenY =  9*dx +  9*dy - 81
```

玩家中心 `(dx,dy)=(11,11)` 对应 `(145,117)`。

### 深度列表 `sub_24F8C @ 0x24F8C`

- `0x24FA4..0x24FD2`：三个 1024×int16 列表各清零 0x800 字节。
- 扫描范围严格为 `[viewX-11, viewX+21) × [viewY-11, viewY+21)`，外层 X、内层 Y。
- owner X/Y pair 相同的 footprint 只生成一个建筑条目。
- 同列再次遇到非末尾 owner 时，`0x252CC..0x2535E` 使用位于数组基址前 2 字节的标签把前一项向后搬移，并把原末项旋转到已存在位置；现代实现使用等价稳定搬移。
- `0x250E0` 插入 sprite id 5000，`0x25162` 插入 sprite id 6000。
- `0x25285..0x25298` 与 `0x2542E..0x25437` 明确拒绝负 sprite id 和大于 `0x2064` 的 id。
- 建筑 sprite id 从 owner world coordinate 减 128×128 cache origin 后读取。

### 绘制 pass `sub_2558B @ 0x2558B`

反汇编中的 `sub_3D643` 调用顺序固定为：

1. `word_7FE2C` 地面层，32×32，X 外循环、Y 内循环；
2. `word_7300C` 非零覆盖层，同样遍历；
3. `word_617FC/61FFC/627FC` 深度列表，含 5000/6000 角色标记；
4. `sub_3D88A` 三个附加特效槽。

## 6. 移位阴影 mask `sub_20B22 @ 0x20B22`

- mask 是交替的 `zeroCount:u16, skipCount:u16` run 序列；zero run 把 framebuffer 字节置零，skip run 保留原像素。
- 非负 offset 先清零 framebuffer 前缀，再从 mask 第一个 zero run 开始。
- 负 offset 把第一个 zero run 减去裁掉的前缀，并在处理结束后清零 framebuffer 尾部 `-offset` 字节。
- run 长度按剩余 64,000 字节裁断；现代实现保留原 zero/skip 顺序。

## 7. 调色板淡变

### 淡出 `sub_3CC97 @ 0x3CC97`

- 从当前 256×RGB6 palette 开始，共提交 64 帧。
- 每帧把每个非零通道减 1；第 64 帧必为全黑。

### 淡入 `sub_3CD17 @ 0x3CD17`

- 外层 `i=64..1`，每帧从原 palette 重新复制后把每个通道饱和减 `i`。
- 共提交 64 个过渡 palette，随后再提交一次未修改原 palette，总计 65 帧。
- `MMAP.COL` 的淡出 64 帧加淡入 65 帧序列 FNV-1a 为 `a543bf4c501f4124`。

## 8. Wrapper 归属审计

以下入口没有额外像素算法，只保留参数门禁并调用已还原低层原语：

- `sub_3D6D1 @ 0x3D6D1` → `sub_20039` framebuffer 提交；
- `sub_3D832 @ 0x3D832` → Y 坐标 `<0xBA` 后调用 `sub_3D1E5`；
- `sub_3D8D8 @ 0x3D8D8` → `sub_2010A` 矩形填充；
- `sub_3D922 @ 0x3D922` → `sub_2005B` 清屏；
- `sub_3D939 @ 0x3D939` → `sub_20087` palette 提交；
- `sub_3D643 @ 0x3D643` → legacy sprite id `<=0x7FFE`，以整数除 2 取得 frame index 后调用 `sub_20354`。

`sub_3D88A` 是附加特效对象的业务绘制调用方，其 framebuffer 写入仍由本阶段原语完成；特效状态所有权归后续 world/scene/battle 模块。

## 9. 现代显示兼容边界

原版 Mode 13h 的 64,000 个字节是 palette index，不是现代 RGB 像素。现代后端必须按以下固定管线呈现：

```text
320×200 index8 + 256×RGB6
  -> palette[index]
  -> RGB6 bit replication to RGBA8
  -> 320×200 streaming texture
  -> nearest-neighbor centered integer viewport
  -> host window
```

窗口最小尺寸为 `320×200`。视口 scale 为 `min(outputWidth/320, outputHeight/200)` 的正整数，目标尺寸固定为 `320*scale × 200*scale`，余区清黑；不得使用线性过滤、任意小数放大或缩放后回读核心缓冲。当前默认 `960×600` 得到精确 3 倍显示。

这是一条平台兼容要求：它允许 DOS 像素在现代系统显示，但不改变游戏侧逐像素真值。对应纯单元测试覆盖 RGB6 `0/31/63`、任意 palette index、非法帧/输出长度与 `960×600`、带黑边窗口、过小窗口视口。

## 10. 自动门禁

对应测试：`tests/unit/render/legacy_render_test.cpp`。

- 合成矩形、RLE 左右裁剪、两像素字形覆盖、shadow-mask、fade 和深度旋转向量；
- `TITLE[0] + CLOUD[0] + MMAP[0] + ASCII/Big5` 组合画面 FNV-1a：`cf173ba0515b7807`；
- 全部 14,101 个字形序列 FNV-1a：`6fa3df724d833333`；
- 全部 78,014 个非空 RLE 帧四角裁剪组合 FNV-1a：`fce6bf593964e433`；
- `MMAP.COL` 129 个淡变 palette 序列 FNV-1a：`a543bf4c501f4124`。
