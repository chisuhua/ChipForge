# fetch_stall protocol — v0.2 (v0.11.0)

> **状态**: v0.2 **rev.14 定稿** —— critic 十五轮 **APPROVE**（判据层 0 BLOCKER / 0 SHOULD_FIX）（`openspec/changes/mmu-chmem-phase-b-c-launch` tasks.md §3.5.3 的验收条件）
> **owner**: mmu-chmem team（原 owner wave5-bp-btb 已于 2026-10-19 S3 defer 到 v1.0.0 ⇒ 2026-10-22 改由 mmu-chmem team 承接）
> **取代**: `fetch_stall_protocol_v0.11.0.md` (Draft v0.1, 237 行) —— 该稿经 critic REJECT（5 BLOCKER + 7 SHOULD_FIX）后撤回
> **上游**: `.rddf/protocols/fetch_stall_requirements_v0.11.0.md`（5 BLOCKER + 7 SHOULD_FIX 的来源）
> **落地约束**: 归属 change **`chmem-ptw-walk`**（未立项），**不作为独立插件落地** —— 理由见 §6

**修订记录**

| 版本 | 日期 | 内容 |
|------|------|------|
| **v0.2 rev.14** | **2026-10-22** | critic 十四轮：**APPROVE_WITH_NITS —— 十四轮以来判据层首次零 BLOCKER**（机制层自 rev.3 起连续 12 轮 APPROVE）。critic 独立复跑：⓪ 范式两条硬指标实测闭合、W1/W2 与真实代码 `:266-282`/`:167` 逐 token 对得上、8 正例全 PASS / 4 违例全 FAIL（**不复用作者脚本**）、作者脚本 13/13 一致。修 2 SHOULD_FIX：**SF-1**（⓪ 的"LHS 含 key 名"可被局部别名绕过 ⇒ `n->operator()(kf) = …` 脱离检查 ⇒ 门静默空转）⇒ 加「**LHS MUST 字面含 key 名 —— MUST NOT 经局部别名/宏间接写 key**」；**SF-2**（操作数定义语句的识别规则没写 + 词表漂移纪律没钉）⇒ ⓪ 补「操作数定义语句 = 同一闭包内 LHS 含该操作数名的语句，**全部**检查」+ W1 逃生条款扩为「**任何新 token 来源**（含定义语句词表变化）MUST 同 commit 修订 W1」+ pin「3c 的两条定义语句 MUST 保持 `:266-270` 词表形态」+ **明文不做"传递闭包"检查**（需 def-use 分析，非 grep 机械可判 ⇒ 反而制造不可判定；漂移的救济就是"改 AC"）。4 NITS：`ch_literal` 去重、⓪ 改为「**全文先剥注释再做一切**」（否则 RHS 注释里的 `;` 会截断终止符）、AC-FS-15 (b) 补「**观察面不钉文件**」澄清（walk 触发挪到哪个插件都不影响该锚）、并把判据自测脚本的归宿写死（**3c change MUST 把等价自测收进 `tests/` 或 `tools/`**，勿依赖 `/tmp`）。 |
| **v0.2 rev.13** | **2026-10-22** | critic 十三轮：**机制层第 12 轮连续 APPROVE**。REJECT 2 个 BLOCKER（**均在 rev.12 的白名单判据自身**）：(1) **提取方式未指定 + W 表混用 qualified 形态** —— 机械提取必得 bare token，而 W 写的是 `operator()` / `MmuKeys::PTW_ACTIVE` / `IBusPlugin::FLUSH` ⇒ 同一合规语义两种合法读法给出**相反**判定（读法 A FAIL / 读法 B PASS）⇒ 检查者可自选解释（与 rev.10 的 `grep -nE` 大小写敏感同构）；(2) **W2 缺 `hold_z`** ⇒ 协议 §9.1 自述的 3g 形状 `FLUSH = br_taken_buf | hold_z` 在**两种读法下都 FAIL** ⇒ 规格在自家作用域内不可满足（与 rev.10 行号 allowlist 同构）。修：新增 **⓪ 提取范式**（写语句 = LHS 含 key 名 + `;` 终止 + 可跨行 + **全部**写语句；RHS = `=` 至 `;`；剥注释；`[A-Za-z_][A-Za-z0-9_]*` 取 token，限定名**每段**都算）；W1/W2 改 **bare-token 形态**；W2 补 `hold_z` / `hold_z_`；§7 item 1 钉 canonical 拼写（成员 `hold_z_reg_` + 语句用 `hold_z`，照 `pc_lag_reg_`/`pc_lag` 惯例）。**SF-1（W1 scope 歧义）** 采纳方案 (a)：W1 scope 扩为「`PTW_ACTIVE` 写语句 **+ `paddr_valid`/`sv_active` 定义语句**」⇒ **操作数投毒**（`paddr_valid = sv_active && hit && !PTW_FAULT`）被机械覆盖；`hit` 在表里的理由因此落地。**并按 critic 建议先做判据自测再送审**：写 `/tmp/opencode/wl_check.py`（13 例）⇒ **13/13 符合预期**（7 正例 PASS / 6 违例全被抓住），**且自测抓到 1 个真缺口**（初版 W1 漏 `bits`/`kXlenBits`/`satp_` ⇒ `sv_active` 真实定义被判 FAIL = 合法实现假红）⇒ 已补入 W1 并把自测结果写进 AC-FS-16。<br>N-1（快照流程：rev.11 快照实际是中途态 —— **本轮起快照一律在 rev.N+1 任何改动落盘前保存**）／N-2（3c 执行陷阱：`MMUPlugin_chmem.h:276-280` 现有注释"PTW_ACTIVE/EXCEPTION_CODE 本增量恒 0、TLB miss 不是 fault"与 3c 目标语义冲突 ⇒ 已在 §6.2 登记"3c 的 tasks MUST 含改写该注释"，**本协议不改源码**） |
| **v0.2 rev.12** | **2026-10-22** | critic 十二轮：**机制层第 11 轮连续 APPROVE**，且 rev.11 的 diff **首次可验证**（`diff /tmp/opencode/protocol_rev10_base.md …` = 4 hunk，机制段零触碰）。REJECT 1 个 BLOCKER —— **交付物结构**：rev.11 把 ```bash 围栏**塞进了表格行内** ⇒ 围栏计数变奇数（5）、AC 表在 `:575` 断裂、`tools/doc_checker.py` 的朴素 toggle 使 **EOF 仍 `in_code_block=True`** ⇒ 尾部 66 行（含 §9/§9.1/§10 跟踪表 38 行）**全部逃检**。修：围栏移出表格（命令改**内联**一行 + `<br>`），实测 `grep -c '^```'` 回到 **4（偶数）**、`EOF in_code_block=False`、skipped 行数回到 116（与 rev.10 一致）。<br>另 2 SHOULD_FIX：**SF-1**（"内联 + 区间内无 fault"**堵不住间接绕过** —— 派生式必须用命名操作数 ⇒ 无法机械区分中性别名；且"候选 + 自证说明"是自证通道）⇒ ①② 改用 **RHS 操作数白名单**（W1/W2 固定表，白名单外标识符 ⇒ FAIL，**与命名无关**；3c 需新操作数 MUST 修订 AC ⇒ spec 变更可见、fail-closed），③ 降级为**提示性扫网** + 候选 MUST 登记**可核对工件**（原文抄录 + 定义处）而非散文自证；**SF-2**（实例同一性锚只覆盖"适配器自持"一种拓扑）⇒ 给两条**机械锚**：(a) **句柄同源断言**（`pb.plugins()` 取句柄，指针相等）+ (b) **因果耦合证明**（好 PTE ⇒ refill / `PADDR_VALID` 0→1；好/坏行为相同 ⇒ 判 FAIL，**与拓扑无关**）。<br>3 NITS：tasks.md 轮次滞后、rev.10 行的 `git add -N` 说法加更正注、`state_out()==4` 残留仅存于历史记录行（正文已用 `PtWalkState::FAULT`） |\n| **v0.2 rev.11** | **2026-10-22** | critic 十一轮：**机制层第 10 轮连续 APPROVE**（上轮 1 BLOCKER + 2 SHOULD_FIX + 3 NITS 全落实）。REJECT 1 个 BLOCKER —— **判据层**：AC-FS-16 ③(d) 的 allowlist 是**行号键控的封闭集合**，而本协议自己的 §7 item 1（3g 在 `ibus_chmem.h` 成员区新增 `hold_z_`，插入点在 `:80`）**必然使 `:143` 漂移** ⇒ 规格在自己声明的作用域（④ 含 3g）内**不可满足（假红）**；而补救路径要么被"固定"堵死、要么等于允许就地扩项（**假绿**）—— 且与 `:24` 的"行号仅作定位辅助"自相矛盾。修：**行号 allowlist → 变换判据**（对注释过滤后的命中行删掉所有 `default`（不区分大小写）后重新 grep，仍含 `fault` ⇒ 候选）。**实测**：pristine 两文件候选为空；按 §7 item 1 插入 `hold_z_` 成员后**仍为空**（漂移免疫）；注入 `!sm.is_in(PtWalkState::FAULT)` ⇒ **被抓住**。理由：`default` 是唯一与 `fault` 相交的合法词（`de-fault`），且 `fault` 不是 `default` 的子串 ⇒ 无法用命名同时消掉两者。另 3 SHOULD_FIX：② 的要求由"只查 `hold` 行"升级为**文件级/逐语句**（与 ① 对称，否则 `FLUSH`/`hold_z_` 两行不在检查对象内）；AC-FS-15 前提断言加**实例同一性**锚（观察对象 MUST 是适配器消费的同一 FSM 实例）+ 驱动权归属 fail-closed 说明；① 的 FAIL 判据改锚**数据流**（fault 不得参与 `PTW_ACTIVE`/`hold` 派生）+ 派生式**必须内联在赋值语句内**（堵住中间变量绕过）+ scope **跟随 AC-FS-15 点名的 producer 文件**（`RiscvMMUPluginChmem` 当前全仓不存在）。5 NITS：AC-FS-15 行标签 rev.9→rev.10/11、"标识符"口径改"出现 fault 子串的位置"、`state_out()==4` 改 `PtWalkState::FAULT`（枚举 4）、harness 驱动权 fail-closed、①③ 分工提示（防把 ③ 当 ① 用）。<br>**可审计性**：rev.10 快照存 `/tmp/opencode/protocol_rev10_base.md` ⇒ 十二轮有真 diff base（`git add -N` 只给空 blob，不可用 —— critic 十一轮已实测指出） |
| **v0.2 rev.10** | **2026-10-22** | critic 十轮：**机制层第 9 轮连续 APPROVE**，上轮 3 SHOULD_FIX + 3 NITS 全落实。REJECT 1 个 BLOCKER —— **规格与自己的 token 枚举自相矛盾**：AC-FS-16 ① 枚举里点名 `PTW_FAULT`（大写），而 ③ 规定的 `grep -nE 'fault'`（照抄 `check_plugin_portability.sh:482` 范式）**大小写敏感** ⇒ 拦不住它自己点名的违例；且 `PTW_FAULT` 是裸 `Payload<uint8_t>` ⇒ CH_MEM 下只能装常量、消费端 `has()` 为 false ⇒ `!PTW_FAULT` ≡ 常量 1 ⇒ **行为门（AC-FS-15 ②）对它天然无感** ⇒ 两道门同时失明。另 2 SHOULD_FIX：① ① 的作用域"该语句行区间"是**语义概念**，检查者可自选边界把 fault 读挪到区间外 ⇒ 钉死为**闭包体/文件整体** + 固定 allowlist（`default` 一词含子串 `fault` ⇒ `MMUPlugin_chmem.h` 1 项 `= default`、`ibus_chmem.h` 3 项（`:43`/`:44` 的 `= default` + `:143` 行尾注释，行首过滤拦不住））；② AC-FS-15 ② 的判别力只来自那 1 拍 FAULT，而无前提断言 ⇒ 补 **anti-vacuity 前提断言**（三类坏 PTE 各自 MUST 观察到 `state_out()==4` ≥1 拍，未观察到 ⇒ 判 FAIL 不是 PASS）。3 NITS：`:55` 的"在途标志"措辞残留（改"无有效翻译的标志"）、AC-FS-14 ⑤ 的失败模式按合并批次口径改写、tasks.md 的"五轮/待第六轮"更新为十轮。<br>**可审计性**：本文件已 `git add -N`（intent-to-add）⇒ 后续轮次有 diff base<br>⚠ **已被 rev.11 更正**：`git add -N` 只把**空 blob** 放进 index，给不了内容 diff base（critic 十一轮实测指出）⇒ 真正的 base 是 `/tmp/opencode/protocol_revN_base.md` 快照 |
| **v0.2 rev.9** | **2026-10-22** | critic 九轮：**机制层第 8 轮连续 APPROVE**，上轮 2 BLOCKER 清零、12/12 落实。rev.8 引入 1 个新 BLOCKER —— 且**恰好证明了"判据比机制更容易出洞"**：它的两处修改**各自**打开了一条绕过路径，组合起来可让"接回 fault 并释放冻结"的实现**同时通过 AC-FS-15 与 AC-FS-16**。(1) **AC-FS-16 ① 的 `grep -nw fault` 把 token 收窄到一个词** —— `-w` 的词边界在 `_`/大写处不成立，实测只命中 `!fault`，`fault_live` / `ptw_fault` / `PTW_FAULT` / `fault_code()` 全漏；而这三个正是**本文自己诱导**的违约写法（(b) 的变量名就叫 `fault_live`、明令禁止的事就叫 `PTW_FAULT`）。(2) **AC-FS-15 ② 的窗口限定「walk 在途」把 fault 拍排除在外** —— `mmu_ptw_chmem.h:283` 是 `FAULT → IDLE` 无条件、`fault()` = `sm.is_in(FAULT)` ⇒ `fault` 只有 1 拍，而那一拍**正是 walk 已终止**；`PTW_ACTIVE = sv & !pv & !ptw_fault` 只在该拍掉 0 ⇒ 窗口外 ⇒ ② 不 RED，与 ② 自己的"会掉 0 ⇒ RED"论证矛盾。**这正是 rev.3 从 FS-C-12 删掉同一限定的教训重演**。修：(1) ① 改**子串** grep（作用域已是语句行区间，区间内无 `default`）+ 排除注释行（范式 `check_plugin_portability.sh:482`）+ 明文禁用 `-w` 并附实测反例；(2) ② 窗口改按**取指事实**定义（从 miss 检出到 `PADDR_VALID=1` 或窗口结束，**含 FAULT 拍及其后各拍**）+ 明文「不得用 FSM 在途定义窗口」。另修 SF-A（AC-FS-15 加"不靠桩"+ 点明生产者 = `MMUPluginChmem` 的 fetch EARLY 闭包 + 指出 `PtwFixture` 只注册 FSM 不能照抄）、SF-B（AC-FS-16 ④ 改"合并交付后整体退役，两处由 AC-FS-14 ⑤ 接管"；AC-FS-14 ⑤ 扩为"两处都必须有，缺一即违约"）、SF-C（`:100`/`:118` 的 `ch_uint<8>(0)` 改 `ch_literal` 形式，对齐 FS-C-4）、N-1/N-2/N-3 |
| **v0.2 rev.8** | **2026-10-22** | critic 八轮：**机制层第 7 轮连续 APPROVE**，上轮 2 BLOCKER **清零**，5 SHOULD_FIX + 7 NITS 12/12 落实。但 rev.7 **新引入** 2 个 BLOCKER，全在「本轮新增的两道 3c 门自身」：(1) **(b) 的载体不存在且被本文四处封死** —— `hold_z_` 在 `ibus_chmem.h`、`fault` 产生在 `mmu_ptw_chmem.h`，跨插件唯一通道是 payload cell，而 §2.1/§5/§6.2/§9.1 四处都写「不引入 cell」⇒ 实现者要么跳过 (b)（静默复现 rev.6 的洞）要么硬接一个 cell（违反 MUST）。(2) **AC-FS-15/16 按字面不可执行**：① `reserved` 编码写错（`R=W=X=0` 实为 `is_ptr` **正常路径**，真 reserved 是 `R=1 && W=1`，既有测试 `test_mmu_ptw_fsm_chmem.cpp:145-157` 是权威）② AC-FS-15 ③ 的提交断言在 3c **无观测面**（MMU 未注册进 CH_MEM 工厂，`grep` = 0 命中；抑制接线归 3g）③ AC-FS-16 的「grep 计数为 0」**不可满足**（pristine 的 `ibus_chmem.h` 有 3 处 `default` 命中）④ AC-FS-16 ② 与 (b) **字面互斥**（同一个 branch-LATE 闭包）。修：**(b) 载体点名 `mmu_keys::EXCEPTION_CODE`**（`mmu_keys.h:86` mode-aware，`MMUPlugin_chmem.h:282` 已写在 `tlb_lookup_ifetch` 同节点，无需新 key），并把四处「不引入」统一限定为「**3c/3g 增量**不新增 key、不接回 `PTW_FAULT`」；AC-FS-15 三类坏 PTE 改按代码编码 + 窗口限定 `sv_active=1`（Bare 下 `PTW_ACTIVE` MUST 为 0，否则全机冻结）+ ③ 移出 3c 门（并入 AC-FS-11 ①）；AC-FS-16 改 `grep -nw` + 明确作用域（语句行区间）+ 加「仅约束 3c/3g，合并批次的 `hold_z_` fault 项是唯一合法例外，由 AC-FS-14 ⑤ 接管」；另修 SF-1（`script()` 与 `script_freeze_then_release()` 后写覆盖前写 ⇒ 改三参数一次设全）、SF-4（FS-C-9 补合并批次例外）、SF-5（作用域改「ifetch 交付面」，FSM 的 `FAULT` 是既有代码）、SF-6（「两坏结局都排除」加双前提）、N-1/N-2/N-3/N-4/N-6 |
| **v0.2 rev.7** | **2026-10-22** | critic 七轮：**机制层继续 APPROVE**，且 critic **撤回**了它的机制改法 A（A 的载体论证经其独立核实成立：`mmu_keys.h:64` `PTW_FAULT` 是裸 `Payload<std::uint8_t>`）。但 REJECT 两点：① **FS-C-18 的 3c 半段零验证** —— §5 自称"可测 MUST"却只挂 AC-FS-14，而 AC-FS-14 自述"不进 3c 门"，§6.2 的 3c 验收项又不含 FS-C-18，AC-FS-9 又明写排除 fault ⇒ 三处互相推诿；且违规非假想，`mmu_ptw_chmem.h:136` 的 `FAULT = 4` 与 `:190 fault_code()` **已经存在**；② **"同批交付"不足以抑制** —— rev.6 把"或让 PC 立即转向 mtvec"写成二选一，但它自身 F+1 行已证明 `FLUSH = hold_z(F+1) = hold(F) = 0` ⇒ 裸提交；PC 重定向改的是 `pc_reg` next，拿不掉 F 拍已发起的读。修：新增 **AC-FS-15（行为负向）+ AC-FS-16（结构反回归）** 并挂进 §5 / §6.2 的 3c 门；删掉"mtvec 二选一"支，改为"抑制只有一个有效途径 = 与 `br_taken_buf` 同构的 1 拍延迟 flush"；FS-C-18 补交付清单 (a) `trap_pending` 第 4 驱动源 / (b) 同构 flush / (c) `mepc ← faulting PC`；§9.1 加"在 fault 语义交付前两者无共用信号"；§1.2 fault 归属改指 FS-C-18；§6.2 依赖图改旁挂节点；桩补"只驱动 busy/valid/cnt，跨页场景须另调 `script()`" |
| **v0.2 rev.6** | **2026-10-22** | critic 六轮：**机制层继续 APPROVE**（上轮 1 BLOCKER + 4 SHOULD_FIX + 7 NITS 逐条落实；编译由 critic 独立复跑 EXIT=0；点名的 3 处行号/断言全部核实通过）。REJECT 只卡 1 个由 rev.5 的 `fault` 排除项**新引入**的洞：`hold` 一个变量承担两个语义（「PC 该冻结」+「本拍取指是垃圾」），fault 排除只该关前者却把后者一起关掉 ⇒ fault 拍发起的 bare 取指在 `F+1` 裸提交，且每拍持续。**修法（不选 critic 的 A）**：A 需要 `fault` 的 live 载体，而 `PTW_FAULT` 是裸 `uint8_t`（模式化属 3d，排在 3c 之后）⇒ 等于把洞挪到 3d。改取**次序硬约束 FS-C-18**：fault 语义 MUST 与 trap redirect 同批交付，在此之前 walk MUST NOT 产生 fault（3c 只做 hit/miss/refill）⇒ 两个坏结局（永久挂死 / 连续裸提交）都被排除在交付物之外。连带：§9.1 重写（含 `EXCEPTION_CODE` 可观测面）、§6.2 依赖图把 fault 移出 3c、新增 AC-FS-14（合并交付项，不在 3c 门）、桩 rst 改 `valid=1`（修"桩自身违反不变式"）+ 补逐拍 skew 表、§7 item 9 case 数重算为 10、`is_trap_1` 两处残留统一、§2.2/§2.4 去重 |
| **v0.2 rev.5** | **2026-10-22** | critic 五轮：**机制层继续 APPROVE**（逐拍表复用上轮逐格验算、嵌套等价、CSR 单点覆盖 7 路 + 2 臂已实证、rev.4 的 2 BLOCKER 由 critic 独立复跑确认清零）。REJECT 只卡 1 个**我自己引入**的矛盾：FS-C-12 主句漏 `fault(C)=0` ⇒ page fault 拍按主句 MUST 为 1 ⇒ `pc_reg` 永不释放，**正是 §9.1 明禁的"永久挂死"**，且与自己同段的派生公式互斥。修：主句补 `fault=0` + "fault 周期 MUST 归 0"，并加 fault 变体 AC。另修 4 SHOULD_FIX（§2.1「产生方」仍写 FSM busy；桩 `script()` 只能给常量 ⇒ 补 live 节点逐拍示例；`reset()` 两范式口径相反 ⇒ 定案不调；AC-FS-13 移出 AC 表）+ 7 NITS（含 `payload.h:122` 上游注释过时、`hold ≡ PTW_ACTIVE` 冗余项说明、AC 重排 1..13） |
| **v0.2 rev.4** | **2026-10-22** | critic 四轮：**机制层面 APPROVE**（§2.3 逐拍表逐格无错、`next_pc` 嵌套与 `hold & !br_taken` 等价、CSR 三处枚举闭合、rev.3 的 4 个 BLOCKER 全清）。剩 2 个**文字级** BLOCKER：① §8.1 桩用了不存在的 `put_or_assign` + `PADDR`/`MMU_VADDR` 宽度写成 32（实为 `ch_uint<64>`）⇒ 改为 `n->operator()(Key) = ...` 直写范式 + `ch_uint<64>`；② FS-C-12 的括号「且该地址的 walk 在途」让「registered busy」实现在进入拍合规 ⇒ 去掉限定，改为按**取指事实**定义。另修 5 SHOULD_FIX（§8 桩注入路径、守卫用 `&`/`\|`、branch-LATE 守卫须「保持当前值」、`is_trap_branch` 落点统一、矩阵标题）+ 5 NITS |
| **v0.2 rev.3** | **2026-10-22** | **核心机制重构**：rev.2 的「`FLUSH` 用 `ptw_active` 全窗」在冻结**进入端错相** —— `imem.sread` 有 1 拍延迟，`FLUSH` 必须对齐**读发起时刻**而非当前拍，否则每页最后一条指令被误杀且永不重取（critic BLOCKER-1，逐拍反例成立）。改为 **`hold` / `hold_z` 双信号**：`hold = ptw_active & !paddr_valid` 驱动 PC 保持，其**1 拍延迟副本** `hold_z` 驱动 `FLUSH`。连带：`pc_reg` mux 改为 `next_pc` 内层 select（branch 压 freeze，critic BLOCKER-2）；`br_taken_buf` **恢复原样不做门控**（rev.2 的「强制 0」被证明与透传逐拍等价，属冗余）；CSR 守卫由「两处」改为**三处**（writeback 的 `write_en_ch` 单点覆盖 7 个 `we_*` + `is_trap_1` 仲裁 + branch-LATE trap 直写，critic BLOCKER-3）；§8.1 测试桩补 `declare_substage` + `build()` 实现（critic BLOCKER-4）；FS-C-12 重新推导（+1 要求被 `hold_z` 吸收，不再需要）；新增 FS-C-13/14/15 + AC-FS-11 |
| v0.2 rev.2 | 2026-10-22 | 修 rev.1 自引入的 2 BLOCKER（`br_taken_buf` 保持丢指令；`PTW_ACTIVE` 解除缺下界）+ 4 SHOULD_FIX + 3 NITS |
| v0.2 rev.1 | 2026-10-22 | 5 BLOCKER 全部有解；B2 的实证结论推翻 v0.1 方案骨架（不需新插件 / 新 key / 框架扩展） |

> **行号基准**：本文所有 `file:line` 基于 **HEAD `d08d3ba`**（2026-10-22）。引用时**以符号名优先**（函数名 / 成员名 / key 名），行号仅作定位辅助（后续 change 会引入漂移）。

---

## 0. 一句话定位

**fetch stall = PTW walk 期间冻结取指，且不得产生任何架构可见副作用。**

本协议给出该语义在 **ChipForge CH_MEM 5-stage 数据通路**下的**具体表达方式**：不新增插件、不新增 payload key、不改框架，而是**扩展既有的 `fetch.FLUSH` 抑制机制**（当前用于 branch flush）+ **给 `pc_reg` 加保持 mux**。

> ⚠ **与 Draft v0.1 的根本差异**：v0.1 假定需要「新建 `fetch_stall_chmem.h` 插件 + 新 payload key + 扩展 PipeBuilder substage 排序 API」。**实证结论是三者都不需要**（§3 逐条给证据）。v0.1 的方案在 CH_MEM 现行拓扑下**无法生效**（`stage_payload_map_` 为空 ⇒ CtrlLink 的 per-stage stall 无消费者），属"看起来接上了但永不触发"。

> ⚠ **本协议最易错的一点（rev.1/rev.2 都栽在这里）**：`imem.sread` 有 **1 拍延迟**（`ibus_chmem.h:83` 注释 + `:163`）—— **周期 N 呈现的指令 = 周期 N-1 发起的读**。因此任何"本拍抑制"的信号都必须对齐到**上一拍的取指发起时刻**。rev.3 用 `hold_z`（`hold` 的 1 拍延迟副本）强制这一点。

---

## 1. 范围

### 1.1 In Scope

| # | 项 | 归属 |
|---|----|------|
| FS-1 | `PTW_ACTIVE` / `PADDR_VALID`（MMU 侧）的**契约**：类型、载体、产生方、消费方、顺序契约、持续契约 | 本协议定义；实现归 `chmem-ptw-walk` |
| FS-2 | `fetch.FLUSH` 的**第二来源**（`br_taken_buf \| hold_z`） | 本协议定义；实现归 `chmem-ptw-walk` |
| FS-3 | `pc_reg` 保持 mux（`hold` 分量，且 branch 优先于 freeze） | 本协议定义；实现归 `chmem-ptw-walk` |
| FS-4 | `hold_z` 延迟寄存器（`hold` 的 1 拍延迟副本，对齐 sread 延迟） | 本协议定义；实现归 `chmem-ptw-walk` |
| FS-5 | CSR 写回的 FLUSH 守卫（**三处**：`write_en_ch` 单点 + **`is_trap_branch`** 仲裁 + branch-LATE trap 直写） | 本协议定义；实现归 `chmem-ptw-walk` |
| FS-6 | 冻结期间的**提交点覆盖矩阵**（14 项逐个判定，含显式"允许"/"排除"） | 本协议定义 |
| FS-7 | 验收判据（可测 MUST）+ 注入机制 + 变异对照 | 本协议定义 |

### 1.2 Out of Scope（显式排除）

- **数据侧（load/store）PTW 期间的访存冻结** —— 归 `chmem-ptw-walk` 的 3e（用 `tlb_lookup_loadstore` 的 `PADDR_VALID` 门控 `is_store_active` + load 重放）。本协议的 FS-C-* 全部针对 **ifetch 侧**；`PTW_ACTIVE` 在本协议中特指"**本拍取指地址在 sv 下无有效翻译**"的标志**（**不是** FSM 在途标志 —— 见 FS-C-12：无论 walk FSM 是否已启动，miss 检出同拍即为 1）
- **`bp.predict` 推测期间是否 flush 已取指令** —— 与 PTW 无关，归 v1.0.0 wave6
- **PipeBuilder substage 排序 API 扩展**（`declare_substage_with_priority` 等）—— 本协议不需要（§3 B1）
- **多周期 predict-with-PTW-fault FSM**（ADR-046 v2.0 + `chlib::ch_state_machine`）—— v1.0.0+ 优化
- **trap PC 跳转 / `mmufault_redirect`** —— 属 R11.1 议题，与 fetch stall **语义正交、交付耦合**（耦合点 = FS-C-18：不止 `next_pc` 的优先级位，还含清单 (a)(b)(c)）；见 §9.1
- **Sv39/48 / 多级 TLB / ASID** —— D1 已收窄
- **PTW FSM 本身**（状态机、页表读、refill、walk 取消）—— 归 `chmem-ptw-walk`；本协议只消费其 `PTW_ACTIVE` / `PADDR_VALID`，并对 walk 取消提出**接口要求**（FS-C-14）。**fault 语义的交付时点归本协议的 FS-C-18**（不在 3c，须与 trap redirect 同批），不以 `chmem-ptw-walk` 的子任务为界

---

## 2. 术语与信号

### 2.1 `PTW_ACTIVE` / `PADDR_VALID` —— 触发、持续与释放

| 属性 | `PTW_ACTIVE` | `PADDR_VALID` |
|---|---|---|
| key | `cf::ip::mmu::payload::mmu_keys<std::uint64_t>::PTW_ACTIVE` | 同 namespace `::PADDR_VALID` |
| 证据 | `ip/mmu/tlm/mmu_keys.h:87` | `ip/mmu/tlm/mmu_keys.h:88` |
| key 类型 | `Payload<cf::plugin::uint_t<1>>`（CH_MEM ⇒ `ch_uint<1>`，**已 mode-aware**） | 同 `uint_t<1>` |
| 载体 | `tlb_lookup_ifetch` 节点的 payload cell | 同一节点（`ibus_chmem.h:140-153` 已在读） |
| 产生方 | **由 look-up 结果组合派生**（`sv_active & !paddr_valid_for_current_addr & !fault`，见 FS-C-12）；**不是** FSM `busy` 状态寄存器。⚠ **`!fault` 项归 FS-C-18 的合并交付；3c 省略该析取项**（3c 不产生 fault ⇒ 含它必撞 AC-FS-16 ①） | MMU 的翻译完成标志 |
| 消费方 | `ibus_chmem.h`：`hold` 计算（`("branch", LATE)`） | 同 `hold` 计算（**仅**参与 `hold`） |
| 语义 | 1 = **本拍取指地址在 sv 模式下尚无有效翻译**（miss 在**检出同拍**即为 1） | 1 = **本拍** fetch 地址的逐地址组合翻译有效（`ibus_chmem.h:155` 的 `select` 已在用） |

**契约（规范性）**：

- **FS-C-1**：`PTW_ACTIVE` 的 cell 值必须是**运行期节点**（`ch_uint<1>`），**不得**是 elaboration 期常量。常量 ⇒ §6 的惰性骨架禁令。
- **FS-C-2**：产生方 MUST 在 `("fetch", Phase::EARLY)` 写这两个 cell（与既有 `PADDR` / `MMU_VADDR` 同一闭包、同一 `emit_lookup()`）。
- **FS-C-3**：消费方 MUST 用 `ch_uint<1>` ↔ `ch_bool` 的**节点转换**（`ch_uint<1>::operator ch_bool()`，上游 `core/uint.h:123-125`，仅 `N==1` 可用；或 `ch_bool(cell.impl())`，范式见 `csr_chmem.h:325`）。**禁** `static_cast<bool>` / `static_cast<std::uint64_t>` / 任何把 `ch_uint<N>` 降成 POD 的写法 —— 那会让 cell 退化为 elaboration 期常量（ADR-040 v3.0 §10.5）。
  > `ch_bool(cell)` 对 `ch_uint<1>` **不会**折常量（走 `impl()` 指针）；真正的折叠风险是 `static_cast<bool>` / `static_cast<std::uint64_t>`。
- **FS-C-4**：消费方 MUST 做 `has()` 守卫，且**守卫前必须先以 `ch_literal<0,1>` 显式初始化局部变量** —— 与 `ibus_chmem.h:141-143` 完全同构：
  ```cpp
  ch::core::ch_uint<1> ptw_active = ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{});
  if (tlb_n && tlb_n->payloads().has(MmuKeys::PTW_ACTIVE)) {
    ptw_active = tlb_n->operator()(MmuKeys::PTW_ACTIVE);
  }
  ```
  **理由**：`ch_reg` / `ch_uint` 的 null impl 在 Simulator 中会 SEGV（v0.3.1 M6 教训）；`ch_uint<N>(0)` 产生的 null impl 同理 ⇒ 必须用 `ch_literal`。
- **FS-C-12**（rev.5 定稿）：**`PTW_ACTIVE` MUST 在 miss 检出的同一拍即为 1**，即对**每一个** in sv 模式、`PADDR_VALID(C)=0` **且 `fault(C)=0`** 的周期 `C`，`PTW_ACTIVE(C)` MUST 为 1 —— **无论 walk FSM 是否已启动**。**fault 周期 MUST 归 0**（§9.1 不变式：否则页错误路径永久冻结）—— **"无翻译"≠"需冻结"**。
  > **rev.5 修正 BLOCKER**：rev.4 的主句漏了 `fault(C)=0`，导致 page fault 拍（`sv_active=1` + `PADDR_VALID=0` + `fault=1`）按主句 MUST 为 1 ⇒ `hold=1` ⇒ `pc_reg` 永不释放；而同一段的派生公式与 §9.1 都要求 0。两条 MUST 互斥。现主句与公式对齐。
  > **rev.3 的措辞缺陷**（历史）：rev.3 写成"…且**该地址的 walk 在途**"，使进入拍 `T`（miss 刚检出、FSM 尚未启动）**不落在该定义内** ⇒ 一个"registered busy"（FSM 启动后才置 1、晚一拍）的实现**文本上合规**。而该实现下 `hold(T)=0` ⇒ `pc_reg(T+1)=B+4`（**PC 跳过 `B`**）、`FLUSH(T+1)=hold(T)=0`（**`T+1` 呈现的 bare-B 垃圾裸提交**），且 `B` 永不以翻译地址被取指 ⇒ 既丢指令又污染状态。
  > ⇒ **`PTW_ACTIVE` 必须由 look-up 结果组合派生**（`sv_active & !paddr_valid_for_current_addr & !fault`），**不得**只由 FSM 状态寄存器驱动。返 `PADDR_VALID` 首次为 1 后可归 0（`hold_z` 已吸收延迟，rev.2 的 "+1" 不需要）。
  > **关于 `hold = ptw_active & !paddr_valid` 的 `& !paddr_valid` 项**：在 FS-C-12 成立时 `hold ≡ ptw_active`（数学上冗余）。该析取项**刻意保留**作为防御：① `PTW_ACTIVE` 若滞后一拍仍有兜底；② `PADDR_VALID` 首 1 的同拍两者不同相。**实现者不必维护两个独立信号** —— 它由同一组输入派生。
  >
  > ⚠ **`fault` 的载体（重要，rev.8 定稿）**：分两个阶段，别混为一谈 ——
  > - **本协议增量（3c / 3g）MUST NOT 新增 key、MUST NOT 接回 `mmu_keys::PTW_FAULT`**。理由：`PTW_FAULT` 是 `Payload<std::uint8_t>`（`mmu_keys.h:64`）—— **裸类型、非 mode-aware**，CH_MEM 下 cell 只能装 elaboration 期常量，读不出 live 节点（`check_plugin_portability.sh` Check 13 的对象）；把它模式化属于 **3d**。
  > - **合并交付（FS-C-18）时 `fault` 走既有载体 `mmu_keys::EXCEPTION_CODE`** —— **无需新 key**：它已是 `Payload<cf::plugin::uint_t<8>>`（`mmu_keys.h:86`，mode-aware），且 `MMUPlugin_chmem.h:282` **已经把**它写在 `tlb_lookup_ifetch` **同一个节点**上（当前是常量 0）⇒ IBus 侧读同一节点即可得到 live 节点，`fault_live = (EXCEPTION_CODE != ch::core::ch_uint<8>(ch::core::ch_literal<0, 8>{}))`（**不得**写 `ch_uint<8>(0)` —— 违反 FS-C-4，null impl ⇒ SEGV）。
  > - ⚠ **跨插件通道只有 payload cell**：`hold_z_` 在 `ibus_chmem.h`，`fault` 产生在 `mmu_ptw_chmem.h` —— 两个不同插件，IBus 读 MMU 的唯一途径就是 `pb.node_of_logic_stage("tlb_lookup_ifetch")` 的 cell（`ibus_chmem.h:140-153` 现有写法）。所以「不引入载体」写在这个上下文里会自相矛盾（rev.7 的缺陷），必须点名 `EXCEPTION_CODE`。
  > - **3c 的 `PTW_ACTIVE` 派生式 MUST NOT 含 `fault` 项**（3c 阶段 `fault` 恒 0，含它只会诱导实现者去接一条尚不存在的通路）。该约束的检查方式见 AC-FS-16。
- **FS-C-18**（rev.6 新增，**fault 路径的交付次序 = 硬约束**）：**ifetch PTW 的 fault 语义 MUST 与 trap redirect（PC ← `mtvec` + `mepc` ← faulting PC）同批交付；在此之前 3c 的 ifetch 交付面 MUST NOT 消费/暴露 fault**（不驱动 `PTW_ACTIVE` 释放、不写 `EXCEPTION_CODE`、不驱动 trap）。⚠ **不要求删掉 FSM 的 `FAULT` 状态** —— 那是 3a 抽出、且已被 `tests/cpu/test_mmu_ptw_fsm_chmem.cpp:145-183` 断言的既有代码（`state()==4` / `fault()==1` / `fault_code()==2`）；本约束的对象是 **ifetch 交付面**，不是 FSM 内部。
  > **为什么不能只"解除冻结"**（下表 = **未接 (b) 项时**的行为；接上 (b) 后 `F+1`/`F+2` 的 `FLUSH` 均为 1）：`hold` 这**一个**变量同时承担两个语义 —— ①「PC 该冻结」②「本拍发起的取指是垃圾、`hold_z` 该压掉」。fault 排除项只该关 ①，却会把 ② 一并关掉。逐拍反例（faulting vaddr = `V`，`fault` 为电平）：

  | 周期 | `paddr_valid` | `fault` | `ptw_active`(FS-C-12) | `hold` | `pc_reg` 下一拍 | 本拍**发起**的读 | 本拍**呈现**的指令 | `FLUSH` | 结果 |
  |---|---|---|---|---|---|---|---|---|---|
  | `F-1` | 0 | 0 | 1 | 1 | 保持 | `V`(bare) | `@V-4`(合法) | 0 | ✓ |
  | `F` | 0 | **1** | **0** | **0** | **`V+4`（释放）** | **`V`(bare)** | `V` 的 bare 取指（faulting 指令本身） | **1**（`hold_z(F)=hold(F-1)=1`） | ✓ 该拍读已被抑制 |
  | `F+1` | 0 | 1 | 0 | 0 | `V+8` | `V+4`(bare) | **`F` 拍 bare-V 的垃圾** | `hold_z(F+1)=hold(F)=0` | ❌ **垃圾裸提交** |
  | `F+2`… | 0 | 1 | 0 | 0 | 继续前进 | … | 上一拍 bare 垃圾 | 0 | ❌ **持续裸提交** |

  > ⇒ 结论：**fault 释放本身不是充分条件**。PC 越过 faulting 指令后，每拍都会执行一条来自任意物理地址的垃圾指令（含 rd 写 / store / branch redirect 等副作用）。**抑制这笔取指只有一个有效途径：一个与 `br_taken_buf` 同构的 1 拍延迟 flush** —— 即把 `fault & !paddr_valid` 也纳入 `hold_z` 的来源（`hold_z <<= (hold | (fault & !paddr_valid))`），使 flush 对齐**读发起拍**。
  > ⚠ **`pc_reg ← mtvec` 单独不够**（rev.6 曾把它写成"二选一"的替代，已删除）：trap redirect 改的是 `pc_reg` 的**下一值**，拿不掉 `F` 拍**已经发起**的那次 bare 读；上表 `F+1` 行正是该读的提交时刻。⇒ trap redirect 是**必要不充分**，flush 项是同批交付里的**必含件**，不是可选项。
  > ⇒ 因此本协议**不**定义 fault 路径的抑制机制（那属于 `csr-chmem-trap-pc-jump` 的范畴），而是**禁止单独交付**：3c 在 trap redirect 落地前 MUST NOT 让 walk 产生 fault。**这一条把"永久挂死"与"连续裸提交"两个坏结局都排除在交付物之外** —— **但只在两个前提同时成立时**：① 3c 阶段真的不产生 fault（AC-FS-15 / AC-FS-16 盯）② 合并批次真的含 (b)（AC-FS-14 ⑤ 盯）。缺任一前提，坏结局都会出现。⚠ 3c 期内坏 PTE 的表现是**持续冻结**（FS-C-12 + AC-FS-15 ② 的要求）—— 这正是实现者「顺手接回 fault 让 CPU 别挂死」的动机来源，靠 AC-FS-16 的结构门拦。
  > **fault + trap 同批交付的完整清单（缺一即违约，AC-FS-14 逐项验）**：
  > **(a)** MMU fault 成为 **`trap_pending` 的第 4 个驱动源** —— 现状 `csr_chmem.h:436-441` 只有 `funct12 ∈ {000,001,302}` 三个源，MMU fault 没有输入口；同时 `mcause_`/`mepc_` 的写仲裁要扩为 **3 路**（writeback-NORMAL CSR 写 / branch-LATE trap 直写 / MmuExceptionHandler），这正是 `csr_chmem.h:349-358` 的 F6 注释登记的扩展点；
  > **(b)** 与 `br_taken_buf` **同构的 1 拍延迟 flush**，用于压掉 fault 拍发起的 bare 取指。**载体已就位、不新增 key**：IBus 从 `tlb_lookup_ifetch` 节点读 `EXCEPTION_CODE`（`MMUPlugin_chmem.h:282` 已在该节点写此 key，当前常量 0；`mmu_keys.h:86` 是 mode-aware `uint_t<8>`）⇒ `fault_live = (EXCEPTION_CODE != ch::core::ch_uint<8>(ch::core::ch_literal<0, 8>{}))`，`hold_z <<= (hold | (fault_live & !paddr_valid))`；
  > **(c)** `mepc ← faulting PC` —— 现状 branch-LATE 直写 `pc_plus_4`（`csr_chmem.h:476-479`），对 fault 必须写 faulting PC 而非 `+4`。
- **FS-C-13**（rev.3 新增）：**`PADDR_VALID` 是逐地址的组合翻译有效位** —— 对同一页内的连续顺序取指，一旦该页翻译完成，`PADDR_VALID` MUST 持续为 1（直到该页被换出 / TLB 失效 / 新 walk 触发）。**禁**把它实现成"walk 完成脉冲"。
  **理由**：§2.3 时序表 `T_x+1` 行依赖 `PADDR_VALID(T_x+1)=1`（fetch 地址 `B+4` 同页）。若它是脉冲，`T_x+1` 回落 vaddr ⇒ `T_x+2`（`FLUSH=0`）**裸提交垃圾**。这是 `chmem-ptw-walk` 的接口要求。

### 2.2 `fetch.FLUSH` —— 既有抑制机制（本协议的扩展点）

| 属性 | 值 | 证据 |
|---|---|---|
| key | `cf::cpu::plugins::IBusPlugin<T>::FLUSH`（`Payload<ch_bool>`） | `ip/cpu/plugins/ibus_chmem.h:42` |
| 当前写入源 | `br_taken_buf` | `ip/cpu/plugins/ibus_chmem.h:167` |
| 当前预填 | `ch_bool(false)` | `ip/cpu/cpu_factory_chmem.h:174-176` |
| 当前语义 | "上一拍取了跳转 ⇒ 抑制错误路径的 fall-through 指令副作用" | `ibus_chmem.h:67` |

**既有消费点（3 处，全部已就位）**：

| 消费点 | 抑制对象 | 证据 |
|---|---|---|
| `reg_file_chmem.h:222,232` | 寄存器写回 | `we = writes_rd && (rd != 0) && !flush` |
| `dmem_chmem.h:129,211` | 存储写 | `is_store_active = (is_sb\|\|is_sh\|\|is_sw) && !flush` |
| `branch_chmem.h:235` | 分支决策 | `taken = taken && !flush`（节点获取/FLUSH 读取见 §2.4 第 3 项） |

**契约**：

- **FS-C-5**：`FLUSH` 的值 MUST 改为 `br_taken_buf | hold_z`（1-bit 逻辑或；`ch_bool` 上用 `|`，**禁**已 deprecated 的 `operator||`）。
- **FS-C-6**：`FLUSH` 的 OR **不得**改变既有 branch-flush 语义（`br_taken_buf` 分量原样保留）⇒ 裸机路径与现状**信号级等价**（§8 AC-FS-4）。
- **FS-C-7**：`FLUSH` 的**发布位置不变** —— 仍在 `("fetch", Phase::NORMAL)` 闭包内（`ibus_chmem.h:167`）。该闭包在 MMU 的 `("fetch", Phase::EARLY)` 之后执行（§3 B1）⇒ 读 `ptw_active` / `paddr_valid` 安全。

### 2.3 `hold` / `hold_z` / `pc_reg` —— 三信号一体的冻结机制（rev.3 核心重写）

| 状态 | 当前驱动 | 证据 |
|---|---|---|
| `pc_reg` | `pc_reg <<= next_pc;`（`("branch", Phase::LATE)` 闭包） | `ibus_chmem.h:71,183` |
| `next_pc` | `select(br_taken, br_target, pc+4)` | `ibus_chmem.h:182` |
| `br_taken_buf` | `br_taken_buf <<= br_taken;`（**rev.3 不改**） | `ibus_chmem.h:184` |
| `pc_lag` | `pc_lag <<= pc_val;`（`("fetch", Phase::NORMAL)` 闭包） | `ibus_chmem.h:168` |

#### 三个必须同时满足的边界条件

**（a）释放端：不能重复取同一条指令。**
若 `pc_reg` 保持到 `PTW_ACTIVE` 归 0 的下一拍，则释放拍 `pc_reg` 仍是 `B` ⇒ 会**重复发起对 `B` 的 fetch** ⇒ 该指令执行两次（store 写两次）。⇒ **PC 保持必须在 `PADDR_VALID` 一为 1 时就解除**，早于 `PTW_ACTIVE` 归 0。

**（b）进入端：不能误杀冻结前最后一拍的合法指令（rev.2 的致命缺陷）。**
`imem.sread` 1 拍延迟 ⇒ 周期 C 呈现的指令 = 周期 C-1 发起的读。设 `T` = 第一拍"发起了未翻译取指"的周期（`hold(T)=1`）。则 `T` 拍呈现的指令来自 `T-1` 发起的读，其地址是**旧页**（TLB 命中、`PADDR_VALID=1`）⇒ **完全合法**。若 `FLUSH(T)=1`，它被杀死；而 PC 从 `T` 起保持 ⇒ **永不重取** ⇒ **每页最后一条指令不执行**（代码 > 4KB 即触发）。
⇒ **`FLUSH` 的 ptw 分量必须用 `hold` 的 1 拍延迟副本 `hold_z`**：`FLUSH(C) = br_taken_buf(C) | hold(C-1)`。

**（c）进入端：不能吞掉合法指令的分支 redirect。**
`T` 拍呈现的（上一拍发起的）合法指令若恰是 taken branch（**页尾回边是极常见模式**），`next_pc(T)=br_target`；但 `hold(T)=1` 会把 `pc_reg` 压回 `B` ⇒ **redirect 被吞**，控制流错误。
⇒ **branch 必须优先于 freeze**。深冻期无需额外处理：`branch_chmem.h:235` 的 `taken = taken && !flush` 在 `FLUSH=1` 时把 `br_taken` 自门控为 0 ⇒ 只有**进入拍**（`FLUSH=0`）会真正发生 redirect。

#### 逐拍时序表（规范性）

设顺序跨页：`@B-4` 为旧页最后一条（TLB 命中），`@B` 为新页第一条（miss）。`T` = 首次 `hold=1` 的周期；`T_x` = `PADDR_VALID` 首次为 1 的周期。

| 周期 | `pc_reg` | 本拍**发起**读的地址 | `paddr_valid` | `ptw_active` | `hold` | `hold_z`(=上一拍 `hold`) | `FLUSH` | 本拍**呈现**的指令（来自上一拍发起） | 是否提交 |
|---|---|---|---|---|---|---|---|---|---|
| `T-1` | `B-4` | `B-4`（hit） | 1 | 0 | 0 | 0 | 0 | `@B-8` | ✓ |
| `T` | `B` | `B`（**miss**，bare） | 0 | 1 | **1** | `hold(T-1)=0` | **0** | **`@B-4`（合法）** | **✓ 不被误杀**（修 rev.2 缺陷 b） |
| `T+1` | `B`（保持） | `B`（bare，垃圾） | 0 | 1 | 1 | `hold(T)=1` | **1** | `bare-B` 垃圾 | ✗ 抑制 ✓ |
| `T+2` … `T_x-1` | `B`（保持） | `B`（bare，垃圾） | 0 | 1 | 1 | 1 | **1** | 垃圾 | ✗ 抑制 ✓ |
| `T_x` | `B` | **`B`（已翻译）** | **1** | 1 | **0** | `hold(T_x-1)=1` | **1** | 垃圾（`T_x-1` 发起） | ✗ 抑制 ✓（修 rev.2 缺陷：垃圾裸提交） |
| `T_x+1` | `B+4`（**已释放**） | **`B+4`（已翻译）** | 1 | 0 | 0 | `hold(T_x)=0` | **0** | **`@B`（正确）** | **✓ 提交一次**（不重复，修缺陷 a） |
| `T_x+2` | `B+8` | `B+8` | 1 | 0 | 0 | 0 | 0 | `@B+4` | ✓ |

**三行关键读法**：

- `T` 拍 `FLUSH=0` —— 因为 ptw 分量取的是 `hold(T-1)=0`。**这是 rev.3 的全部意义**。
- `T_x` 拍 `FLUSH=1` —— 因为 ptw 分量取 `hold(T_x-1)=1`，且此拍 `pc_reg` 的 next 已是 `B+4`（`hold=0` 解除保持）。
- `T_x+1` 拍 `FLUSH=0` —— `hold_z = hold(T_x) = 0`；且 `T_x` 发起的那次读是**翻译后**的 `B` ⇒ 正确提交，无重复。

**branch-at-entry 变体**（缺陷 c）：若 `T` 拍呈现的 `@B-4` 是 taken branch，则 `br_taken(T)=1`（`FLUSH(T)=0` ⇒ 未被自门控）⇒ `pc_reg(T+1)` MUST 为 `br_target` 而非 `B`（靠 `select` 内层嵌套顺序，见 FS-C-8）。此时 `T` 发起的 bare-B fetch 变成**投机垃圾**，在 `T+1` 被 `FLUSH=1` 压掉 ✓。

**契约**：

- **FS-C-8**：`("branch", LATE)` 闭包内 MUST 计算
  ```cpp
  // hold = 本拍发起的取指地址"还没翻译完"（1-bit 逻辑量）
  auto hold = ptw_active_ch & !paddr_valid_ch;
  // next_pc: 优先级 trap > branch > freeze > sequential
  auto next_pc = select(br_taken, br_target,
                        select(hold, pc_val, pc_val + ch_uint<32>(ch_literal<4,32>{})));
  pc_reg <<= next_pc;
  ```
  ⇒ **branch 压 freeze、freeze 压 sequential**（缺陷 c 的修法；用 `select` 嵌套的自然优先级，**不用** `hold & !br_taken` 的等价写法 —— 前者为 §9.1 的 trap 预留留出插入位）。
- **FS-C-9**：MUST 新增 1 拍延迟寄存器
  ```cpp
  ch::core::ch_reg<ch::core::ch_bool> hold_z_{...};   // 初值 ch_bool(false)
  // ("branch", LATE) 闭包内:
  hold_z_ <<= hold;
  ```
  `FLUSH`（fetch NORMAL）读 **`hold_z_`**，**不**读 `hold`。**这是缺陷 b 的修法，不是可选项。**
  > **命名**：`_z` 后缀表示"1 拍延迟副本，用于对齐 `imem.sread` 的 1 拍读延迟"（与 `pc_lag` 同一用途；项目内尚无其它 `_z` 先例，故此处点明）。
- **FS-C-10**：`br_taken_buf` 的驱动**保持原样**（`br_taken_buf <<= br_taken`），**不得**加 freeze 门控。理由：冻结期 `FLUSH=1` ⇒ `branch_chmem.h:235` 把 `br_taken` 自门控为 0 ⇒ 锁存量恒为 0，"强制 0" 与"透传"**逐拍等价**（rev.2 的 FS-C-9 mux 是冗余）。**⚠ 但该等价性依赖 `branch_chmem.h:235` 的自门控，MUST 在 `chmem-ptw-walk` 3g 加注释锚定**（否则未来删掉自门控会静默破坏本协议）。
- **FS-C-11**：`FLUSH` 发布点（fetch NORMAL）与 `hold` / `hold_z` 计算点（branch LATE）MUST 消费**同一组** `ptw_active` / `paddr_valid` 节点（排除"只冻一半"）。
- **FS-C-15**（rev.3 新增，trap 优先级预留）：`next_pc` 的最终形态 MUST 为 `select(trap_pending, trap_target, select(br_taken, br_target, select(hold, pc_val, pc_val+4)))` —— trap **最外层**。当前 `ibus_chmem.h` 未接 trap（R11.1 裁决：ECALL/EBREAK/MRET redirect 归独立 change `csr-chmem-trap-pc-jump`）⇒ 本协议只要求**嵌套结构留出插入位**，不实装 trap。

### 2.4 提交点与新增状态覆盖矩阵（冻结期副作用全抑制）

| # | 提交点 / 新增状态 | 冻结期行为 | 机制 | 状态 |
|---|---|---|---|---|
| 1 | 通用寄存器写回 | 抑制 | `FLUSH`（`reg_file_chmem.h:232`） | ✅ 既有 |
| 2 | 存储写（sb/sh/sw） | 抑制 | `FLUSH`（`dmem_chmem.h:211`） | ✅ 既有 |
| 3 | 分支决策（错误路径） | 抑制 | `FLUSH`（`branch_chmem.h:233-235`：`:233` 取 fetch 节点、`:234` 读 `FLUSH`、`:235` `taken && !flush` 自门控） | ✅ 既有 |
| 4 | **CSR 写（writeback-NORMAL，7 路 `we_*`）** | **抑制** | **需新增 FLUSH 守卫** —— **单点**：`csr_chmem.h:325` 的 `write_en_ch` 是**单一** `ch_bool`，一处 `& !flush` 即覆盖 `we_300/305/340/341/342/344/304` 全部 7 路（`:329-335`），进而覆盖 `mcause_wb`（`:381`，由 `we_342` 派生）与 `mepc_wb`（`:398`，由 `we_341` 派生） | ❌ **本协议补** |
| 5 | **CSR trap 仲裁（writeback-NORMAL，绕开 `we_*`）** | **抑制** | **需新增 FLUSH 守卫**：`csr_chmem.h:383` 的 `is_trap_1 = select(is_trap_branch, 1, 0)` 直接驱动 `mcause_final`（`:386-389`）与 `mepc_final`（`:399-402`）⇒ MUST 改成 `is_trap_branch & !flush`（`&`/`|` 1-bit 逻辑） | ❌ **本协议补** |
| 6 | **CSR trap 直写（branch-LATE，ECALL/EBREAK 的 `mcause`/`mepc`）** | **抑制** | **需新增 FLUSH 守卫**：`csr_chmem.h:473-478` 的 `mcause_ <<= select(funct12_eq_000, 11, select(funct12_eq_001, 3, mcause_))` 与紧随的 `mepc_` | ❌ **本协议补** |
| 7 | `pc_reg` 前进 | 保持 | **需新增** `hold` 分量（`ibus_chmem.h:182-183`） | ❌ **本协议补** |
| 8 | `hold_z_` 推进（**新增流水寄存器，非提交点**） | 推进（设计如此） | **需新增** `ch_reg<ch_bool>`（`ibus_chmem.h` 内，与 `pc_reg`/`pc_lag` 同级） | ❌ **本协议补** |
| 9 | `br_taken_buf` 前进 | **保持原样** | 由 `branch_chmem.h:235` 自门控间接为零；**不加 mux**（FS-C-10） | ✅ **无需改** |
| 10 | `pc_lag` 前进 | 无需 | 冻结期 `pc_val` 不变 ⇒ 每拍写入同值 | ✅ 无需改 |
| 11 | **`mcycle_`（自由计数器）** | **继续计数（允许）** | `csr_chmem.h:499` `mcycle_ <<= mcycle_ + 1`，**刻意不挂 FLUSH**（R2.5 自由计数 + `:328` 明写"mcycle_ 不写"） | ✅ **显式允许** |
| 12 | **`mul_div_fsm_chmem.h` 的 3 个 `ch_reg`** | **不涉及** | `counter_reg_`/`result_reg_`/`busy_cycles_reg_`（`ip/cpu/plugins/mul_div_fsm_chmem.h:235-237`）= M 扩展 FSM 内部状态（且是 PoC 占位 `:161-162`），非指令提交；该插件**未在 `cpu_factory_chmem.h` 注册**（grep 零命中） | ✅ **显式排除** |
| 13 | 加载（`ld`）访存副作用 | 无 | `aread` 是组合读，无写 | ✅ 无需改 |
| 14 | `tohost` 探针 | 允许 | 只读探针（`dmem_chmem.h:220-221`），非架构状态 | ✅ 无需改 |

> **第 4/5/6 项为什么是三处**：CSR 有**两个闭包 × 三条通路**——
> - writeback-NORMAL 闭包内：① 7 路 `we_*`（受 `write_en_ch` 单点控制）；② **trap 优先仲裁** `is_trap_branch` → `is_trap_1`（**完全绕开** `write_en_ch`）
> - branch-LATE 闭包内：③ ECALL/EBREAK 的 `mcause`/`mepc` 直写
> 只守第 ① 处 ⇒ ②③ 成漏洞。
> **⚠ 触发条件按代码实际写**：`is_trap_branch = funct12_eq_000 || funct12_eq_001`（`csr_chmem.h:367`）**不检 opcode** ⇒ 任意 `imm=0` 的 I-type（`addi rd,rs1,0`、`lw rd,0(rs2)`）funct12 均为 `0x000`，同样触发 ② 的 `mcause_` 写入。这不是"只有 ECALL/EBREAK 才有风险"，而是**冻结期几乎任意在途 I-type 都会裸写 `mcause_`**。（opcode 漏检是 `csr_chmem` **预存缺陷**，不归本协议修，但守卫落点必须按此事实写。）
> **第 11 项为什么必须显式"允许"**：`mcycle` 是自由计数器，语义上必须持续走。若误加 FLUSH 守卫 ⇒ 冻结期时间停走 ⇒ 与 RISC-V 语义冲突。
> **第 12 项为什么显式排除而非沉默**：rev.1/rev.2 的矩阵只列"要抑制的"，未说明"不涉及的" ⇒ 读者无法判断是遗漏还是有意。
> **未列入矩阵但冻结期仍在写的 cell**：`TRAP_PENDING` / `TRAP_TARGET`（`csr_chmem.h:459-461` 的 branch-LATE 直写）在冻结期**仍会被写**。当前**零消费者**（`grep TRAP_PENDING ip/cpu/plugins/ibus_chmem.h` = 0）⇒ 不构成架构可见副作用 ⇒ 本协议**不要求**守卫。⚠ 但 `csr-chmem-trap-pc-jump` 接线 C.1 后它们变成可见状态 ⇒ **届时 MUST 重新评估并补守卫**（这正是"矩阵需显式列允许/排除"的理由）。

---

## 3. 5 个 BLOCKER 的解（逐条：选择 / 理由 / 证据）

### B1 — `declare_substage` 不产生调度顺序

**结论：不需要扩展 substage 排序 API。** 采用 ADR-040 v3.0 §10.6 的**同 stage 名 + phase 分桶**契约。

- 实证：`pipe_builder.h:122-132` `declare_substage()` 只写 `substage_parent_` + 建 `nodes_` 条目，**无 reader**；`:215-224` `canonical_stage_order()` = **`stages_` 首现序**（`std::find` 去重后按出现顺序 push）。
- 实证：`elaborate()`（`:413-424`）的遍历是 **stage 名（canonical 序）× phase（EARLY→NORMAL→LATE）** 二重循环 ⇒ 同 stage 名内 phase 顺序有保证。
- **canonical 序实测**：

  | 序 | stage | 首次出现处 |
  |---|---|---|
  | 1 | `fetch` | `cpu_factory_chmem.h:211`（`at_stage("fetch", EARLY, populate)`） |
  | 2 | `decode` | `cpu_factory_chmem.h:212`。⚠ **`:213/:214/:215/:216` 注册的 stage 名仍是 `"decode"`**（它们 populate 的是 execute/memory/writeback/branch 的**节点**，但 `at_stage` 的 stage 参数写的是 `"decode"`）⇒ 这些**不**引入新 stage |
  | 3 | `execute` | `cpu_factory_chmem.h:387`（`at_stage("execute", LATE, ...)`） |
  | 4 | `memory` | `cpu_factory_chmem.h:420`（`at_stage("memory", LATE, ...)`） |
  | 5 | **`branch`** | `pb->build()`（`:444`）→ IBus setup（`ibus_chmem.h:71`）；IBus 是第 3 个注册的 plugin（`:331`） |
  | 6 | **`writeback`** | `pb->build()` → RegFile setup（`reg_file_chmem.h:114`）；RegFile 是第 4 个（`:332`） |

  ⇒ **canonical 序 = `fetch, decode, execute, memory, branch, writeback`**（`branch` 在 `writeback` **之前**）。
  > 常见误读：以为 `:213-216` 的 populate 序决定 execute→memory→writeback→branch 的位次 —— 错，那四行的 stage 参数全是 `"decode"`。

- 本协议的生产者/消费者落位：

  | 角色 | 落位 | 证据 |
  |---|---|---|
  | 生产者（MMU 写 `PTW_ACTIVE`/`PADDR_VALID`） | `("fetch", Phase::EARLY)` | `MMUPlugin_chmem.h` 的 `at_stage("fetch", Phase::EARLY, emit_lookup)` |
  | 消费者 A（发布 `FLUSH`，读 `hold_z`） | `("fetch", Phase::NORMAL)` | `ibus_chmem.h:70` |
  | 消费者 B（算 `hold`，驱动 `hold_z` / `pc_reg`） | `("branch", Phase::LATE)` | `ibus_chmem.h:71` |

- 跨 stage 读（消费者 B 读 `("fetch", EARLY)` 写的 cell）为何安全：`"branch"`（序 5）在 `"fetch"`（序 1）之后 ⇒ 同一次 `elaborate()` 内 producer 必先于 consumer。
- `tlb_lookup_ifetch` 只是**节点名**（`declare_substage` 的副产物），**不参与调度** ⇒ requirements doc 的 S6（"fetch substage 所有权"）不成立。
- **保留意见**：`declare_substage` 仍无 reader ⇒ 将来若真要 substage 调度仍需 ADR-048 扩展 —— 但**本协议不依赖它**。

### B2 — CtrlLink 的 per-stage stall 在 CH_MEM 下不生效

**结论：不走 CtrlLink。** fetch stall 走 **payload 信号路径**（`FLUSH` 扩展 + PC 保持 mux）。

- `elaborate()` 的 per-stage CtrlLink 聚合（`pipe_builder.h:435-452`）只对 `stage_payload_map_` 的条目调用 `entry.applier(...)`；而 `stage_payload_map_` 只由 `register_stage_payload_connector()`（`:344-382`）填充。
- **实证**：`register_stage_payload_connector` **全仓仅在 `tests/framework/test_elaborate_connector.cpp:69,114,167,255` 调用**，生产代码零调用 ⇒ CH_MEM 5-stage 的 `stage_payload_map_` **为空** ⇒ **CtrlLink 的 stall/flush 在 CH_MEM 下没有任何消费者**。
- **补充实证**：`should_stall_stage()`（`:303-310`）只在 `run()`（TLM 路径，`:185`）被调用；`elaborate()` 从不调用它。
- 结论：`register_ctrl_link("fetch", ...)` 在 CH_MEM 下**不冻结任何东西**；`branch_chmem.h:128` / `hazard_chmem.h:201` / `mmu_exception_handler_chmem.h:73` 的现有注册在 CH_MEM 侧同样无下游。
- ⇒ 走 CtrlLink 等于"接了个没插头的线"（G3 同款陷阱）。改用 payload 信号路径。

### B3 — payload key 类型与归属

**结论：不新增 payload key。**

- `PTW_ACTIVE` / `PADDR_VALID` 已是 mode-aware（`mmu_keys.h:87-88` `Payload<uint_t<1>>`）⇒ 直接承载 `ch_uint<1>` 节点。
- `FLUSH` 是 `Payload<ch_bool>`，声明在 `ibus_chmem.h:42`（CH_MEM-only 文件）⇒ 其全部消费者也在 `_chmem.h` 内 ⇒ TLM 侧零编译影响。**不满足 ADR-040 §10.5 的双模要求，但当前用法不违规**（无 TLM 消费者）；若将来要在 TLM 侧消费 `FLUSH`，MUST 先改 `uint_t<1>`（不在本协议范围）。
- key 归属：`mmu_keys` 归 `ip/mmu/tlm/mmu_keys.h`、`FLUSH` 归 `ip/cpu/plugins/ibus_chmem.h` ⇒ **无需**把任何 key 挪到 `ip/cpu/core/payload_common.h`（v0.1 的 S1 建议作废）。
- ✅ **注（2026-10-22 已解除）**：CSR 守卫（§2.4 第 4/5/6 项）要读 `csr_keys::CSR_WRITE_EN` 等作条件。这批 key 在基线 `962ecb2` 是 `Payload<bool>`（POD），曾是本协议的硬前置 —— **已由 `chmem-cpu-keys-modality` 解除**（2026-10-22 archive，commit `d08d3ba`；现为 `uint_t<1>` = `ch_uint<1>`）⇒ §6.2 的依赖链**首环已闭合**。

### B4 — Bare path 判定公式与 D4 决议矛盾

**结论：严格按 `satp` 的 MODE 字段判定，禁用 `satp_ppn == 0` 析取项。**

- 权威口径：`ip/mmu/tlm/MMUPlugin.cpp:97-110` —— `satp_mode` 由 `sv_mode_` + `satp_value_` 的 MODE 位解码（Sv32=bit31，Sv39/48=bit[63:60]，Bare=0），判定式 `if (sv_mode_ == SvMode::Bare || satp_mode == 0)`；`:89-93` 注释明写「`satp_ppn_ == 0` **不等于** Bare」。
- v0.1 §2.4 的 `satp_ppn = 0 OR sv_mode = Bare` 公式**作废**。
- 本协议的落点：fetch 冻结的触发条件是 `hold = ptw_active & !paddr_valid`，而 `ptw_active` 只在 **Sv-mode 且确需 walk** 时为 1 ⇒ Bare 路径下 MMU 根本不进入 PTW ⇒ `ptw_active` 恒 0 ⇒ `hold` 恒 0 ⇒ 天然不冻结。**无需**在 fetch stall 侧重复实现 Bare 判定（避免两处口径漂移）。
- ⚠ 这条对 **FS-C-4 的初值**有要求：`ptw_active` 的局部初值必须是 `0`（不是 `1`），否则 MMU 未注册（Bare）时 `hold` 恒 1 ⇒ **永久冻结**。

### B5 — `PTW_ACTIVE` 信号源

**结论：契约已就位（key + 载体 + 顺序 + 类型转换），live 节点由 `chmem-ptw-walk` 提供。**

- key 类型由 `chmem-mmu-keys-modality` 解除（`uint_t<1>`）✓
- 载体 = `tlb_lookup_ifetch` 节点的 cell ✓
- 顺序契约 = 同 stage + `EARLY`/`NORMAL`/`LATE`（§3 B1）✓
- 类型转换 = `ch_uint<1>` → `ch_bool` 走 `operator ch_bool()`（N==1）**非折叠** ✓
- **剩余**：Phase B（路线 C）写的是**常量 0** ⇒ live 信号由 `chmem-ptw-walk` 提供 ⇒ 见 §6 落地时序约束。

---

## 4. 7 个 SHOULD_FIX 的处置

| ID | 内容 | 处置 |
|----|------|------|
| **S1** | payload key 归属地错误（应放 `payload_common.h`） | **作废** —— 本协议不新增 key（§3 B3） |
| **S2** | 插件计数 8→9/10 与 plan 不一致 | **本协议不新增插件** ⇒ 计数不变。`MMUPluginChmem` + `RiscvMMUPluginChmem` 的注册计数变更归 **C.4** |
| **S3** | "byte-identical" 措辞与实测冲突 | **本协议不承诺 byte-identical**。准确表述见 §8 AC-FS-4：`hold` 恒 0 时 `FLUSH` 的值与现状**逐位相同**（`br_taken_buf \| false == br_taken_buf`）⇒ **零新增 FAIL**（相对判据，见下） |
| **S4** | 审查时序冲突（12-01 晚于其解锁的截止） | **已消解** —— v0.2 起草窗口 = Sprint 3 后段（11-06 ~ 11-30），早于 C.4 入口（11-30） |
| **S5** | 阈值自相矛盾（≥5% vs ≥10% CoreMark）+ 风险 ID 悬空 | **作废该阈值** —— CoreMark 属 BP 范畴（`feat-bp-btb-gshare` 已 defer 到 v1.0.0）⇒ fetch stall 不以 CoreMark 为判据，改用**信号级判据**（§8），无阈值歧义。风险 ID 改用本协议 §9 的 `R-FS-*` |
| **S6** | fetch substage 所有权未对账（`MMUPlugin.cpp:57` 已有 `declare_substage("fetch","tlb_lookup_ifetch",1)`） | **已对账且一致** —— Phase B 的 `MMUPlugin_chmem.h` 采用**同名** `declare_substage("fetch", "tlb_lookup_ifetch", 1)`；且 substage 不参与调度（§3 B1）⇒ 所有权问题不成立 |
| **S7** | "`mmufault_redirect` 是 Phase D 范畴"论据过时 | **已过时** —— `cpu_factory_chmem.h:338` 已注册 `RiscvCsrPluginChmem<T>`（Zicsr 8 指令 + trap 3 指令 ECALL/EBREAK/MRET）⇒ CSR 通路存在。trap PC 跳转的归属由 **R11.1** 单独裁决（§9.1） |
| **NIT-1** | `mmu_ptw_chmem.h:165` 注册新 stage `"mmu_ptw"` | **需一句注释**：该 stage 仅用于 **FSM 构建**（`create_fsm()`），**不交付 payload** ⇒ 不违反 ADR-040 §10.6，但新增 stage 名会进入 `canonical_stage_order()`（位置取决于注册时机），故须注释说明"不参与 payload 交付" |

---

## 5. 规范性要求汇总（可测 MUST）

| ID | 要求 | 验证点 |
|----|------|-------|
| **FS-C-1** | `PTW_ACTIVE` cell 必须是运行期节点，不得为常量 | 变异对照：折常量 ⇒ 测试 RED |
| **FS-C-2** | 生产者在 `("fetch", EARLY)` 写 | 代码位置断言（`at_stage` phase + 同闭包同写） |
| **FS-C-3** | 消费方走节点转换，禁 `static_cast` 降 POD | grep 反回归 + 变异对照 |
| **FS-C-4** | `has()` 守卫 + 守卫前 `ch_literal<0,1>` 初始化；初值 **0**（Bare 不冻结） | MMU 未注册路径单测（不得 SEGV、不得冻结） |
| **FS-C-5** | `FLUSH = br_taken_buf \| hold_z` | 单测断言两分量 |
| **FS-C-6** | OR 不改变 branch-flush 语义 | 相对判据：零新增 FAIL（AC-FS-4） |
| **FS-C-7** | `FLUSH` 仍在 `("fetch", NORMAL)` 发布 | 代码位置断言 |
| **FS-C-8** | `next_pc` 按 `trap > branch > freeze > sequential` 嵌套；`hold = ptw_active & !paddr_valid` | AC-FS-2/3/7 + 代码位置断言 |
| **FS-C-9** | `hold_z` = `hold` 的 1 拍延迟副本，`FLUSH` 读 `hold_z` **不读 `hold`** | AC-FS-8（进入端不误杀）；变异：改读 `hold` ⇒ RED。**合并交付后按 FS-C-18 (b) 追加 fault 项**（`hold_z <<= (hold \| (fault_live & !paddr_valid))`）；判别力仍在「`FLUSH` 读 `hold_z` **不读** `hold`」，`_z` 延迟本身不变 |
| **FS-C-10** | `br_taken_buf` 驱动**保持原样**，不加 mux；等价性依赖 `branch_chmem.h:235` 自门控，须加注释锚定 | AC-FS-1 的 branch 分量回归 |
| **FS-C-11** | `FLUSH` 与 `hold` 消费同一组 `ptw_active` / `paddr_valid` 节点 | AC-FS-12（`impl()` 指针相等）+ 代码位置断言 |
| **FS-C-12** | `PTW_ACTIVE` 在每个（sv 模式、`PADDR_VALID=0`、**`fault=0`**）的取指周期为 1（miss 检出同拍即 1）；**fault 周期归 0**；`PADDR_VALID` 首 1 同拍可归 0 | **AC-FS-9** + AC-FS-11 |
| **FS-C-13** | `PADDR_VALID` 是逐地址组合有效位（同页顺序取指持续为 1），非脉冲 | AC-FS-11 的同页多拍断言 |
| **FS-C-14** | walk 在取指地址变更（branch redirect）后 MUST 被取消或按地址打标，不得把旧地址的翻译结果当作新地址的 | AC-FS-7 ③ |
| **FS-C-15** | `next_pc` 嵌套留出 trap 最外层插入位 | 代码结构断言（嵌套层级） |
| **FS-C-16** | CSR 三处守卫：`write_en_ch`（`:325`，覆盖 7 路 `we_*` + `mcause_wb`/`mepc_wb`）/ **`is_trap_branch`**（`:367`，覆盖 `is_trap_1` → `mcause_final`/`mepc_final`）/ branch-LATE trap 直写（`:473-478`） | AC-FS-5 |
| **FS-C-17** | `mcycle_` **不挂** FLUSH（冻结期继续计数） | **AC-FS-10** |
| **FS-C-18** | ifetch PTW 的 fault 语义 MUST 与 trap redirect 同批交付（清单 a/b/c）；在此之前 3c 的 ifetch 交付面 MUST NOT 消费/暴露 fault；`fault` 走既有 `EXCEPTION_CODE` cell，**不新增 key** | **3c 负向检查：AC-FS-15（行为）+ AC-FS-16（结构）**；同批交付后：AC-FS-14 |

---

## 6. 落地时序约束（**规范性** —— 违反即返工）

### 6.1 惰性骨架禁令

`PTW_ACTIVE` 在 Phase B（路线 C）下是**常量 0**。若此时先把 §2.2/§2.3/§2.4 的接线落地，得到的机制**恒不触发** ⇒ 与 `ctrl-link-chmem-reg-handle` 期间 A4 的「恒 false 惰性骨架」是**同一类陷阱**。

⇒ **规范**：§2.2 / §2.3 / §2.4-第 4/5/6/7/8 项的接线 **MUST 与 PTW 的 live `PTW_ACTIVE` 同批落地**，即归 `chmem-ptw-walk`，**不得**作为"先接线后填驱动"的独立增量。

### 6.2 依赖图

```
chmem-cpu-keys-modality ✅ 已 archive (2026-10-22, commit d08d3ba)
   └─ CSR_WRITE_EN / TRAP_PENDING / TRAP_TARGET 已模式化 (CSR 守卫的前提, 已闭合)
            │
            ▼
