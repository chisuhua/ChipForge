# ADR-093 — TLM 模式移除契约（Phase-2：框架层单模化 + 门禁体系改版）

> **Status**: 🚧 **Proposed（v0.1，2026-10-28，architect 主持；★ **待 G-1a/G-1b/G-3 达成后转 Accepted**）**
> **上游**: **ADR-092 v1.2**（TLM 参考实现的冻结与演进停止 —— ★ **Phase-1 已落地**）· **ADR-091 v1.4**（CH_MEM 开放核形态）· ADR-088 v1.4 §10（三段迁移路径）· ADR-083 v1.2 · ADR-040 v3.0
> **下游**: `include/cf/plugin/uint_t.h` · `bundles/mem_bundles.h` · `tools/check_plugin_portability.sh` · `ip/**`（TLM 版业务文件）· `src/cf_plugin/bridge/` · `tools/cpu_sim/` · `tests/CMakeLists.txt`
> **执行文件（待 planner 落）**: `.rddf/plans/`（★ 归 builder 起草）
> **编号说明**: `ADR-090` 已被 `openspec/changes/vexii-riscv-parity-poc` 预留 ⇒ 本 ADR 取 **ADR-093**。

---

## Context

### 1. 三段路径中的当前位置

| 阶段 | 内容 | 状态 |
|---|---|---|
| **Phase-1** | ★ **冻结保留**（TLM 代码不动 + 演进停止 + 标注为设计参考） | ✅ **ADR-092 已落地**（32 文件 + 门禁升级 + 参考基准清单） |
| ★ **Phase-2** | ★ **移除 TLM 依赖**（框架层单模化 + bundle 单模化 + 门禁改版 + TLM 代码删除） | 🚧 **本 ADR** |
| （Phase-3 = 阶段 C） | TLM 模式退役的最终收口 | 归 ADR-088 |

⇒ ★ **Phase-1 与 Phase-2 之间有硬门禁**（ADR-092 §D6 的 G-1/G-2/G-3）⇒ ★ 本 ADR 的「启动条件」= 那三条 + 本 ADR §D2 的 G-1a/G-1b。

### 2. ★ Phase-2 的**真实工作量**已被 D-I 分类表收敛（ADR-092 §D3.1）

★ **实测口径**：TLM **510 case** vs CH_MEM **176 case** ⇒ ★ **478 个 TLM-only case**（★ **数字更正**：先前引用的「686」是 `--list-tests | wc -l` 的**行数** —— Catch2 的 tag 独占一行 ⇒ 行数 ≈ 2× case 数 ⇒ **不是** case 数）。

★ **逐 family 判定（42 family）后的分布**：

| 判定 | family 数 | 性质 |
|---|---:|---|
| ★ **真缺口** | **8** | ★ **集中在 2 个子系统**：MMU 语义面（6）+ Cache（1）+ 随 MMU 的 SoC demo（1）⇒ ★ 实装**已有归属** |
| **不可迁** | 15 | ★ **形态差异**（TLM 特有语义 / 工具 / 多周期 FSM cycle 不可比）⇒ **永远补不齐** |
| **重复** | 7 | CH_MEM 侧已有等价覆盖 |
| **可迁** | 5 | 与后端无关（JSON 拓扑 / ELF 加载 / 探针），低成本 |

⇒ ★★ **Phase-2 的实质不是「补测试」，而是「等两个子系统实装」** ⇒ ★ **这是外部依赖，不是工作量问题** ⇒ ⇒ ★ **Phase-1（冻结）应长期维持**。

### 3. Phase-2 的技术动作（已由 ADR-092 §D7 锁定）

