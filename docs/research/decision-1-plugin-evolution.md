# Reference 1：决策 1 深度展开 — VexiiRiscv 5 病灶 × ChipForge 机制级回应

> **主控文档**：[`docs/architecture/roadmap-evolution.md`](../architecture/roadmap-evolution.md) §2（决策 1）
> **关联**：执行路径图决策 1 的全部论证细节；引用本文档的结论做 v0.10.0 / v1.0.0 / v1.2.0 / v1.3.0 实施依据

---

## 0. 关键纠正

VexiiRiscv **并没有抛弃 Plugin 范式**。其 Framework 文档明确写着 Plugin/Fiber/Retainer 仍是核心抽象（BtbPlugin/GSharePlugin/ExecuteLanePlugin 等全部仍是 Plugin）。Dolu1990 抛弃的是 **VexRiscv 2017 年代的 Plugin 实现机制**（隐式共享信号、无协商、无线道抽象），然后用**同一范式 + 新机制**（Database 依赖注入、Fiber 编译期协商、Pipeline API 站级显式化）重建了它。ChipForge 决策 1 复刻该动作：**范式不动，机制升级**。

---

## 1. 理由 #1：Frontend / Branch Prediction 是隐式事件网

**VexRiscv 病灶机制本质**：branch prediction 的 Predict（F 级猜）/ Verify（EX 级对答案）/ Learn（commit 级回写预测器）三个动作全部揉在 fetch 流水线里，预测器状态更新与流水线 flush 信号耦合在同一个隐式事件网中，加一个 GShare 就要改 5 个文件。

### (a) ChipForge 当前暴露度：**中**

- 当前仅 static 预测（BTFN）
- `P2#6 mispredict 恢复通路` 已占位 — 问题已识别但未机制化
- 7stage superscalar 的 `lane_counters` use-after-free（commit `b82af0f`）本质上就是"跨 lane 隐式共享状态"事故，与 VexRiscv frontend messy 同根
- 尚未爆发的唯一原因：预测器复杂度目前 ≈ 0。一旦加 BTB，同样的 messy 会准时出现

### (b) 不抛弃 Plugin 的演进机制

1. 新建 **PredictPort / LearnPort** 两个显式端口抽象（落 ADR-075）
2. flush 统一走 CtrlLink 广播（复用 ADR-045），预测器 Plugin 只是众多消费者之一
3. CI 新增**第 11 条门禁**：fetch 族 Plugin 文件内禁止直接 `#include` 预测器头文件，强制经 Port 通信
4. 分级开关 `bp_mode = none|static|btb|btb+gshare+ras` 做成 JSON 配置 + elaboration 期 `Result<>` 校验

### (c) 性能超越映射（2026-09-29 扩展：v1.4+ dual 全家桶追平线显式化）

| 指标 | static BTFN（当前） | BTB-only | BTB+GShare+RAS | **v1.4+ dual 全家桶（HARD 候选，2026-09-29 新增）** | 对标 |
|------|---------------------|----------|----------------|---------------------------------------------------|------|
| mispredict 率（CoreMark 估） | ~25–30% | ~12% | ~7% | **~3–5%（dual + HW prefetch）** | VexiiRiscv 同配置 ~7–9%（single-issue）；dual 全家桶 ~3–5% |
| CoreMark/MHz 目标 | ~1.5 **[估]** | ≥1.9 | ≥2.3 | **≥4.5 [估]**（dual + HW prefetch + WB + SB 全家桶）| **双追平线**：<br>• **v1.0.0 single-issue 追平线 = VexiiRiscv single-issue 官方 2.4–2.6**（目标 2.3 即为追平线）<br>• **v1.4+ dual 全家桶追平线 = VexiiRiscv dual+prefetch 官方 5.24**（目标 ≥4.5 即为 85% 追平线，偏差 ≤15%）|
| DMIPS/MHz 目标 | ~1.1 **[估]** | ≥1.4 | ≥1.7 | **≥2.4 [目]**（dual 全家桶）| **v1.4+ 追平 VexiiRiscv 官方 2.50（dual-issue）= 96% 追平线** |

### (d) 不可行回退条件（客观信号）

- **信号 1**：实现 GShare 时，fetch 族 4 个 Plugin 之间不经 Port 的直达引用 **>6 条**（CI grep 计数）且连续 2 个 PR 压不回 ≤2 条
- **信号 2**：BTB 落地后 `bp_mode=btb` 相对 `bp_mode=static` 在 CoreMark 上提升 **<10%**（正常应 ≥20%）

