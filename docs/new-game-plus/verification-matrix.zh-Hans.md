# NG+正式要求与验证矩阵

## 依据和判定

本表对应主设计、52份正式字段页及四份跨字段合同，共57份页面。它只登记实现位置、可观察证据和验收缺口，不新增规则、不复制公式。规则以所链接的正式页面为准。

- 源码核对证明所检查的类型、分支和提交边界；词法候选数量不证明所有写点已覆盖。
- 组件测试、实际战斗、实际菜单、实际结局和连续游戏流程分别登记；固定宽值夹具不证明自然成长或整体平衡。
- 当前Linux、Sanitizer及Windows完整166项结果见实现清单；历史平台结果不替代当前最终验证。

公共实现与证据位置：

| 标记 | 当前实现 | 当前测试源码 |
|---|---|---|
| R | `src/model/role_state.cpp`、`src/model/runtime_snapshot.cpp`；signed64权威值、Legacy表示边界、结构与保存域分开检查 | `tests/unit/model/role_state_test.cpp`、`runtime_snapshot_test.cpp` |
| T | `src/model/playthrough_transition.cpp`；验证源后建立完整BASE候选，再继承曾入队角色及恢复指定字段 | `tests/unit/model/playthrough_transition_test.cpp` |
| P | `src/persistence/toml_snapshot.cpp`、`toml_slot.cpp`、`ordinary_slot.cpp`；用途、版本、指纹、字段、引用及失败状态检查 | `tests/unit/persistence/toml_snapshot_test.cpp`、`toml_slot_test.cpp` |
| B | `src/battle/battle_setup.cpp`、`battle_session.cpp`、`battle_renderer.cpp`；结算、呈现、消息和随机状态 | `tests/unit/battle/battle_data_test.cpp`、`ngplus_session_test.cpp` |
| U | `src/app/legacy_game_runtime.cpp`及`src/ui/`；配置传入、普通存读档、菜单、通关保存与标题继承 | `tests/unit/ui/title_menu_test.cpp` |

下表的“覆盖”描述检查对象，不表示整个NG+已完成。R的当前保存检查遍历队伍与曾入队角色；未保存角色从BASE读取。T按整名角色复制继承字段，然后明确恢复例外字段；不按周目反向夹值。

## 全部字段页