chmem-ptw-walk (6.5d)
  ├─ 3a  build_pt_walk_fsm 自由函数抽取
  ├─ 3b  RiscvMMUPluginChmem 适配器
  ├─ 3c  walk 触发 + refill（**hit/miss/refill only —— fault 语义 NOT in scope, 见 FS-C-18**）
  │      └─ 验收项：FS-C-12（`PTW_ACTIVE` 在 miss 检出同拍为 1，AC-FS-9）/ FS-C-13（`PADDR_VALID` 逐地址持续）/ FS-C-14（walk 取消或按地址打标）/ **FS-C-18 的 3c 半段（AC-FS-15 行为 + AC-FS-16 结构）**
  ├─ 3d  mmu_keys 剩余 key 模式化
  ├─ 3e  MEM_ADDR 源接线 + 数据侧冻结
  ├─ 3f  测试
  └─ 3g  ★ 本协议 §2.2/§2.3/§2.4(4,5,6,7,8) 的接线 + 测试
            │
            ▼
Phase C wiring (C.1 / C.4 / C.5) → C.6 → Phase D → Phase E

  ┌─ (旁挂, 不是 3c 的子任务) ─────────────────────────────────────┐
  │ FS-C-18: PTW fault 语义 MUST 与 `csr-chmem-trap-pc-jump`        │
  │          （+ C.1 的 TRAP_PENDING/TRAP_TARGET 接线）**同批交付**  │
  │          ⇒ AC-FS-14。在那之前 walk MUST NOT 产生 fault          │
  │          ⇒ 3c 的门 = AC-FS-15（行为）+ AC-FS-16（结构）          │
  └────────────────────────────────────────────────────────────────┘
                    ↑ 依赖 (a) trap_pending 第 4 驱动源
                    ↑ 依赖 (b) 与 br_taken_buf 同构的 1 拍延迟 flush
                    ↑ 依赖 (c) mepc ← faulting PC