任一信号触发 → 停堆 GShare，先做 Learn 通路 RCA，禁止继续。

---

## 2. 理由 #2：Pipeline 必须重写以支持 Multi-Issue / Late-ALU

**VexRiscv 病灶机制本质**：流水线是"一根线"，issue 宽度 1 被硬编码在每条 stage 的信号命名与 hazard 检查里；late-alu（第二 ALU 在更后级完成）要求 forwarding 网络按"结果可用时刻"重构。

### (a) ChipForge 当前暴露度：**低** — 唯一已交学费

- 7stage superscalar 已经实测（`[cpu-integration]` 81/81 PASS，`build_7stage_superscalar` 在册）
- `lane_counters` use-after-free（commit `b82af0f`）证明：**问题出在共享可变状态，不是 Plugin 范式本身** —— 修法是给每 lane 独立计数器，范式一行没动

### (b) 不抛弃 Plugin 的演进机制

1. **Lane 显式化**：新增 `LaneId` 作为 Payload 一级字段，PayloadStore 升级为 per-lane keying（`store.get(LaneId, key)`）
2. **多 lane 仲裁器 = 一个新 Plugin**：`LaneArbiterPlugin` 消费各 lane writeback 请求，按端口数仲裁（**仲裁是组合逻辑，不需 FSM 豁免**）
3. **Late-ALU = 站级重构而非重写**：PipeBuilder 已支持 `at_stage(N)`，late-alu 是把 IntAlu 第二实例注册到 EX2，forwarding 表 elaboration 期 `Result<>` 校验完整性
4. CI 门禁：7stage dual-issue 配置进 `chipforge_tests` 矩阵常驻

### (c) 性能超越映射

| 指标 | single-issue 5/7stage | dual-issue 目标 | 对标 |
|------|----------------------|-----------------|------|
| IPC（CoreMark） | 0.85–0.95 | **≥1.55** | VexiiRiscv dual-issue 实测 IPC ~1.6–1.8；XiangShan 是 OoO 4–6 发射，不同量级不硬比 |
| FMAX（Artix-7） | 100 MHz 基线 | **≥90 MHz（允许 -10%）** | VexiiRiscv dual-issue 在 FPGA 上同样付 10–15% FMAX 税 |
| LUT（Artix-7） | ~5k **[估]** | ≤9k（<1.8x） | VexiiRiscv single→dual 面积约 1.7–2x |
| DMIPS/MHz | ≥1.7 | **≥2.4** | 追平 VexiiRiscv 官方 2.50 的 96% |

### (d) 不可行回退条件

- 信号 1：dual-issue 原型中需要绕过 `at_stage` 回调的裸指针 **>8 处**（code review 计数）
- 信号 2：dual-issue IPC 实测 **<1.35**（即收益 <40%）且 RCA 归因于"仲裁开销吃掉发射收益"

任一触发 → 砍 dual-issue 分叉，v1.4 转 single+late-alu 保 FMAX 路线。

---

## 3. 理由 #3：Plugin 系统本身到极限（+ 第 6 条 tech debt）

### 三个子病灶

1. **服务查找靠类型猜**：`pluginOf[T]` 式隐式查找，依赖顺序由实例化次序隐式决定
2. **编译期无协商**：Plugin A 需要"流水线至少 3 级"这种约束没有表达通道，只能靠运行时崩溃
3. **错误处理无范式**：配置错误 = 抛异常/段错误，调用方无法区分"我的配置错了"还是"框架 bug"

### (a) ChipForge 当前暴露度：**中高** — 防线投入最多

| 子病灶 | ChipForge 已建防线 | 证据 |
|--------|--------------------|------|
| 隐式顺序 | **ADR-048**（v0.7.0）：Plugin 注册规范序 + `check_canonical_ordering()` 运行时断言 | `test_canonical_ordering.cpp` 4 测试 PASS |
| 编译期协商载体 | **ADR-037 v2.0**（adr.md:1235，Phase 6c M5 落地）：elaboration 语义兑现，`pb.elaborate(ctx)` 一次性发射 DAG | `[elaborate]` 4 API PoC |
| 错误处理范式 | **ADR-047**（v0.6.0）：静态配置期 `std::expected<T, PluginError>` Result 范式 | `verify_plugin_decision.sh` Check 5 |
| 静默 cell 读 | PayloadStore fail-fast（v0.3.1 M6）：`get()` 缺失抛异常 | `check_plugin_portability.sh` Check 8 |

**唯一未解决的真实缺口**：Plugin 之间没有 **capability 协商 API**（VexiiRiscv 的答案是 Fiber/Retainer）—— 这是当前唯一明确落后于 VexiiRiscv 的框架机制。

