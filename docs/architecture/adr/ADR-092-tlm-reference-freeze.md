# ADR-092 — TLM 参考实现的冻结与演进停止（Phase-1）

> **Status**: ✅ **Accepted（v1.1，2026-10-28，architect 主持；v1.0 = D0 决策链 D0-①~⑨ + R-1/R-2/R-3 + D-H-1/D-H-2/D-I-1 全部裁决；★ **v1.1 = 文档文本同步完成登记**（22 文件，见 §D7））**
> **上游**: ADR-088 v1.4 §10（三段迁移路径；本 ADR 是其**阶段 C 的 Phase-1 准备**，★ **不**替代其 5 条硬前置）· ADR-083（Plugin-style 单一 source of truth；★ **v1.2 表述纠偏**见「相关」）· ADR-040 v3.0 · ADR-091 v1.0（CH_MEM 开放核形态）
> **下游**: `tools/check_tlm_freeze.sh`（冻结门禁升级）· `ip/**`（32 个冻结文件）· `docs/architecture/tlm-frozen-reference.md`（参考基准清单，★ 新建）· `openspec/specs/tlm-coverage-parity-ledger/spec.md`（★ **D-I 的落点 = ADR-088 阶段 B 台账**）
> **编号说明**: `ADR-090` 已被 `openspec/changes/vexii-riscv-parity-poc` 预留 ⇒ 本 ADR 取 **ADR-092**。
> **执行文件（待 planner 落）**: `.rddf/plans/`（★ 归 builder 起草）

---

## Context

### 1. 决策背景：移除 TLM 依赖，但**先不删代码**

用户裁决（2026-10-28）：**移除 CppTLM 依赖** + **移除 `chstream_module` / `chstream_adapter`**；但 ★ **TLM 代码先不删除**，改为**清理出来后标注「演进停止」**，**保留作为 CH_MEM 版与 `ip/cpu/core` 开放核的设计参考**。

⇒ **本 ADR 的决策对象 = 「冻结」这一个决策**（停止演进 + 保留 + 机检）。
⇒ **「删除 / 框架层单模化」是另一个独立决策** ⇒ 归 **ADR-093**（本 ADR 仅登记其准入门禁，见 D6）。

### 2. 为什么「冻结」比「删」更优（实测依据）

| 维度 | 直接删除 | ★ **冻结保留** | 取证 |
|---|---|---|---|
| **测试覆盖** | ❌ TLM binary **1017 case** vs CH_MEM binary **331 case** ⇒ **686 case 消失** | ✅ **保留** | 实测 `--list-tests` 计数 |
| **冻结门禁有效性** | ❌ Check 7「TLM-only 文件不应含 ch 类型实例化」⇒ **文件集为空 ⇒ 真空 PASS**（ADR-088 点名的「最坏假绿形态」） | ✅ **文件集非空 ⇒ 检查仍有效** | `tools/check_tlm_freeze.sh` 检测面逻辑 |
| **TLM 参考语义** | ❌ 丢失 | ✅ **保留为可执行基准**（优于散文规格） | ADR-088 硬前置③ |
| **框架层单模化** | ✅ 立即可做 | ⚠️ **推迟**（`uint_t` 别名化后 TLM 代码不可编译 ⇒ 见 §4） | `include/cf/plugin/uint_t.h` 模态分支 |
| **文档纠偏** | ✅ 现在做 | ✅ **现在可做**（只标注不删） | — |

⇒ ★★ **冻结把三个风险（686 case 消失 / 真空 PASS / 参考语义丢失）全部推迟**，让**开放核主线（ADR-091）先行**，再回头做删除。

### 3. 现状：冻结机制**已存在**（可直接升级，不必新造）

`tools/check_tlm_freeze.sh`（ADR-088 §2 冻结契约）现状：

| 项 | 现状 |
|---|---|
| 检测面 | `ip/` 下所有非 `_chmem.h` 的 `.h`（**41 候选**）− 9 个双模共享文件 |
| 判定 | 相对 `.rddf/state/tlm-freeze-base.txt` 的 commit，**函数定义集合差集 = 新增** ⇒ 违规 exit 1 |
| CI 级 | warn（`continue-on-error`）；脚本级非 0 |
| 豁免 | `.rddf/state/tlm-freeze-allowlist.txt`（**函数级**） |

⇒ ★ 现状是「**禁新增函数**」；本 ADR 要升级为「**禁任何修改 + 必须带冻结标记**」（D1）。