```

> ⚠ **3c 执行陷阱（N-2）**：`ip/mmu/plugins/MMUPlugin_chmem.h:276-280` 的现有注释写「`PTW_ACTIVE` / `EXCEPTION_CODE` **本增量恒 0** —— 语义事实：无 PTW 时恒不 active；**TLB miss 不是 fault**（回退 vaddr，与 Bare 同构）」。该注释对 **Phase B 现状**成立（其自带的"本增量"限定没错），但与 **3c 目标语义直接冲突**（3c：miss ⇒ `PTW_ACTIVE=1` ⇒ 冻结 + walk + refill）⇒ 3c 实现者照它实现会把 AC-FS-9/15/16 **全做反**。⇒ **3c 的 tasks MUST 含一条「落地时改写该注释」**（本协议不改源码 —— 属 3c 交付物）。
> ⚠ **顺序要点**：`fault` 语义**不在** 3c，而在 Phase C 之后与 trap redirect 合并。**光靠"不写代码"不构成保证** —— `mmu_ptw_chmem.h:136` 的 `FAULT = 4` 与 `:190 fault_code()` 已经存在（3a/3c 复用同一 FSM）⇒ 3c 的验收必须含 **AC-FS-15（行为）+ AC-FS-16（结构）** 两道负向检查，否则"顺手接回 fault 让 CPU 别挂死"不会看到任何红灯。
> ⚠ **`PTW_ACTIVE` 派生式 MUST NOT 含 `fault` 项**（3c 阶段 `fault` 恒 0，含它只会诱导实现者去接一条尚不存在的通路）。**3c/3g 不新增 key、不接回 `PTW_FAULT`**；合并交付时 `fault` 走既有 `mmu_keys::EXCEPTION_CODE` cell（已 mode-aware，`MMUPlugin_chmem.h:282` 已写在 `tlb_lookup_ifetch` 同节点）—— 不需要改造 3d 的 `PTW_FAULT`。

### 6.3 归属调整

| 原计划 | 新归属 | 理由 |
|--------|--------|------|
| Phase 3.5 / 3.5.3「起草 fetch-stall v0.2 protocol」（3d） | **本文件**（Phase 3.5 内完成） | 5 BLOCKER 全解，实际投入 < 3d（§7） |
| 「实装 `fetch_stall_chmem.h`（1-1.5d）」 | **取消**（不新建插件） | §3 B2：CtrlLink 路径在 CH_MEM 无消费者；FLUSH 路径不需要新插件 |
| fetch stall 接线实现 | `chmem-ptw-walk` 的 3g | §6.1 惰性骨架禁令 |

---

## 7. 实装清单（`chmem-ptw-walk` 的 3g 子项）

| # | 文件:行 | 改动 | 估时 |
|---|---------|------|------|
| 1 | `ip/cpu/plugins/ibus_chmem.h`（成员区，与 `pc_reg`/`pc_lag` 同级） | 新增 `ch_reg<ch_bool> hold_z_`（初值 `ch_bool(false)`）。**canonical 拼写（MUST）**：成员 = `std::unique_ptr<ch_reg<ch_bool>> hold_z_reg_`（照 `pc_lag_reg_` 惯例），**语句一律用局部引用名 `hold_z`**（照 `pc_lag` 惯例；`get_hold_z()` 的 lazy-init getter 照 `get_pc_lag()`）—— **MUST NOT 在语句里混用 `hold_z_` / `hold_z`**（协议正文里的两种写法各 30+ 处，实现时以本条为准）| 0.1d |
| 2 | `ip/cpu/plugins/ibus_chmem.h:167`（fetch NORMAL 闭包） | `FLUSH` 源改为 `br_taken_buf \| hold_z_`（**读 `hold_z_`，不读 `hold`**） | 0.25d |
| 3 | `ip/cpu/plugins/ibus_chmem.h:171-185`（branch LATE 闭包） | 读 `ptw_active` / `paddr_valid`（FS-C-4 范式）；算 `hold`；`next_pc = select(br_taken, br_target, select(hold, pc_val, pc_val+4))`；`hold_z_ <<= hold` | 0.5d |
| 4 | `ip/cpu/plugins/ibus_chmem.h:184` | **不动**（`br_taken_buf <<= br_taken`），但加注释锚定 FS-C-10 的等价性依赖 | 0.05d |
| 5 | `ip/cpu/plugins/csr_chmem.h:325`（writeback NORMAL） | `write_en_ch = ch_bool(...) & !flush`（**单点覆盖 7 路 `we_*` + `mcause_wb`/`mepc_wb` 两条手臂**） | 0.25d |
| 6 | `ip/cpu/plugins/csr_chmem.h:367`（writeback NORMAL） | `is_trap_branch = (funct12_eq_000 \| funct12_eq_001) & !flush`（覆盖 `is_trap_1` → `mcause_final`/`mepc_final`） | 0.15d |
| 7 | `ip/cpu/plugins/csr_chmem.h:473-478`（branch LATE） | `mcause_` / `mepc_` 的 trap 直写加 `!flush` 守卫。**⚠ 形式必须为「`flush=1` 时保持当前值」**：`select(flush, ch_uint<32>(mcause_), <原式>)`，或把 `!flush` 并入 `funct12_eq_000`/`funct12_eq_001`。**禁**写成 `select(flush, ch_uint<32>(ch_literal<0,32>{}), <原式>)` —— 那是冻结期**写入 0**，比不守更糟（架构可见副作用） | 0.15d |
| 8 | `ip/cpu/plugins/csr_chmem.h`（`flush` 读取） | 两个闭包各加一次 `flush` 读取（`node_of_logic_stage("fetch")` + `has(FLUSH)` 守卫 + 缺省 `ch_bool(false)`）；CSR 已有 `("fetch", NORMAL)` 闭包（注册行 `:494`，`mcycle_` 在 `:499`）可作范式 | 0.15d |
| 9 | `tests/cpu/test_cpu_chmem_fetch_stall.cpp`（新） | **11 个 case**（AC-FS-1/2/3/4/5/6/7/8/10/11 + 顺序契约 AC-FS-12）＋测试桩 producer（§8.1）。**不含** AC-FS-9 / AC-FS-15 / AC-FS-16（这三条是 `chmem-ptw-walk` 3c 的真 MMU 验收项）、AC-FS-13（5 个 gate，跑 CI 脚本）、AC-FS-14（合并交付项） | 1.5d |
| 10 | `tests/CMakeLists.txt` | 手工加入 `CHMEM_TEST_SOURCES` | 0.1d |

**合计 ≈ 3.2d**（含测试）。**不含** PTW FSM 本体（3a/3c）。

> ⚠ 第 2/3 项的 `ptw_active` / `paddr_valid` / `FLUSH` 读取 MUST 走 FS-C-4 的「`ch_literal<0,1>` 初始化 + `has()` 守卫」范式 —— MMU 未注册时（当前 `cpu_factory_chmem` 尚未注册 `MMUPluginChmem`）恒 `false` ⇒ 与现状等价。
> ⚠ `mmu_ptw_chmem.h:165` 的 `at_stage("mmu_ptw", Phase::NORMAL, create_fsm)` 需补一句注释：**该 stage 仅用于 FSM 构建（`create_fsm()`），不交付 payload** ⇒ 不构成 ADR-040 §10.6 的跨 stage 数据依赖（NIT-1）。
> ⚠ `MMUPlugin_chmem.h` 的 `hit = hit || hit_lane[i]` 用了已 deprecated 的 `operator||`（既有代码，FS-C-5 只约束新写法）；新增行必须用 `|`。

---

## 8. 验收判据（AC）

### 8.1 注入机制（rev.4 修正）

所有 AC 需要**可控的 `ptw_active` 驱动**，而 Phase B 写的是常量 0。注入方式：**测试桩 producer 插件**（范式 = `tests/cpu/test_cpu_chmem_keys_modality.cpp` / `tests/mmu/test_mmu_keys_modality.cpp`）。

#### (a) 桩如何进入被测流水线（rev.4 新增 —— 原稿缺此定义，实现者会撞墙）

**问题**：`CpuFactoryChmem::build_cpu()` 在**返回前**就调用了 `pb->build()`（`cpu_factory_chmem.h:444`），而 `PipeBuilder::build()` 遍历 `plugins_` ⇒ **`build_cpu()` 返回后无法再注册任何插件**。而 fetch-stall 的 AC 又必须跑真实 5-stage（populate `:211-216`、fetch→decode 前送 `:223-231`、decode/execute/memory LATE 链接 `:361-417`/`:420-436`）—— 那些是 factory **内部 lambda**，测试无法复用。

**规范：二选一，MUST 在 3g 里明确落地是哪一种**

- **方案 A（推荐）**：给 `build_cpu()` 加可选入参 `std::vector<std::unique_ptr<PluginBase>> extra_plugins = {}`，在 `pb->build()` 之前追加注册。零复制、无漂移。
  > ⚠ **所有权副作用**：`unique_ptr` 移交后测试**丢失句柄**，无法再调 `script()`。⇒ 测试须先保存**裸指针**（`auto* stub = new StubPtwProducer{...}; extra_plugins.emplace_back(stub);`）再传入，并在 `script()` 时用该裸指针。这是方案 A 唯一需要注意的点；若不愿引入裸指针，走方案 B。
- **方案 B**：测试自建 pb 并**逐条复制** factory 接线。若走此路，MUST 在测试文件头列出复制的闭包清单（`:211` fetch populate / `:212-216` decode populate / `:223-231` fetch→decode 前送 / `:361` decode LATE / `:387` execute LATE / `:420` memory LATE / `:444` build），并对 drift 加注释锚定。

#### (b) 桩必须满足的三个前提

1. **`tlb_lookup_ifetch` 节点当前不存在** —— `cpu_factory_chmem.h` **未注册 `MMUPluginChmem`**（`grep MMUPluginChmem ip/cpu/cpu_factory_chmem.h` 零命中）⇒ `pb.node_of_logic_stage("tlb_lookup_ifetch").get()` 返回 `nullptr`。**必须由桩自己建节点**：`declare_substage` 会 `nodes_.emplace(sub, make_shared<PipeNode>(sub))`（`pipe_builder.h:122-132`）。
2. **`PluginBase::build()` 是纯虚**（`plugin_base.h:69` `virtual void build(PipeBuilder&) = 0`）⇒ 桩必须实现它，否则是抽象类、无法实例化。
3. **cell 类型必须精确匹配 key 的 `T`** —— `PayloadStore::has()` 按 `typeid` 判（`payload.h:165`/`:232`），而 `put`（`payload.h:123-128`）**不校验类型**就写入（`cells_[&key]; cell = std::move(value)`）⇒ 类型不符时 `has()` 静默 `false`（消费端回退默认值），报错位置极具误导性。
   > ⚠ **上游注释已过时**：`payload.h:122` 注释声称"写入 type-checked（typeid 校验仍保留）"，与 `:123-128` 的实现**相反** —— 校验只存在于 `get`（`:141`/`:158`，且是**抛异常**）与 `has`（`:165`，静默 false）。凡使用 `has()` 守卫的路径（含 `ibus_chmem.h:141-143`）**不会**抛，只会静默走默认值。
   `mmu_keys<std::uint64_t>` ⇒ `PADDR` / `MMU_VADDR` 的 cell 是 **`ch_uint<64>`**（不是 32）；`PTW_ACTIVE` / `PADDR_VALID` 是 `uint_t<1>` ⇒ **`ch_uint<1>`**。

```cpp
// 测试桩: 在 ("fetch", EARLY) 写 PTW_ACTIVE / PADDR_VALID / PADDR / MMU_VADDR，值由用例脚本化
class StubPtwProducer : public PluginBase {
 public:
  void setup(PipeBuilder& pb) override {
    // ① 自建 tlb 节点（factory 未注册 MMUPluginChmem ⇒ 节点不存在）
    pb.declare_substage("fetch", "tlb_lookup_ifetch", 1);
    pb.at_stage("fetch", Phase::EARLY, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("tlb_lookup_ifetch").get();   // ① 保证非空
      // ② 直写范式 = MMUPlugin_chmem.h:278-282 的权威写法；无 put_or_assign 这种 API
      n->operator()(MmuKeys::PTW_ACTIVE)  = ch::core::ch_uint<1>(busy_);
      n->operator()(MmuKeys::PADDR_VALID) = ch::core::ch_uint<1>(valid_);
      // ③ key 的 T = std::uint64_t ⇒ cell 是 ch_uint<64>，必须精确匹配
      n->operator()(MmuKeys::PADDR)       = ch::core::ch_uint<64>(paddr_);
      n->operator()(MmuKeys::MMU_VADDR)   = ch::core::ch_uint<64>(mmu_vaddr_);
    });
  }
  void build(PipeBuilder&) override {}      // ② 纯虚必须实现

  // 用例在 create_simulator() 之前调用 script()，传入**逐拍变化的 live 节点**
  // ⚠ 必须传节点/表达式，不能传常量：ch_reg::operator<<= 存的是"每拍重算的
  //    next-value 表达式"（范式: tests/cpu/test_cpu_chmem_keys_modality.cpp:173
  //    `src_->operator<<=(!(*src_))` 自翻转）。传常量 ⇒ 整轮恒定，AC-FS-2/3/8
  //    需要的"连续 N 拍 busy 后释放"序列无法构造。
  void script(ch::core::ch_uint<1> busy, ch::core::ch_uint<1> valid,
              ch::core::ch_uint<64> paddr, ch::core::ch_uint<64> vaddr) {
    busy_ <<= busy;  valid_ <<= valid;  paddr_ <<= paddr;  mmu_vaddr_ <<= vaddr;
  }

  // 逐拍序列示例（AC-FS-2/3：连续 3 拍冻结后释放）
  // ⚠ 本函数**只**驱动 busy_/valid_/cnt_；需要真实地址的场景（AC-FS-7 跨页、
  //    AC-FS-11 页内多拍）须另调 script(...) 一并设 paddr_/mmu_vaddr_，
  //    否则二者停在 rst 值 0（同页、paddr=0）⇒ AC-FS-7 的跨页条件不成立。
  //   cnt_ 每拍自增；busy = select(cnt_ < n_freeze, 1, 0)，valid = select(cnt_ < n_freeze, 0, 1)
  //   ⚠ 地址两路**一并传入**：ch_reg::operator<<= 是后写覆盖前写，分两次调会把冻结
  //     序列冲掉。AC-FS-7（跨页）/ AC-FS-11（页内多拍）MUST 用本签名一次设全。
  void script_freeze_then_release(ch::core::ch_uint<32> n_freeze,
                                  ch::core::ch_uint<64> paddr_expr,
                                  ch::core::ch_uint<64> vaddr_expr) {
    cnt_ <<= cnt_ + ch::core::ch_uint<32>(ch::core::ch_literal<1, 32>{});
    busy_  <<= select(cnt_ < n_freeze, ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}),
                                       ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}));
    valid_ <<= select(cnt_ < n_freeze, ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
                                       ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}));
    paddr_ <<= paddr_expr;
    mmu_vaddr_ <<= vaddr_expr;
  }
  //   ⚠ 测试 MUST 只调 `script()` 或只调 `script_freeze_then_release()` 之一 ——
  //     二者都写 busy_/valid_，叠加调用 = 后写覆盖前写。
 private:
  // ⚠ 初值 MUST 满足 FS-C-12 不变式：rst 态 = "已翻译 / 未冻结"
  //   （busy=0 且 valid=1）。若初值写成 busy=0 & valid=0，则 tick 0 处于
  //   sv 模式下的**不可能状态**，该拍发起的 bare 读会在 tick 1 裸提交
  //   —— 即**测试桩自己制造**被测缺陷。
  ch::core::ch_reg<ch::core::ch_uint<1>>  busy_{ch::core::ch_literal<0, 1>{}};
  ch::core::ch_reg<ch::core::ch_uint<1>>  valid_{ch::core::ch_literal<1, 1>{}};
  ch::core::ch_reg<ch::core::ch_uint<64>> paddr_{ch::core::ch_literal<0, 64>{}};
  ch::core::ch_reg<ch::core::ch_uint<64>> mmu_vaddr_{ch::core::ch_literal<0, 64>{}};
  ch::core::ch_reg<ch::core::ch_uint<32>> cnt_{ch::core::ch_literal<0, 32>{}};
};