| # | 动作 | 规模 |
|---|---|---|
| ① | ★ 框架层单模化：`uint_t.h` 删模态分支 ⇒ ★ **`cf::plugin::uint_t<N>` 保留为 `ch::core::ch_uint<N>` 的别名** ⇒ **57 个引用文件零改动** | ★ **1 人日** |
| ② | `CF_PLUGIN_USE_CH_MEM` **167 处**归零 ⇒ CppHDL 成唯一依赖 | 0.5 人日 |
| ③ | bundle 单模化：`bundles/mem_bundles.h` → `bundle_base` 派生（= `bundles/README.md:103` 的 Phase 5 形态） | 1~2 人日 |
| ④ | ★ **门禁体系改版**（见 §D3） | ★ **1~2 人日** |
| ⑤ | 删 TLM 代码：`src/cf_plugin/bridge/`(8) + `tools/cpu_sim/`(2) + `ip/` TLM 版(54) + 下线 `ExternalProject_Add(cpptlm)` | 1~2 人日 |
| ⑥ | 二 binary → 一 binary（`CHMEM_TEST_SOURCES` 并入 TLM binary 的 GLOB）+ CI/脚本同步 | 0.5~1 人日 |
| ⑦ | 文档同步（~25 活文件）+ ADR-040 升 v4.0 + AGENTS.md 双模表述清除 | 1 人日 |

⇒ ★ **合计 ≈ 6~10 人日**（★ 与 ADR-091 §D8 的开放核 6~10 人日**量级相当** ⇒ ★ 二者**必须串行或错峰**，不可并行 —— 见 §风险 R-1）。

---

## Decision

### D1 ★ 框架层单模化：`uint_t` 保留为别名（★ 最小代价路径）

```cpp
// include/cf/plugin/uint_t.h（Phase-2 后）
namespace cf::plugin {
template <unsigned N> using uint_t = ch::core::ch_uint<N>;   // ★ 单模
using bool_t = ch::core::ch_bool;
}
```

⇒ ★ **57 个引用文件零改动**（`ip/` 下 27 个也是）⇒ ★ `CF_PLUGIN_USE_CH_MEM` 167 处归零 ⇒ ★ **CppHDL 成唯一依赖** ⇒ ★ **CppTLM 依赖真正移除**。

⚠️ ★ **代价**：类型名 `cf::plugin::uint_t` 与 `ch::core::ch_uint` 同义但不同名 ⇒ **认知负担** ⇒ ⇒ ★ **替代方案（全改 57 文件）MUST NOT 在 Phase-2 做**（★ 改动面大 + 与 §D4 的门禁改版撞车）⇒ ★ 长期可在阶段 C 之后单独清理。

### D2 ★ Phase-2 的启动条件（★ G-1a/G-1b + G-2 + G-3）

| 门禁 | 判据 | 归属 | 状态 |
|---|---|---|---|
| **G-1a** | ★ **MMU 语义面 6 个 family** ⇒ **实装**（`chmem-ptw-walk` + `mmu-chmem-phase-b-c-launch`）**或** 显式登记为已知边界 | 外部 change | ⬜ **未满足** |
| **G-1b** | ★ **Cache 1 个 family** ⇒ **实装**（`verilator-l1cache-e2e-coverage`）**或** 显式登记 | 外部 change | ⬜ **未满足** |
| **G-2** | ★ **参考基准清单就位**（`docs/architecture/tlm-frozen-reference.md`：32 文件 × 语义职责）⇒ ADR-088 硬前置③ 满足 | ADR-092 §D2 | ⬜ **未做** |
| **G-3** | ★ **门禁体系改版 + 变异自检**（见 §D3）⇒ MUST NOT 后补 | 本 ADR §D3 | ⬜ **未做** |

⇒ ★★ **G-1a/G-1b 是外部依赖**（MMU + Cache 实装）⇒ ★ **Phase-2 在它们完成前不应开工** ⇒ ★ **Phase-1 冻结维持**。

⇒ ★ **零工作量项（可立即做，与 G-1a/b 无依赖）**：
| # | 动作 | 收益 |
|---|---|---|
| **Z-1** | `chmem-config` + `cpu-configs` + `negotiate` 加入 `CHMEM_TEST_SOURCES` | ★ **零代码改动** ⇒ 41 个 case 在 CH_MEM 侧也跑（★ 消除「binary 归属」盲区） |
| **Z-2** | `soc` / `elf-loader` / `multi-section` / `probe` 迁到 CH_MEM 侧 | ★ 与后端无关 ⇒ 低成本（★ 可选） |