### 4. ★ 框架层单模化与「TLM 保留在构建里」**不可并存**（⇒ Phase-1 不可单模化）

**技术链条**：

1. 单模化 ⇒ `include/cf/plugin/uint_t.h` 只留 CH_MEM 分支 ⇒ `cf::plugin::uint_t<N>` = `ch::core::ch_uint<N>` 的别名
2. TLM 代码把 `uint_t` 当**纯 POD 算术**用（`address{0}`、`address + 4`）
3. `ch_uint` 在 TLM 编译下**不是 POD**：构造需 context、运算是 DAG 操作

⇒ ★ **TLM 代码会编译失败或语义错误** ⇒ 二选一：

| | TLM 代码 | 框架层 | 686 case |
|---|---|---|---|
| **(a) ★ 采纳** | 保留 **+ 仍在构建** | **保持双模** | ✅ 保留 |
| (b) | 保留源码 + 排除构建 | 立即单模化 | ❌ 消失 |

⇒ **D-H-1 裁决 = (a)** ⇒ **框架层单模化推迟到 ADR-093（Phase-2）**。

### 5. ★ 现状检测面存在**误判**（本轮实测发现）

```
ip/cpu/configs/cpu_config_chmem_json.h    ← P2 刚落地的 CH_MEM 配置解析器，被误纳入冻结集
```

根因：检测面规则是「`ip/` 下所有非 `_chmem.h` 的 `.h`」⇒ 而该文件叫 `_chmem_json.h`（**不匹配** `_chmem.h`）⇒ 被误判为 TLM-only。

⇒ ★ **D1 必须同时修正检测面**（见 D1.3）。

---

## Decision

### D1 ★ TLM 参考实现的冻结契约（Phase-1）

#### D1.1 冻结集 = **32 个文件**（实测：41 候选 − 9 双模共享）

| 目录 | 数量 | 文件 |
|---|---|---|
| `ip/cpu/plugins/` | 8 | `branch_predictor.h`(319) · `dbus.h`(150) · `hazard.h`(249) · `ibus.h`(139) · `mmu.h`(108) · `mmu_exception_handler.h`(69) · `reg_file.h`(199) · `stage_link.h`(119) |
| `ip/mmu/lib/` | 6 | `tlb.h`(250) · `tlb_base.h`(85) · `tlb_entry.h`(71) · `tlb_lookup.h`(79) · `tlb_factory.h`(39) · `multi_level_tlb.h`(52) |
| `ip/mmu/policies/` | 5 | `lru_policy.h`(65) · `no_replacement_policy.h`(32) · `rrip_policy.h`(59) · `tlb_replacement_policy.h`(44) · `fifo_policy.h`(47) |
| `ip/cpu/arch/riscv/` | 5 | `branch.h`(139) · `csr.h`(43) · `decode.h`(154) · `int_alu.h`(135) · `lsu.h`(82) |
| `ip/cache/policies/` | 3 | `replacement_policy.h`(98) · `lru_policy.h`(124) · `no_replacement_policy.h`(73) |
| `ip/cpu/` | 2 | `cpu_factory.h`(582) · `picolibc_host_memory.h`(184) |
| `ip/mmu/tlm/` | 1 | `MMUPlugin.h`(151) |
| `ip/cache/plugins/` | 1 | `L1CachePlugin.h`(227) |
| `ip/cpu/configs/` | 1 | ⚠️ **`cpu_config_chmem_json.h`(359) —— 误判，见 D1.3** |

⇒ ★ **9 个双模共享文件**（`ip/cpu/tlm/cpu_keys.h` · `ip/mmu/tlm/mmu_keys.h` · ★ **`ip/mmu/lib/ptw.h`** · ★ **`ip/mmu/lib/memory_interface.h`** · `ip/cpu/arch/riscv/decoder_table.h` · `ip/cpu/arch/riscv/payload_riscv.h` · `ip/cpu/core/payload_common.h` · `ip/cpu/arch/riscv/mul.h` · `ip/cpu/arch/riscv/mul_div_fsm.h`）**不冻结**（被 CH_MEM 侧 include ⇒ 双模共享）。

⇒ ★ **更正一处常见误解**：`ip/mmu/lib/` 的 8 个文件里，**`ptw.h` + `memory_interface.h` 是双模共享**（被 `MMUPlugin_chmem.h` include）⇒ 真正 TLM-only 的 lib 只有 **6 个**。

#### D1.2 冻结强度（★ D-H-2 裁决）

