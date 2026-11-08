# tasks —— CH_MEM 开放核的层次化装配框架地基（阶段 ①-A ~ ①-D）

> **决策权威**：ADR-094（🚧 Proposed **v0.2**）· **契约细则**：`parity-gate-contract.md` S-17 / S-17.1
> **实施设计**：`design.md`（★ 含 13 处实施面清单 · 5 条门禁设计 · 5 个已知缺口 G1~G5）
> **AC 权威**：`proposal.md` 的 `## Acceptance Criteria` 段（★ v0.2 起 AC 源 = proposal，★ 本文件是**派生执行清单**）

---

## 0. 前置（★ 动手前必须先闭合，否则本 change 不该开工）

- [ ] **0.1** ★ **缺口 G1 裁决**（门禁 ①④ 的判据文字在台账中缺失）⇒ architect 确认 `design.md` §4 的推断判据（★ 未经确认 ⇒ 相关 task 不得开工）
- [ ] **0.2** ★ **缺口 G2 裁决**（台账「10 处」实为 13+2 处）⇒ architect 确认 `design.md` §2.1 的分组口径（★ 影响 task 粒度）
- [ ] **0.3** ★ **缺口 G3 裁决**（★ 生产代码零 `declare_out` 调用 ⇒ ①-D owner 切换后测试批量变红）⇒ architect 定测试基线处置（改断言 vs 改端口名）
- [ ] **0.4** ★ **缺口 G4 裁决**（上游 `Component::build()` 同样在 `create_ports` 前置 `built_`）⇒ architect 判是否需上游回执
- [ ] **0.5** ★ **缺口 G5 登记**（r.14 无「创建时机维度」）⇒ 确认「长期项 · 不在本次收口」的处置（★ 触发条件 = 幽灵节点被实际观测到 ⇒ 立即升级 ADR 修订）
- [ ] **0.6** ★ **物理事实定稿**（`design.md` §3.1）⇒ ① root 持有类型 = `shared_ptr` ② root 构造复用 `ConstructionFrame` ③ `top_module_name`（`top`）与 root 实例名（`cpu`）分离
- [x] **0.7** ★ **`tools/check_ac_v_coverage.py` 扩展到 L4**（★ 支持「V 侧面标记」判定「同一 V 的同一侧面」）⇒ 0 孤儿 / 0 重复 / 0 遗漏
      ★ ✅ **2026-11-08 完成**：★ `CHANGES` += L4 两个 change · `V_UNIVERSE` += V-27~V-43（含 **V-27b**）
      · ★ **侧面感知**（`AC.aspect` + 按 `(V, 侧面)` 分组）· `CROSS_ALLOWED` 键升级为 `V|侧面`
      · ★ self-test **3/6 → 8/8**（含「不同侧面合法」/「同侧面违规」两例）· 单测 **24 → 37**
      ⇒ ★ 实测：★ **并集覆盖 28/28 · 无孤儿 V · 无未授权重复分配**
      ★ 附带：★ 归档后机检会崩（change 移到 `archive/*-<name>/`）⇒ ★ 加 `resolve_proposal_path()` 回退 + 3 单测
- [x] **0.8** ★ **`plan §K34 / §K35 / §K36` 落盘**（★ `.rddf/plans/backend-parity-open-core.md` 现 **0 命中** ⇒ V-39 / V-35 / V-36 的判据落点不存在）
      ★ ✅ **2026-11-08 补建占位**（★ 内容在实施期按批填）：★ **§K34** = ADR-094 的实施期推导与证据落点
      （★ 实施面清单已落 `design.md` §2）· ★ **§K35** = 阶段 1「同名普查」报告落点（★ 判据 = V-39 · 含**增量普查**规则）
      · ★ **§K36** = 迁移期基线结论落点（★ 判据 = V-35 / V-36 · ★ 捕获时机 MUST = ①-C 出口）

---

## 1. ①-A 框架地基（★ 出口 V-41a · V-42 · V-43 · V-34）

