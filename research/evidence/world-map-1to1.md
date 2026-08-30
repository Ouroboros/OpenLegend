# B6 世界地图 1:1 证据

## 真值与可复现产物

- 唯一行为真值：当前原版 `Z.COM` / `Z.DAT` 机器码。
- IDA：`/mnt/d/Dev/Crack/IDA/idat.exe -A`，仅 headless；每次导出后恢复 `research/ida/databases/Z_DAT.i64`。
- IDA 脚本：`research/ida/scripts/ida_b6_world_xrefs.py`，SHA256 `b7db49dbd593a9e70b3f2ba5e0d3dd0c8e7c11571a12291e41ab56828db24cc6`。
- 汇编/伪码报告：`research/ida/reports/Z_DAT.b6_world_xrefs.txt`，332,095 bytes，SHA256 `af02c2699e4cd9f3fea4642e5c09b740c00f93d8fb200a9e699fea3d1b730a53`。
- headless 日志：`research/ida/logs/Z_DAT.b6_world_xrefs.log`，SHA256 `5caab6a1108ecd1e9cd8cd82cc436ce301426ea2afdf186dbb32251938969f5a`。
- 独立 oracle：`research/tools/generate_b6_world_goldens.py`，SHA256 `d9959b905669d506eb6fcb258c6837edee59256b91c3e2a7982f3c58b76f8c0d`。
- oracle 输出：`research/evidence/world-map-goldens.json`，SHA256 `b906e335fd903a376b0a6d65c26ee09d525a43a43b373def538df4edfbea3d2e`。

oracle 不链接或调用 OpenLegend C++；它独立实现 int16le、五层缓存、累计 IDX、RLE、深度列表、RGB4 最近色、8 级 alpha、LCG 和固定移动轨迹。

## 入口闭环

| 地址 | 合同 |
|---|---|
| `0x2399E` | 无输入 tick、步行动画归零、待机随机动画、200 tick 体力恢复 |
| `0x23AA6` | 待机动画每 4 tick 推进、方向专属 `54/52/50/48` 帧偏移 |
| `0x23B3E` | 三个天气粒子的生成、水平推进和同 tick 重启 |
| `0x23C98` / `0x23F28` | 陆地/船 X/Y 移动、方向、帧、碰撞、缓存提交和天气滚动 |
| `0x241DC` / `0x2422A` | cache-local 与 world 坐标同步 |
| `0x24278` | `11/98` 边界触发 origin 重算、五层重载、存档 header 回写 |
| `0x24417` | 相机移动时天气按等角投影反向平移 |
| `0x24496` | 玩家踏入船当前/下一格时切换乘船状态 |
| `0x24559` | 船体当前格/下一格的海岸过渡探测 |
| `0x24667` | 陆地阻挡探测，保留八段原始封锁区间 |
| `0x246F9` | 船到陆地的下船探测，保留六段陆地区间与 surface 条件 |
| `0x24A02` | 世界会话从 Ranger header 初始化坐标、方向、船、队伍与物品 |
| `0x24C23` / `0x24C8D` | 五层文件打开/关闭 |
| `0x24CE8` | 五层缓存总加载 |
| `0x24D43..0x24F17` | EARTH/BUILDING/SURFACE/BUILDX/BUILDY 逐层 128 行读取 |
| `0x24F8C` | 32×32 可视区深度列表重建 |
| `0x2558B` | 地表→surface→建筑/角色/船→天气的像素覆盖顺序 |
| `0x25911` | 前 84 场景的双入口、条件 0 与条件 2（队伍 IQ≥70） |
| `0x26A92` | 每 50 次移动尝试的 HP/MP/体力消耗，并保留 poison 索引原 BUG |
| `0x206AD` / `0x20899` / `0x208A9` | 裁剪 RLE 和逐像素 8 级半透明混合 |
| `0x3D34A` | RGB4 颜色立方到当前 RGB6 palette 的最近色表 |

## 五层资源与缓存

五个文件均为 `480×480×int16le = 460,800 bytes`，行主序索引为 `y*480+x`：

| 文件 | SHA256 |
|---|---|
| `EARTH.002` | `286bd09db291159c1f828ed59d0ef5f812c04129b9708f8d28a675f4ae10f2b7` |
| `SURFACE.002` | `51639da4730377cd9775737b2124a99e9e47910ec0c73537ac2b259b9d092aa2` |
| `BUILDING.002` | `903c9cc978ad9cedab894d63c2f9f57e6fd855940cdf9426663a8eae7dd592b4` |
| `BUILDX.002` | `642f584eeccabee20ff73744f7ea74b0d78be521c7d509730bdb6e14d6fbf80c` |
| `BUILDY.002` | `77531b0f9b6b06e0c96fdf73f471d9d759af3553b8dd300855d0c64ecb62257a` |