| 项 | 从（现状） | ★ 到 |
|---|---|---|
| **判据** | 函数定义集合差集（禁**新增函数**） | ★ **文件内容 diff**（禁**任何修改**：改函数体 / 改签名 / 改常量 / 改注释中的语义） |
| **标记** | 无 | ★ 每个冻结文件头**必须**带冻结标记（缺 = 违规） |
| **豁免** | 函数级白名单 | ★ **文件级白名单**（须显式登记 + **architect 签**） |

#### D1.3 ★ 检测面修正（★ 本轮实测发现的误判）

| 项 | 处置 |
|---|---|
| **`ip/cpu/configs/cpu_config_chmem_json.h`** | ★ **移出冻结集** ⇒ 新增 `.rddf/state/tlm-frozen-exclude.txt`（★ **CH_MEM-only 文件排除清单**）⇒ 规则从「文件名匹配 `_chmem.h`」改为「**显式排除清单 + 共享清单**」 |
| 规则可演进性 | ★ 未来新增 CH_MEM-only 文件时**必须**同步该清单（否则误判 ⇒ 门禁假红） |

#### D1.4 冻结标记形式（统一模板）

```cpp
// ip/cpu/plugins/dbus.h
//
// ★★ FROZEN — TLM 参考实现（ADR-092 §D1）
//   · 演进停止：任何修改须登记 .rddf/state/tlm-freeze-allowlist.txt + architect 签
//   · 用途：CH_MEM 版与 ip/cpu/core 开放核的**设计参考**
//   · 不参与 CH_MEM 路径；真删时点 = ADR-093（Phase-2）
//   · 机检：tools/check_tlm_freeze.sh（内容 diff + 标记存在性）
```

### D2 ★ 保留为「设计参考」的定位与载体

| 项 | 决策 |
|---|---|
| 定位 | ★ **参考基准**（reference baseline）⇒ 承载**语义**（PTW walk / TLB 2 级 / cache 替换策略 / stage 拓扑 / 多周期 FSM 语义）⇒ **实现本身即规格**（优于散文） |
| 载体 | ★ **新建 `docs/architecture/tlm-frozen-reference.md`**：32 个文件 × 各自的**语义职责**（不是行数清单）⇒ ★ **这份清单即 ADR-088 硬前置③「TLM 参考实现的语义已抽成规格」的载体** |
| 与 ADR-088 硬前置③ 的关系 | ★ **以「实现 + 职责清单」满足**，MUST NOT 额外写散文规格（⇒ 避免双份漂移） |
| 使用纪律 | ★ **只读参考** ⇒ 新代码 MUST NOT `#include` 冻结文件（除双模共享 9 个）⇒ 新核 MUST 复用 `arch/riscv/` 的**语义**而非**代码** |

### D3 ★ D-I：覆盖缺口的处置方法（★ **按 family 分类，不按数字**）

**问题**：若按「补齐 686 case」执行 ⇒ 需迁移**大量在 CH_MEM 上不存在的功能**。

⇒ ★ **D-I-1 裁决：改用 family 级分类清单**，落点 = ★ **ADR-088 阶段 B 台账**（`docs/architecture/tlm-coverage-parity-ledger.md`）⇒ **不新造机制**。

| TLM-only family（实测 32 个标签） | 分类 | 理由 |
|---|---|---|
| `[cpu-integration]`（3/5/7/10 stage 拓扑 · superscalar `dispatch_width=2`） | ★ **不可迁** | CH_MEM 是**固定 5 级全组合通路** ⇒ 拓扑类在 CH_MEM 上不存在 |
| `[mul-div-fsm]` `[mul-div-fsm-integration]` `[cycle-parity]` | ★ **不可迁** | CH_MEM 组合式 vs TLM radix-2 ⇒ cycle **不同构不可比**（ADR-089 已记录） |
| `[riscv-tests]` `[rv32um]` | ★ **重复** | CH_MEM 侧已有 48 ELF 全 PASS（功能重复覆盖） |
| `[bare-shortcut]` `[shadow-bug]` `[mmu-exception-handler]` `[paddr-propagation]` `[real-memory]` `[ptw]` `[multi-section]` | ★ **真缺口（P1）** | CH_MEM 的 MMU **只有 TLB 命中路径**（无 PTW walk）⇒ 依赖 `mmu-chmem-phase-b-c-launch` |
| `[soc]` `[cpu-l1-mmu-demo]` `[cpu-sim]` `[dhrystone]` `[performance]` | 待分类 | 依赖 SoC 级接线（ADR-091 S6） |
| `[api]` `[bundles]` `[result-paradigm]` `[elf-loader]` `[isa-xlen]` `[cpu-configs]` `[stall]` `[probe]` `[picolibc]` `[base-addr]` `[base-window]` | 待分类 | 多数已有 CH_MEM 侧等价覆盖 |