- [ ] **1.1** ★ `PipeBuilder` 持有**恰一个** root `Component` + 只读访问器 `root_component()`（r.1）⇒ 判据：root 与 `pb.ctx()` 同 ctx · root 名 == 契约常量 `cpu` · ★ **该 root 与层次化 emitter 的 `root` 参数是同一对象**（指针相等）★ **变异**：两者指向不同对象 ⇒ 归属判据失真 ⇒ RED（V: V-31 · V-34）
- [ ] **1.2** ★ root 构造 MUST 包在 `Component::ConstructionFrame` 内（r.1 · 上游先例 `CppHDL/include/device.h:24-27`）⇒ 判据：root 自身的节点 owner == root ★ **变异**：不包 Frame ⇒ root 的 `ch_reg` 归错层 ⇒ RED
- [ ] **1.3** ★ `root_component()` 返回 `ch::Component&`（★ 与上游 `toVerilogHierarchical(ctx, Component& root, ...)` 的引用形参对齐 ⇒ 可为同一对象）（r.1）
- [ ] **1.4** ★ 单元 MUST 经 `root.create_child<T>(name)` 创建，MUST NOT 裸 `make_unique`；返回值 MUST 检查非空（r.2）⇒ ★ **变异**：改回裸 `make_unique` ⇒ 组件树缺该单元 ⇒ 层次化发射 FATAL（V: V-31）
- [ ] **1.5** ★ 负向三变体：① 漏 `add_child` ⇒ FATAL ② ctor 传 `nullptr` + 事后 `add_child` ⇒ RED ③ 登记但不 `build` ⇒ FATAL（V: V-31）
- [ ] **1.6** ★ 新增层 2 基类 `include/cf/plugin/layer2_unit.h`：**编译期**校验 `io_type` 每成员为标量 `ch_in`/`ch_out`（r.11 · r.16）⇒ ★ **变异**：某成员改成 bundle / `ch_stream` ⇒ **编译不过** ★ **正向**：绕过基类直接继承 `ch::Component` ⇒ 门禁 grep MUST 报出（V: V-42）
- [ ] **1.7** ★ `build()` 幂等语义（r.10）：同一状态重复调用 ⇒ **成功早退**（不重跑 `negotiate/setup/build`、不产生重复登记）；状态已变 ⇒ **fail-closed**（`PluginError::BuildStateConflict`）；★ `built_` MUST 只在**成功收尾**时置（★ 现状 `pipe_builder.h:144` 在 `negotiate` 之前置 ⇒ fail-open）；★ **插件集指纹的字段与顺序 MUST 显式定义**（V: V-43）⇒ ★ **变异**：build 失败后再 `elaborate()` ⇒ MUST **不**执行 stage 闭包 ⇒ RED；build 成功后新增 plugin 再 build ⇒ MUST 失败 ⇒ RED
- [ ] **1.8** ★ `PluginError` 新增 4 码：`BuildStateConflict` / `ContractRegistryMismatch` / `NodeOwnershipEscape` / `ProviderConflict`（★ 现 `plugin_error.h:32-52` 无这 4 个）（r.10 · r.13 · r.14 · r.15）
- [ ] **1.9** ★ provider 槽位单例（r.15 前半）：同一 ctx 第二次安装 provider ⇒ MUST **立即失败**（V: V-41）⇒ ★ **变异**：装两个 provider ⇒ RED
- [ ] **1.10** ★ **r.6 的负向前置断言**：①-A 阶段 root-scope **MUST NOT 全局化**（顶层端口 owner 仍 `nullptr`）⇒ 顶层端口的声明位置判据（`scope_depth() == 0`）**延到 ①-D 生效**（V: V-31）⇒ ★ **变异**：①-A 就全局化 ⇒ 阶段 1 全量 `kMissing` ⇒ 门禁恒红 ⇒ MUST 被拦
- [ ] **1.11** ★ `RootScopeGuard`（RAII）：进入 `set_current(root)`、退出**恢复先前 `current_`**（r.1）⇒ 判据：嵌套 / 多 `PipeBuilder` 场景不串扰
- [ ] **1.12** ★ 2 TU 链接探针通过（框架头与新核头零 `<ch.hpp>` include）（V: V-34）
- [ ] **1.13** ★ **注释订正**：§2.1 的 #4 #5 #6 #7 #8 #9 #16（`pipe_builder.h` 的 `built_` 三处 + `ctx()` 注释 + `declare_in/out` 注释 + `regfile_chmem.h` 基类继承说明）
- [ ] **1.14** ★ **出口 gate**：V-41a / V-42 / V-43 / V-34 四条判据逐条有证据 + `--self-test` 变异自检全绿 + 双 binary 编译通过
      ★ **回滚 commit**：`（实施时填）` ⇒ 单 commit 可回