### D3 ★ 门禁体系改版（★ MUST NOT 后补 —— 防「真空 PASS」）

| Check | 现状（双模） | Phase-2 后（单模） |
|---|---|---|
| **Check 2** | `ip/*/plugins/` 必须用 `ch_*`（TLM 文件扫） | ★ **扫描面改为「全量 `ip/**`」**（TLM 文件已删 ⇒ 原检查对象为空 ⇒ **真空 PASS**） |
| **Check 3** | `Plugin::build()` 内不调 `pb.run()` | ★ **改**：TLM 模式不存在 ⇒ 检查项**退役** ⇒ **MUST 显式报「已退役」** |
| **Check 4** | 存储优先 `array_store`(TLM) **或** `ch_mem`(CH_MEM) | ★ 改：**只查 `ch_mem`**（`array_store` 退役） |
| **Check 5** | `at_stage` 回调内禁运行期 `if(ch_bool)` | ★ **保留**（ADR-088 硬前置⑤ 明示） |
| **Check 6** | 禁源/头文件内 `#define CF_PLUGIN_USE_CH_MEM` | ★ **改**：开关本身退役 ⇒ **MUST 显式报「已退役」** |
| **Check 7** | TLM-only 文件不应含 ch 类型实例化 | ★ **退役**（TLM 文件已删 ⇒ **真空 PASS**）⇒ **MUST 显式报「已退役」** |
| **Check 13** | payload key 必须模式化（`x-chmem-supported`） | ★ **改**：恒成立 ⇒ 需重新设计判据（★ 否则退化为「恒 PASS」） |
| 其余（1/8/9/10/11/12） | — | ★ 保留 |

⇒ ★★ **验收形式 = 变异自检**（★ ADR-088 硬前置⑤ 原文要求）：
> ★ 注入一个**旧 Check 5 能抓的** `if(ch_bool)` ⇒ ★ **新检查 MUST 仍 RED**
> ⇒ ★ 且：**Check 2/3/6/7/13 的「退役」MUST 是**显式输出**，MUST NOT 静默 PASS

⇒ ★ **门禁改版 MUST 与代码删除同批交付**（★ 否则删除时门禁「看起来全绿」实则失效）。

### D4 ★ bundle 单模化

| 项 | 决策 |
|---|---|
| 形态 | ★ `bundles/mem_bundles.h` → `bundle_base` 派生 + `CH_BUNDLE_FIELDS_T` + `ch::core::ch_uint`（★ = `bundles/README.md:103` 的 **Phase 5 计划**） |
| ★ **G-1f 隐患** | ★ **字段 MUST NOT 带字面量初始化器**（ADR-091 v1.4 §D5.1）⇒ ★ 老 bundle 的 `uint_t<64> address{0}` **必须去掉**（★ 否则 CH_MEM 下 include 即报错） |
| 消费者 | ★ 老 bundle 的 14 个消费者中，TLM 侧（bridge + cache plugin + 5 个 test）随 TLM 删除 ⇒ 剩 MMU CH_MEM 侧 + 新核 |
| 与新核的关系 | ★ **两套并存到 Phase-2 结束** ⇒ Phase-2 后老 bundle 也单模化 ⇒ ⇒ ★ 新核的 `CpuBusReq`/`CpuBusResp` **保持独立**（形态专属命名 ⇒ ADR-091 §D3a.3） |

### D5 ★ 二 binary → 一 binary

| 项 | 决策 |
|---|---|
| 现状 | `chipforge_tests`（TLM，`GLOB_RECURSE` 自动收集）+ `chipforge_tests_chmem`（CH_MEM，**显式列表**） |
| Phase-2 后 | ★ **合并为 1 个 binary**（TLM 模式消失 ⇒ `chipforge_tests_chmem` 退役）⇒ ★ CHMEM 的显式源文件**并入** GLOB 收集（★ `CHMEM_TEST_SOURCES` 的显式清单需逐条核对哪些不在 `GLOB_RECURSE` 命中范围） |
| CI / 脚本 | ★ `tools/run_chipforge_tests.sh` + `.github/workflows/architecture-gates.yml` 同步 |
| ★ 前置 | ★ **CI 具备 build 能力**（ADR-088 §5.2 判据 4/7 ⇒ ★ **A4 尚未做**）⇒ ★ **D5 MUST 等 A4** |

