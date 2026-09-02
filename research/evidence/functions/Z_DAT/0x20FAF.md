# 函数证据：`sub_20FAF` `0x20FAF..0x212C0`

状态：`platform_adapted`

来源：当前 `Z.DAT` 机器码；审计包 `tmp/b9-review/0x20FAF.txt`

## 1. 独立机器恢复

- 入口初始化局部状态：循环标志1、主菜单选择0、load子页标志0、槽选择0、最终结果0。
- 从 `title.idx/title.grp` 建立9帧archive，载入 `title.big` 到320×200 framebuffer；在 `(117,137)` 依次绘制主菜单底图legacy id `0`与选择0的legacy id `2`，present后执行65级淡入。
- 主菜单和load槽页选择均为0..2三项环绕。`Down=0x98`清自身状态后递增并由2回0；`Up=0x9E`清自身状态后递减并由0回2。
- `Enter=0x0D`、`Space=0x20`、keypad Insert=`0x96`共用确认分支；确认时清last-key及三枚确认键态。
- 主菜单选择0退出标题循环并保留结果0；选择1进入load子页、槽选择归零并重新载入 `title.big`；选择2直接关闭Miles、恢复键盘/平台并以状态0退出，不经过world shutdown链或music fade。
- load子页确认把结果设为 `slot+1`并退出循环；`Esc=0x1B`仅在load子页有效，清last-key与整个Esc状态、返回主菜单选择1并重新载入 `title.big`。主菜单Esc无副作用。
- 每个未退出进程的循环迭代都按当前页重画并present：主菜单底图id0 + `2+2*selection`；load底图id8 + `10+2*slot`，纵坐标均为 `137+20*selection`。
- 新游戏确认的最后一帧present后调用 `sub_26B5E`；load槽确认的最后一帧present后，清 `(115,135,135,65)`，在 `(120,160)`绘制id16的please-wait并present，再调用 `sub_26208(slot)`。

## 2. 汇编 → C++ 映射

- 输入、三项环绕与页状态：`ui::TitleMenuController::handle_key`。
- `title.big`、9帧archive、主/load/wait像素顺序：`ui::TitleMenuRenderer::render_background/render/draw_legacy_id`。
- 确认组三键、Up/Down/Esc清态：`LegacyGameRuntime::handle_key`、`menu_key_state_reset`、SDL `main`、`LegacyKeyboard::clear_confirmation_states/clear_state/clear_last_key`。
- 选择后的最后一次present、new/load continuation和please-wait门禁：`LegacyGameRuntime::handle_title_result`、`advance_scene_effect`、`advance`、`finish_presented_tick`、`begin_new_game`、`perform_pending_io`。
- 标题Quit：`handle_title_result`不置music-fade标志，随后由runtime/audio/platform RAII逆序关闭。

## 3. 单向终审

- 从 `0x20FAF` 入口独立恢复后才读取现代实现。
- 逐块覆盖资源载入、初始绘制/淡入、last-key分派、两级三项环绕、全部清态、Esc返回、每轮重绘、new/load/quit三个出口和load wait帧。
- 本轮零新增差异，无需修正或重启。
- `sub_247DD/sub_24C23`启动owner修正后废弃旧结论并再次从入口覆盖：标题renderer复用标题前载入的palette，TITLE archive、菜单循环、wait帧及new/load/quit出口均未改变；零新增差异。

## 4. 平台适配

- DOS last-key轮询与同步嵌套循环改为SDL事件和宿主帧continuation；按键优先级、状态清除、选择后present边界保持。
- TITLE archive固定scratch与手工载入改为容器/RAII；原始像素、legacy id换算与覆盖顺序保持。
- 标题Quit的Miles/IRQ退出改为SDL音频设备、混音器和平台RAII销毁；不执行world-exit music fade的可观察合同保持。
- 宿主窗口配置保存是平台边界附加行为，不宣称为DOS exact副作用。

## 5. 验证与结论

- Linux app Debug `proc_96cf`：14/14。
- 独立原资产golden固定main选择0..2、load槽0..2与please-wait共7个framebuffer hash；单测覆盖三键确认、Up/Down环绕、load Esc、选择后present门禁、load失败返回及标题Quit不fade。
- 原程序动态差分：`blocked_runtime_oracle`。

`final_review = converged_no_new_differences`
