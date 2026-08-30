# 函数证据：`sub_2DF0E` `0x2DF0E..0x2E078`

状态：`implemented_pending_review`

KDEF opcode10。队伍槽只扫描1..5，首个 signed `<=0` 槽写 role ID；role ID0 因而被视为空。无论是否找到槽，随后都处理目标角色：四个携带物中每个 ID!=-1 均以原 count（包括0/负数）调用 `sub_2E571`、显示 notice、清为(-1,0)；再解绑两件装备与修炼物品的全局 item.user，角色三字段清-1、item_experience清0。清理不得包在“成功插槽”分支内。

C++ case10 已分离插槽/清理，不再把 count0 改成1，并复用 exact all-match inventory helper。真实 script11：slot1=role0、tail=0 后加入 role1 到slot1；carried item109 count0 不改变两个重复库存槽但仍出 notice/清字段；装备10/11、修炼12和各 item.user 全解绑。Linux Debug scene 通过；最终双向 REVIEW 尚未执行。