⇒ ★ **判据**：★ **MUST NOT 以「case 数」作删除判据**（686 是**形态差异**的产物，不是覆盖缺口）⇒ **以 family 分类的「真缺口」清单**为准。

### D4 ★ `ch_stream` 的层次区分（D-B 落地，避免误删 `Payload<T>`）

| 机制 | 作用域 | 语义 | 决策 |
|---|---|---|---|
| ★ `Payload<T>` + `PipeNode` | **核内 stage 之间** | 「跨 PipeNode 隔离（每个 PipeNode 有自己的 PayloadStore）」（`include/cf/plugin/payload.h:10`）⇒ 无握手，同拍数据通路 | ★ **保留不动** |
| ★ `ch_stream<T>` | **IP 之间 / 核 ↔ 从设备** | `valid`/`ready`/`fire()`/`isStall()` ⇒ **有握手**的接口协议 | ★ **采用** |

⇒ ★ 两者**层次不同，不是竞争关系** ⇒ ★ **MUST NOT** 以「采用 `ch_stream`」为理由删除 `Payload<T>`。

⇒ ★ **与 ADR-083 §4.2 的关系**：该条拒绝的是「**CppTLM** ch_stream 作为 IP 间通信」⇒ ★ **本决策修订其适用范围**（详见 **ADR-083 v1.2**）。

### D5 ★ Phase-1 的门禁改版（★ 轻量版；重版本在 ADR-093）

| 门禁 | Phase-1 处置 |
|---|---|
| `tools/check_tlm_freeze.sh` | ★ **升级**（D1.2 内容 diff + 标记存在性 + D1.3 检测面修正）；CI 级由 warn → ★ **阻塞**（冻结集是活契约） |
| `tools/check_plugin_portability.sh` | ★ **不改**（双模仍在 ⇒ Check 2/4/6/7/13 全部仍有效 ⇒ ★ **无真空 PASS 风险**） |
| 其余 5 个架构门禁 | 不改 |

### D6 ★ Phase-2（ADR-093）的准入门禁

⇒ **删除 TLM 代码 + 框架层单模化** MUST 满足**全部**三条方可开工：

| # | 门禁 | 判据 | 归属 |
|---|---|---|---|
| **G-1** | ★ **真缺口清单归零或全部显式登记** | D3 的 family 分类表完成；★ **「真缺口」类**（★ 当前 = MMU 语义面 6 个 family）逐条：已迁 CH_MEM / 登记为已知边界 / 判定为形态差异 | ADR-088 阶段 B 台账 |
| **G-2** | ★ **参考基准清单就位** | `docs/architecture/tlm-frozen-reference.md` 完成（32 文件 × 语义职责）⇒ ADR-088 硬前置③ 满足 | 本 ADR D2 |
| **G-3** | ★ **门禁体系改版 + 变异自检** | Check 2/3/4/6/7/13 改单模语义 + 空集合**显式报「已退役」** + ADR-040 v4.0 + ★ **变异自检**（注入旧 Check 5 能抓的 `if(ch_bool)` ⇒ 新检查 MUST 仍 RED） | ADR-093 |

⇒ ★ **G-3 MUST NOT 后补**（⇒ 否则 Check 7 真空 PASS ⇒ 门禁在删除时失效 ⇒ 正是 ADR-088 点名的最坏假绿形态）。

### D7 ★ 明确**不**在本 ADR 范围内