// script_freeze_then_release(n, paddr, vaddr) 的逐拍实测序列（n=3；ch_reg 在**同拍末**更新，
// select 读的是**当前拍**的值 ⇒ 存在 1 拍相位，下表已吸收，不是缺陷）
//
//   tick | 读到 cnt_ | busy_ | valid_ | hold | 说明
//   -----+-----------+-------+--------+------+-------------------------------
//    0   |     0     |   0   |   1    |  0   | rst 态 = 已翻译（合法）
//    1   |     1     |   1   |   0    |  1   | 冻结第 1 拍
//    2   |     2     |   1   |   0    |  1   | 冻结第 2 拍
//    3   |     3     |   1   |   0    |  1   | 冻结第 3 拍（n 拍 ✓）
//    4   |     4     |   0   |   1    |  0   | 释放（T_x）
//
// ⇒ busy=1 的窗口恰为 tick 1..n（n 拍）。AC-FS-2 写"连续 3 拍冻结"时 n_freeze=3。
```

- **`PADDR` / `MMU_VADDR` 必须驱动**：否则 `ibus_chmem.h:155` 的 `select(paddr_valid, paddr, mmu_vaddr)` 无法构造"fetch 地址 = translated B+4"（AC-FS-3 ④）。
- **`<<=` 必须在 Simulator 创建之前设置**（`Simulator` 初始化时快照寄存器链）。
- **reset 语义：本套 AC 一律不调用 `Simulator::reset()`**。理由与口径说明：项目内两个范式**口径相反** ——
  - `tests/cpu/test_cpu_chmem_keys_modality.cpp:190` / `tests/mmu/test_mmu_keys_modality.cpp:175` 在 `create_simulator()` **之后**调 `reset()`；
  - `tests/cpu/test_cpu_chmem_vendored_elf.cpp:155-159` **不调** `reset()`（注释说明 reset 会把 `ch_reg` 打回 `rst_val`）。

  `reset()` 会把 `busy_`/`valid_`/`cnt_` 打回 `ch_literal<0,N>` ⇒ 脚本序列从第 2 拍起才生效，与 AC-FS-1「次拍起」的措辞叠加后容易错拍。**本协议定案：不调 `reset()`**，所有断言从**第 1 拍**（`create_simulator()` 后的首个 tick）起算；AC 文本中"次拍"特指"sread 1 拍延迟导致的下一拍"，与 reset 无关。
- 全部 cell / 寄存器初值用 `ch_literal<0,N>`（FS-C-4），**不得**用 `ch_uint<N>(0)`（null impl ⇒ SEGV）；`ch_reg<ch_bool>` 的 rst_val 用 `ch_bool(false)`（项目既有 idiom，`ibus_chmem.h:115`）。
- **⚠ 规范性检查项（本节自带，不进 ctest）**：本节桩代码 MUST **能通过编译**。rev.2 用了不存在的 API（空指针）、rev.3 用了不存在的 `put_or_assign` + 错宽度 —— 两轮同类缺陷 ⇒ 桩的"可编译"是**作者/评审侧自检**，不是测试判据（故**不**列在 §8.2 的 AC 表内）。
  **rev.8 已实测通过**（rev.4/5/6/8 各跑一次，结论一致；rev.8 的演练文件已同步三参数 `script_freeze_then_release` **并新增 (b) 的 fault 项表达式**）：
  ```
  g++ -std=c++23 -fsyntax-only -DCF_PLUGIN_USE_CH_MEM \
      -I include -I . \
      -I build/_deps/install/include/cpp-hdl{,/core,/ast,/bundle,/axi4,/utils,/bv,/cpu,/lnode} \
      /tmp/opencode/fs_stub_check.cpp
  → exit 0，零 error
  ```
  剩余 warning：仅上游 `utils/logger.h:33/40` 的 2 条 `-Wformat-security`（非本协议代码）。演练文件里的 `#define CF_PLUGIN_USE_CH_MEM` 重复定义已删除（此前会与命令行 `-D` 冲突）。
  演练文件同时照抄了 §2.3 的三信号表达式（`hold` / `next_pc` 嵌套 select / `hold_z_ <<= hold` / `FLUSH = br_taken_buf | hold_z`）**以及 FS-C-18 (b) 的合并批次表达式**（`fault_live = ch_uint<8>(exc_code) != ch_uint<8>(ch_literal<0,8>{})`，`hold_z_ <<= hold | (fault_live & !paddr_valid)`）⇒ §2.3、§8.1 与 (b) 的 API 面**均已编译验证**，非纸面推演。**⚠ 范围说明**：编译只证明 API 面存在，不证明 (b) 的语义正确 —— 语义由 AC-FS-14 ⑤ 在合并批次验。