---

## 2. ①-A′ RegFile 前置适配（★ ①-D 的硬前置）

- [ ] **2.1** ★ `ip/cpu/core/regfile_bridge_chmem.h:124` 的 `std::make_unique<RegFileComponent>(nullptr, "regfile")` ⇒ 改 `root.create_child<RegFileComponent>(...)` + 以 `pb.ctx()` 调 `build()`（r.2 · r.3）⇒ 判据：owner == root 且出现在层次化产物里 ★ **变异**：改回裸 `make_unique` ⇒ 层次化发射 FATAL（V: V-40）
- [ ] **2.2** ★ **注释订正**：§2.1 的 #1 #2 #3（装配所有权叙述从「plugin 局部装配」改为「框架 root 持有」）
- [ ] **2.3** ★ **出口 gate**：`regfile_bridge` 经 `create_child` 创建 + owner == root + 层次化发射含该单元
      ★ **回滚 commit**：`（实施时填）`

---

## 3. ①-B 观测登记（★ 出口 V-37 · V-38 · V-41b）

- [ ] **3.1** ★ 登记簿 + S-17.1 逻辑契约名清单落地（D-3.2 · D-3.10）⇒ 字段集 = `{node_id, raw_name, has_driver, source}`（★ **先不带** `sanitized_name`）· **value-only**；★ **每侧各生成**（`node_id` 是 ctx 私有编号）
- [ ] **3.2** ★ manifest 产出者 = 框架、时机 = `elaborate()` 收尾（★ 与闸①同一时刻同一处 ⇒ 不可能漂移）（D-3.10）★ **MUST NOT** 让 runner 反扫 ctx 自建 manifest
- [ ] **3.3** ★ 闸①（框架内）：扫出的节点集合 == **调用方注入**的期望键集（★ 框架 MUST NOT 内置 parity 概念）（r.13）⇒ 失败 ⇒ typed `PluginError::ContractRegistryMismatch` ⇒ gate `error_kind = contract_registry_mismatch`（★ **不得在 gate 侧另立字面量** ⇒ 用 S-10 表已登记的值）
- [ ] **3.4** ★ 闸①在 manifest 缺失时 ⇒ `ERROR(2)` + `manifest_missing`（★ 防「闸②退化为『有 manifest 才查』」）（V: V-37）⇒ ★ **变异**：摘掉闸① ⇒ 静默通过 ⇒ RED
- [ ] **3.5** ★ provider 安装期自检（r.15 后半）：装 `manifest` provider ⇒ 校验每个键的 node 存在且 raw 名一致；装 `identity` provider ⇒ 校验精确匹配到**唯一**节点；不符 ⇒ 立即失败（`provider_conflict`，★ 消息点名**逻辑名 + 两个候选节点名**）（V: V-41）
- [ ] **3.6** ★ 绑定提供者一致：同一 case 中两侧解析出的**逻辑契约名集合**一致；★ 报告 MUST 标注每个观测的 provider（`manifest` / `identity`）（V: V-38）⇒ ★ **变异**：删 `identity` provider ⇒ 预载核侧 `kMissing` ⇒ RED
- [ ] **3.7** ★ **注释订正**：§2.1 的 #10 #11（runner 精确名解析 ⇒ 逻辑契约名解耦；报错文案不再把逻辑名当节点名）★ ※ 该改动**在 ①-D 生效**（⇒ 与 3.x 同批切换解析路径）
- [ ] **3.8** ★ **出口 gate**：V-37 / V-38 / V-41b 三条判据逐条有证据 + 变异自检全绿
      ★ **回滚 commit**：`（实施时填）`

---

## 4. ①-C 阶段 1（★ 出口 V-39 + V-35/V-36 基线捕获）

