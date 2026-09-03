# 函数证据：`sub_2DF0E` `0x2DF0E..0x2E078`

状态：`platform_adapted`
最终REVIEW：`converged_no_new_differences`

## 1. 物理边界与字节身份

- 入口/出口：`0x2DF0E..0x2E078`，362字节，91条指令。
- 当前IDA loaded bytes SHA256：`a884afc61c5a9b4bc46539ae2b7b6e00eb0024455c36b669494a61840fcdb4a2`。
- 原始`Z.DAT`文件偏移`0x2790E`起362字节SHA256：`39407bc9f8117c533fa91b1ef4af9c5cef7a7a2e5a975203de8d9fcd29fd84bf`。
- loaded/raw共有21个差异字节；每个均位于绝对地址operand高字节，loaded值比raw增加`0x02`，完整解释为DOS加载基址`+0x20000`重定位，无控制流或立即数差异。
- 唯一caller为`sub_2C319:0x2C58E`的opcode10分派：有符号读取`argument(1)`并压栈；helper返回后在共享`0x2C530`路径回收4字节、把脚本PC固定加2。helper末尾`xor eax,eax`返回0，但caller不解释该返回值。

## 2. 入口到出口机器行为

| 基本块 | 机器行为 |
|---|---|
| `0x2DF1B..0x2DF43` | 只扫描队伍槽1..5。槽值按signed word与0比较，首个`<=0`槽写入role ID低16位；槽0不参与。没有空槽时也继续后续角色清理。 |
| `0x2DF43..0x2E046` | 按slot 0..3处理角色四组携带物。ID恰为`-1`才跳过；其他ID读取同槽count并调用`sub_2E571(item,count)`。 |
| `0x2DF73..0x2DFB9` | 重新读取item ID，以190字节item record的byte2名称和Big5格式串`得到%s\0`构造提示；格式串7字节为`b16fa8ec257300`，SHA256=`eb9e0c4723aada0749bc6596182ed148a08cfe7ecd4debc3f3967cdb1535af6e`。 |
| `0x2DFB9..0x2E02A` | 设名称字节长为N：绘制`x=150-(4*N+16), y=40, width=8*N+52, height=27`的style4面板；文字位于`(160-(4*N+16),45)`，阴影5、前景7。随后present、等待任意非零键，再调用`sub_2D653`重绘并present裸场景。 |
| `0x2E02A..0x2E040` | 只有上述notice及裸场景present全部完成后，才把当前携带物ID/count写为`(-1,0)`，然后进入下一槽。 |
| `0x2E046..0x2E072` | 四槽完成后，把角色word 23、24、61、62依次写为`-1,-1,-1,0`：清两件装备引用、修炼物引用和修炼经验。 |
| `0x2E072..0x2E078` | 返回0。 |

函数中不存在物品记录word38 `item.user`的读取或写入；清角色装备/修炼引用不能扩写成解绑全局物品user。队伍插槽是否找到也不控制携带物转移与角色字段清理。

`sub_2E571`在这里是被委托的背包添加主体：全部重复槽、首个ID=-1槽、残留count和16位回绕合同仍由scene-event `audit_order=45`独立关闭；本条只固定调用参数和调用时序。面板、文字、等待键及present primitive也继续按各自closure独立计数。

## 3. 当前原资产域

独立Python按68槽真实宽度扫描全部1,018条KDEF：

- opcode10共80次，完整`<IIh:script_id,pc,role_id>`参数流SHA256为`bdad3c7d40a1a5a512ca7b6784924e527094b966b8a923b112b5c4a1d169f543`；首条`(10,160,1)`，末条`(999,50,76)`。
- 共26个role ID：`1,2,9,16,17,25,26,28,29,35,36,37,38,44,45,47,48,49,51,53,54,58,59,61,63,76`，范围1..76，全部落在320条角色记录内。
- `RANGER.GRP` 114,242字节，SHA256=`07b99e3c1676e18691f00d6dfe713121faa6f7429e43666bc83e1568cecb68ab`。上述26个角色共104个基准携带槽，其中67个非空；非空item ID全在1..171，count集合为`1,2,3,5,6,10,15,20,30,50,86,90,100`；所有空槽均恰为`(-1,0)`。
- 真实script11在PC50执行`(10,1)`；role1基准携带物依次为`(135,1),(84,1),(3,5),(11,10)`。
- 当前原资产没有非法role、非法item、负count或`-2`携带哨兵，因此现代边界保护不改变合法资产行为。

完整逐角色携带物、装备、修炼物与经验向量写入`scene-goldens.json:kdef.opcode_10_join_role`，oracle文件SHA256为`79551a99f932ad92c45087874e7c04ec282a89b69dc89ce428766d688ca31e1b`。

## 4. 汇编→C++收敛

首轮从机器入口独立推导后，对照发现三类产品差异：

1. 现代case10生成ASCII `item <id> <count>`，原版显示Big5 `得到+物品名`动态面板。
2. 现代在首个notice返回前已转移并清空全部携带槽、清装备/修炼字段；原版每件物品要等其notice确认和裸场景present完成后才清当前槽，全部物品结束后才清装备/修炼字段。
3. 现代复用退队清理helper，把三条全局`item.user`额外写为-1；原函数没有任何对应写入。

修正后废弃首轮结论，并从`0x2DF0E`重新完整推导：

- `SceneSession::run_event(case10)`保持首个signed `<=0`队伍槽、槽0不参与和PC+2；
- opcode10专用`join_role_items` continuation逐项执行add→原Big5 notice→裸场景present→当前槽清`(-1,0)`，四槽结束后才清角色四字段；
- synthetic count0重复库存槽仍不改变两个count但照常显示物品109原提示；首个notice与其present期间当前携带槽及装备字段仍保持原值，下一notice前才清前槽；
- synthetic物品10/11/12的`item.user=1`及真实script581物品1/2/3的`item.user=49`在入队后保持不变；退队仍由`sub_2E078`的`audit_order=34`独立终审；
- 真实script11验证team slot1原值0仍作为空槽写role1，四件携带物逐项提示并在各自present后清除。

第二轮入口到全部出口未发现新增差异或未决项。现代用宿主帧continuation替代DOS函数内同步等待，合法输入的状态、文字、几何、像素primitive参数、按键/present边界和脚本顺序一致，归类`platform_adapted`。

## 5. 验证

- 当前`Z.DAT` headless IDA重新导出完整91条指令、唯一caller、全部data refs和短callee bodies；原资产保持只读，IDA只打开`tmp/`数据库副本。
- 独立oracle覆盖80次opcode10参数流及26个真实角色的完整基准记录；二次生成逐字节一致（`proc_11ff`）。
- Linux app Debug根BUILD脚本：14/14通过（`proc_9f2d`）。
- order33静态门通过（`proc_1574`）；reverse framework 577行有效、scene-event剩65项（`proc_90e9`）。
- 动态原程序差分仍按统一策略记为`blocked_runtime_oracle`，不替代本次静态机器码与独立资产oracle。