- ✗ **TLM 代码的删除** ⇒ ADR-093（Phase-2）
- ✗ **框架层单模化**（`uint_t.h` 别名化 / `CF_PLUGIN_USE_CH_MEM` 归零）⇒ ADR-093
- ✗ **bundle 单模化**（`bundles/` → `bundle_base`）⇒ ADR-093（★ 新核的 bundle 落地见 ADR-091 D4）
- ✗ **`check_plugin_portability.sh` 的单模改版** ⇒ ADR-093
- ✗ **二 binary → 一 binary 合并** ⇒ ADR-093
- ✗ **`ch_stream` 的接口契约**（`CpuBusReq`/`CpuBusResp` 真名 + 握手语义）⇒ **ADR-091 增补**（D-J）
- ~~✗ **ADR-083 的表述纠偏**（`ch_stream` 归类 / `MemReqBundle` 悬空引用）~~ ⇒ ✅ **已完成（ADR-083 v1.2 + 文档文本同步，2026-10-28）**：
  - ★ **ADR-083 v1.2**：§1.2 架构图（`ch_stream` 移出 CppTLM 框）+ §4.2 拒绝范围收窄 + Status/来源/关联 + §5.1 修订表
  - ★ **文档文本同步（22 文件）**：`AGENTS.md` · `docs/GLOSSARY.md` · `docs/architecture/{overview,background-and-goals,code-framework-mapping,interface-design,tech-selection}.md` · `docs/templates/IP_TEMPLATE.md` · `docs/lessons/m1-m5-cpu-implementation.md` · `ip/{README,cpu,cache,memory,peripheral,interconnect}/README.md` · `ip/cpu/rtl/README.md` · `tests/bundles/README.md` · 4 份 `ip/cpu/configs/*.json` · `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md`
  - ★ **有意的边界**：★ **研究快照不改**（`docs/research/*` · `docs/architecture/plugin-research/*` = 历史留痕）· ★ **归档不改**（`openspec/changes/archive/*` · `openspec/specs/*` · `.rddf/roadmap/archive/*`）

---

## Consequences

### 正面

1. ★ **686 case 保留** ⇒ 「删掉 463/686 个 case 后没有自动机制能发现语义退化」这一盲区**不存在**
2. ★ **Check 7 不真空 PASS** ⇒ 冻结门禁在 Phase-1 全程有效
3. ★ **TLM 参考语义以「可执行规格」形式保留** ⇒ ADR-088 硬前置③ **以实现满足**，MUST NOT 另写散文规格
4. ★ **开放核主线（ADR-091）不被阻塞** ⇒ Phase-1 可全速推进
5. ★ **冻结门禁从「禁新增函数」升级为「禁任何修改 + 强制标记」** ⇒ 冻结**可被机检**，MUST NOT 靠人工自觉

### 负面 / 成本

1. ⚠️ **框架层单模化推迟** ⇒ `uint_t` 双模分支与 `CF_PLUGIN_USE_CH_MEM`（167 处）继续存在 ⇒ ★ 阶段 C 的技术债延后（⇒ Phase-2 一次性还清）
2. ⚠️ **32 个冻结文件继续参与构建与测试** ⇒ 二 binary 继续存在 ⇒ CI 时长不减
3. ⚠️ **冻结标记需逐文件添加**（32 处）⇒ 一次性工作量（约 0.5 人日）
4. ⚠️ **「双模」与「冻结」并存 ⇒ 认知负担** ⇒ ★ 缓解：冻结标记 + 参考基准清单 + 本 ADR 的 D2 职责表
5. ⚠️ **D3 的 family 分类工作**（32 个标签逐条判）是 Phase-2 的长尾前置 ⇒ ★ 缓解：★ **可与 Phase-1 并行**（不阻塞开放核）

---

## Alternatives considered

| 备选 | 结论 | 理由 |
|---|---|---|
| **A. 直接删除 TLM** | ❌ 拒绝 | ★ 686 case 消失 + Check 7 真空 PASS + 参考语义丢失 ⇒ **三个风险同时发生**，而用户已明确「先不删」 |
| **B. 保留但不冻结**（现状） | ❌ 拒绝 | ★ 冻结门禁只「禁新增函数」⇒ **改函数体 / 改常量仍可发生** ⇒ 冻结不可信 ⇒ 参考基准会被悄悄改掉 |
| **C. 冻结 + 排除构建** | ❌ 拒绝（Phase-1） | ⇒ 框架层可立即单模化，但 686 case 消失 ⇒ 违反「先不删」的意图 ⇒ 归 Phase-2（ADR-093） |
| **D. 按「补齐 686 case」执行 D-I** | ❌ 拒绝 | ★ 大量 case 在 CH_MEM 上**不存在**（3/7/10 stage 拓扑 / 多周期 FSM cycle parity）⇒ 迁移不可行 ⇒ 改为 family 分类（D3） |
| **E. 另写散文规格替代实现** | ❌ 拒绝 | ★ 实现**即**规格（ADR-088 硬前置③ 原文「语义已抽成规格」）⇒ 散文 + 实现 = 双份漂移源 ⇒ 只做「文件 × 语义职责」清单 |
| **F. 新造一套冻结机制** | ❌ 拒绝 | ★ `check_tlm_freeze.sh` **已存在**且检测面正确（除 D1.3 误判）⇒ 升级即可 |
| **G. 用 `ch_stream` 取代 `Payload<T>`** | ❌ 拒绝 | ★ 层次不同（核内 stage 间 vs IP 间）⇒ `Payload<T>` 保留不动（D4） |

