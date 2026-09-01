# B3 状态模型与存档运输汇编合同

状态：assembly-reviewed / asset-exhaustive
真值：当前 `Z.DAT` 机器码、当前原版 RANGER/R/S/D 字节与独立 Python 解析

## 1. 证据范围

本工作包只闭环状态所有权和物理存档运输：

- `sub_26208 @ 0x26208..0x265AA`：读取 1–3 号槽；
- `sub_265AB @ 0x265AB..0x269AA`：写入 1–3 号槽；
- `RANGER.IDX/.GRP`、`ALLSIN*`、`ALLDEF*`、`R1–R3`、`S1–S3`、`D1–D3` 当前全部文件；
- `research/ida/reports/Z_DAT.targets.txt` 中上述两个函数的完整伪码与机器码；
- `research/ida/reports/Z_DAT.persistence_xrefs.txt` 中状态区机器码 data xref；
- 独立 Python 按小端 word/累计尾解析得到的 golden；
- `tests/unit/persistence/save_slot_test.cpp` 的 C++ 实现门禁。

历史端口只用于提供字段名称候选。文件边界、偏移、宽度、读写顺序和运行时依赖均由当前原版机器码与当前字节重新验证。

最终 headless xref 导出覆盖：header 45 refs/5 functions、roles 1,045/94、items 223/25、scene metadata 36/11、magics 34/14、shops 19/4。`0x2004D → 0xA0000` 是 Mode 13h 显示目标，仅因数值落在 magic 地址区间而形成范围假阳性，脚本和报告已显式排除；剩余 magic unknown words 没有当前机器码引用。

## 2. 当前资产基线

### 2.1 RANGER 六段

`RANGER.IDX` 恰为 6 个 `u32le` 累计尾：

```text
[836, 59076, 97076, 101444, 114092, 114242]
```

对应六段：

| 段 | 长度 | 当前记录合同 |
|---|---:|---|
| header/team/inventory | 836 | 12 个 header word + 6 个队员 word + 200×(物品 id, 数量) |
| roles | 58,240 | 320×182 bytes（91 words） |
| items | 38,000 | 200×190 bytes（95 words） |
| scene metadata | 4,368 | 84×52 bytes（26 words） |
| magics | 12,648 | 93×136 bytes（68 words） |
| shops | 150 | 5×30 bytes（15 words） |

当前基线：

```text
RANGER.IDX  24 bytes      SHA256 52c1545c8c0aa4d7919916ae840fb15f2772c567811ab9b9056e55b2e17ff92c
RANGER.GRP  114242 bytes  SHA256 07b99e3c1676e18691f00d6dfe713121faa6f7429e43666bc83e1568cecb68ab
```

### 2.2 S/D 状态

- S：100×49,152 bytes；每场景为 `6×64×64×int16le`，总计 4,915,200 bytes。
- D：100×4,400 bytes；每场景为 `200×11×int16le`，总计 440,000 bytes。
- 100 项 IDX 均为严格累计尾；S 每项增加 49,152，D 每项增加 4,400。

```text
ALLSIN.IDX  SHA256 6d35d9c9b233cd389261d58ca2f17d4f12f6dba1f429ac8ff6217ce3b10ab94a
ALLSIN.GRP  SHA256 830ae313ccabe310a16d330eac83647a9c81a6c23efce6069ca87dc653f0e154
ALLDEF.IDX  SHA256 99ad387b0ec7790ea684e3d9edf4777b35273703108b1c2ba0d584ddffc51c20
ALLDEF.GRP  SHA256 3633122f6a43f0b5dd390c2fa2516766d735a064ca955c8766b73232230a4480
```

初始 `ALLSINBK`/`ALLDEFBK` 与对应基线逐字节相同。三槽的配对 IDX 也与模板 IDX 相同，但第 4 节证明原版运行时不读取或写入这些 slot IDX。

## 3. `sub_26208` 读取顺序

槽参数是零基：`0 → #1`、`1 → #2`、`2 → #3`。

### 3.1 S 覆盖工作副本

- `0x26247..0x26278`：以模式 `0x202` 打开 `S1.GRP/S2.GRP/S3.GRP`；
- `0x2627D`：创建/截断 `ALLSINBK.GRP`；
- `0x2628E..0x262A5`：两个文件均 seek 到 0；
- `0x262AA..0x262D6`：严格循环 2 次，每次读取再写入 `0x258000 = 2,457,600` bytes；
- 总运输量严格为 4,915,200 bytes。