- **⚠ 桩测不到的事**：`ptw_active` 由桩**直接驱动**，因此"真 MMU 驱动下 miss 检出同拍是否为 1"（FS-C-12）**原理上测不到** ⇒ 该条作为 `chmem-ptw-walk` 3c 的验收项（AC-FS-9），不靠桩。

### 8.2 判据

| ID | 判据 | 判据形式 |
|----|------|---------|
| **AC-FS-1** | `FLUSH` 两分量均生效 | 桩注入 `ptw_active=1 & paddr_valid=0` ⇒ 下一拍起寄存器写使能 0、store 使能 0；既有 `br_taken_buf=1` 路径行为不变（回归） |
| **AC-FS-2** | 保持窗内 PC 不前进 | 桩：`ptw_active=1 & paddr_valid=0` 连续 3 拍 ⇒ `pc_reg` 不变 |
| **AC-FS-3**（释放端） | **释放拍不丢指令、不重复执行** | 逐拍断言（按 §2.3 表）：`T_x` 拍 `pc_reg` next = `B+4`；`T_x` 拍 `FLUSH=1`；`T_x+1` 拍 `FLUSH=0`；`T_x+1` 拍**发起的读地址 = `B+4`（不重复 B）**；目标寄存器在 `T_x+1` 写**一次**且值为正确指令结果 |
| **AC-FS-4**（相对判据） | 裸机路径**零新增 FAIL** | `hold` 恒 0 ⇒ `FLUSH` 与现状逐位相同。判据 = **每个 binary 各自**：改前 vs 改后 **failing-set 逐条相等**（`chipforge_tests` 各自、`chipforge_tests_chmem` 各自；不跨 binary 比较，不引具体数字 —— 并发 change 会改基线） |
| **AC-FS-5** | CSR 三通路都被冻结抑制 | 桩冻结期：① 注入写 `mscratch` 的 CSRRW ⇒ `mscratch_` 不变（验 `write_en_ch` 单点守卫）；② 注入 `imm=0` 的普通 I-type（如 `addi x5,x6,0`，funct12=0）⇒ `mcause_` 不变（验 **`is_trap_branch`** 守卫，这正是 opcode 漏检放大面）；③ 注入 ECALL ⇒ `mcause_`/`mepc_` 不变（验 branch-LATE 直写守卫） |
| **AC-FS-6** | **变异对照 ≥5 全红** | ① `FLUSH` 漏 OR `hold_z` ⇒ AC-FS-1 / AC-FS-8 RED；② `pc_reg` 漏保持 ⇒ AC-FS-2 / AC-FS-7 RED；③ `hold` 漏 `!paddr_valid` 项（写成只用 `ptw_active`）⇒ AC-FS-3 RED；④ `FLUSH` 改读 `hold`（去掉延迟）⇒ AC-FS-8 RED；⑤ 去掉 branch 压 freeze（`next_pc` 只 `select(hold, ...)`）⇒ AC-FS-7 RED |
| **AC-FS-7**（branch redirect，rev.3） | walk 期间的 branch redirect 不被吞 + 旧 walk 不得污染 | 顺序跨页且 `@B-4` 为 taken branch：① `pc_reg` 下一拍 = `br_target`（不是 `B`）；② `br_target` 路指令正确提交一次；③ 旧 walk 完成时**不得**把 `paddr_valid` 误报给新地址（FS-C-14） |
| **AC-FS-8**（进入端，rev.3） | **冻结前最后一拍合法指令不被误杀** | 顺序跨页（`@B-4` 写 rd 指令、`@B` miss）：断言 `@B-4` 的 rd **恰好被写一次**、且 PC 最终越过 `B`。变异对照：`FLUSH` 改读 `hold`（去掉 `_z`）⇒ RED |
| **AC-FS-9**（3c 验收项） | **进入拍 `PTW_ACTIVE` 时序（FS-C-12）** —— **不靠桩** | 真 MMU 驱动：构造 `PADDR_VALID=0` 的首拍 `T`，断言 `PTW_ACTIVE(T)=1`（含"registered busy"反例：晚一拍置 1 ⇒ RED），且 `pc_reg` 在 `T` 拍**不**前进（越过 `B` 即 RED）。⚠ fault 路径不在本条：3c 的负向检查见 **AC-FS-15/16**，合并交付后见 AC-FS-14 |
| **AC-FS-10** | `mcycle_` 冻结期继续计数（FS-C-17） | 桩：冻结 3 拍 ⇒ `mcycle_` 增 3（不增 ⇒ RED，说明误加了守卫） |
| **AC-FS-11** | `PTW_ACTIVE` 覆盖整个 walk + `PADDR_VALID` 持续契约 | 构造 `vaddr[15:12] ≠ paddr[15:12]`：① `T_x-1` 的 bare fetch 读到的是一条**会写 rd 的合法指令** ⇒ 断言该 rd **未被写**；② `T_x` 之后同页连续 3 拍 `PADDR_VALID` 恒 1（非脉冲） |
| **AC-FS-12** | 顺序契约成立 | 生产者 `("fetch", EARLY)`、消费者 `("fetch", NORMAL)` + `("branch", LATE)`；单测断言消费者读到的 `ptw_active` 与生产者写入的是**同一 DAG 节点**（`impl()` 指针相等，范式见 `test_mmu_keys_modality.cpp`） |
| **AC-FS-13** | 5 架构 gate 全 PASS | `verify_adr` / `verify_plugin_decision` / `check_plugin_portability`（13/13）/ `doc_link_check` / `roadmap-r9-check` |
| **AC-FS-14**（FS-C-18，**合并交付项 —— 3c 的门是 AC-FS-15/16**） | fault 拍取指不得裸提交 + PC 转向 trap + 清单 a/b/c 逐项 | PTW fault 与 `csr-chmem-trap-pc-jump`（+ C.1 `TRAP_PENDING`/`TRAP_TARGET`）**同批交付时**：① 该拍发起的 bare 取指**未被提交**（rd 写 / store / branch 三者任一，逐拍断言）；② `pc_reg` 下一拍 = `mtvec`；③ `mmu_keys::EXCEPTION_CODE` cell 被置（**直接读该 cell**，`mmu_keys.h:86`，`uint_t<8>` 已 mode-aware；CH_MEM 侧 CPU 消费通路 `hazard_chmem.h:182-187` 明写 defer 到 C.1/C.5）；④ `mepc` = faulting PC（**不是** `pc_plus_4`）；⑤ **结构**：`hold_z` 含 fault 项（清单 b）**且** `PTW_ACTIVE` 派生式**含** fault 排除项（FS-C-12 的 `fault` 周期归 0 —— 两处都必须有，缺一即违约：**只有 `hold_z` 那处** ⇒ 合并批次的 `next_pc` 最外层是 trap（FS-C-15）⇒ 不会永久冻结，但 fault 拍会**多一个 `hold`/`FLUSH` 拍**（取指被多压 1 拍）；**只有 `PTW_ACTIVE` 那处** ⇒ 裸提交（rev.6 的洞） |
| **AC-FS-15**（**3c 验收项**，FS-C-18 的 3c 半段 —— **行为负向检查**，rev.10/11 修订） | **3c 的 walk 不消费 fault ⇒ 无效翻译不得被放行** | **测试层级**：plugin 级 pb + 直接注入 PTE。⚠ **不靠桩**（同 AC-FS-9）：`PTW_ACTIVE` / `PADDR_VALID` MUST 由 **3c 的真 MMU 适配器**产生（即 `MMUPluginChmem` 的 `("fetch", EARLY)` 闭包 `ip/mmu/plugins/MMUPlugin_chmem.h:281-282` 由常量改为派生式），**不得**桩驱动这两个 cell —— 桩驱动下本条是**真空通过**。⚠ **派生链在 scope 内**：该闭包 MUST 以 `sv_active` / `paddr_valid`（及其定义语句）为 `PTW_ACTIVE` 的**唯一**数据来源 —— **这两条定义语句也在 AC-FS-16 ① 的检查 scope 内**（否则 `paddr_valid = sv_active && hit && !PTW_FAULT` 这类**操作数投毒**不在机械覆盖内）。**注意** `tests/cpu/test_mmu_ptw_fsm_chmem.cpp:46-62` 的 `PtwFixture` **只注册 `PtWalkFsmPlugin`**，观测不到这两个 cell ⇒ 不能照抄它当 harness。<br>向 walk 喂三类**坏 PTE**（编码以代码为准）：① **`V=0`**（invalid，L0/L1 均 → FAULT，`:267`/`:274`）；② **`V=1 && R=1 && W=1`**（reserved，`is_reserved = r_sig && w_sig`，`:251`/`:268`/`:275` → FAULT）；③ **仅在 level-0 响应**喂非叶指针 **`V=1 && R=W=X=0`**（`:276` → FAULT）。⚠ **level-1 的 `is_ptr`（`:270`）是正常路径**（L0_WAIT → L1_WAIT），**不得**当坏 PTE。<br>逐拍断言：① 该窗口内 `PADDR_VALID` **不得**为 1；② **`PTW_ACTIVE` 恒为 1**，窗口按**取指事实**定义 = 「从该地址 miss 检出起，直到 `PADDR_VALID=1` 或测试窗口结束」，**含 walk 进入 `FAULT` 的拍及其后各拍**（即"该地址的翻译始终无效"的全窗）+ `sv_active=1`。⚠ **不得用 "FSM 在途/未在途" 定义窗口** —— `mmu_ptw_chmem.h:283` 是 `FAULT → IDLE` **无条件**、`fault()` = `sm.is_in(FAULT)`（`:189`）⇒ `fault` 只有 1 拍，而那一拍**正是 walk 已终止**；用"在途"定义会把违约拍排除在窗口外（rev.3 从 FS-C-12 删掉同一限定的教训，见 `:94`）。这是锋刃：实现 `PTW_ACTIVE = sv & !pv & !ptw_fault` 只会在 FAULT 拍掉 0 ⇒ 窗口**含该拍**时断言 ② 才 RED。<br>**前提断言（anti-vacuity，与 AC-FS-9 同款）**：三类坏 PTE **各自** MUST 观察到 walk 确实进入 `FAULT`（`state_out()` == `PtWalkState::FAULT`（枚举值 **4**，见 `ip/cpu/plugins/mmu_ptw_chmem.h:136`；访问器 `:187`）或 `fault()==1`）**≥1 拍**；**未观察到 ⇒ 本条判 FAIL（不是 PASS）**。<br>**观察对象 MUST 是适配器实际消费的那一个 FSM 实例**（**角色定义，与所有权无关** —— 驱动权在测试侧还是适配器侧是另一回事）⇒ 两条机械锚，MUST 至少满足其一：**(a) 句柄同源断言**：harness 取到的 `PtWalkFsmPlugin*` 与适配器消费的实例 **指针相等**（范式 = AC-FS-12 的 `impl()` 指针相等断言；入口 = `PipeBuilder::plugins()`，`include/cf/plugin/pipe_builder.h:476-478`；若适配器自持 FSM，则 MUST 暴露 test-facing 访问器如 `ptw_fsm()`）；**(b) 因果耦合证明（无 accessor 依赖，与拓扑无关；**观察面不钉文件** —— `PADDR_VALID` 是 payload cell + FSM 实例由 harness 直接喂，不经任何插件内部 ⇒ walk 触发挪到哪个插件都不影响本锚）**：除坏 PTE 外，harness MUST 再喂一次**好 PTE** 并断言 refill 发生 / `PADDR_VALID` 由 0→1；**若好/坏 PTE 下行为相同（或 `PADDR_VALID` 恒 0）⇒ 观察到的是诱饵实例 ⇒ 判 FAIL**（refill 必然经过被消费的实例）。<br>**驱动权归属**：测试直注 PTE 还是适配器自驱 walk（两者写同一组 FSM 输入）MUST 在 3c 里明确落地（与 §8.1 "二选一，MUST 在 3g 里明确落地" 同款问题）；当前由本前提断言 **fail-closed** 兜底（自驱覆盖注入 ⇒ 观察不到 FAULT ⇒ 判 FAIL）。依据：② 的判别力**只**来自那 1 拍 —— `fault` 只在 FAULT 拍为 1（`:283` 无条件回 IDLE）。若 harness 喂了坏 PTE 但 FSM 常驻 IDLE（没发 `start` / `pte_valid`），违例实现的 `ptw_fault` 恒 0 ⇒ `PTW_ACTIVE ≡ sv & !pv` ≡ 1 ⇒ ② 与 ① 双双绿 ⇒ **本条对目标违例零判别力** ⇒ 必须用前提断言把这条跑法判死。<br>⚠ **Bare 模式不适用**：`PTW_ACTIVE` 在 Bare 下 MUST 为 0（FS-C-4），窗口必须限定 `sv_active=1`，否则会逼实现者违反 FS-C-4（全机冻结）。<br>**注（既有缺陷，非本条引入）**：`mmu_ptw_chmem.h:220` 的注释把 reserved 写成「`V=1` 且 `R=1 W=0 X=0`」，与 `:247`/`:251` 的代码及 `test_mmu_ptw_fsm_chmem.cpp:145-157` 的既有断言（`{V=1,R=1,W=1,X=0}` → `fault_code=2`）**不一致** ⇒ 引用时以代码为准；该注释修正登记为既有债 |
| **AC-FS-16**（**3c 验收项**，FS-C-18 的 3c 半段 —— **结构反回归**，rev.13 重写） | `PTW_ACTIVE` / `hold` 不得消费 fault | **⓪ 提取范式（MUST，三条判据共用 —— 不指定则同一语句的两种合法读法给出相反判定）**：**⓪-a 全文先剥注释**（`//` 与 `/* */`）**再做一切**（否则 RHS 区域注释里的 `;` 会把终止符截断 ⇒ RHS 取短）；**⓪-b 写语句** = LHS **字面**含对应 key 名（`PTW_ACTIVE` / `hold` / `hold_z_` / `FLUSH`）、以 `;` 终止的语句（**可跨行**，**全部**写语句都要查，不只第一条）；**LHS MUST 字面含 key 名 —— MUST NOT 经局部别名/宏间接写 key**（如 `const auto kf = IBusPlugin::FLUSH;` 后 `n->operator()(kf) = …` ⇒ 写语句 LHS 无 key 名 ⇒ 脱离检查 ⇒ 门对该写点**静默空转**）；**⓪-c 操作数定义语句** = 同一闭包内 LHS 含该操作数名（`sv_active` / `paddr_valid`）的语句，**全部**检查；**⓪-d 取该语句 `=` 与终止 `;` 之间**的文本 ⇒ 用 `[A-Za-z_][A-Za-z0-9_]*` 提取 **token 集**（含限定名的**每一段**：`MmuKeys::PTW_ACTIVE` ⇒ {`MmuKeys`, `PTW_ACTIVE`}；`operator()` ⇒ {`operator`}）⇒ 与白名单比对。<br>**① RHS 操作数白名单（主判据，机械可判定）**：`PTW_ACTIVE` 写语句 **+ 其操作数定义语句**（`paddr_valid` / `sv_active` 的定义语句，见 scope 说明）RHS 的 token 集 MUST ⊆ **W1** = {`sv_active`, `paddr_valid`, `paddr_valid_for_current_addr`, `hit`, `chc`, `ch_bool`, `ch_uint`, `ch_literal`, `select`, `n`, `operator`, `MmuKeys`, `PTW_ACTIVE`, `bits`, `kXlenBits`, `satp_`, `satp`} + 运算符/标点。**W1 词表取自真实代码**（`MMUPlugin_chmem.h:266-269` 的 `sv_active` / `paddr_valid` 定义 + `:280-282` 的写语句），非臆造。**白名单外 token ⇒ FAIL**（`no_exc` / `derive_ptw_active` / `nf` / `fault` / `PTW_FAULT` / `FAULT` 一律 FAIL，**与命名无关** ⇒ 中性别名绕不过）。**3c 若需新操作数、或 `sv_active`/`paddr_valid` 定义语句的词表形态变化，MUST 在**同一 commit** 内修订本 AC 的 W1**（spec 变更 ⇒ 对 reviewer 可见）—— **fail-closed，无自证通道**。**pin**：3c 的这两条定义语句 MUST 保持 `MMUPlugin_chmem.h:266-270` 的词表形态（`bits` / `kXlenBits` / `satp_`）。⚠ **不做"传递闭包"式检查** —— 那需要 def-use 分析（哪个 token 在哪定义），非 grep 机械可判 ⇒ 写了反而制造新的不可判定；**漂移的救济就是"改 AC"**（spec 可见、fail-closed）。<br>**② 消费侧同款**：`ip/cpu/plugins/ibus_chmem.h` 内 `hold` / `hold_z_` / `FLUSH` **三条写语句**的 RHS token 集 MUST ⊆ **W2** = {`hold`, `hold_z`, `hold_z_`, `ptw_active`, `paddr_valid`, `br_taken_buf`, `br_taken`, `br_target`, `pc_val`, `pc_reg`, `pc_lag`, `chc`, `ch_bool`, `ch_uint`, `ch_literal`, `select`, `n`, `operator`, `FLUSH`, `MmuKeys`, `PTW_ACTIVE`, `PADDR_VALID`} + 运算符/标点。⚠ **W2 MUST 含 `hold_z` / `hold_z_`** —— 协议 §9.1 自述的 3g 形状就是 `FLUSH = br_taken_buf | hold_z`（缺 `hold_z` ⇒ **自家作用域内不可满足**，与 rev.10 的行号 allowlist 同构）。**`fault` / `ptw_fault` / `PTW_FAULT` / `FAULT` / `EXCEPTION_CODE` 均不在 W2 内** ⇒ 3c/3g 阶段一旦出现即 FAIL（合并交付后按 AC-FS-14 ⑤ 接管）。<br>**③ 提示性扫网（非判据，供 reviewer 复查）**：变换判据列出候选 —— 对命中行删掉所有 `default`（不区分大小写）后仍含 `fault` 者为候选。命令（**内联，不开围栏**）：`for f in ip/mmu/plugins/MMUPlugin_chmem.h ip/cpu/plugins/ibus_chmem.h; do grep -ni fault "$f" | grep -vE '^[0-9]+:[[:space:]]*(//|/\*|\*)' | sed 's/default//Ig' | grep -ni fault; done`。（a）**大小写不敏感是 MUST**（`grep -ni`）—— ① 点名的 `PTW_FAULT` / `PtWalkState::FAULT` 是大写；（b）**禁** `-w`（实测 `grep -nw fault` 对 `fault_live`/`ptw_fault`/`PTW_FAULT`/`fault_code()` **只命中 `!fault`**）；（c）排除**整行**注释行（范式 = `tools/check_plugin_portability.sh:482`/`:501`）；（d）`default` 是唯一与 `fault` 相交的合法词（`de-fault`），且 `fault` 不是 `default` 的子串 ⇒ 无法用命名同时消掉两者。**候选 MUST 在该 change 的 `tasks.md` 登记「可核对工件」**：① 该行的**原文抄录** ② 其**定义处**（file:line + 原文）—— 供 reviewer 逐字 diff，**不接受散文式自证**。**实测（rev.11）**：pristine 两文件 ⇒ 候选**为空**；按 §7 item 1 在 `ibus_chmem.h:80` 插入 `hold_z_` 成员后 ⇒ 候选**仍为空**（行号漂移免疫）；注入 `auto bad = !sm.is_in(PtWalkState::FAULT);` ⇒ **被扫出**。<br>**④ 作用域不得由被检者自选收窄**；且 **scope MUST 跟随 AC-FS-15 点名的 producer 所在文件** —— 若 3c 把派生式挪进 `RiscvMMUPluginChmem`（⚠ **该类当前全仓不存在**，仅 3 处注释提及），scope 随之迁移。<br>**⑤ 作用域 = 仅 3c / 3g 交付物**。**合并交付后本条整体退役**（scope 已限定 3c/3g）：届时按 FS-C-12 `PTW_ACTIVE` 派生式**需含** fault 信息（否则 fault 拍 `PTW_ACTIVE=1` ⇒ 永久冻结，即 rev.5 的 BLOCKER），按 (b) `hold_z_` **需含** fault 项 —— **两处**均由 **AC-FS-14 ⑤** 接管。<br>**分工提示（防误用）**：①② 是**活体接线**的锚（机械、逐语句、命名无关）；③ 是**惰性骨架**的扫网（提示 + 可核对工件）；**行为门 AC-FS-15 才是反真空的那道**。三者不可互相替代。<br>**判据自测（rev.13，13/13 符合预期）**：作者按 ⓪ 的提取范式写了一个判定脚本（`/tmp/opencode/wl_check.py`）跑正/反例 —— **正例 7 条全 PASS**：3c `PTW_ACTIVE = sv_active && !paddr_valid`、别名 `paddr_valid_for_current_addr`、`paddr_valid = sv_active && hit`、`sv_active` 的 satp MODE 解码、3g `FLUSH = br_taken_buf | hold_z`、`hold_z_ <<= hold`、`hold = ptw_active & !paddr_valid`；**反例 6 条全被抓住**：`PTW_ACTIVE` 直读 `MmuKeys::PTW_FAULT`、**操作数投毒**（`paddr_valid = sv_active && hit && !PTW_FAULT`）、中性别名 `no_exc`、抽 helper（`derive_ptw_active`）、消费侧 `FLUSH` 接 `ptw_fault`、`hold_z_` 接 `exc_code`。**自测已抓到 1 个真缺口**：初版 W1 漏了 `bits` / `kXlenBits` / `satp_` ⇒ `sv_active` 的真实定义语句被判 FAIL（合法实现假红）⇒ 已补入 W1。**教训**：机械判据必须先做正/反例自测再提交。<br>**脚本归宿**：本文件引用的是作者原型（`/tmp/opencode/wl_check.py`，**不入库**）⇒ **3c change MUST 把等价自测收进 `tests/` 或 `tools/`**（否则该门在 3c 落地时没有可复现的自测载体）。|

