# B6 世界地图 1:1 证据

状态：`implemented_pending_review`；本文件记录已有实现/测试，不替代最终汇编↔C++ REVIEW。

## 真值与可复现产物

- 唯一行为真值：当前原版 `Z.COM` / `Z.DAT` 机器码。
- IDA：`/mnt/d/Dev/Crack/IDA/idat.exe -A`，仅 headless；每次导出后恢复 `research/ida/databases/Z_DAT.i64`。
- IDA 脚本：`research/ida/scripts/ida_b6_world_xrefs.py`，SHA256 `b7db49dbd593a9e70b3f2ba5e0d3dd0c8e7c11571a12291e41ab56828db24cc6`。
- 汇编/伪码报告：`research/ida/reports/Z_DAT.b6_world_xrefs.txt`，332,095 bytes，SHA256 `af02c2699e4cd9f3fea4642e5c09b740c00f93d8fb200a9e699fea3d1b730a53`。
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
| `0x24278` | `11/98` 边界触发 origin 重算、五层重载；不写RANGER header |
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

原版打开文件和每次cache总加载均保持 EARTH→BUILDING→SURFACE→BUILDX→BUILDY；现代实现用显式`kLayerLoadOrder`保留该机器顺序，同时层枚举索引仍按渲染语义访问。每层执行 128 次读取：文件偏移 `960*(origin_y+row)+2*origin_x`，每行 256 bytes。`origin = clamp(world-64, 0, 352)`。原始初始状态 `(357,235)` 得到 origin `(293,171)`、local `(64,64)`；五层 cache 的 FNV1a64 已固定在 oracle JSON。连续 35 格固定轨迹验证 local 超过 98 后 origin 从 7 重载为 42，local 回到 64。

## 移动、碰撞、船和入口

- 方向编号严格为 `up=0, right=1, left=2, down=3`；主循环优先级为 left→up→down→right→Esc menu→idle。四个方向分别合并并清零 translated-key 对：left=`0x9A/0x9D`、up=`0x9E/0x9F`、down=`0x97/0x98`、right=`0x99/0x9C`；按住后必须等待后续 make/repeat 重新置位，不能每个宿主帧连续移动。方向命中不消费 Esc odd edge，后续无方向 tick 才能打开菜单；若期间 Esc keyup，则请求随原键态消失。
- 步行帧基址 `5002/5016/5030/5044`，步进 `2..12`；船帧基址 `7430/7438/7446/7454`，步进 `2..6`。
- 步行封锁区间严格为：`358..362, 374..380, 458..464, 506..670, 818..824, 838, 934..936, 1016..1022`。
- 下船陆地区间严格为：`4..356, 364..372, 382..456, 672..954, 466..504, 1000..1014`；顺序保持机器码原顺序。
- 船海岸区间严格为：`358..362, 374..380, 458..464, 458..464, 506..610, 1016..1022`，包括原版重复区间。
- 固定轨迹 `right→up→left→down` 返回 `(357,235)`；初始向左命中场景 70，世界坐标不先提交。
- 条件 2 的合成入口分别验证 IQ 69 不触发、IQ 70 触发；party只扫描slot0起的连续前缀，首个空槽后的高IQ角色不得触发。
- `sub_25911`在进入scene前先present更新朝向后的完整世界帧，再执行64级fade-to-black；scene返回后先反转朝向，以仍保留的world cache、weather、idle、50步计数和palette present世界帧并执行65级fade-from-black，fade结束后才继续原移动调用者的碰撞/坐标/cache/weather后半段。两侧fade期间均冻结输入和世界palette周期。真实scene70因目标建筑阻挡保持`(357,235)`，合成可行入口移动到`(358,235)`；已旋转palette及第49→50次移动计数均跨scene保持。
- world移动仅修改原全局运行态；`sub_24278`不写RANGER header。`sub_265AB`保存槽时才复制world X/Y、方向、in_ship及船字段。现代`WorldSession::sync_persistent_state`只在保存导出边界调用；scene菜单保存不覆盖SceneSession维护的scene方向。
- 固定船轨迹验证 `(108,100)` 上船、海岸格移动、在 `(107,100)` 下船以及 header 的 current/next 船坐标。`in_ship`保留raw 16位：移动对任意非零走乘船分支，但render只有值1绘中心船，其他非零跳过中心玩家节点；提交不归一化为bool。
- 每 50 次移动尝试保留 `sub_26A92` 的原 BUG：第二个判断读取“队伍槽号对应角色”的 poison，而非队员 role id；随后减少真实队员的 HP、MP、体力。

## 绘制与周期状态