- [ ] **4.1** ★ **消费** `resolve_node_id` 的三态化（`kOk` / `kMissing` / `kAmbiguous`）⇒ 阶段 1 期间 `kAmbiguous` ⇒ 门禁报 `observation_ambiguous:<key>`（`ERROR(2)`）
      ★ ★ **2026-11-08 边界划定**：★ **实现**归姊妹 change `parity-observation-ambiguity-fail-closed`（★ = ADR-091 **V-25** 的落地
      ⇒ ★ 它改**同一个文件** `tools/parity_runner/parity_sim_runner.cpp` ⇒ ★ 本任务 **MUST NOT** 重复实现）
      ⇒ ★ **前置** = 姊妹 change 已实施并归档（★ 依赖已解除：其 `depends_on: backend-parity-observability` 已归档）
      ⇒ ★ 消费侧判据：★ 歧义 case ⇒ 门禁 `ERROR(2)` + `error_kind=observation_ambiguous:<key>`（V: V-39）
- [ ] **4.2** ★ **前置同名普查**（★ **一次性 · 只读**）：范围 = 现网 52 case + CH_MEM binary 全量 case 的 ctx；同名节点组数 MUST = 0（或已逐组定位并修复）⇒ 报告落 `plan §K35` ⇒ ★ **变异**：注入一个同名节点 ⇒ 普查 MUST 报出该组 ⇒ RED ⇒ ★ **不通过 ⇒ 先修存量，不切阶段 1**
- [ ] **4.3** ★ **普查窗口钉住**：记录 commit + CH_MEM binary SHA；★ **新增 case ⇒ 增量普查**（V: V-39）
- [ ] **4.4** ★ **V-35 基线捕获**（★ **时机 MUST = 本批出口 / ①-D 开工前** —— ADR-094:157「切换后**无法事后补**」）：逐 case 的逐拍全观测量 hash 链（FNV，与既有 `pchash` **同构造** ⇒ 零新范式）+ 终值逐位数组 + 环境标识（commit / cpu 配置 / 语料集）⇒ 机器产物为**临时**（构建目录 / CI artifact）★ 结论落 `plan §K36` ⇒ ★ **变异**：捕获后人为注入一处观测差异 ⇒ hash 链 MUST 不同 ⇒ RED（V: V-35）
- [ ] **4.5** ★ **V-36 方向守卫基线捕获**：抓运行日志（★ 上游 `CHREQUIRE` **只打日志、不抛** ⇒ `log_level::error` + 前缀 `Requirement failed [<cond>]:`）⇒ 按 **4 个 overload 前缀**（`out <<= in` / `in <<= in` / `out <<= out` / `in <<= out`）作**稳定匹配键**归类（★ 不匹配全文 ⇒ 上游改文案尾部不影响）⇒ ★ **抓取器自检**：一轮捕获**零匹配** ⇒ MUST **ERROR**（★ 否则「抓取器失效」表现为「零误报」= 恒绿假绿）（V: V-36）
- [ ] **4.6** ★ **出口 gate**：V-39 有证据（普查报告）+ V-35/V-36 基线已落 `plan §K35`/`§K36` + 抓取器自检非零匹配
      ★ **回滚 commit**：`（实施时填）` ⇒ ★ 基线是**临时产物**，revert 后需重捕获

---

## 5. ①-D 阶段 2（★ **不可拆** · 出口 V-40 + V-35/V-36 比对与退役）