### (b) 不抛弃 Plugin 的演进机制

新增 `Plugin::negotiate(CapabilityTable&)` 生命周期钩子（落新 ADR-082 或并入 ADR-037 v3.0）：

- 在 `build()` 之前跑一轮 —— 每个 Plugin 声明 `provides = {…}` 和 `requires = {…}`（例：BranchPlugin requires "flush_broadcaster"、MMUPlugin provides "paddr_consumer"）
- 框架拓扑排序后依次回调
- 缺依赖 → `Result<>` fail-fast（复用 ADR-047）
- 顺序错误 → 复用 ADR-048 断言

**做完这步，ChipForge Plugin 机制能力 ≡ VexiiRiscv Fiber/Retainer 能力**。

CI 门禁：`verify_plugin_decision.sh` 新增**第 8 项**——所有 `build()` 内跨 Plugin `dynamic_cast` 计数必须 = 0（强制走 negotiate 拿 handle，禁类型猜）。

### (c) 性能超越映射（演进速度代理指标）

| 指标 | 当前 | negotiate 落地后 | 对标 |
|------|------|------------------|------|
| 新增一个预定义配置（如"5stage+BTB+2-way"）所需修改文件数 | ~6 个 | ≤2 个（JSON + Factory 一行） | VexiiRiscv 用 Scala macro 做到 1 个 |
| elaboration 期捕获配置错误占比 | ~50% | ≥90% | — |

### (d) 不可行回退条件

- 信号 1：negotiate 钩子落地后仍有 Plugin 在 `build()` 内做跨 Plugin `dynamic_cast` **>0 处**且 CI 豁免申请连续 3 次被批准 → 重做本 API 单点
- 信号 2：CI 豁免标记总数 **>20 处** → 承认 D4 约束集与实际需求系统性错配，启动 v2.0 约束模型重估

---

## 4. 理由 #4：Write-Through D$ 锁死高频上限

**VexRiscv 病灶机制本质**：write-through 把每次 store 都变成 DRAM 事务，频率越高 store 带宽需求越大，DRAM 延迟不变 → store stall 占比随 FMAX 线性恶化。**这是架构上限锁死，不是缓存策略选择错误**。

### (a) ChipForge 当前暴露度：**高** — 与 VexRiscv 病情最相似

- L1 DCache 256×1 direct-mapped，write-through（现状）
- `Phase 6d.7 L1Cache refill FSM` 已把 refill 做成 `ch_state_machine`（ADR-046 豁免的第二个消费方）
- ADR-044（L1 Cache↔MMU VIPT 锁定）设计方向已锁

**好消息**：write-back 所需的全部机制件都已备齐 —— FSM 豁免（ADR-046）、双缓冲 commit swap（ADR-040 v2.0 `array_store`）、PADDR 契约（ADR-049）。这是**"机制已就位、只差实装"的演进**，不是探索。

### (b) 不抛弃 Plugin 的演进机制

1. **write-back + dirty 位 FSM**（落新 ADR-084）：refill FSM 从 4 态扩到 6 态（`IDLE/LOOKUP/REFILL/WRITEBACK/DONE/FLUSH`），`ch_state_machine` DSL 实装
2. **Store buffer**（2–4 项，write-back 必备搭档）：作为 L1Cache 内部 `array_store` 实现，**不作为独立 Plugin** —— 避免 D4 交互面扩大
3. **Non-blocking 是 v1.4 候选**，不进 v1.x hard gate（VexiiRiscv 用 3 年迭代，ChipForge v1.x 拿到 80% 收益即可）
4. CI 门禁：`array_store` 双缓冲 commit 顺序断言（ADR-040 v2.0 已有）+ write-back FSM 每态的 TLM≡CH_MEM 对拍（ADR-080 协议的第一个真实负载）

### (c) 性能超越映射

| 指标 | write-through（当前） | write-back + store buffer | 对标 |
|------|-----------------------|--------------------------|------|
| store 密集负载 DRAM 事务数 | 1.0x 基线 | **0.15–0.3x** | VexiiRiscv 同机制收益 3–5x |
| FMAX 可扩展上限 | ~100 MHz | **≥150 MHz 解除 DRAM 侧限制** | — |
| CoreMark/MHz（100+ MHz 档） | 随频率衰减 | 保持 ≥95% 低频分数 | — |

### (d) 不可行回退条件

- 信号 1：write-back FSM 的 TLM≡CH_MEM 对拍在**连续 2 个 sprint** 无法收敛到 100% match
- 信号 2：write-back 落地后 CoreMark 实测提升 **<5%**