⚠ **AC-FS-15 为什么不含"提交侧"断言**（rev.7 的 ③，已移出 3c 门）：rev.7 曾要求「窗口内无 rd 写 / store / branch 提交」，但 3c **没有 CPU 级观测面** —— `grep -rn 'MMUPluginChmem\|RiscvMMUPluginChmem' ip/cpu/cpu_factory_chmem.h` = **0 命中**（注册归 C.4），抑制接线（`FLUSH`/`hold`）归 **3g** ⇒ 该断言在 3c 要么真空通过、要么必然 RED。已并入 **AC-FS-11 ①**（3g）与 C.4 后的回归项 |

> **编号说明**：AC-FS-13 曾是「桩可编译」，已降级为 §8.1 末的**规范性检查项**（不进 ctest）⇒ 本表该号给了 gates。**AC-FS-15/16 是 rev.7 新增的 3c 负向检查**（FS-C-18 的 3c 半段）—— 它们**不**进本协议的测试文件，而进 `chmem-ptw-walk` 3c 的验收门（与 AC-FS-9 同批）。

---

## 9. 风险与回滚

| ID | 风险 | 缓解 | 回滚 |
|----|------|------|------|
| R-FS-1 | `FLUSH` 语义被重载（branch flush + PTW freeze 合流）导致既有 branch 测试漂移 | AC-FS-4（零新增 FAIL）+ AC-FS-1 的 branch 分量回归 | 单点回退 `ibus_chmem.h:167` |
| R-FS-2 | `pc_reg` 保持 mux 把 `select` 放进关键路径 | CH_MEM 是组合 DAG ⇒ 无 cycle 代价；Verilog 侧 2 级 select | 回退实装项 2/3 |
| R-FS-3 | ~~`CSR_WRITE_EN` 模式化未完成~~ | ✅ **已解除**（`chmem-cpu-keys-modality` archive 2026-10-22 `d08d3ba`）⇒ 实装项 5/6/7 可开工 | — |
| R-FS-4 | `has()` 守卫或 `ch_literal` 初始化遗漏 ⇒ null impl SEGV | FS-C-4 + 单测覆盖"MMU 未注册"路径 | — |
| R-FS-5 | 覆盖矩阵再漏某个提交点 | 矩阵 14 项含 2 项"允许"/2 项"排除"；新增 commit 点须同步矩阵（评审清单项） | — |
| R-FS-6 | 数据侧 PTW 未冻结 ⇒ load 返回垃圾 / store 写到未翻译地址 | **显式 out of scope**（§1.2）⇒ 归 `chmem-ptw-walk` 3e；本协议不承诺 | — |
| R-FS-7 | 冻结期发生 branch redirect ⇒ 在途 walk 变**陈旧**（walking `B`，fetch 已转 `br_target`） | FS-C-14 把"walk 取消 / 地址标签"作为 `chmem-ptw-walk` 的**接口要求**；`paddr_valid` 必须按**当前** fetch 地址给出（FS-C-13 逐地址契约），否则 AC-FS-7 ③ RED | — |
| R-FS-8 | `FLUSH` 的等价性依赖 `branch_chmem.h:235` 的 `taken && !flush` 自门控 | FS-C-10 要求在 `ibus_chmem.h:184` 加注释锚定；AC-FS-6 ⑤ 覆盖 | — |

