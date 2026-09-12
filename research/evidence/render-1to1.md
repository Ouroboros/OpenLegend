# B2 软件绘制汇编合同

状态：assembly-reviewed
真值：`Z.DAT` 原版机器码与完整汇编

## 1. Indexed framebuffer

- `sub_3D6D1 @ 0x3D6D1..0x3D6E0`是15-byte/3-instruction提交wrapper：栈探针后tail-jump到`sub_20039`，没有本地RET、分支、fixup或参数读取。
- `sub_20039 @ 0x20039..0x2005B`保存通用复制寄存器后执行`CLD`，从`[dword_51C7F]`向线性VGA `0xA0000`一次`REP MOVSD`复制`0x3E80`个dword，恰为64,000个后备缓冲字节，然后RET直接返回wrapper的caller。
- `sub_2010A @ 0x2010A`：按 320 字节 stride 逐行填充矩形。
- `sub_2D501 @ 0x2D501`：以上、左、右、下顺序调用四次矩形填充形成1像素直角框，不改内部像素；现代映射为 `IndexedFramebuffer::outline_rectangle`。
- 核心真值固定为 `320×200×8-bit indexed framebuffer`；DOS index 字节不可直接作为现代颜色提交，宿主 RGBA 只在最终显示兼容层生成。

## 2. RLE 精灵 `sub_20354 @ 0x20354`

- 帧头为 `width:u16le, height:u16le, xOffset:i16le, yOffset:i16le`。
- 左上角严格为 `(anchorX-xOffset, anchorY-yOffset)`。
- 每行先读一个 payload 长度，payload 内重复 `skip:u8, count:u8, pixels[count]`。
- 原汇编分别跳过完全不可见行、裁掉左侧 run 前缀、裁掉右侧 run 后缀并按 320 stride 写入。
- 当前 78,014 个非空帧全部在左上、右上、左下、右下四个 anchor 实际执行；独立 Python oracle 的四 framebuffer 组合 FNV-1a 为 `fce6bf593964e433`。

## 3. 字形写入

### Big5 `sub_20615 @ 0x20615`

- 16×16、每行 2 字节、从每个字节的 bit 7 到 bit 0。
- 每个置位 bit 先在当前像素写 foreground，再在右侧像素写 right-shadow。
- 相邻置位 bit 会覆盖前一个 right-shadow；现代实现保留该逐字节覆盖顺序。

### ASCII `sub_20663 @ 0x20663`

- 8×16、每行 1 字节；每个置位 bit 使用与 Big5 相同的两像素写法。

### 文本与缓存 `sub_3D1E5 @ 0x3D1E5`、`sub_3D27A @ 0x3D27A`

- ASCII 正常前进 8 像素。
- `_` 使用 `FONT3.E16` 第 32 个空白字形但只前进 4 像素。
- Big5 code 为 `lead<<8 | trail`，前进 16 像素。
- Big5 索引：`(lead-0xA1)*157 + (trail<0xA1 ? trail-0x40 : trail-0x62)`。
- `sub_3D27A`固定208 bytes、63条指令、8个CFG块、3个条件分支、1个无条件跳转、9处重定位、4次direct call、2个caller和2个本地RET；raw/loaded SHA256为`2184f101736a6b043b9275525bd771cf193e0e14aa50bac9a94c73a8890a323d`/`7ae1394fcff40636b1abb15da18eb68a374e4e6617566b686c31b985bec5baf4`。
- `FONT3.C16` miss 时先写tag，再读取 32 字节到当前64槽环形缓存，随后推进replacement；hit返回首个匹配slot且不seek/read、不推进。全部13,973个合法编码映射SHA256为`4b3ce05fbebaf79ea5a5c7d5197aeb80e362e3a60e401e8745252139169aadaa`，cache trace SHA256为`c7f2602f1c04d455c15cfc3a29d8bb38dec57a28c5ae305bd38928dccc6d8936`。
- 原版启动链固定加载`FONT3.E16/FONT3.C16`；现代Basic UI、scene和battle renderer曾错误加载非3字体，现已修正并从只读FONT3资产三次确定性重生成B5/B7/B8及战斗角色状态Golden。正式文件SHA256依次为`2543ef3cca3099a89dcfaaec6022c64936d5e3fdfb481f98bb7dd0c7cf23b186`、`41258dd5f705488da5580b141d83e90f60034123913a9ef02f67a889d30456e8`、`ab3b9a67ceec89430176a5469899e3e3ed318efc7d44a1a4947272115e77d6ab`、`d6d9c58f61afba15cb327da007c6e4cbd1ddc00744897aff70226fe5a8144416`。
- 畸形lead/trail、短字体及原DOS seek/read失败继续由现代安全拒绝并归类平台适配；两个caller只立即取得当前32-byte字形，局部cache生命周期及返回span不改变合法只读资产的可观察像素。
- 128 个 ASCII 与 13,973 个 Big5 字形全部绘制；独立 oracle 的序列 FNV-1a 为 `6fa3df724d833333`。完整owner终审见`research/evidence/functions/Z_DAT/0x3D27A.md`。