任一触发 → 降级为 write-through + write buffer（保 FMAX ~120MHz），write-back 推迟 v1.4。

---

## 5. 理由 #5：Verification Golden Model 与 RTL 两套代码

**VexRiscv 病灶机制本质**：自研 golden model 与 RTL 是两套独立代码，golden model 本身无人验证（"谁来验证验证者"）。

### (a) ChipForge 当前暴露度：**低** — 这条是反向领先

三层防线已在：

1. **双模对拍**（TLM 功能 golden + CH_MEM RTL 语义）—— **同一源文件**生成，golden model 与 RTL 不存在两套代码，从根消灭"golden model 自身漂移"问题
2. **riscv-tests 40/40**（外部权威 ISA 测试）+ MMU Sv32 真 PTW 47 测试 110 断言
3. **CI 9 架构门禁**硬阻塞 —— 验证不仅查功能，还查架构纪律本身

缺口：无指令级 lockstep（Spike 对拍）、无 trace 可视化（Konata 类）、无随机指令流 fuzzing。

### (b) 不抛弃 Plugin 的演进机制

1. **ADR-080 双模对拍协议**进 CI：每次 commit 跑固定种子 trace，TLM 输出 ≡ CH_MEM 输出（事务级粒度），match 率必须 100%，否则 PR 阻塞
2. **Spike lockstep**（v1.2.0，PoC-9）：commit 级 trace 对齐 Spike，逐指令比对 PC/rd/CSR
3. **Konata 替代**：不自研可视化，输出 Konata 兼容 JSON trace 格式，直接复用 VexiiRiscv 生态工具（零成本借力）

### (c) 性能超越映射（验证速度代理指标）

| 指标 | 当前 | 目标 | 对标 |
|------|------|------|------|
| bug 在 TLM 层发现占比 | 待测 | ≥70% | VexiiRiscv 无 TLM 快模，等价比例近 0% |
| 单个 bug 调试周期 | Verilator 编译 5min + sim 10min | TLM 5 秒 | — |
| lockstep 容量（v1.2.0） | — | 1M 条随机指令零分歧 | VexiiRiscv RVLS+Spike |

### (d) 不可行回退条件

- 信号 1：ADR-080 对拍上线后连续 4 周失败率 >5% 且多数归因于"双模语义本身无法对齐"
- 信号 2：lockstep 接入后 Spike 分歧率 >0.1% 且 RCA 显示分歧源于 TLM golden 与 Spike golden 双 golden 冲突 → 以 Spike 为唯一 golden，TLM 降级为性能模型

---

## 6. 三家独有可超越点（决策 1 的关键产出）

| 对象 | 它没解决的机制问题 | ChipForge 双模为什么能解 |
|------|---------------------|--------------------------|
| **VexiiRiscv** | DSE 改配置必须 SpinalHDL elaboration（分钟级 JVM）+ Verilator 编译（5–15min）+ 仿真 → 50 配置 = 数天 | TLM 毫秒级，Top-N 才进 CH_MEM → **50 配置 = 1 小时** |
| **XiangShan** | difftest 与 RTL 两套代码，微架构改动验证收敛周期以周计 | 双模同源对拍把"golden vs RTL"漂移归零；bug 秒级定位 |
| **VexRiscv** | write-through 锁死高频 / golden 不可信 / frontend 不可演进（结构性冻结） | ChipForge 对 3 条各有在册机制（理由 #4/#5/#1），每条都有已落地 ADR 做地基 |

> **一句话**：VexiiRiscv 用重写解决了 VexRiscv 的 5 条病，但继承了"单执行语义"这个 SpinalHDL 结构性约束；ChipForge 用机制演进解决同样的病，同时保有"双执行语义"这个 CppTLM/CppHDL 结构性资产。

---

## 附录：决策 1 综合回退触发条件（汇总）

任一信号触发 → 启动 v2.0 框架评估（**不早于 2029 年**）：

1. 实现 GShare 时，fetch 族 4 个 Plugin 之间不经 Port 的直达引用 **>6 条**且连续 2 个 PR 压不回 ≤2 条
2. dual-issue 原型需要绕过 `at_stage` 回调的裸指针 **>8 处**
3. CI 豁免标记（`CF_PLUGIN_USE_FSM_EXEMPT` + waiver 注释）总数 **>20 处**
4. ADR-082 negotiate() 落地后仍有 Plugin 在 `build()` 内做跨 Plugin `dynamic_cast` **>0 处**且豁免连续 3 次被批准