### 3.2 D 覆盖工作副本

- `0x26303..0x2632A`：选择并打开 `D1.GRP/D2.GRP/D3.GRP`；
- `0x26330`：创建/截断 `ALLDEFBK.GRP`；
- `0x2633F..0x26359`：两端 seek 到 0；
- `0x2635C..0x26384`：读取再写入 `0x6B6C0 = 440,000` bytes；
- `0x26387..0x26398`：关闭两端。

### 3.3 RANGER 六段读入

- `0x263BF..0x263F2`：仅在未缓存时读取公共 `RANGER.IDX`；失败路径打印 `"ranger.idx not found"` 并调用 `sub_20C32`；
- `0x26414..0x26445`：选择 `R1.GRP/R2.GRP/R3.GRP`；
- `0x2644A..0x26573`：按公共 IDX 的相邻累计尾，逐段 seek/read 到：
  1. `word_C0834`（header/team/inventory）；
  2. `word_9014C`（roles）；
  3. `word_A2744`（items）；
  4. `unk_9E4CC`（scene metadata）；
  5. `unk_9F5DC`（magics）；
  6. `word_DC694`（shops）。

每次长度都由 `next_end - previous_end` 计算，没有结构体 padding 参与文件运输。

## 4. `sub_265AB` 写入顺序与 IDX 真相

写入顺序严格为 **S → D → 更新 header/team/inventory → R**。

### 4.1 S/D

- `0x265EA..0x26665`：创建/截断 `S#.GRP`，打开 `ALLSINBK.GRP`，仍按 2×`0x258000` 复制；
- `0x26680..0x2671D`：创建/截断 `D#.GRP`，复制 `0x6B6C0` bytes，并关闭 S/D 两侧句柄。

### 4.2 保存前把运行状态回写 RANGER header

- `0x2673A..0x26750`：6 个队员从 `word_C0B78[]` 写到文件头 word 12..17；
- `0x26752..0x2677B`：200 组背包 id/count 写到 word 18..417；
- `0x2677D..0x267FB`：回写坐标、朝向、船坐标、编码和 in-ship 字段。

当前头区 word 顺序：

| word | byte | 语义 | `sub_265AB` 来源 |
|---:|---:|---|---|
| 0 | 0 | in ship | `word_5450C` |
| 1 | 2 | in sub-map | 此函数不赋值，保留状态区原字节 |
| 2 | 4 | main map X | `dword_C0B88` 低 word |
| 3 | 6 | main map Y | `dword_C0B8C` 低 word |
| 4 | 8 | sub-map X | `word_D295C` |
| 5 | 10 | sub-map Y | `word_D295A` |
| 6 | 12 | facing | `word_544F2` |
| 7 | 14 | ship X | `word_C0BA4` |
| 8 | 16 | ship Y | `word_C0BA0` |
| 9 | 18 | previous/paired ship X | `word_C0BA6` |
| 10 | 20 | previous/paired ship Y | `word_C0BA2` |
| 11 | 22 | encode flag | `word_5450E` |

现代`WorldSession`对应原world全局运行态，不在每次移动提前写header。保存wait-frame已经present后，`LegacyGameRuntime::perform_pending_io`先用保存前snapshot写S/D，再调用`sync_persistent_state`并重新导出R snapshot，最后写R：world菜单同步方向与全部world字段；scene菜单同步world坐标/船字段但保留`SceneSession`当前方向。由此保持`sub_265AB`的S→D→运行态回写→R顺序；S/D失败不会提前修改header，R失败前则已完成机器要求的回写。

### 4.3 RANGER

- `0x26801..0x2683E`：同样只读取公共 `RANGER.IDX`；
- `0x26860..0x2687B`：创建/截断 `R1.GRP/R2.GRP/R3.GRP`；
- `0x2687D..0x2699A`：按六个累计尾逐段 seek/write；
- `0x2699D`：关闭 R 文件。

### 4.4 原版不依赖 slot IDX

`sub_26208` 和 `sub_265AB`：

- 不打开 `R1.IDX/R2.IDX/R3.IDX`；
- 不打开 `S1.IDX/S2.IDX/S3.IDX`；
- 不打开 `D1.IDX/D2.IDX/D3.IDX`；
- 不写任何 IDX；
- R 段边界只来自公共 `RANGER.IDX`；
- S/D 长度是机器码中的固定运输量。

