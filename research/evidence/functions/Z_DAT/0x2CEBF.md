# 函数证据：`sub_2CEBF` `0x2CEBF..0x2D1CD`

状态：`platform_adapted`

来源：当前 `Z.DAT` 机器码；`tmp/startup-review/0x2CEBF.txt`、`0x3D8AD.txt`、`0x3D8D8.txt`；`research/ida/logs/death-ui-helpers.txt` 的 `sub_2050A @ 0x2050A..0x20615`；`research/ida/reports/Z_DAT.b6_world_xrefs.txt` 的 `unk_54294 @ 0x54294` 原始576字节

## 1. 独立机器恢复

- 函数仅在宽和高都严格大于10时绘制；否则不写framebuffer并直接返回。
- 首阶段调用 `sub_3D8AD -> sub_2050A` 绘十一段矩形，依次为顶部长度 `w-10,w-8,w-6,w-4,w-2`、中部 `w × (h-10)`、底部对称五段，形成削去五级角的圆角区域。
- `sub_2050A` 取source palette index、目标像素和style。`unk_54294` 的9×64原始表第 `r` 行严格等于 `floor(component*r/32)`；因此style `r` 的RGB4分量为 `floor(source*r/32)+floor(target*(8-r)/32)`，再经4096项 `byte_53283` 最近色表回写palette index。
- 第二阶段调用纯色矩形primitive绘十二段index255边线：顶部/底部横线、两侧竖线及四组2×1/1×2圆角连接；坐标和长度与现代实现逐条对应。
- 当前相关合法调用域中，游戏主菜单、医疗/解毒/共享选人、离队、系统/槽位/等待框使用style3；scene读档/退出菜单使用style4。style3是source 3/8加target 5/8，style4才是两侧各4/8。

## 2. 汇编 → C++ 映射

- style3菜单域：`BasicUiRenderer::draw_box/update_panel_palette/blend_panel_pixel`，固定source index0、style3、index255边线。
- style4场景域：`SceneSession::blend_panel_rectangle/blend_panel/draw_panel_border/draw_panel`，其 `source/8 + destination/8` 等价于两侧各 `floor(component*4/32)`。
- 带显式权重的战斗域：`BattleRenderer::blend_pixel`，保留 `source_weight` 与 `8-source_weight` 的两次独立整数除法及RGB4最近色回查。
- DOS全局palette/framebuffer改为各renderer持有的palette、显式 `IndexedFramebuffer` 与缓存lookup；合法调用的最终palette index保持。

## 3. 差异、废弃与重启

1. 旧证据只登记SceneSession且状态为待审，未覆盖游戏菜单使用的同一机器primitive。
2. 首轮调用方重审发现 `BasicUiRenderer::draw_box` 是直角白框加index0实心填充；与本函数的圆角混色面板不同，立即废弃该轮并恢复十一段混色、十二段边线和RGB4 lookup。
3. 初次恢复又把style3误作style4，C++和独立Python oracle共同使用4/8+4/8而产生伪一致。读取 `sub_2050A` 与 `unk_54294` 原始表后确认style3必须为3/8+5/8，再次废弃并修正两者。
4. 最终从 `0x2CEBF` 入口重新逐块覆盖严格尺寸门、十一段混色调用参数、style透传、十二段边线和唯一出口，并沿style3/style4合法调用点核对现代映射，零新增差异。

## 4. 平台适配

- 机器依赖全局320×200线性framebuffer且缺少完整宿主越界保护；现代在Basic UI入口拒绝越界矩形，Scene/Battle路径进行显式framebuffer边界处理。当前原始资产全部合法调用均在屏内，安全边界不改变可观察结果。
- palette变化时现代重建4096项lookup，替代机器全局表生命周期；相同palette下查表内容和首个最小距离tie规则一致。

## 5. 验证与结论

- `unk_54294` 九行已逐字节验证为 `floor(component*r/32)`。
- 独立Python oracle使用当前 `mmap.col`、`FONT.C16`与合成indexed背景，且不导入/执行OpenLegend代码；style3的world/scene主菜单帧与现代C++逐像素比较。
- SceneSession style4的opcode24菜单golden继续固定主面板、退出确认残留及两层退出尾链。
- 原程序动态差分：`blocked_runtime_oracle`。

`final_review = converged_no_new_differences`
