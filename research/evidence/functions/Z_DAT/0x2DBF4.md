# 函数证据：`sub_2DBF4` `0x2DBF4..0x2DD45`

状态：`platform_adapted / converged_no_new_differences`

## 1. 入口与物理字节

- 当前 `Z.DAT` loaded入口为 `0x2DBF4..0x2DD45`，共337字节、83条指令。
- loaded函数字节SHA256：`d2e9050a19528c43b63e2eb23b5e0bba12d90d42e14016c994e20d9d763574d2`。
- 原始 `Z.DAT` 唯一匹配offset为 `0x275F4`，raw函数字节SHA256：`e00185c2c24269ac8363297748f1b62ccb6d0aca63330049ab12c475a47755dc`。
- loaded/raw共有21个差异字节，全部由DOS加载基址 `+0x20000` 的绝对地址重定位解释，无未覆盖字节。

## 2. 唯一caller与解释器宽度

完整CodeRefsTo只有 `sub_2C319:0x2C745` 一个物理callsite，即jump-table case26。

`0x2C718..0x2C744`把KDEF `pc+5..pc+1` 五个word逐一 `movsx` 后压栈，因此callee参数顺序固定为：

1. 目标scene；
2. 目标event；
3. event_1 delta；
4. event_2 delta；
5. event_3 delta。

callee返回后跳到共享尾部 `0x2C639`，执行 `add esp, 0x14` 和KDEF PC `+6`；返回值不参与任何条件。

## 3. 当前与外部事件区

当前事件区分支条件与scene归档状态一致：

- 目标scene为 `-2`；或
- scene状态非0，并且状态不为1，或状态为1且目标scene等于当前scene。

当前路径只把event `-2`解析为当前event；其他event值直接作为22-byte事件记录下标。机器不为event `-1`提供特殊分支。

目标scene不是 `-2` 且scene状态为0，或状态为1且目标scene不同，进入外部归档路径。该路径必要时打开 `ALLDEF.IDX`，从 `ALLDEFBK.GRP` 按目标scene偏移载入完整4,400-byte事件区；event参数不解释 `-2/-1`，修改后把整区写回并关闭文件，当前scene内存事件区不被替换。

现代 `GameSnapshot` 常驻100个scene事件区，直接写目标scene记录，保持合法域的最终可观察状态；越界scene/event及外部负event被安全拒绝，不执行机器未定义的越界内存访问，故归类 `platform_adapted`。

## 4. 三字段16位加法

两个归档分支均按固定顺序对目标事件记录word2、word3、word4执行x86 `add word ptr, r16`：

- delta由caller从int16 KDEF word有符号扩展；
- 存储只使用delta低16位；
- 每字段独立按二补码16位回绕；
- 不钳位、不饱和，也不把delta `-2`解释为“保持”；
- 前一字段结果不会跳过后续字段。

现代case26使用显式 `wrapping_add`，先在 `uint16_t` 域相加，再按 `0x8000`边界还原 `int16_t`，不依赖实现定义的窄化转换。两条机器路径最终均返回0。

## 5. 全KDEF域

独立解析当前1,018条KDEF得到121次opcode26。按 `<script:u32, pc:u32, 5×int16>` 串联的完整参数流SHA256为：

`5637e9a38a976f3ad7d1aa8aa1eb0e54d039ecf305cfece583e8b28d82660b9c`

域分布：

- 115次scene `-2`；6次显式外部scene；
- 显式外部调用为scene73/event2两次、scene27/event0四次；
- event只出现 `0,1,2,4,5,6`，没有event `-2/-1`；
- delta仅有 `(0,0,1)` 21次和 `(0,1,0)` 100次；
- 首条为script95 pc88：`(73,2,0,0,1)`；末条为script799 pc13：`(-2,1,0,1,0)`。

正式oracle同时冻结全部121条参数流、七组scene/event频次、两种delta模式和六条外部scene调用。

## 6. C++ REVIEW与验证

从机器入口单向对照 `SceneSession::run_event(case26)`：

- scene `-2`、当前路径event `-2`、三字段顺序、显式16位回绕和PC `+6`一致；
- 外部scene写入目标 `GameSnapshot` 记录，当前scene同event保持不变；
- 现有synthetic script11固定当前event `32767+1→-32768`、`-2+2→0`、`3+3→6`；script13固定scene69/event5的 `10/20/30 + -1/-2/-3 → 9/18/27`，并核对scene70同event与当前scene身份不变；
- 首轮完整入口→caller→C++ REVIEW未发现合法域产品差异，无需修改产品代码或测试。

验证：

- `research/tools/generate_b7_scene_goldens.py --data-root ..`成功并二次重生成幂等；
- `research/evidence/scene-goldens.json` SHA256：`ee33a2024e75aaa9d90d37b4b62db4d9ec6b12c8ea01cb9c5169b37e12cab03f`；
- `./build.sh app --config Debug`：14/14通过（`proc_cccb`）；
- 原程序动态oracle继续统一登记为 `blocked_runtime_oracle`。

结论：`platform_adapted / converged_no_new_differences`。