### 9.1 与 R11.1（trap PC 跳转）的边界

trap PC 跳转与本协议**语义正交、交付耦合**（耦合点 = FS-C-18）：本协议冻结的是**取指**（PTW 期间的内部状态）；R11.1 处置的是**异常**（`mepc` ← faulting PC，PC ← `mtvec`）。**在 fault 语义交付之前**，两者没有任何共用信号。

**唯一交互点**（FS-C-15）：`next_pc` 的优先级 MUST 为 `trap > branch > freeze > sequential`。R11.1 裁决（2026-10-22）把 ECALL/EBREAK/MRET redirect 交给独立 change `csr-chmem-trap-pc-jump`（插入点 = `ibus_chmem.h:171-185` 的 `update_pc`，**同一处**）⇒ 届时把 trap 提到**最外层**即可，`hold` 层**不动** —— 但**前提是该批交付同时给出对齐读发起拍的 flush 项**（清单 (b)，见 FS-C-18）；`pc_reg ← mtvec` 单独不抑制 fault 拍已发起的读。

另外：若 ifetch PTW 以 **page fault** 结束（PTE `V=0`），`PTW_ACTIVE` MUST 归 0（否则 CPU 永久挂死）。**但"归 0"本身不够** —— 它会让 `hold` 归 0 ⇒ PC 越过 faulting 指令并**每拍执行一条 bare 地址的垃圾指令**（逐拍反例见 FS-C-18）。两害的根因是同一个：`hold` 承担了"PC 冻结"与"压掉垃圾取指"两个语义。

