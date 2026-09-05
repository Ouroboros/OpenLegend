# 函数证据：`sub_34AEC` `0x34AEC..0x34C47`

状态：`platform_adapted / converged_no_new_differences`

映射：`BattleSetup::ai_escape_plan`与`BattleSession`逃跑/物品重定位continuation。

## 机器身份与边界

- 347 bytes、92条指令、9条分支、13处加载重定位；raw SHA256 `6e0f72a5051e1073414fade410106d3632865629b3acebc78382eb027e960831`，loaded SHA256 `93623d56fd7d837faae5f2326a6bae3f9cd643f168f1ebfb83283810c59669c0`。
- 13处加载重定位相对raw均为`0x20000`，归一化后逐字节一致。
- 两个caller：AI逃跑动作传原始参数0；AI普通物品重定位传原始参数1，返回后立即以mode0使用物品。
- 六个direct call依次为Watcom栈检查、movement path建图、两次32位绝对值、AI移动、休息wrapper；callee owner不随本项关闭。
- 原程序动态差分仍登记为`blocked_runtime_oracle`；本项结论来自原始`Z.DAT`完整机器码、加载重定位、caller上下文、独立原资产golden与现代测试。

## 汇编合同

入口把actor当前x/y作为source建立movement path图。最大得分为32位0，随后严格按x=0..63外层、y=0..63内层扫描；只有`path[y*64+x]`恰等于actor signed round value的格才是候选。

每个候选从slot0起扫描全部combatant，只按side过滤同伴；hidden、HP、role id、occupancy和活跃状态均不读取。异side槽贡献`abs(candidate_x-other_x)+abs(candidate_y-other_y)`，差值、绝对值和累计均为32位。候选总分strict大于当前最大值才替换，因此同分保留x外/y内的首格；最大初值0使总分0不写目的地。

扫描后仅当最大得分strict大于0时写目的x/y，并调用`sub_3650E(actor,mode0,value0)`；即使目的与actor原地相同也调用移动owner。第二参数恰为0时随后调用`sub_34AD3`休息，任意非0时跳过休息。函数无自定义业务返回，两个caller均不消费偶然返回值。

现代将同步函数拆成typed plan：`ai_escape_plan`保持建图、path等号、x/y顺序、仅side过滤、32位曼哈顿和、strict最大与空目的语义；AI逃跑用语义bool `rest_after_move=true`，有目的时执行mode0/value0逐格移动、无目的或移动结束时休息；AI普通物品用false，有目的时移动后恢复物品、无目的时直接使用。原始参数0/1的倒置含义由正向bool封装。非法actor/坐标安全失败属于合法域外平台适配。

每格render/present/两次tick及移动内部路径规则属于`sub_3650E` owner；休息wrapper已由order16关闭，均不由本项传播closure。

## REVIEW与验证

完整汇编→C++ REVIEW覆盖全部92条指令、9分支、13重定位、两个caller、六个call和全部出口，产品实现无需修正；最终从入口复核零新增差异。

真实field2向量固定source `(10,20)`、round value3、两个敌方时目的`(7,20)`且距离和20。新增独立路径边界固定：唯一异side槽与source同坐标、hidden=1且HP=0时，round1四个同分候选中x/y首格为`(9,20)`；全部同side时得分0且无目的；round0时原地`(10,20)`得分14并成为目的。逃跑休息与物品重定位不休息均有回归。

独立原资产golden双生成逐字节一致，正式SHA256为`9bbccc5bcc46eb9f606757a5b13f83bad5ecbaa156b248314ade6e7ae1296e7e`。根`./build.sh app --config Debug`完成Linux Debug构建，14/14测试通过；机器、静态/golden、closure和reverse framework门禁均通过。