### D6 ★ 明确**不**在本 ADR 范围内

- ✗ **TLM 代码的冻结/标注** ⇒ ADR-092（已完成）
- ✗ **开放核的实现** ⇒ ADR-091 / `backend-parity-open-core`
- ✗ **MMU PTW / Cache 的实装** ⇒ `chmem-ptw-walk` / `mmu-chmem-phase-b-c-launch` / `verilator-l1cache-e2e-coverage`（★ **外部依赖**）
- ✗ **CI build 能力** ⇒ A4（`backend-parity-ci`）
- ✗ **台账内容更新** ⇒ ADR-088 阶段 B（★ 台账在 architect 的 `exclude_paths` 内 ⇒ 归该阶段 owner）
- ✗ **`cf::plugin::uint_t` → `ch::core::ch_uint` 的改名清理** ⇒ 阶段 C 之后单独处理（★ 见 §D1 代价）

---

## Consequences

### 正面

1. ★ **CppTLM 依赖真正移除** ⇒ 单模型（CH_MEM）+ 双后端 + 一道门禁的**目标态达成**（ADR-088 §10）
2. ★ **`uint_t` 别名化 ⇒ 57 个引用文件零改动** ⇒ 最小代价
3. ★ **门禁体系从「双模语义」改「单模语义」** ⇒ ★ 消除「真空 PASS」这一最坏假绿形态（ADR-088 点名）
4. ★ **D-I 分类表把 Phase-2 的阻塞收敛为 2 个外部依赖** ⇒ ★ 决策依据从「模糊的工作量估计」变为「可追踪的 2 个 change」

### 负面 / 成本

1. ⚠️ **Phase-2 被外部依赖阻塞**（MMU + Cache 实装）⇒ ★ **在此之前 TLM 代码必须继续冻结维护**（⇒ 冻结门禁持续生效）
2. ⚠️ **两套 bundle 并存的窗口**（Phase-1 + Phase-2）⇒ 漂移风险 ⇒ 缓解：H-3 等价性自检（ADR-091）+ 本 ADR §D4
3. ⚠️ **`uint_t` 别名的认知负担**（§D1）⇒ 长期清理归阶段 C 之后
4. ⚠️ **★ Phase-2 与开放核（ADR-091）量级相当（各 6~10 人日）⇒ 不可并行** ⇒ ★ **必须串行或错峰**（★ roadmap R5 风险）
5. ⚠️ **门禁改版与代码删除必须同批** ⇒ ★ 单批改动面大 ⇒ 需拆 commit（按 `CONTRIBUTING.md` §3.1 粒度）

---

## Alternatives considered

| 备选 | 结论 | 理由 |
|---|---|---|
| **A. 现在就启动 Phase-2** | ❌ 拒绝 | ★ G-1a/G-1b 未满足 ⇒ 真缺口（MMU/Cache）**无保护**就删 TLM ⇒ 恰是 ADR-088 §5.2 判据 6 要防的盲区 |
| **B. 把 `uint_t` 全改成 `ch::core::ch_uint`** | ❌ 拒绝 | ★ 57 文件改动 + 与门禁改版撞车 ⇒ 收益仅「认知干净」⇒ 不值 |
| **C. 保留双 binary（TLM + CH_MEM）** | ❌ 拒绝 | TLM 模式消失后 TLM binary 无意义 ⇒ 且 CI 负担翻倍 |
| **D. 门禁改版延后到删除之后** | ❌ 拒绝 | ★ 删除时门禁「看起来全绿」实则 Check 2/7 **真空 PASS** ⇒ ★ ADR-088 硬前置⑤ 原文：「MUST NOT 后补」 |
| **E. 按「补齐 478 个 case」推进** | ❌ 拒绝 | ★ D-I 分类表证明其中约 **3/4 是形态差异或重复覆盖** ⇒ 永远补不齐 ⇒ ★ **错误的工程目标** |
| **F. 用「无基线检测能力」当护栏** | ❌ 拒绝 | ★ 现状 `check_coverage_parity_ledger.sh` **无**「头部计数 == 条目级实测」等式校验 ⇒ 「脚本仍 PASS」**不构成证据**（ADR-088 §5.3.3 V-6 原文） |