⇒ **本协议的处置：FS-C-18** —— fault 语义 MUST 与 trap redirect（`csr-chmem-trap-pc-jump`，含 C.1 的 `TRAP_PENDING`/`TRAP_TARGET` 接线）**同批交付**；在此之前 **PTW walk MUST NOT 产生 fault**。本协议**不**在这里预留抑制机制（3c/3g **不新增 key、不接回** `PTW_FAULT`，也**不改** `hold` 语义），以避免留下恒不触发的惰性骨架（§6.1 同类禁令）。**但合并交付时 `fault` 有现成载体**：`mmu_keys::EXCEPTION_CODE`（`mmu_keys.h:86` mode-aware；`MMUPlugin_chmem.h:282` 已写在 `tlb_lookup_ifetch` 同节点，届时由常量改为 live 节点）。

**可观测面（写清，避免误读为 3c 就要端到端 trap）**：`EXCEPTION_CODE` 的 key 是 `mmu_keys::EXCEPTION_CODE`（`mmu_keys.h:86`，`Payload<uint_t<8>>`，**已 mode-aware** ⇒ 可承载 `ch_uint<8>` 节点）；CH_MEM 侧的 CPU 消费通路（`mmu_keys::EXCEPTION_CODE → cpu_keys::CPU_EXCEPTION_CODE`）**尚未接线**（`hazard_chmem.h:182-187` 明写 defer 到 C.1/C.5；TLM 侧在 `ip/cpu/plugins/mmu.cpp:128-130`）⇒ 验收时**直接读 `mmu_keys::EXCEPTION_CODE` cell**，不要等 CPU 侧消费。

---

## 10. 跟踪

| Date | Event | Decision |
|------|-------|----------|
| 2026-10-16 | 起草 Draft v0.1（237 行） | Draft v0.1 |
| 2026-10-16 | critic REJECT + 5 BLOCKERS + 7 SHOULD_FIX | protocol 撤回，降级为 requirements doc |
| 2026-10-22 | `chmem-mmu-keys-modality` archive ⇒ B5 的 key 类型半解除 | requirements doc §1.5 更新 |
| 2026-10-22 | `ctrl-link-chmem-reg-handle` archive ⇒ A1/A2 stateful halt 通道就位 | — |
| 2026-10-22 | Phase B（路线 C）落地 ⇒ `PTW_ACTIVE` 契约确立 | — |
| 2026-10-22 | **v0.2 rev.1**：5 BLOCKER 全解；B2 推翻 v0.1 方案骨架 | critic 二轮 review |
| 2026-10-22 | critic 二轮：5 BLOCKER **已解**，但 rev.1 自引入 2 BLOCKER + 4 SHOULD_FIX + 3 NITS | 修 rev.2 |
| 2026-10-22 | **v0.2 rev.2**：`br_taken_buf` 强制 0 + 逐拍表；FS-C-12 解除下界；CSR 7 个 `we_*` + trap 守卫；canonical 序修正；AC 注入机制 | critic 三轮 review |
| 2026-10-22 | critic 三轮：REJECT —— rev.2 的 `ptw_active` 全窗**进入端错相**（误杀页尾最后一条指令）；`pc_reg` 保持吞 redirect；CSR 漏第三条写入路径（`is_trap_1` 仲裁）；§8.1 桩空指针 + 缺纯虚实现 | 修 rev.3 |
| 2026-10-22 | **v0.2 rev.3**（本文件）：`hold`/`hold_z` 双信号（`FLUSH` 对齐读发起时刻）；`next_pc` 内层 select（branch > freeze）；`br_taken_buf` 恢复原样；CSR 三处守卫（`write_en_ch` 单点 + `is_trap_1` + branch-LATE）；桩补 `declare_substage` + `build()`；FS-C-12 重推导；新增 FS-C-13/14/15 + 进入端/branch-redirect 两条 AC（旧编号 AC-FS-11/12，现重排为 **AC-FS-8** / **AC-FS-7**） | critic 四轮 review |
| 2026-10-22 | critic 四轮：**机制层面 APPROVE**（逐拍表逐格无错、`next_pc` 嵌套等价、CSR 枚举闭合、rev.3 的 4 BLOCKER 全清）。REJECT 只卡两点：§8.1 用了不存在的 `put_or_assign` + `PADDR`/`MMU_VADDR` 宽度写错（应为 `ch_uint<64>`）；FS-C-12 的括号限定「且该地址的 walk 在途」使进入拍合规漏洞 | 修 rev.4 |
| 2026-10-22 | **v0.2 rev.4**（本文件）：§8.1 桩改直写范式（`n->operator()(Key) = ...`）+ `ch_uint<64>` + 新增「(a) 桩如何进入被测流水线」路径规范（`build_cpu()` 已在返回前 `pb->build()` ⇒ 二选一方案）+ AC-FS-13（桩可编译）；FS-C-12 去限定改按取指事实定义 + AC-FS-14（3c 验收）；5 SHOULD_FIX（守卫用 `&`/`\|`、branch-LATE 守卫须"保持当前值"、`is_trap_branch` 落点统一、矩阵标题、`TRAP_PENDING` 注）+ 5 NITS | 待 critic 五轮 review |
| 2026-10-22 | **`chmem-cpu-keys-modality` archive**（commit `d08d3ba`）⇒ `CSR_WRITE_EN` 模式化闭合 ⇒ 本协议唯一的硬前置解除 | §6.2 首环闭合 |
| 2026-10-22 | critic 五轮：**机制层继续 APPROVE**（上轮 2 BLOCKER 由 critic 独立复跑编译确认清零；rev.4 的 14 项改动逐条核对无误）。REJECT 只卡 1 个由 rev.4 自身引入的矛盾：FS-C-12 主句漏 `fault=0` ⇒ page fault 期永久冻结（与同段公式 + §9.1 互斥） | 修 rev.5 |
| 2026-10-22 | **v0.2 rev.5**（本文件）：FS-C-12 主句补 `fault=0` + "fault 周期 MUST 归 0" + fault 变体 AC（AC-FS-9 ②）；§2.1「产生方」改派生公式；§8.1 桩补 live 节点逐拍示例（`script_freeze_then_release`）+ reset 口径定案（不调 `reset()`）+ `payload.h:122` 过时注 + 方案 A 所有权副作用；AC-FS-13 移出 AC 表（降为 §8.1 规范性检查项）⇒ AC 重排 1..13；补 `hold ≡ PTW_ACTIVE` 冗余项说明 | 待 critic 六轮 review |
| 2026-10-22 | critic 六轮：**机制层继续 APPROVE**。REJECT 只卡 1 个 rev.5 自身引入的洞：`fault` 排除项把「压掉垃圾取指」一起关掉 ⇒ fault 拍 bare 取指在 `F+1` 裸提交并持续 | FS-C-18 次序约束 |
| 2026-10-22 | **v0.2 rev.6**（本文件）：新增 **FS-C-18**（fault 语义 MUST 与 trap redirect 同批交付；在此之前 walk MUST NOT 产生 fault）+ AC-FS-14；§9.1 重写；§6.2 把 fault 移出 3c；桩 rst 改 `valid=1` + 逐拍 skew 表；§7 item 9 case 数 11→10；`fault` 载体说明（不引入 cell，`PTW_FAULT` 裸 uint8_t 属 3d）；`is_trap_1`/§2.2/§2.4 收尾 | 待 critic 七轮 review |
| 2026-10-22 | critic 七轮：**机制层继续 APPROVE**，且 critic 撤回其机制改法 A（载体论证成立）。REJECT 两点：FS-C-18 的 3c 半段零验证（且 `mmu_ptw_chmem.h:136` 的 FAULT 通路已存在 ⇒ 违规非假想）；"同批交付"里把 `mtvec` 重定向写成替代方案不足以抑制已发起的读 | 修 rev.7 |
| 2026-10-22 | **v0.2 rev.7**（本文件）：新增 AC-FS-15（行为负向：`PADDR_VALID=0 ⇒ PTW_ACTIVE=1` 恒成立、坏 PTE 不放行）+ AC-FS-16（结构反回归：`PTW_ACTIVE`/`hold` 不得消费 fault），挂进 3c 门；FS-C-18 补交付清单 (a)(b)(c)；删除 "PC ← mtvec 二选一" 支；§9.1/§1.2/§6.2 同步；§7 case 数 11 | 待 critic 八轮 review |
| 2026-10-22 | critic 八轮：**机制层第 7 轮连续 APPROVE**，上轮 2 BLOCKER 清零、12/12 落实。但 rev.7 新增的两道 3c 门自身有 2 个 BLOCKER：(b) 的 `fault` 载体不存在且被本文四处封死；AC-FS-15/16 按字面不可执行（reserved 编码错、3c 无观测面、grep 规格不可满足、② 与 (b) 互斥） | 修 rev.8 |
| 2026-10-22 | **v0.2 rev.8**（本文件）：(b) 载体点名 `mmu_keys::EXCEPTION_CODE`（已 mode-aware、已在同节点）+ 四处「不引入」加阶段限定；AC-FS-15 三类坏 PTE 按代码编码（`V=0` / `R=1&&W=1` / level-0 的 `R=W=X=0`）+ 窗口限定 `sv_active=1` + ③ 移出 3c 门；AC-FS-16 改 `grep -nw` + 语句行区间作用域 + 加合并批次合法例外；SF-1/4/5/6 + N-1/2/3/4/6 | 待 critic 九轮 review |
| 2026-10-22 | critic 九轮：**机制层第 8 轮连续 APPROVE**，上轮 2 BLOCKER 清零。rev.8 新引入 1 BLOCKER：两道 3c 门**各自**可绕过（`grep -nw` 漏 `fault_live`/`PTW_FAULT`；"walk 在途"窗口排除 fault 拍）⇒ 组合起来 `PTW_ACTIVE = sv & !pv & !ptw_fault` 能同时过两门 | 修 rev.9 |
| 2026-10-22 | **v0.2 rev.9**（本文件）：AC-FS-16 ① 改子串 grep + 禁 `-w`（附实测反例）+ 排除注释行范式；AC-FS-15 ② 窗口改按取指事实定义（含 FAULT 拍）+ 明文禁"在途"；AC-FS-15 加"不靠桩"与生产者点名；AC-FS-16 ④ 改"合并交付后整体退役"；AC-FS-14 ⑤ 扩为"`hold_z` 与 `PTW_ACTIVE` 两处缺一即违约"；`ch_uint<8>(0)` → `ch_literal` | 待 critic 十轮 review |
| 2026-10-22 | critic 十轮：**机制层第 9 轮连续 APPROVE**。REJECT 1 BLOCKER：AC-FS-16 的 grep 规格大小写敏感 ⇒ 拦不住它自己点名的 `PTW_FAULT`，而行为门对 `PTW_FAULT` 天然无感（裸 uint8_t ⇒ 常量）⇒ 两门同时失明 | 修 rev.10 |
| 2026-10-22 | **v0.2 rev.10**（本文件）：AC-FS-16 ① 加「大小写不敏感是 MUST（`-i`）」+ 作用域钉死为文件整体 + 固定 allowlist（`default` 含 `fault` ⇒ 1+3 项）+ 明禁被检者自选边界；AC-FS-15 补 anti-vacuity 前提断言（坏 PTE 各自 MUST 观察到 `state_out()==4` ≥1 拍，未观察到 ⇒ FAIL）；`:55` 措辞、AC-FS-14 ⑤ 口径、tasks.md 轮次同步；本文件 `git add -N`（有 diff base） | 待 critic 十一轮 review |
| 2026-10-22 | critic 十一轮：**机制层第 10 轮连续 APPROVE**。REJECT 1 BLOCKER：AC-FS-16 的行号 allowlist 因 §7 item 1 必然漂移 ⇒ 假红；补救路径又留"就地扩项"假绿通道 | 修 rev.11（变换判据） |
| 2026-10-22 | **v0.2 rev.11**（本文件）：行号 allowlist → **变换判据**（删 `default` 后仍含 `fault` ⇒ 候选；已实测漂移免疫 + 能抓住违约）；② 升级为逐语句文件级；AC-FS-15 加实例同一性锚 + 驱动权 fail-closed；① 改锚数据流 + 派生式内联 + scope 跟随 producer；5 NITS；rev.10 快照留 `/tmp/opencode/protocol_rev10_base.md`（真 diff base） | 待 critic 十二轮 review |
| 2026-10-22 | critic 十二轮：**机制层第 11 轮连续 APPROVE**，rev.11 的 diff 首次可验证。REJECT 1 BLOCKER：**交付物结构** —— rev.11 把围栏塞进表格行 ⇒ 围栏奇数、表格断裂、`doc_checker.py` 对尾部 66 行失明 | 修 rev.12（围栏移出表格） |
| 2026-10-22 | **v0.2 rev.12**（本文件）：围栏移出表格（命令内联）⇒ `grep -c '^```'` 回到 4、`EOF in_code_block=False`；①② 改 **RHS 操作数白名单**（W1/W2，命名无关的机械判据，无自证通道）；③ 降为提示性扫网 + 候选登记可核对工件；AC-FS-15 前提断言加**句柄同源**与**因果耦合**两条机械锚；3 NITS | 待 critic 十三轮 review |
| 2026-10-22 | critic 十三轮：**机制层第 12 轮连续 APPROVE**。REJECT 2 BLOCKER（rev.12 白名单判据自身）：提取方式未指定 + W 表混用 qualified 形态（两种读法相反判定）；W2 缺 `hold_z` ⇒ 协议自述的 3g 形状必然假红 | 修 rev.13（⓪ 提取范式 + bare-token W + W2 补 hold_z） |
| 2026-10-22 | **v0.2 rev.13**（本文件）：⓪ 提取范式（写语句定位 + RHS 切片 + 剥注释 + token 提取，限定名每段都算）；W1/W2 改 bare-token；W2 补 `hold_z`/`hold_z_`；§7 item 1 钉 canonical 拼写；SF-1 采纳方案 (a)（W1 scope 含 `paddr_valid`/`sv_active` 定义语句 ⇒ 操作数投毒被覆盖）；**判据自测 13/13**（自测抓到 1 个真缺口并修正）；N-2 登记为 3c 执行陷阱 | 待 critic 十四轮 review |
| 2026-10-22 | critic 十四轮：**APPROVE_WITH_NITS —— 判据层首次零 BLOCKER**（机制层连续 12 轮 APPROVE）。2 SHOULD_FIX（⓪ 的别名洞 + 定义语句识别/词表纪律）+ 4 NITS | 修 rev.14 |
| 2026-10-22 | **v0.2 rev.14**（本文件）：⓪ 加「先剥注释再做一切」+「LHS MUST 字面含 key 名，禁别名/宏间接写」+「操作数定义语句 = 同一闭包内 LHS 含该操作数名的语句，全部检查」；W1 逃生条款扩为任何新 token 来源 + pin `:266-270` 词表形态 + 明文不做传递闭包检查；`ch_literal` 去重；AC-FS-15 (b) 补观察面不钉文件；自测脚本归宿写死（3c MUST 收进 `tests/`/`tools/`） | 待 critic 十五轮（仅核对 diff） |
| 2026-10-22 | critic 十五轮：**APPROVE**（判据层零 BLOCKER、零 SHOULD_FIX；仅 2 条 NIT：快照流程债 + ⓪ 内层编号同形）⇒ **可进入 §3.5.3 验收归档流程** | ✅ 定稿 rev.14 |
| ✅ | **v0.2 rev.14 定稿** ⇒ 归 `chmem-ptw-walk` 的 3g（≈3.2d；3c 需同时满足 **AC-FS-9 + AC-FS-15 + AC-FS-16**；合并交付时 AC-FS-14） | **done 2026-10-22** |
