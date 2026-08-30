# 函数证据：`sub_2FEDF` `0x2FEDF..0x2FF87`

状态：`implemented_pending_review`

KDEF opcode50。一次扫描全部200个库存 slot，为五个参数 item ID 分别置 presence flag；完全不读取 count。五 flag 全1返回 true_offset，否则 false_offset；caller 宽度8。重复 item 参数亦只按各自 flag 处理。

旧 C++ 用五个汇总 count>0；现逐项 `inventory_contains_id`。唯一真实 script676：五个 ID138..142 均以 count0 存在仍 true→移除并 dialogue2482；缺任一 ID false→dialogue2481。Linux Debug scene 通过；最终双向 REVIEW 尚未执行。
