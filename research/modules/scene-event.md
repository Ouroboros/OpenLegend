# 场景、事件与对话工作包

状态：`implemented_pending_review`；基础框架和诊断日志已完成，按有限 handler 组继续业务实现

## 1. 有限范围

当前专项报告锁定 `research/inventory/scene-event-closure.tsv` 的 96 个函数，包括场景会话、六层绘制、事件入口、KDEF 解释器、事件 helper、battle 入口和公共绘制/RNG helper。最终范围仍需与全局 ownership 人工对账。

## 2. 已有实现

场景资源、六层绘制、移动/碰撞、跳转/出口、TALK、基础 KDEF switch、app 同步链、天气和部分角色/物品副作用已有 C++ 与测试。所有行统一视为 `pending_mapping` 或 `implemented_pending_review`，不得继承旧完成状态。

原 `sub_29391..sub_295D0` 对当前/工作副本场景归档的固定记录 load/flush/reload 已映射到全100场景常驻 `GameSnapshot` 与 persistence 导入导出；正确 opcode WIDTHS 扫描确认311条外部事件修改、0条外部 opcode38 地图替换；真实 script436 固定外部 scene7 事件在首个同步 dialogue 前写回且当前 scene70 会话不切换。`sub_296E6` 的 normal/jump 入口、视口 clamp、状态清零和事件格统计，以及 `sub_29819/sub_299A1` 的水平/垂直动画、碰撞与坐标更新均已登记为 `implemented_pending_review`；垂直脚本步严格保留 step正负与direction0/3的原映射，真实 script343 的五帧图片和hash已重算。`sub_29B3C` 已恢复64×64事件格扫描并只推进事件word7，`sub_29C36` 已恢复交互脚本前的重绘呈现边界，`sub_29D2D` 六层绘制也改为从word7取事件图片；synthetic双格动画、空/有效交互和专用framebuffer hash均已固定。`sub_2B308/sub_2B3B4` 的物品与自动事件入口已恢复命中后的同步呈现、非正脚本上下文和 event_3 仅 `-1` 禁用语义；`sub_2C0BB` 场景名称按字节长度动态绘制原暗化圆角面板，并保留按键后裸场景呈现再检查自动事件的顺序。

opcode19 坐标/视口 clamp、opcode23 角色 use_poison 写入、opcode36 性别分支、opcode37 道德16位回绕后 clamp、opcode38 六层地图全图替换、opcode39 场景开放、opcode40 朝向/基础图片、opcode42 六槽女性队员条件、opcode51 随机江湖闲谈、opcode52/53 原 Big5 道德/声望数值面板、opcode55 当前事件字段条件和 opcode56 声望/十四书触发均已映射，并由真实 scripts 235/28/328/149/434/420/445/692/825/828/464/2 固定边界。opcode 0/13/14 的呈现/淡入淡出已提交；opcode 25 `sub_2ED8D` 的视口平移、opcode 27 `sub_2F053` 的玩家/事件图片动画、opcode 30 `sub_2F171` 的碰撞感知逐格行走、opcode 44 `sub_2F9F2` 的双事件联动图片动画、opcode 57 `sub_301D1` 的玩家/三事件两阶段动画，以及 opcode 62 `sub_30B81` 的结局前置双事件动画均已恢复原范围边界、逐帧呈现和对应 BIOS tick 等待，并登记为 `implemented_pending_review`。其后 `sub_30C3D/sub_31241` 已接入 ENDCOL 调色板、ENDWORD 标题/职员表滚动、KEND 221帧影像、两次任意键等待，以及关闭宿主资源后输出原终端文字并以状态0退出。opcode58 `sub_302E0/sub_30480/sub_30510` 已改为按胜负逐场推进的五轮随机试炼，保留重复 RNG 消费、轮间 8 tick 黑屏与条件恢复；opcode59 `sub_30559` 已恢复 i=6 越过队伍数组读取首库存 item ID 的原 BUG、完整离队装备清理和 36 个跨场景事件禁用。opcode60 `sub_30A5A` 已修正为只比较 current_picture，并覆盖当前/外部场景及 begin/end 伪命中反例；opcode61 `sub_30B45` 已固定 event11..24 全十四项 current_picture=4664 的 true/false 分支。opcode10 join 已保留队伍 slot signed `<=0`、无空槽仍清角色、count0 携带物及装备/修炼解绑；opcode41 携带物修改保留非正 count 及仅 ID==-1 为空；opcode12 休息只恢复首个非正槽前的连续队伍前缀；opcode22 清 MP 保留 slot0 无条件与后续槽仅 role ID>0。opcode2/共享 `sub_2E571` 已保留全部重复 ID 槽加法、首空槽残留 count 及无 normalize；opcode32/`sub_2F39C` 只改首匹配槽并在非正时压缩删除，shop 亦按首 money 槽判价/扣款。十四书与书信触发只按 ID presence。opcode33/35 magic 写入保留 ID0 空槽、满槽覆盖0和显式槽；opcode34 IQ 保留 `[0,100]` clamp。opcode54 清全部84条 RANGER scene metadata 并恢复2/38/75/80四特例；100份场景地图不等于100条 metadata。opcode45/47 speed/attack 保留 `[0,100]` clamp；opcode46/48 maximum MP/HP 不 clamp、current 直接赋新 maximum，且 HP notice 才检查在队。opcode49 直接写 mp_type。opcode18/43 及 opcode50 五物品条件均只检查 item ID 存在、不读取 count；opcode20 只判断最后队伍槽 signed `>0`，不遍历全队。opcode28/29 已固定 morality inclusive 区间与 attack 第三参数未读取；opcode31 只读取首个 money 槽而不汇总重复槽。opcode63 `sub_31284` 已固定角色 word14 写入；opcode64 `sub_312A6` 已恢复实际购买、反馈后商人 event_3=939 写回与 continuation 所有权；opcode65 `sub_31945` 已恢复 scene0/1 等隐藏范围、全部事件字段和固定 RNG 激活；opcode66/67 音乐/音效 wrappers 已固定相对 fade/dialogue 的命令顺序。opcode58 失败后 `sub_2E659` 已恢复 DEAD.BIG 死亡画面、动态日期/姓名、三槽读取、方向循环、退出确认与非Y返回；读取前保留清屏呈现边界，失败后回收至原选中项。opcode62 后续无返回转入的完整结局及 shutdown 链已登记为 `implemented_pending_review`。其余 handler 继续按机器码和真实脚本分组补齐。

## 3. 实现阶段与最终 REVIEW

允许先按完整 KDEF helper/handler 组补齐 B7 实现，再统一 REVIEW。最终 REVIEW 必须按机器码入口而非 opcode 命中顺序执行，逐项登记参数宽度、PC 推进/改写、同帧继续、跨帧让出、阻塞条件、状态副作用、异常出口和原 BUG，并反查全部 C++ 行为。

## 4. 停止线

96 行和后续 ownership 对账补入项全部关闭；任何 `pending_mapping`、`pending_implementation`、`implemented_pending_review` 或 remaining 非空都阻止 B7 完成。