| 正式字段页 | 必须保持的处理 | 实现与覆盖 |
|---|---|---|
| [add_hp](fields/add-hp.zh-Hans.html) | 静态物品值来自资产；回血按正式分母及实际最大HP计算 | B；普通物品、菜单与AI效果及溢出回滚 |
| [add_poison](fields/add-poison.zh-Hans.html) | 静态物品值来自资产；正毒统一上毒，负毒按药效 | B；玩家、AI、暗器、普通物品与独立溢出伤血 |
| [attack_twice](fields/attack-twice.zh-Hans.html) | 下一周目恢复BASE；本周固定域0～1 | R／T／P；恢复、非法域拒绝、双击费用时机 |
| [attack_with_poison](fields/attack-with-poison.zh-Hans.html) | 曾入队实际值继承；附毒读取实际值 | R／T／B／P；上毒、秘笈增量与溢出 |
| [attack](fields/attack.zh-Hans.html) | 实际值继承；装备生效值单独计算 | R／T／B／P；宽攻击、剧情增长、装备及AI统计 |
| [defence](fields/defence.zh-Hans.html) | 实际值继承；装备生效值单独计算 | R／T／B／P；宽防御、装备正负增量与失败 |
| [equipment](fields/equipment.zh-Hans.html) | 下一周目清为两槽−1；本周验证类型与槽位 | R／T／P／B；装备引用、固定搭配及清空 |
| [experience](fields/experience.zh-Hans.html) | 累计经验继承；个人分配后乘当前周目倍率 | R／T／P／B；资产门槛、表外递推、奖励与升级 |
| [fame](fields/fame.zh-Hans.html) | 下一周目恢复BASE；本周剧情加算检查溢出 | R／T／P；剧情声望、提示、200门槛 |
| [fist](fields/fist.zh-Hans.html) | 继承不夹；升级及修炼保留各自固定写回边界 | R／T／B；跨100继承、零增量及增长失败 |
| [frames](fields/frames.zh-Hans.html) | 下一周目恢复BASE十五帧；存档检查原生表示域 | R／T／P／B；帧数组、动画及恢复 |
| [hidden_weapon_cap_step](fields/hidden-weapon-cap-step.zh-Hans.html) | 当前CFG供既有敌方增长；不限制玩家字段 | 配置模型／敌方模型／B；不同STEP、负STEP和溢出 |
| [hidden_weapon](fields/hidden-weapon.zh-Hans.html) | 实际值继承；暗器结算及有限选择器分开 | R／T／B；射程、宽伤害、伤势及附毒 |
| [hp_cap_step](fields/hp-cap-step.zh-Hans.html) | 当前CFG供既有敌方最大HP增长；不夹玩家HP上限 | 配置模型／敌方模型／B；BASE、STEP、入场当前HP及失败 |
| [hp](fields/hp.zh-Hans.html) | 进入下一周目回复到继承后的最大HP | R／T／B／U；治疗、伤害、休息、显示和恢复 |
| [hurt_cap_step](fields/hurt-cap-step.zh-Hans.html) | 从CFG计算本周内伤量程，包含零量程 | 配置模型／R／B／P；内伤写回、百分比门槛及保存域 |
| [hurt与medicine](fields/hurt-medicine.zh-Hans.html) | 内伤、医疗、药品及显示使用各自正式规则 | R／B／U；满HP治伤、超长回血、调用路径及RNG；19位能力浮点取整错误已复现并改为精确整数分式，Linux／Sanitizer关联各5/5 |
| [increased_life](fields/increased-life.zh-Hans.html) | 继承并验证1～10，不夹非法输入 | R／T／P；Legacy往返、固定域拒绝和成长 |
| [inventory_count](fields/inventory-count.zh-Hans.html) | signed64数量；继承白名单并保源顺序与重复 | R／T／P／B／U；库存事务、MAX数量、消费和显示 |
| [iq](fields/iq.zh-Hans.html) | 继承并验证0～100；用于资质条件与修炼费用 | R／T／P／B；负资质上界、费用及剧情写回 |
| [item_experience](fields/item-experience.zh-Hans.html) | 下一周目清0；一次修炼成功清0并丢余量 | R／T／P／B；费用不足、成功、历史及失败保角色 |
| [item_state.users](fields/item-state-users.zh-Hans.html) | 本周保存全部user；下一周目完整恢复BASE | R／T／P；引用、定义比较、普通往返及重建 |
| [item_type](fields/item-type.zh-Hans.html) | 静态定义不继承；供库存白名单判断 | R／T／P；静态变化拒绝与Type 3／4筛选 |
| [knife](fields/knife.zh-Hans.html) | 继承不夹；升级及修炼保留固定写回边界 | R／T／B；MAX继承、固定写回域及失败 |
| [knowledge](fields/knowledge.zh-Hans.html) | 实际值继承，不按周目夹值 | R／T／P／B；宽值、资格与伤害分支 |
| [level](fields/level.zh-Hans.html) | 实际等级继承；不按周目封顶 | R／T／P／B；30级后成长、门槛显示及MAX继承 |
| [magic_ids](fields/magic-ids.zh-Hans.html) | 完整继承、固定ID验证；空槽与熟练度配对 | R／T／P／B／U；单招空首槽、NG+剧情首个0／−1空槽及菜单十槽容量均已修复；实际攻击、12个剧情指令及9个菜单病例通过Linux／Sanitizer专项，原有Legacy病例保持 |
| [magic_levels](fields/magic-levels.zh-Hans.html) | 保存并继承熟练度，不把显示等级当持久值 | R／T／P／B；十级后效果、使用增长、逐槽消息和宽值 |
| [make_item_experience](fields/make-item-experience.zh-Hans.html) | 所有角色下一周目清0 | R／T／P／B；制作奖励、产量、材料及失败RNG |
| [martial_level_step](fields/martial-level-step.zh-Hans.html) | 当前CFG供既有敌方增长及对照；不封顶玩家修炼 | 配置／敌方／武功模型、B；费用、成长、几何冻结 |
| [maximum_hp](fields/maximum-hp.zh-Hans.html) | 实际上限继承；不按周目夹值 | R／T／P／B／U；成长、治疗、药品、宽显示及溢出 |
| [maximum_mp](fields/maximum-mp.zh-Hans.html) | 实际上限继承；不按周目夹值 | R／T／P／B；成长、攻击、休息、当前值约束及溢出 |
| [MoneyId](fields/moneyid.zh-Hans.html) | 按正式钱币编号筛选；不是角色继承数值 | T／P；钱币174、MAX数量、扣款与保序 |
| [morality](fields/morality.zh-Hans.html) | 下一周目恢复BASE；剧情写回保0～100 | R／T／P；剧情受检加算、边界与恢复 |
| [mp_cap_step](fields/mp-cap-step.zh-Hans.html) | 当前CFG供既有敌方最大MP增长；不夹玩家上限 | 配置／敌方／B；BASE、当前MP例外、负STEP拒绝 |
| [mp_type](fields/mp-type.zh-Hans.html) | 继承并验证0～2，不夹非法输入 | R／T／P／B；枚举、修炼转换及颜色 |
| [mp](fields/mp.zh-Hans.html) | 下一周目回复到继承后的最大MP | R／T／P／B；耗内、MP攻击、恢复与上下界 |
| [name](fields/name.zh-Hans.html) | UTF-8实际值继承，可编码为最多10 bytes Big5 | R／T／P／U；别名、完整五字、NUL和非法编码拒绝 |
| [nickname](fields/nickname.zh-Hans.html) | UTF-8实际值继承，同姓名编码边界 | R／T／P；昵称往返、超长、NUL和非法编码拒绝 |
| [no_magic_count](fields/no-magic-count.zh-Hans.html) | 按角色／无关联秘笈ID继承成功历史 | R／T／P／B／U；NG+开关、40亿历史、普通与通关组合 |
| [physical_power](fields/physical-power.zh-Hans.html) | 下一周目100；本周固定保存域0～100 | R／T／P／B／U；医疗、移动、攻击、闲置及休息 |
| [poison_cap_step](fields/poison-cap-step.zh-Hans.html) | 已取消，不读取旧配置决定毒上限 | 配置／R／B／P；NG+毒0～99，关闭时保Legacy历史域 |
| [practice_item](fields/practice-item.zh-Hans.html) | 下一周目−1；本周必须引用秘笈 | R／T／P／B／U；装备修炼、费用、清空和类型拒绝 |
| [role_level_step](fields/role-level-step.zh-Hans.html) | 仅当前CFG供既有敌方等级增长 | 配置／敌方／B；最低等级1、不同STEP及溢出 |
| [sex](fields/sex.zh-Hans.html) | 所有角色恢复BASE；不推断其他值语义 | R／T／P；原生signed16域、自宫状态、BASE恢复 |
| [speed](fields/speed.zh-Hans.html) | 继承固定域实际值；装备速度单独计算 | R／T／P／B；启用与关闭模式、100边界及装备速度 |
| [sword](fields/sword.zh-Hans.html) | 继承不夹；升级及修炼保留固定写回边界 | R／T／B；5万亿继承、增长和零增量 |
| [taking_counts](fields/taking-counts.zh-Hans.html) | 下一周目恢复BASE；剧情执行与可保存域分开 | R／T／P／B；首同ID、首空槽、负数量执行与保存拒绝 |
| [taking_items](fields/taking-items.zh-Hans.html) | 下一周目恢复BASE；固定ID不机械扩宽 | R／T／P／B／U；携带槽、入队转移和消费时机 |
| [unused](fields/unused.zh-Hans.html) | 继承原值，不解释未知语义 | R／T／P；scalar映射、signed64往返及整名复制 |
| [unusual](fields/unusual.zh-Hans.html) | 继承不夹；修炼保持固定边界，升级不新增增长 | R／T／B；5万亿继承、修炼100及升级分支 |
| [用毒／抗毒／解毒](fields/use-poison-anti-poison-detoxification.zh-Hans.html) | 能力实际值继承；毒量0～99；各上毒路径统一 | R／T／P／B／U；毒发、上毒、药效、AI选择、宽结果与显示 |