因此现代运行时 `load_numbered_slot` 可在所有 slot IDX 缺失或损坏时正常读槽；`write_numbered_slot` 只写 `S#.GRP → D#.GRP → R#.GRP`，并保持已有 IDX 原字节不变。成对 IDX/GRP 的全量扫描和 `write_snapshot` 是离线格式审计/导出能力，不是原版运行时依赖。

## 5. 模型所有权与 lossless 规则

### 5.1 所有者

- `model::GameState` 是可变游戏状态所有者；
- `model::GameSnapshot` 是跨 `persistence` 边界的值对象；
- `persistence` 只解析、验证和运输 snapshot，不拥有运行期状态；
- SDL、DOS 句柄和 TOML 类型均不进入模型。

### 5.2 RANGER 字段

`include/openlegend/model/legacy_types.hpp` 首先定义不同 tag 的 `CharacterId`、`ItemId`、`SceneId`、`MagicId`、`BattleId`、`TalkId`，以及 `WorldCoord`、`SceneCoord`、`PaletteIndex`；队伍、背包和各 RANGER 记录 ID 的公开入口不裸传含义不明的整数。

`include/openlegend/model/game_snapshot.hpp` 为六段建立不同 owning record：

- `RangerHeader`；
- `RoleRecord`；
- `ItemRecord`；
- `SceneMetadataRecord`；
- `MagicRecord`；
- `ShopRecord`。

每种 record 保留完整原字节，并提供 little-endian signed/unsigned word 访问。字段 namespace 按文件顺序命名所有当前结构字段；数组字段以 `*_begin` 加固定计数表达。无法从当前证据得出业务名称的 3 个 item word 和 5 个 magic word 明确标为 unknown，不猜测、不清零。

角色、物品、场景元数据、武学和商店的所有字符串仍作为原始 Big5/字节区保存；本阶段不做 Unicode 往返，避免改变 padding、终止符或不可见字节。

### 5.3 S/D 字段

- S 由 `SceneLayer` 标记六层：地面、建筑、装饰、事件索引、建筑高度、装饰高度；
- D 由 `SceneEventField` 标记 11 words：不可行走、索引、三个事件、当前/结束/起始图片、图片延迟、X、Y；
- snapshot 保留整块字节和 100 个累计尾；修改单一 word 只改变其对应 2 个字节；
- 六个定长 RANGER table 使用 heap-owned vectors，`RangerState::valid()` 固定验证 `320/200/84/93/5`；`GameSnapshot` 有 `<4096` bytes 的编译期尺寸门禁，避免 Windows 默认 1 MiB 栈在嵌套 import/export/round-trip 时重现 `0xC00000FD`。

### 5.4 导入约束

在进入 `GameState` 前必须同时满足：

- RANGER IDX 恰为六项且等于当前六段累计尾；
- RANGER GRP 长度等于 114,242；
- 成对 S/D 归档 IDX 恰为 100 项、严格定宽累计；
- S/D GRP 分别恰为 4,915,200 / 440,000；
- 运行时槽读取只验证固定 S/D 长度和公共 RANGER IDX。

非法 snapshot 不替换 `GameState` 已有状态。

## 6. 自动门禁

对应测试：`tests/unit/persistence/save_slot_test.cpp`。

- 独立 Python golden：初始主地图 `(357,235)`、角色 0 `Level=1, HP=32`、场景 0 地面首格 `942`、事件 0 当前图片 `5166` 等；
- 基线、工作副本和三槽共 5 套、每套 6 文件 load→write 后逐文件 byte-for-byte 相同；
- 重新加载的 `GameSnapshot` 逐字段/逐字节相同；
- header、role、item、S、D 各修改一个 word，证明每个输出只有目标 2-byte 位置允许变化，其他未知字节和全部 IDX 不变；
- 运行时槽测试删除全部 slot IDX 后仍成功读取；写槽前放入非法 sentinel IDX，写槽后逐字节不变；
- 覆盖 RANGER IDX 截断/非单调/错边界、RANGER GRP 截断、S/D IDX 截断/错步长、S/D GRP 截断、非法 snapshot、非法槽号、缺失文件和写失败；
- Linux 与 Windows 的 `core/app × Debug/Release` 全部通过：core 5 项 CTest，app 6 项 CTest。

测试生成物只写入 `OpenLegend/build/<platform>-<target>/tests/generated/<Config>/`；原版目录保持只读。