---

## Verification

- [ ] **V-1** ★ **冻结集正确**：`.rddf/state/tlm-frozen-exclude.txt` 就位且 `ip/cpu/configs/cpu_config_chmem_json.h` **不在**冻结集内；★ 冻结集 = **32 个文件**（41 候选 − 9 共享 − 1 误判修正）
- [ ] **V-2** ★ **冻结标记齐备**：★ 32 个文件**全部**含冻结标记 ⇒ 缺任一 ⇒ 门禁 RED
- [ ] **V-3** ★ **禁任何修改**：★ 对任一冻结文件做**任意内容改动**（含改函数体 / 改常量 / 改注释）⇒ 门禁 MUST RED
- [ ] **V-4** ★ **变异对照（鉴别力）**：★ 往冻结文件**新增一个函数** ⇒ MUST RED（★ 证明判据从「函数差集」升级到「内容 diff」后**没有变松**）
- [ ] **V-5** ★ **参考基准清单就位**：`docs/architecture/tlm-frozen-reference.md` 存在且 32 个文件**逐一**有语义职责条目 ⇒ ★ 抽样核对职责描述与代码实际语义一致（★ MUST NOT 空泛）
- [ ] **V-6** ★ **零回归**：★ TLM binary **1017 case** 与 CH_MEM binary **331 case** 计数**不变**；★ 现有 52 case parity 基线**逐位不变**（预载核不动）
- [ ] **V-7** ★ **门禁无假绿**：★ `check_plugin_portability.sh` **全绿**且 ★ **Check 2/4/6/7/13 仍有效**（★ 判据：★ 注入一个旧 Check 5 能抓的 `if(ch_bool)` ⇒ MUST 仍 RED ⇒ 防「改完门禁反而松了」）
- [ ] **V-8** ★ **双模仍在**：★ `CF_PLUGIN_USE_CH_MEM` 使用点 **167 处仍在**（⇒ 证明 Phase-1 未误触单模化）
- [ ] **V-9** ★ **文档同步完成**：★ 22 个文件已同步（★ `AGENTS.md` + `docs/architecture/*` 5 篇 + `docs/GLOSSARY.md` + 模板 + lessons + 9 个 IP/README + `tests/bundles/README.md` + 4 份 configs JSON + 1 个 roadmap objective）⇒ ★ 复查「`不使用.*ch_stream`」类断言**仅剩已修正的合法表述**；★ **`MemReqBundle` 悬空引用在活文档中归零**（★ 设计文档**自带**该名字的 `struct` 定义处**保留** ⇒ 那是设计意图命名，非悬空）；★ **有意的边界**：研究快照 + 归档**未改**

---

## Related

- **ADR-088 v1.4 §10**（三段迁移路径）—— 本 ADR 是阶段 C 的 **Phase-1 准备**；★ **不**替代其 5 条硬前置（② CI build 能力 / ⑤ 门禁体系改版 仍待 ADR-093）
- **ADR-089 v1.4**（双后端差分 parity 门禁）—— V-6 的基线来源
- **ADR-091 v1.0**（CH_MEM 开放核形态与端口架构）—— ★ 开放核主线（D-J 接口契约在其增补中）
- **ADR-083 v1.2**（表述纠偏）—— ★ `ch_stream` 归类 + `MemReqBundle` 悬空引用（配套文档）
- **ADR-040 v3.0**（TLM→HDL 移植性约束）—— Phase-2 的 v4.0 改版对象
- **ADR-085 / ADR-086**（MMU PTW / L0-L1 TLB 架构）—— ★ D3「真缺口」（MMU 语义面）的实装依据
- `docs/architecture/tlm-coverage-parity-ledger.md` —— ★ **D-I 的落点**（ADR-088 阶段 B 台账）
- `tools/check_tlm_freeze.sh` —— ★ **D1 的升级对象**
- `.rddf/state/tlm-shared-files.txt`（9 个双模共享）· `.rddf/state/tlm-freeze-base.txt` · `.rddf/state/tlm-freeze-allowlist.txt`
- 上游：`/workspace/project/CppHDL/include/bundle/stream_bundle.h`（`ch_stream`）· `/workspace/project/CppTLM/include/core/chstream_module.hh`（★ CppTLM 的 `chstream_*`，**与 `ch_stream` 是不同物**）