## 跨字段合同

| 正式页面 | 要求集合 | 当前证据与缺口 |
|---|---|---|
| [主索引](../new-game-plus-design.zh-Hans.html) | 引用全部字段及跨字段规则；状态与证据范围一致 | 本表逐页登记57页；页面计数及引用检查只证明覆盖目录和链接结构 |
| [人物继承](support/data-and-inheritance.zh-Hans.html) | 实际入队历史、曾入队完整继承、恢复指定字段、未入队BASE、整个世界重建 | T源码逐项检查；实际入队、离队保留、Legacy不反推及转换模型已有测试；同一状态四次真实KDEF结局、通关保存与标题继承进入NG2～NG5已通过 |
| [库存](support/inventory.zh-Hans.html) | Type 3／4／MoneyId白名单，保序、保重复、数量不窄化，装备／秘笈不继承 | T／R／P；模型检查MAX数量、重复与源不变；游戏消费与显示分别已有执行测试 |
| [战斗结算](support/battle-calculation.zh-Hans.html) | 武功、装备、内伤、毒、医药、经验与敌方BASE按规定顺序和局部提交边界结算 | B；实际战斗组合覆盖NG1／2／3／4／5／999；不将组件或固定战斗升级为全部剧情及平衡验收 |
| [持久化](support/persistence.zh-Hans.html) | 版本／用途／资产、全部必需字段及域、S／D压缩、普通路由、Legacy转换、通关与标题继承、失败保持状态 | R／P／U；严格schema、域／引用、压缩、I/O故障及资源准备失败已有测试；同一状态连续获经验战斗、完整普通磁盘往返、公开标题普通读取、四次真实KDEF结局及标题继承到NG5已执行 |