- [ ] **5.1** ★ **同批不可拆**（★ 一个 commit 序列，★ 验收失败 ⇒ **整批 revert**）：owner 切换（root-scope 覆盖**整个构建期**：`build()` + `elaborate()`）+ 登记上线 + manifest 上线 + **r.14 归属守卫生效**（r.14 · D-6）（V: V-40）
- [ ] **5.2** ★ **r.14 归属守卫**：构建收尾时 ctx 中**除 clock/reset 外**每个节点 owner ∈ `{root}` ∪ `{已登记单元}`；排除项 = ① ctx 级 clock/reset ② `node_ownership_suppressed()` 生效期间建的节点；★ 失败 ⇒ fail-closed（`PluginError::NodeOwnershipEscape` ⇒ gate `error_kind = node_ownership_escape`）⇒ ★ **变异**：把某节点挂到 root 之外 ⇒ MUST RED（★ **反向**：把 `register_observation()` 用的顶层端口移进组件作用域内创建 ⇒ MUST RED）（V: V-40）
- [ ] **5.3** ★ **同批改动 ①**：`tools/parity_runner/parity_sim_runner.cpp` 的 `resolve_node_id` 精确名解析 ⇒ 改 **manifest 驱动**（★ 顶层端口名从 `reg_1` 变 `cpu_reg_1` ⇒ 精确名**全量失效**）★ **理由**：不改则 runner 读不到任何观测（V: V-40）
- [ ] **5.4** ★ **同批改动 ②**：`tools/verilator_runner/sim_main.cpp` 的 `top__DOT___reg_N`（★ 31 处 **编译期硬编码**）+ `top__DOT__dmem` / `_pc` / `flush_count` / `bp_*` ⇒ 改 manifest 驱动或改名；`cpu_verilator_sim.cpp:320` 的 `--top-module top` ⇒ 同步（★ **模块名 `top` 不变** ⇒ 只改成员访问路径）（V: V-40）
- [ ] **5.5** ★ **同批改动 ③**：`ip/cpu/core/regfile_bridge_chmem.h` 的裸 `make_unique` ⇒ `root.create_child`（★ 依赖 ①-A′ 已完成 ⇒ **前置不可省**）（V: V-40）
- [ ] **5.6** ★ **注释订正**：§2.1 的 #12 #13 #14 #15（`sim_main.cpp` 注释前提 + `cpu_verilator_sim.cpp` 硬编码 + `ibus_port_chmem.h` 的「顶层端口名 = 契约裸名」前提）
- [ ] **5.7** ★ **V-35 比对**：切换前后 hash 链**逐 case 相等**（硬）+ 终值逐位相等（第二判据）⇒ ★ 不等 ⇒ 归因到具体 case 与观测量（V: V-35）
- [ ] **5.8** ★ **V-36 比对**：阶段 2 触发集 ⊆ 阶段 1 触发集（**无新增误报**）且阶段 2 触发集为空（★ 「减少」是**预期收益**⇒ 只展示不参与判定）⇒ 若出现合法触发 ⇒ 走 S-17 REG-4 allowlist（**architect 审**）⇒ ★ **不是放宽判据**（V: V-36）
- [ ] **5.9** ★ **迁移期判据退役**：V-35 / V-36 在阶段 2 验收后 **MUST 整体退役**（★ 不得留成僵尸判据 ⇒ 机器产物不进长期报告 schema ⇒ 不留只服务迁移期的永久字段）（V: V-35 · V-36）
- [ ] **5.10** ★ **测试基线处置**（★ 依赖 0.3 的裁决）：owner 切换后端口名变化 ⇒ 批量测试断言的处置按 0.3 结论执行
- [ ] **5.11** ★ **出口 gate**：V-40 有证据（归属守卫生效 + 变异 RED）+ V-35/V-36 比对通过且已退役 + 既有回归零退化
      ★ **回滚 commit**：`（实施时填）` ⇒ ★ **整批 revert**（★ MUST NOT 只回退其中一项）

---

## 6. 范围外（★ 明确不做）

- [x] ✗ 不把层次化产物接进 parity 门禁（★ D-4：门禁仍消费**扁平** `top.v`；回滚路径 = 整组丢弃层次化产物）
- [x] ✗ 不引入 `ch_stream` 作层 2 装配域的连接机制（★ D-5）
- [x] ✗ 不做多 root / 多核形态（★ 跨 ctx 多 root 违 G-1p ⇒ 另立 ADR）
- [x] ✗ 不做 FSM 私有 ctx 的归属裁决（★ ADR-094 残留 ④，形态未定）
- [x] ✗ 不做「首批成层单元」的**落地**（⇒ 姊妹 change `chmem-hierarchical-unit-landing`）
- [x] ✗ 不改 `parity-gate-contract.md` 的 S-1~S-16 判定语义（★ 只增 S-17 的登记细则）