## 4. 调色板

- `sub_20087 @ 0x20087` 按 256×RGB6 顺序写 VGA DAC；`sub_3D939` wrapper在回扫bit3置位后从DAC index0开始提交全部768 bytes，不写source或indexed pixels。
- `sub_3CBE3 @ 0x3CBE3` 把 entries 224..231 与244..252 各右旋一格后提交完整 palette；runtime 持有跨 world/scene 的5 tick相位，scene jump 不重置，模态等待帧不推进。
- 机器world/scene均先复制当前index帧，再在signed余数1分支写DAC，所以当前帧立即使用旋转后palette。现代在host present前预览该旋转，仅在present成功后提交会话counter/palette；失败和重复render不推进状态。
- MMAP.COL 首次/第五次右旋后 FNV-1a64 为 `898e23463574ae76`，第六次为 `6055f0cfd75adaa6`。
- 核心保留 0..63 原值；显示兼容层使用 `(value<<2)|(value>>4)` 展开到 8-bit，不反写核心 palette。
- 对每个 framebuffer 字节严格执行 `color=palette[index]`，依次输出 `R8,G8,B8,255`；该转换位于 `compat`，与 SDL API 解耦并可独立单测。完整MMAP palette FNV-1a64=`0xb7546b614cf2c7cc`，64,000-pixel展开帧FNV-1a64=`0x20a030c1cef0fc6d`。

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

- 先按颜色0..255、分量R/G/B顺序把全局768-byte palette复制到私有栈缓冲；全局源palette不修改。
- 固定执行64轮。每轮把私有副本的每个非零byte减1、零保持，然后以同一scratch地址提交一次，共64帧；第`n`帧严格为`max(initial_byte-n, 0)`。
- RGB6合法域（每分量0..63）的第64帧全黑；机器完整byte域中的64..255在第64帧仍为0..191，不能把“最终全黑”扩张到任意byte输入。
- 每次分量访问后还有50次私有寄存器忙等递增，总计49,152次分量访问、2,457,600次递增；该循环不读时钟、输入或RNG，也不写共享状态。现代实现省略CPU忙等并由成功present驱动64帧continuation，保留调色板序列和完成边界。

### 淡入 `sub_3CD17 @ 0x3CD17`

- 204-byte owner含76条指令、24个CFG块、10个条件分支、3个跳转、3个call和3处palette重定位；16个caller分属11个owner且不消费EAX，无外部跳入内部块。
- 外层 `i=64..1`。每帧都按颜色0..255、R/G/B顺序从只读源palette重新复制768 bytes，再对scratch执行`i`个饱和减1 pass；第`k`帧严格为`max(initial_byte-(65-k),0)`。
- 每帧还执行`64-i`个只读源palette的填充扫描，比较结果和flags均不使用；64轮固定49,152次复制、1,597,440次scratch减暗访问及1,548,288次无效源比较。现代省略无状态填充扫描，属于CPU时序平台适配。
- 共提交64个scratch过渡palette，随后再提交一次未修改源palette，总计65帧。RGB6域第1、2帧均全黑，第64帧为源减1，第65帧恢复源；完整byte域的第1帧可保留0..191。
- 现代按值保存65个独立snapshot，并只在成功present后推进；scene入口/内部jump及战斗排序后黑帧等caller额外present由caller continuation单独编码，不并入本owner提交数。
- 独立机器Golden合同/向量SHA256为`ecbbd42daffe04214599fddcc934ef205e41d99004d4ce74f4cf7ad21e912cf6`/`0877f1303d292785c58ac2a1ba5ddf5c20c7e0a2fb7295bba0d0cf615455fa85`；`mmap.col`与synthetic完整65帧串联SHA256为`f82ec722fa1c5eff432011680af9c4e69d54dddeb5789f51cfaf6164e69071a4`/`9c350d4e27059f9776abf9136ce35c08c8ff5b668f44423bf6a3021147bf3bbc`。
- `MMAP.COL` 的淡出64帧加淡入65帧序列FNV-1a为`a543bf4c501f4124`。

## 8. Wrapper 归属审计

以下入口没有额外像素算法，只保留参数门禁并调用已还原低层原语：