## 当前剩余验收

| 要求 | 完成所需证据 | 当前状态 |
|---|---|---|
| 全部数值写点及中间量 | 按实际调用者核对权威值、运算边界、写回时机；不能只看词法计数 | 本轮源码审阅与相关执行验证完成；AI概率门与伤害、治疗、休息的随机增量已收敛signed64，新游戏增量亦在运算前提升；已核对角色成员指针、Legacy表示检查、TOML编解码、继承、经验／修炼、敌方成长与医疗分式计算；阅读行数不作为完成标准，字段规则按本表对应实现与执行测试核对；高级武功已核对正式几何权重、耗内补偿、按类去重中位数及连续成长公式；保留中间精度、仅最终floor并检查signed64范围，93个ID在10～80级共744组正式表值与既有1～10020级单调性测试均通过Linux／Sanitizer；敌方初始化、升级、修炼、制作、经验、轮末状态、HP／MP命中、施毒／治疗和投掷已定位相关写点。本次重新读取当前run_event的完整0～67分派，核对库存与携带入口、入队历史、休息候选、攻击／HP／MP及固定能力受检写回；指令参数、事件ID、坐标和动画仍为原生类型。已独立核对库存、入队／离队、携带物增减、固定能力受检夹值及试炼休息；商店已核对扣钱及加物均成功后再提交候选库存与stock；共享用药／治疗／解毒、装备／秘笈转移和库存消费已核对角色与RNG局部提交边界。战斗自由函数、升级／修炼／制作／战后结算已检查候选角色、库存及RNG提交；HP／MP命中、轮末状态、施毒／解毒／医疗、玩家与AI暗器、AI物品和休息已检查数值域及受检运算。正毒增量由模型限制为剩余毒量，坐标及hit_count保固定战场域；范围攻击按原逐命中提交，不改为整招事务。已补齐BattleSetup构造、敌方角色访问、状态显示、移动体力与round_value、攻击熟练度／耗内、AI毒／辅助计划以及战后消息逐条提交。结合模型、持久化、世界／剧情及实际菜单路径的既有核对，未发现尚未修复的数值缺陷；此结论为源码审阅及测试证据，不是穷举所有状态或动态原版oracle证明 |
| 同一状态连续战斗、普通存读、真实结局及标题继承 | 实际攻击与结算、完整磁盘快照、KDEF动画及输入、下一周目状态和源档不变 | UI6／17实际2/2：Linux48.84秒，ASan＋UBSan145.55秒，BUILD_EXIT均0；原战斗0实际主角与奖励、公开标题普通读取、KDEF1017结局和四次继承已检查。最新组合已改成战斗后经真实场景出口返回世界，完成淡入后用系统菜单保存，再由新会话公开标题读取，连续四次结局／继承到NG5；预期保存快照只按真实世界朝向同步header，其余全量比较。Linux UI17与全部武功表值2/2通过（51.16秒），Sanitizer同项2/2通过（150.23秒），BUILD_EXIT均0；日志`tmp/ngplus-continuous-menu-save-world-direction-{linux,sanitizer}.log`。现保留宽值链并增加普通NG1终点角色，同样执行四次战斗／菜单存读档／真实结局／标题继承。新链暴露普通字号UTC时间列越界，修复后Linux UI17／18共2/2通过（96.18秒，`tmp/ngplus-utc-ordinary-flow-linux-retry.log`）；随后姓名列分配及地点／完整UTC行逐像素专项也已通过。Sanitizer新链UI17／18已2/2通过（292.06秒，`tmp/ngplus-utc-ordinary-flow-sanitizer-retry.log`）；最新布局的Linux像素专项1/1通过（0.29秒）。当前最终布局与全部测试的Linux／Sanitizer完整套均166/166通过（145.11／308.59秒），BUILD_EXIT均0，日志`tmp/ngplus-current-complete-{linux,sanitizer}.log`。两种初态和出口坐标均由夹具选择，未冒称自然全剧情路线 |
| 自然成长与整体平衡 | 普通数值、真实经验／修炼增长以及NG2～NG5的可解释结果 | 用户已选择负责完整自然游玩与平衡验收，尚未收到实际结论；见[游玩验收交接](manual-acceptance.zh-Hans.md)。普通成长组合已通过：NG2～NG5每周32场，共128场实际战斗、葵花宝典修炼及原始KDEF931休息；最终等级82、攻击611、最大HP2757、熟练度1131、无关联成功历史32，完整普通磁盘往返一致。合法−1空武功槽入口拒绝已修正；Linux与Sanitizer全套各166/166覆盖该版本。初态为设定的普通NG1终点，未证明完整自然剧情或整体平衡；随机临时量版本已完成Linux／Sanitizer完整166项回归；随后解毒模型的两个随机量显式改signed64，解毒修改的Linux／Sanitizer相关回归均82/82，125.06／292.54秒；随后九处静态效果／制作／耗内输入扩宽的相关回归均84/84，122.77／294.03秒，BUILD_EXIT均0。再发现首槽−1、单招在后槽时攻击入口选错槽；修复后Linux／Sanitizer battle4／28专项均2/2通过，2.97／6.58秒，BUILD_EXIT均0。随后NG+剧情学习−1空槽及菜单容量修复也分别通过Linux／Sanitizer专项2/2和1/1；空槽修正版已完成Linux／Sanitizer完整166/166回归，145.73／318.28秒；之后医疗精度修正的关联验证各5/5，9.57／17.16秒；实际AI稀疏槽映射修复的Linux／Sanitizer关联各4/4通过，1.47／1.35秒；普通成长每周末已改为从实际回读快照继续继承，包含这项与医疗半伤边界的完整Linux166/166（162.13秒）及ASan＋UBSan166/166（319.85秒）已通过，BUILD_EXIT均0；日志`tmp/ngplus-exact-medicine-sparse-ai-{linux,sanitizer}-full.log`，未发现编译警告或Sanitizer错误 |
| 最终平台验收 | 当前最终代码经Windows统一BUILD实际执行全部适用测试 | 当前Linux／Sanitizer／Windows均166/166通过，145.11／308.59／165.11秒，BUILD_EXIT均0；日志`tmp/ngplus-current-complete-{linux,sanitizer,windows}.log`。Windows经build.bat完整执行，无专项regex；未发现编译警告或运行错误匹配 |
| 最终完成审计 | 每项正式规则有当前实现和同等范围的证据，所有必需缺口关闭 | 当前代码差异、三平台结果、受保护文件、文档结构与产物已核对；待用户完整游玩与平衡验收结论后完成最终判定，Goal保持未完成 |
