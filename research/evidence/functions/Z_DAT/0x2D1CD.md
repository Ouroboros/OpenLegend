# 函数证据：`sub_2D1CD` `0x2D1CD..0x2D372`

状态：`platform_adapted`

## 物理范围与控制流

当前 `Z.DAT` 本体范围为 `0x2D1CD..0x2D372`，共421字节、163条指令。入口先以 `0x58` 调用栈边界探测；随后对宽、高分别执行有符号 `<=10` 拒绝。任一尺寸不满足时不绘制并走唯一epilogue；均严格大于10时固定调用 `sub_3D8AD` 十一次，无循环或中途分支。

有效路径EAX沿用第十一次 `sub_3D8AD` 返回，拒绝路径沿用栈探测结果。全部物理caller均忽略EAX，因此现代void绘制接口不丢失可观察分支。

## 十一段圆角混色区域

以入口 `(x,y,w,h)` 表示，调用序列为：

1. `(x+5,y,w-10,1)`；
2. `(x+4,y+1,w-8,1)`；
3. `(x+3,y+2,w-6,1)`；
4. `(x+2,y+3,w-4,1)`；
5. `(x+1,y+4,w-2,1)`；
6. `(x,y+5,w,h-10)`；
7. `(x+1,y+h-5,w-2,1)`；
8. `(x+2,y+h-4,w-4,1)`；
9. `(x+3,y+h-3,w-6,1)`；
10. `(x+4,y+h-2,w-8,1)`；
11. `(x+5,y+h-1,w-10,1)`。

本函数只混色，不画白边、不绘头像sprite，也不读正文或按键。传入的index255参数未被本体读取；source palette index0、RGB4 lookup和style4逐次原样传给 `sub_3D8AD -> sub_2050A`。style4每个RGB6分量为 `floor(source/8)+floor(destination/8)`，再以 `r*256+g*16+b` 查询4096项最近色表。

## Caller与合法域

完整B7 xref仅有 `sub_2CC21` 的两个callsite：

- `0x2CD87`：首个对话页绘制头像框；
- `0x2CE66`：非末页重绘scene后的后续头像框。

两处都仅在对话style不等于2/3时调用，固定传 `60×62`、source index0、style4和同一lookup。合法左上角由style选择为 `(23,12)`、`(237,125)`、`(237,12)` 或 `(23,125)`；四个矩形均完整位于320×200 framebuffer内。每框混色3,660个互不重叠像素：顶部五行270、中部52行3,120、底部五行270，四角合计60像素保持原值。caller随后才在 `(x+2,y+59)` 画HDGRP头像并调用 `sub_2D372` 画白边。

## 汇编→C++映射与平台边界

- `SceneSession::blend_panel` 十一次调用 `blend_panel_rectangle` 的坐标、宽、高和顺序逐项一致；
- `blend_panel_rectangle` 固定读取 `palette_[0]`，执行source/destination各除8并经 `rgb4_lookup_` 回写，等价于机器style4；
- `SceneSession::draw_overlay` 保持“混色底→头像sprite→白边”的caller顺序；style2/3不进入头像路径，其他六种合法布局与机器一致；
- 通用 `draw_panel` 在调用混色前同样拒绝 `w<=10 || h<=10`；本函数的两个实际头像caller恒为 `60×62`。

机器矩形callee按320×200全局线性地址裸写，越界坐标会覆盖非法地址；现代 `blend_panel_rectangle` 将矩形裁剪到对象持有的framebuffer。当前四个合法位置不触发裁剪，逐像素结果一致；非法坐标保护及对象化palette/lookup生命周期归为宿主平台适配。

本轮先从机器码独立恢复163条指令、两个caller及十一组参数，再单向映射C++。入口到出口静态门复核指令数、全部十一处callee地址、尺寸分支、style4公式、头像调用顺序和3,660像素面积，零新增产品差异，因此未修改产品代码或测试。

## 验证

- 独立oracle新增 `portrait_blend_contract`，固定物理范围、163条指令、严格尺寸门、十一段符号矩形、两个caller、四个合法位置及3,660像素面积；双次生成逐字节一致。
- 既有真实style0/1/4 C++整帧分别固定左上、右下、右上头像框hash，覆盖混色底、HDGRP sprite与白边组合顺序；当前KDEF无style5，第四个左下位置由机器caller参数与C++ style5分支静态核对。
- `scene-goldens.json` SHA256：`91e1318007c8fdb7e61d34abe4e1eb67f78a6dc623fd7c99a1704c59a00f93d1`。
- `./build.sh app --config Debug`：14/14通过（`proc_b19d`，含scene、render、reverse framework及SDL smoke）。
- 原程序动态差分：`blocked_runtime_oracle`。

`final_review = converged_no_new_differences`