原版每层执行 128 次读取：文件偏移 `960*(origin_y+row)+2*origin_x`，每行 256 bytes。`origin = clamp(world-64, 0, 352)`。原始初始状态 `(357,235)` 得到 origin `(293,171)`、local `(64,64)`；五层 cache 的 FNV1a64 已固定在 oracle JSON。连续 35 格固定轨迹验证 local 超过 98 后 origin 从 7 重载为 42，local 回到 64。

## 移动、碰撞、船和入口

- 方向编号严格为 `up=0, right=1, left=2, down=3`；主循环优先级为 left→up→down→right。
- 步行帧基址 `5002/5016/5030/5044`，步进 `2..12`；船帧基址 `7430/7438/7446/7454`，步进 `2..6`。
- 步行封锁区间严格为：`358..362, 374..380, 458..464, 506..670, 818..824, 838, 934..936, 1016..1022`。
- 下船陆地区间严格为：`4..356, 364..372, 382..456, 672..954, 466..504, 1000..1014`；顺序保持机器码原顺序。
- 船海岸区间严格为：`358..362, 374..380, 458..464, 458..464, 506..610, 1016..1022`，包括原版重复区间。
- 固定轨迹 `right→up→left→down` 返回 `(357,235)`；初始向左命中场景 70，世界坐标不先提交。
- 条件 2 的合成入口分别验证 IQ 69 不触发、IQ 70 触发。
- 固定船轨迹验证 `(108,100)` 上船、海岸格移动、在 `(107,100)` 下船以及 header 的 current/next 船坐标。
- 每 50 次移动尝试保留 `sub_26A92` 的原 BUG：第二个判断读取“队伍槽号对应角色”的 poison，而非队员 role id；随后减少真实队员的 HP、MP、体力。

## 绘制与周期状态

- 核心仍为 `320×200×index8 + 256×RGB6`；B6 不引入 RGBA 或 SDL 依赖。
- 地面与 surface 使用 `MMAP.IDX/GRP`，legacy id 按 `/2` 取累计 archive entry；角色、船和建筑走同一 RLE 覆盖规则。
- 初始 framebuffer：SHA256 `8b925f9bb4d8378cd9a134965e0ae95e56e85360036a38f64387e073a486bc2a`，FNV1a64 `0x6f6cf22b7c8cb4b8`。
- 天气 alpha 表为 `table[weight][rgb6] = floor(rgb6*weight/32)`；source weight 为 6..8，destination weight 为 `8-weight`。
- RGB4 最近色使用目标 `(component*4+2)` 与当前 RGB6 palette 的平方距离，严格 `<` 保留首个同距 index。
- seed 1、300 tick 的天气粒子和最终像素由 oracle 固定；framebuffer SHA256 `e8a96eda7898d8fc13ca270754b1d10b5cf05e0994db8cc82d3218aecf00a313`，FNV1a64 `0xdff4c0d05bd3426b`。
- 相机移动时天气位置保留 `sub_24417` 的等角位移；全部粒子 x>500 时同 tick 重新生成。

## 随机遭遇审计

世界主循环可达调用中没有战斗入口。`0x31C75` 的 callers 仅为 `0x2DE03` 与 `0x30480`（场景/事件链），不从 `0x20D35` 世界主循环、四个移动函数或 `0x25911` 直接可达。因此 B6 不臆造“世界随机战斗”；世界随机消费仅为待机动画和天气，战斗触发留给 B7/B8 的场景事件机器码。此结论是对原版行为的保真，而不是省略计划项。

## 实现边界

- `openlegend_world` 拥有只读五层数据、128×128 cache 和世界会话瞬态；`model::GameState` 仍是持久状态唯一所有者。
- 世界会话只经 `RangerState` 的 header/角色记录回写；persistence 不拥有或解释世界瞬态。
- `LegacyGameRuntime` 在新游戏接受或读档成功后创建世界会话；SDL 主循环把 held key 状态按原优先级传入，再执行 idle/weather tick。
- 场景入口在 B6 形成 typed `scene_request`；真正同步进入场景属于 B7，不在世界模块伪造场景行为。

## 自动验证

`tests/unit/world/world_map_test.cpp` 覆盖：

1. 五层尺寸、坐标值与五个 cache hash；
2. 初始 framebuffer 独立 golden；
3. 固定移动轨迹、阻挡、场景 70、IQ 条件 2；
4. 11/98 cache reload；
5. 上船、船移动、海岸过渡、下船与 header 同步；
6. weather RNG 消费和 300 tick alpha framebuffer；
7. 待机动画方向帧、50 次移动消耗原 BUG、200 tick 体力恢复；
8. `LegacyGameRuntime` 和 SDL held-key 接线、scene request。

阶段门禁（最终 B6 工作树）：

- Linux core Debug/Release：各 9/9；
- Linux app Debug/Release：各 10/10；
- Windows clang-cl core Debug/Release：各 9/9；
- Windows clang-cl app Debug/Release：各 10/10；
- Linux ASan+UBSan：9/9；
- Linux 与 Windows Debug：SDL dummy video/software renderer/dummy audio smoke 均返回 0；
- GCC/clang-cl 构建输出无项目 warning，`git diff --check` 通过。
