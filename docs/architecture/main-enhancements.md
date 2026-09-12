# `main` 增强功能边界

本文件记录只属于 `main` 的宿主策略和功能增强。这些内容不是 `Z.COM`、`Z.DAT` 或原始资产证明的原版行为，不得写入 `research/` 作为机器事实。`original` 继续以 `research/` 中的原版合同为准。

## 输入 repeat 策略

原版世界/场景主菜单与玩家行动菜单不等待额外 BIOS tick；两类菜单与世界行走都消费 IRQ1 make/typematic 产生的方向脉冲。现代宿主必须避免在这些菜单循环中额外插入一帧 BIOS tick 等待。

`main` 由 `LegacyGameRuntime::direction_repeat_context()` 分类方向输入：世界/场景行走为 `movement`，战斗空间光标为同属 Movement 域的 `battle_cursor` 子上下文；标题主菜单、商店、游戏菜单、死亡主菜单、战斗列表为 `menu`；三处 SAVE LIST 为同属 Menu 域的 `save_list` 子上下文；确认框、对话、动画、过场及错误提示为 `none`。

Movement 与 Menu 各有独立按键栈、deadline 和进度。Movement 与 Menu 之间切换时，仍按住的键必须真实 keyup 后重新按下；`menu` 与 `save_list` 切换则保留同一菜单链。死亡 SAVE LIST 逐键关闭的 present 门只限制业务层消费，不改变 `save_list` 上下文或清除 repeat 进度。SDL 宿主 repeat 在受管理域内全部抑制。

Movement 首方向立即动作并启动 `movement_repeat_delay_ms`；同一移动链切向立即动作但不重设首次 deadline，进入 repeat 后从下一 BIOS tick 继续，且每个 BIOS tick 最多一步。小地图每个稳定 tick 产生的 `present -> after_scene_present` 延续仍归 Movement，不能在每走一步后短暂改成 `none` 并重设首次 deadline。释放当前方向后回退到最近仍按住方向，不重新等待；全部方向释放才结束链。

战斗空间光标使用 Movement 的 delay 与 BIOS tick 节奏，但离散方格的组合键保留既有长按方向作为 repeat carrier：新方向 keydown 只立即移动一格并作为候补，旧方向下一 tick 继续；释放 carrier 后才提升最近仍按住的候补方向。

Menu 首次按键立即导航，每次切向或方向回退均重新等待 `menu_repeat_delay_ms`，之后按 `menu_repeat_interval_ms` 的 steady-clock deadline 重复。Up/Down/Left/Right、数字键盘别名与 Page Up/Down 共用该调度，Home/End 只接受首次 keydown。

菜单等待取下一 BIOS tick 与下一 menu deadline 的较小值，并允许 SDL 事件提前唤醒；只有真实 BIOS tick 才调用游戏 `advance()`，menu deadline 与 SDL 唤醒均不得推进动画、战斗或淡变时钟。配置默认值为 `movement_repeat_delay_ms=500`、`menu_repeat_delay_ms=500`、`menu_repeat_interval_ms=55`；两个 delay 允许零，interval 必须为正。

## 扩展死亡存档列表

`main` 保留原死亡菜单三槽状态机作为 `original` 基线，但在 `LegacyGameRuntime` 检测到死亡菜单后改由 `DeathMenuController` 接管宿主输入。底层画面覆盖为“載入進度／離開睡覺去”两项：前者进入 `001–999` SAVE LIST，后者继续使用原文确认及仅大写 `Y` 退出。该分流不写入 `SceneSession` 的机器选择状态，也不改变 `original`。

## 显示增强

可配置 IN-GAME RES、WIN SIZE、扩展地图视野和 RGBA 淡变属于 `main`，完整合同见 [`configurable-in-game-resolution.md`](configurable-in-game-resolution.md)。
