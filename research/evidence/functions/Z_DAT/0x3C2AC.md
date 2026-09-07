# 函数证据：`sub_3C2AC` `0x3C2AC..0x3C563`

状态：`platform_adapted`

映射：`BattleSetup::apply_battle_crafting`、`BattleSetup::commit_battle_crafting`、`BattleSession`。

## 机器身份

fresh临时IDA数据库从只读`Z.DAT`重建本owner：695 bytes、171条连续指令、33个IDA flow block、20个条件分支、2个无条件跳转、35处fixup、12次direct call且无本地RET。raw/loaded SHA256分别为`b4d9ca468242ba9ee849d655c3cfd8a14bc3055c9d5b6935c5fe8e14833f248b`与`9bfb26c8efcf38761e0ca9e7cb95c81ffd741a29c3544d2c35b34c13caac6f06`；35个loaded dword均严格为raw+`0x20000`，逆变换后全695 bytes匹配。

12个call目标按机器顺序为`0x3ED1E,0x3EEC7,0x3D612,0x3AA85,0x3EF4A,0x2CEBF,0x3D832,0x3D6D1,0x20C32,0x3D612,0x2B227,0x2B227`。唯一入口xref为`sub_3B387:0x3B6A1`；caller先拒绝side非0与练功物品ID恰为-1，真实提示抑制参数恒0。六个出口`0x3C30D/0x3C31B/0x3C392/0x3C3BB/0x3C4E1/0x3C55E`进入独立共享尾`0x3CBDB..0x3CBE3`，字节`83c4185d5f5e5bc3`；共享尾不传播为本owner closure。

## 行为合同

制造需求为signed `need_make_item_experience * (7-IQ/15)`，IQ除法向零截断；角色制造经验以unsigned word参与signed 32位比较，随后需求字段还必须signed大于0。完整word域的单次乘积最大绝对值为`71,794,688`，C++ `int32_t`乘法有定义。库存按0..199只取首个所需材料ID槽；后续同ID材料槽不参与资格。

五个配方按顺序以signed `material_count >= recipe_count`且产物ID不等于-1建立可用flag。至少一个可用后持续消费`bounded(5)`，不可用槽会造成重试；提示抑制参数非0也先完成这段选择RNG，然后不显示、不制造、不清经验。

正常路径严格先执行battle render、Big5 `%s 製造出 %s`、固定`(55,30,210,27)`框、`(62,35)`文字、present、清旧last-key并等待新非零键，之后才扫描产物库存和写状态。已有产物取首个同ID槽，消费`bounded(3)+1`并对数量word低16位相加；新产物写首个ID=-1槽且保留其旧数量word再低16位加1，不消费数量RNG。随后从先前记住的材料槽低16位扣配方数量，回绕结果signed不大于0时调用独立`sub_2B227`左移删除，成功最后清制造经验。

若提示后既无已有产物又无ID=-1槽，则库存已满：不消费数量RNG、不扣材料、不清制造经验。材料/产物同槽、材料ID=-1、空槽旧count非0、负配方数量和产物数量回绕均服从上述原地次序。

## 汇编→C++ REVIEW与回归

逐块对照`BattleSetup`的prepare/commit与`BattleSession`的preview/present/input/commit，覆盖全部171条指令、33块、22分支、35处fixup、12次call、唯一caller与六个出口，未发现合法caller域产品差异。Session在制造框前从共享RNG完成配方拒绝采样但不改库存；确认后才消费已有产物数量RNG并提交库存、材料和制造经验，等价原同步等待边界。非法角色/物品索引安全拒绝、typed结果与宿主continuation分相归类平台适配。

独立原资产oracle新增十组向量，锁定seed1选择序列`3,3,3,0`、抑制路径RNG终态`3295386429`、已有产物数量2及终态`4182499122`、首材料槽阻断后续同ID槽、新空槽旧count 7→8且无数量RNG、满库存提示后零提交、材料删除后左移、negative requirement/signed材料、产物count回绕及材料ID=-1与新产物同槽别名。向量SHA256=`8a3918fd77b1c109714d70b3b0c2a51eb8e01c8ae3d22560f3221beaa698c50f`。

两份临时Golden逐字节一致；第三次正式生成一致，历史71键逐值不变。72键正式Golden SHA256=`c9963790de069ce61cd899976114187d608561644e75e70b7ab8ae2dc65736b4`。直接API回归与BattleSession最终库存`material 3→1/product 4→6`、制造经验清0及共享RNG终态`4182499122`断言覆盖机器提交顺序。

本owner归类`platform_adapted / converged_no_new_differences`。栈探测、flag清零、RNG、renderer、格式化、box/text、present/input、库存删除、caller和共享尾均保持独立owner。