- 核心仍为 `320×200×index8 + 256×RGB6`；B6 不引入 RGBA 或 SDL 依赖。
- world frame 成功呈现后按 `(counter+1)%5` 推进 runtime 全局计数，余数1时把 palette entries 224..231 与244..252 各右旋一格；旋转结果从下一帧 render 生效，不提前改变本次 present。该计数进入 scene 时传入并在外层 scene tick 后回写，往返不重置。真实 MMAP.COL 首次/第五次旋转后 FNV-1a64 为 `0x898e23463574ae76`，第六次为 `0x6055f0cfd75adaa6`。
- 地面与 surface 使用 `MMAP.IDX/GRP`，legacy id 按 `/2` 取累计 archive entry；角色、船和建筑走同一 RLE 覆盖规则。
- 初始 framebuffer：SHA256 `8b925f9bb4d8378cd9a134965e0ae95e56e85360036a38f64387e073a486bc2a`，FNV1a64 `0x6f6cf22b7c8cb4b8`。
- 世界人物四方向 28 个 MMAP 行走帧均为合法非空 RLE；每帧含 305–402 个源像素。回归测试在初始位置、四方向轨迹和连续右移 35 步（覆盖完整 2–12 帧循环与 cache 重载）后逐步重绘，并确认当前人物帧至少一个源像素仍存在于 framebuffer 的 `(145,117)` anchor 区域。
- `LegacyGameRuntime` 同步链在四次方向输入后逐步断言 view 仍为 `world` 且 render 成功；键盘回归另固定四组成对键码、left→up→down→right 优先级、命中整组清零和 repeat 再置位。因此当前固定真实资产路径未复现“移动几步后人物消失”。运行日志另记录 view、坐标、方向、帧号、深度列表是否包含玩家和 present 失败，以捕获真实运行环境中的后续复现。
- 天气 alpha 表为 `table[weight][rgb6] = floor(rgb6*weight/32)`；source weight 为 6..8，destination weight 为 `8-weight`。
- RGB4 最近色使用目标 `(component*4+2)` 与当前 RGB6 palette 的平方距离，严格 `<` 保留首个同距 index。
- seed 1、300 tick 的天气粒子和最终像素由 oracle 固定；framebuffer SHA256 `e8a96eda7898d8fc13ca270754b1d10b5cf05e0994db8cc82d3218aecf00a313`，FNV1a64 `0xdff4c0d05bd3426b`。
- 相机移动时天气位置保留 `sub_24417` 的等角位移；全部粒子 x>500 时同 tick 重新生成。
- 无输入 tick 保持 `sub_2399E` 判定/恢复→`sub_23B3E` 天气→条件 `sub_23AA6` 待机动画的顺序；20 tick步行frame复位计数只在`in_ship==0`时递增。现代拆分为 `idle_tick()`、`periodic_tick()`、`idle_animation_tick()`，不再把动画推进提前到天气之前。

## 随机遭遇审计

世界主循环可达调用中没有战斗入口。`0x31C75` 的 callers 仅为 `0x2DE03` 与 `0x30480`（场景/事件链），不从 `0x20D35` 世界主循环、四个移动函数或 `0x25911` 直接可达。因此 B6 不臆造“世界随机战斗”；世界随机消费仅为待机动画和天气，战斗触发留给 B7/B8 的场景事件机器码。此结论是对原版行为的保真，而不是省略计划项。

## 实现边界

- `openlegend_world` 拥有只读五层数据、128×128 cache 和世界会话瞬态；`model::GameState` 仍是持久状态唯一所有者。
- 世界移动不即时改写 `RangerState` 的world header；`LegacyGameRuntime`仅在`sub_265AB`保存边界按S→D→运行态写回→R顺序调用`sync_persistent_state`。persistence只运输snapshot，不拥有或解释世界瞬态。
- `LegacyGameRuntime` 在新游戏接受或读档成功后创建世界会话；SDL 主循环通过 `LegacyKeyboard::world_direction()`按原优先级读取成对键态，世界态在调用 runtime 前以 `consume_world_direction()`清除命中方向的整组状态，后续 repeat keydown 才能重新置位，再执行 idle/weather tick。
- 场景入口在 B6 形成 typed `scene_request`；同步场景会话和双侧present/fade由app在B6/B7调用边界承接，world模块只保存并恢复移动continuation。

## 自动验证

`tests/unit/world/world_map_test.cpp` 覆盖：

1. 五层尺寸、坐标值与五个 cache hash；
2. 初始 framebuffer 独立 golden；
3. 固定移动轨迹、阻挡、场景 70、IQ 条件 2；
4. 11/98 cache reload；
5. 上船、船移动、海岸过渡、下船、移动后header保持及保存边界同步；
6. weather RNG 消费和 300 tick alpha framebuffer；
7. 待机动画方向帧、船上不递增步行复位计数、50 次移动消耗slot-poison原 BUG、200 tick 体力恢复；
8. `LegacyGameRuntime`、SDL方向接线、四组成对键码与消费/repeat合同、入口world预present/64级fade、返回world预present/65级fade、scene continuation与world瞬态保留；
9. 每步 world render 的当前 MMAP 人物帧可见性、raw `in_ship`中心帧分支，以及 app 每步 view/render 稳定性。

当前实现切片门禁（状态仍为`implemented_pending_review`，不代表最终双向REVIEW完成）：

- Linux core Debug/Release：各13/13；
- Linux app Debug/Release：各14/14；
- Windows clang-cl core Debug/Release：各13/13；
- Windows clang-cl app Debug/Release：各14/14；
- Linux app Debug ASan+UBSan：14/14；
- app矩阵内SDL dummy video/software renderer/dummy audio smoke通过；
- 独立B6 oracle与受控golden逐字节相同，reverse-framework validator为577行有效；
- 五项原版资产SHA256保持固定值，正式`Z_DAT.i64`无diff，`git diff --check`通过。
