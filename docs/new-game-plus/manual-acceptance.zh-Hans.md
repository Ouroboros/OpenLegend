# NG+完整游玩验收交接

## 当前验证结果

当前代码经平台统一BUILD完整执行166项测试，Linux、ASan＋UBSan及Windows均166/166通过，耗时分别145.11、308.59、165.11秒，BUILD_EXIT均0。日志位于仓库`tmp/ngplus-current-complete-{linux,sanitizer,windows}.log`。

覆盖包括普通数值与宽值角色的连续战斗、世界菜单保存、新会话标题读取、真实结局、通关保存及四次标题继承到NG5；128场普通成长；93个武功ID的正式成长表；存档UTC时间列及地点的逐像素检查。初态、剧情入口及部分路线由测试夹具指定，这些结果不证明完整自然剧情或整体平衡。

详细规则与执行证据见[验证矩阵](verification-matrix.zh-Hans.md)和[实现清单](implementation-tasks.zh-Hans.md)。

## Windows启动

本次实际验收产物：

```text
E:\Game\OpenLegend\OpenLegend\build\windows-app\src\platform\sdl3\Debug\openlegend.exe
```

程序从可执行文件同目录读取`openlegend.toml`。该产物目录当前没有配置文件；在此目录新建以下配置，可使用现有游戏数据、存档目录及默认NG+规则：

```toml
[paths]
data_dir = 'E:/Game/OpenLegend/data'
save_dir = 'E:/Game/OpenLegend/save'

[new_game_plus]
enabled = true
```

未指定的NG+参数使用正式默认值。标题开始新游戏后选择`全新遊戲`进行本周目；完成真实结局后保存通关进度，再从标题新游戏选项的`New Game+`选择通关槽位进入下一周目。

本次交接未修改用户现有启动配置、未替换父目录的可执行文件。

## 用户验收与反馈

用户已选择负责完整游玩验收。尚待确认：

- 从正常新游戏自然推进完整剧情，记录实际入队、离队、练功、装备及消耗品的使用。
- 实际菜单保存和重新启动读取后继续游戏，完成真实结局并保存通关进度。
- 通过标题继承连续进入NG2～NG5，核对实际成长的继承、指定字段恢复及世界重建。
- 对正常成长路线中的敌方强度、战斗节奏、经验和武功成长给出验收结论；出现问题时记录周目、配置、场景或战斗、操作步骤及结果。

反馈请注明“验收通过”或具体问题，并附可复现问题的存档和日志位置。收到问题后继续修复和验证；收到通过结论后再完成最终审计。当前Goal保持待验收，尚未宣称全部NG+完成。