---

## Verification

- [ ] **V-1** ★ **单模化生效**：`CF_PLUGIN_USE_CH_MEM` 使用点 **167 → 0**；★ `uint_t.h` 只剩 CH_MEM 分支；★ **57 个引用文件零改动**（编译通过 + `git diff --stat` 无 ip/ 业务文件）
- [ ] **V-2** ★ **门禁改版有效**：★ Check 2/3/6/7/13 改单模语义；★ **退役项 MUST 显式报「已退役」**（★ **MUST NOT** 静默 PASS）
- [ ] **V-3** ★ ★ **变异自检**：★ 注入一个**旧 Check 5 能抓的** `if(ch_bool)` ⇒ ★ **新检查 MUST 仍 RED**（★ ADR-088 硬前置⑤ 的验收形式）
- [ ] **V-4** ★ **真空 PASS 检测**：★ 人为清空 Check 2/7 的扫描面 ⇒ ★ 脚本 MUST **显式报「该检查已退役 / 扫描面为空」** ⇒ **MUST NOT** 返回 0
- [ ] **V-5** ★ **TLM 依赖归零**：`grep -rn "cpptlm\|CppTLM"` 在 `include/` `ip/` `src/` `tools/` ⇒ **0 命中**；★ `ExternalProject_Add(cpptlm)` 已下线
- [ ] **V-6** ★ **bundle 单模化**：★ `bundles/mem_bundles.h` 为 `bundle_base` 派生；★ **字段无字面量初始化器**（G-1f）；★ `CH_BUNDLE_FIELDS_T` 宏可用（作用域内 `using ch::core::bundle_base;`）
- [ ] **V-7** ★ **binary 合并**：★ 单 binary 覆盖原两者的**全部** case；★ `run_chipforge_tests.sh` + CI 同步
- [ ] **V-8** ★ **零回归**：★ 原 TLM binary 的**不可迁** family 在合并后**仍保留在 CH_MEM binary**（★ 迁移的 case 全 PASS）；★ **真缺口 family 已被 G-1a/G-1b 覆盖或显式登记**
- [ ] **V-9** ★ **八门禁全绿** + ADR-040 升 v4.0 + AGENTS.md 双模表述清除

---

## Related

- **ADR-092 v1.2**（TLM 参考实现的冻结与演进停止）—— ★ **Phase-1 已落地**；★ §D3.1 = **本 ADR 的门禁依据**
- **ADR-091 v1.4**（CH_MEM 开放核形态）—— ★ **量级相当 ⇒ 须串行**（§Consequences 负面 4）；★ §D5.1 的 G-1f 是本 ADR §D4 的直接依据
- **ADR-088 v1.4 §10**（三段迁移路径 + 阶段 C 5 条硬前置）—— ★ 本 ADR 是阶段 C 的**准备**，★ **不**替代其硬前置
- **ADR-083 v1.2**（`ch_stream` 归类纠偏）· **ADR-040 v3.0**（Phase-2 升 v4.0 的对象）
- **ADR-085 / ADR-086**（MMU PTW / L0-L1 TLB 架构）—— ★ G-1a 的实现依据
- `.rddf/plans/chmem-bp-btb-gshare-increments.md` · `ip/cpu/docs/bp-btb-gshare-design-brief.md` —— TLM 参考语义
- `docs/architecture/tlm-coverage-parity-ledger.md` —— ★ **阶段 B 才更新**（本 ADR 不改）
- `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md` —— 本 ADR 是其阶段 B/C 的技术前提