- `sub_3D6D1 @ 0x3D6D1` → `sub_20039` framebuffer提交；fresh机器审计固定107个callsite/62个owner，全部传入同一对死参数但owner/callee只读取active framebuffer global。caller分布为main/title/world/UI 41处、scene/event/ending 41处、battle 25处；完整终审见`research/evidence/functions/Z_DAT/0x3D6D1.md`。
- `sub_3D832 @ 0x3D832` → Y 坐标 `<0xBA` 后调用 `sub_3D1E5`；
- `sub_3D8D8 @ 0x3D8D8..0x3D8FF`是39-byte/10-instruction单块矩形wrapper：六个caller参数中只转发`x,y,width,height,color`，第六dword完全不读；底层`sub_2010A @ 0x2010A..0x20145`为59 bytes/28 instructions，以`u32(global+320*y+x)`为起点，`CLD; REP STOSB`按完整u32 width写color低byte，每行起点加320。行循环后测`DEC BX`，因此行数为`((u16(height)-1) mod 65536)+1`，zero-height会执行65,536行；返回EAX为起始地址低byte替换成color。
- fresh xref固定71个callsite/11个owner且均清理六个dword；`0x2A7DF/0x2D1BD/0x2D4F1/0x2D587`透传EAX，其余不用于绘制决策。`sub_2A7E8:0x2A867`跳入`0x2A7DA`复用`0x2A7DF`作为上箭头第五次填充和返回。title/name/attribute、上下箭头、圆角/头像白边及item轮廓的正式坐标全在320×200内。
- 现代`IndexedFramebuffer::fill_rectangle`在合法域保持起点、逐行width字节、320-byte pitch、行顺序和8-bit颜色；显式owner、uint16尺寸、zero-height安全no-op、bool结果、完整越界拒绝及失败短路归类平台适配。本轮把旧signed `x+width/y+height`先加后判改为先比较剩余容量，消除极端宿主坐标在拒绝前溢出；独立四操作整帧FNV-1a64=`0xcca0d464aa7bcf4d`。完整终审见`research/evidence/functions/Z_DAT/0x3D8D8.md`。
- `sub_3D922 @ 0x3D922` → `sub_2005B` 清屏；
- `sub_3D939 @ 0x3D939..0x3D950`是23-byte/6-instruction单块palette wrapper：0分支/跳转/fixup，依次调用8-byte栈探测和`sub_20087`。callee为54 bytes/36 instructions/5块，轮询`0x3DA`后向`0x3C8`写index0，再按升地址向`0x3C9`写768个source byte，每byte固定8个NOP。
- fresh xref固定6个callsite/5个owner：四处全局palette、两处栈scratch；两处偶然EAX尾透传且无业务消费者。首轮修正world/scene palette较机器晚一host frame的差异，改为当前present预览且成功后提交；修正后入口重审零新增差异。机器/向量合同SHA256=`570f993bd13b228230d216d9d8864b28d70c2799d94be5914b4f1ecd925867d7`/`edd329f9d3a0ea9a01d94eca9f49ec5736d21d92b6a36710c5c5bc07fc4e0d74`，完整终审见`research/evidence/functions/Z_DAT/0x3D939.md`。
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

这是一条平台兼容要求：它允许 DOS 像素在现代系统显示，但不改变游戏侧逐像素真值。`src/platform/sdl3/main.cpp`只在`SDL_RenderPresent`成功后调用`LegacyGameRuntime::finish_presented_tick`，失败路径返回且不推进UI/scene/battle continuation。对应纯单元测试覆盖 RGB6 `0/31/63`、完整64,000 pixel与全部256个palette index、source不变、非法帧/输出长度，以及`960×600`、带黑边窗口和过小窗口视口；runtime回归另锁定render完成但尚未收到成功present信号时不得推进。

## 10. 自动门禁

对应测试：`tests/unit/render/legacy_render_test.cpp`。

- 合成矩形、直角框内部保持、RLE 左右裁剪、两像素字形覆盖、shadow-mask、fade和深度旋转向量；淡入/淡出均覆盖完整byte域、源不变及保留帧深拷贝；
- 原物品格 `(55,62,40,40)` 在背景色7上的普通色0/选中色255边框 FNV-1a：`63eb8c2a7f900ed9` / `e154c07ba899cba5`；
- `TITLE[0] + CLOUD[0] + MMAP[0] + ASCII/Big5` 组合画面 FNV-1a：`cf173ba0515b7807`；
- 全部 14,101 个字形序列 FNV-1a：`6fa3df724d833333`；
- 全部 78,014 个非空 RLE 帧四角裁剪组合 FNV-1a：`fce6bf593964e433`；
- `MMAP.COL` 129 个淡变 palette 序列 FNV-1a：`a543bf4c501f4124`。
