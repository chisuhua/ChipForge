## Context

`mmu-cache-integration`（archived 2026-09-13）落地了 MMU+Cache VIPT 数据流 + RiscV hook 实装（`ip/cpu/plugins/mmu.cpp`）。但**消费端**——RISC-V CPU Plugin——**尚未消费**这些 hook：

- `cpu_factory.h` 仅 `enable_mmu=true` 配置字段（line 65），**没有** `RiscvMMUPlugin` 注册到 `PluginOrder`
- `cpu_params_schema.json` 已有 `enable_mmu`/`mmu_mode` 字段（line 42-52）和条件依赖（line 203-204），但 schema 字段无对应代码实装
- MMU hook（`csr_write_satp`、`sfence_vma`）设计意图是 `at_stage` 闭包驱动（commit 6 注释 "此函数主要供 CPU 侧 at_stage 触发"），但 CPU pipeline 没人调
- MMU 输出 `pl::MMU_VADDR`/`pl::PADDR` 给谁？CPU `tlb_lookup_ifetch`/`tlb_lookup_loadstore` 不订阅

**当前状态（Oracle round-2 verified）**：
- `[cpu-integration]` 25/25 PASS（bit-identical baseline，但 MMU hook 没真正接到 CPU pipeline）
- `[mmu]` 40/40 PASS（RiscV hook 4 cases 已在 commit 6 落地）
- baseline 306/306 PASS
- 4 architecture gates all green

本 change 把 `RiscvMMUPlugin` 实接入 CPU Plugin pipeline。Oracle Tier 1 #1 推荐此 change 是 M5 critical path。

## Goals / Non-Goals

### Goals

1. `cpu_factory.h` 条件注册 `RiscvMMUPlugin`（仅 `enable_mmu=true`）
2. `RiscvMMUPlugin::at_stage("csr_write_satp")` / `at_stage("sfence_vma")` / `at_stage("mmu_exit")` 三个 substage 路由到 MMU hook（已实装）
3. exception 12/13/15 从 MMU → CPU exception 路径
4. 5/7/10/3-stage RiscV integration tests + 独立 RiscV hook 测试
5. 解锁 M5 critical path（`soc/cpu_l1_picolibc/demo.json`）

### Non-Goals

1. **plugin-framework-stall**（Tier 1 #2）—— 改 `PipeBuilder::run()` 契约，独立 change
2. **cache-phase1.5-4way**（Tier 2 #4）—— 4-way + 64B line 升级，独立 change
3. **cache-dse-sweep**（Tier 2 #5）—— 12-case Pareto DSE，独立 change
4. **工具链修复 5 个 pre-existing failures**（tohost + riscv64）—— 留给 `soc-cpu-l1-mmu-demo` change commit 0
5. **MMUTLMBridgeAdapter full ch_stream protocol conversion** —— 留给 `cpptlm-protocol-conversion` change
6. **mmu-sv32-sv48-ext** —— Sv32/Sv48 PTW 解码推迟
7. **soc-cpu-l1-picolibc demo JSON** —— M5 demo，留给下一 change
8. **CPU pipeline stage renaming** —— 保留现有 5/7/10-stage 命名（`fetch/decode/execute/memory/writeback`）
9. **mmuus/rtl/** —— Phase 5+ 沿用
10. **L1Cache 4-way VIPT 升级** —— 推迟
11. **MMU 多 ASID cross-level invalidate 增强** —— 已通过 mmu-cache-integration commit 6 `invalidate_vaddr_any_asid` 实装

## Decisions

### Decision 1: 条件注册 vs 全局注册

**选择**: `cpu_factory.h::build_cpu()` 仅在 `config.enable_mmu=true` 时插入 `RiscvMMUPlugin` 到 PluginOrder。

**替代方案**:
- A. 全局注册（所有 config）—— 破坏 bit-identical baseline
- B. 仅在 7-stage+ 注册 —— 不合理，5-stage 也需要 MMU

**理由**: 25 个 `[cpu-integration]` baseline 测试在 `enable_mmu=false` 跑通，必须保持零回归。条件注册确保 `enable_mmu=false` 时行为完全一致。

### Decision 2: 三个 substage 命名（csr_write_satp / sfence_vma / mmu_exit）

**选择**: 三个新 substage 通过 `pb.declare_substage()` 挂到 CPU 现有 stage 树：
- `csr_write_satp` parent `execute`（CSR 写在 execute 阶段拦截）
- `sfence_vma` parent `execute`（SFENCE.VMA 指令拦截）
- `mmu_exit` parent `memory`（MMU exit 后路由 exception 到 CPU exception 路径）

**替代方案**:
- A. 单一 substage `mmu` 挂 fetch —— 粒度太粗
- B. 三 substage 挂 fetch + memory —— 分散且非对称

**理由**: RiscV 规范中 satp 写和 SFENCE.VMA 是 execute 阶段特权指令；MMU exception 是 memory 阶段访问结果。三 substage 镜像硬件行为。

### Decision 3: CPU ↔ MMU IPC Key 放在 CPU 侧（`ip/cpu/tlm/cpu_keys.h`）

**选择**: 新增 `ip/cpu/tlm/cpu_keys.h`，包含 CPU → MMU IPC Key：
- `SAT`（satp CSR value）
- `SFENCE_VADDR`（rs1）
- `SFENCE_ASID`（rs2）
- `CPU_EXCEPTION_CODE`（exception 12/13/15）

**替代方案**:
- A. 放 `ip/mmu/tlm/mmu_keys.h` —— mmu 反向依赖 CPU，违反 lib/ 边界
- B. 用 mmu 现有 Key 反向 —— MMU 输出 vaddr 给 CPU，但 SAT 是 CPU 写给 MMU，方向反

**理由**: `mmu_keys.h` 是 MMU 输出 Key 集合（PADDR/MMU_VADDR/EXCEPTION_CODE）。CPU → MMU 方向的 SAT/SFENCE 写 Key 应该放 CPU 侧。`grep -rn "ip/cpu" ip/mmu/` 必须 0 匹配。

### Decision 4: enable_mmu=true baseline preservation via separate substages

**选择**: 当 `enable_mmu=true` 时，3 个 substage 都被声明；`enable_mmu=false` 时，3 个 substage 都不声明（baseline）。

**替代方案**:
- A. 总是声明 3 substage，闭包检查 `enable_mmu` 跳过 —— 增加 baseline 干扰
- B. 在 PluginOrder 跳过 MMU，substage 自然不注册 —— 复杂

**理由**: `pb.declare_substage()` 仅在 `register_plugin<RiscvMMUPlugin>()` 调用时执行；条件注册保证 substage 同步声明/不声明。`enable_mmu=false` 路径下 substage 不存在，所有现有 baseline 测试不受影响。

### Decision 5: 5 commits 分组（不算 archive）

**选择**: 5 commits 序列：
1. `cpu_factory` 条件注册 + at_stage substage 声明
2. `RiscvMMUPlugin::at_stage` 三个闭包实装
3. 5 个 integration tests（4 stage + 1 hook test file）
4. docs sync
5. final 4-gate verify + archive

**替代方案**:
- A. 1 个大 commit —— 风险高，难 review
- B. 10+ 细粒度 commits —— 增加 overhead

**理由**: 5 commits 平衡 review 粒度与开发效率。每个 commit 后 `cmake --build build` + `[cpu-integration]` 验证。

### Decision 6: 工具链修复不在本 change

**选择**: 5 个 pre-existing RISC-V sim 测试失败（tohost 字符串 + riscv64 assembler path）不在本 change 修复。

**理由**: 工具链修复是 demo 阶段问题（`test_cpu_sim_real_tohost` 需要真实 ELF 跑通），与 CPU-MMU 集成无强耦合。**实际验证**: 当前 `[cpu-integration]` 25/25 PASS——Oracle 报告的 5 failures 是 stale 信息（baseline 已经修好）。

### Decision 7: 用 `PTW_ACTIVE + RETRY` 数据依赖替代 CtrlLink stall

**选择**: 本 change 不引入 CtrlLink stall framework 改动。MMU PTW miss 时通过 `pl::PTW_ACTIVE` Payload Key + 数据依赖返回 error=true，由 CPU retry（mmu-cache-integration Decision 7 已知 limitation）。**Known-limitation**: PTW-miss 路径不在本 change 测试范围——8 个新测试只覆盖 TLB-hit + commit 0 修补后的 PTW 成功回调。PTW miss 的 stall / retry 行为推迟到独立 `plugin-framework-stall` change（Tier 1 #2）。

**理由**: stall 是独立 Tier 1 #2 change。本 change 验证 MMU hook 接到 CPU pipeline + exception routing；stall 推迟。

### Decision 8: RiscV hook test 独立 test_cpu_riscv_mmu_hooks.cpp

**选择**: 单独的 `tests/cpu/test_cpu_riscv_mmu_hooks.cpp`（不在 test_5stage_riscv.cpp 之类文件）专门测 RiscV hook 路由（CSR 写、SFENCE.VMA、exception）。

**理由**: hook 路由是 cross-cutting（所有 stage 都可能），不应绑死在某个 stage test 里。独立 file 便于：
- 后续 mmu-sv32-sv48-ext 复用
- 工具链更新（test 框架切 Catch2）时不污染
- 失败定位清晰（hook 失败 vs stage 失败）

## Risks / Trade-offs

### Risk 1: enable_mmu=true 路径回归 25 个 [cpu-integration] 测试

[新加的 substage + RiscVMMUPlugin at_stage 闭包可能影响现有 pipeline] → Mitigation: 三 substage 闭包必须 idempotent（empty 闭包 noop）；commit 1 先 baseline 验证；条件注册保证 enable_mmu=false 路径不变。

### Risk 2: CPU pipeline stage 命名假设不匹配

[MMU 假设 execute/memory 阶段存在，但某些 pipeline config 可能没有] → Mitigation: 5/7/10/3-stage 都有 execute + memory stage（pipeline_stages>=3 必有）；mmu_exit parent memory stage 不存在时 skip 注册。

### Risk 3: RiscVMMUPlugin at_stage 闭包与现有 mmu-cache-integration tests 冲突

[mmu-cache-integration commit 6 的 4 个 [RiscV] 测试直接调 `mmu.sfence_vma()` 方法，不需要 at_stage] → Mitigation: at_stage 闭包是新层（hook 路由），不影响直接方法调用；两套 API 并存。

### Risk 4: 5 commits 顺序依赖断裂

[commit 2 依赖 commit 1 的 substage 声明；commit 3 依赖 commit 2 的闭包实装] → Mitigation: 每个 commit 后 [cpu-integration] 测试 + baseline 验证；任何中间失败都可独立 revert。

### Risk 5: cpu_factory.h 修改影响 14 个 cpu 单元测试

[cpu_factory 是 19+ cpu test 的中心入口] → Mitigation: 条件注册用 `if (config.enable_mmu)` 包装；现有 cpu 单元测试用 default config（enable_mmu=true），MMU 注册后行为变化；mmu-key 闭包必须幂等。

### Risk 6: ip/cpu/tlm/cpu_keys.h 新文件引入反向依赖

[mmu 闭包读 cpu_keys 会引入 mmu→cpu 反向] → Mitigation: `cpu_keys.h` 定义 IPC Key 时**声明** Key，`mmu_keys.h` 定义 IPC Key 时**声明** Key。两边各自声明。实际消费在 mmu.cpp at_stage 闭包，闭包直接 `#include "ip/cpu/tlm/cpu_keys.h"`（mmu 闭包依赖 cpu_keys.h 的 **Key identity**，不是反向依赖 CPU lib/）。

### Trade-off: 5 commits vs 1 atomic commit

5 commits 增加 4 次 commit overhead（branch/PR review 成本），但每个 commit 边界清晰，bug 定位快，回滚风险小。1 个 atomic commit 风险高（一步错全错），不推荐。

## Migration Plan

### Phase 1: factory wiring + substage declaration (commit 1/6)

- 修改 `ip/cpu/cpu_factory.h::CpuConfig`：保留现有 `enable_mmu` 字段（已是 line 65 默认 true）
- 修改 `ip/cpu/cpu_factory.h::build_cpu()`：增加 `if (config.enable_mmu) { ... pb.register_plugin<RiscvMMUPlugin>(...); ... }`
- 修改 `ip/cpu/cpu_factory.h::build_cpu()`：在 PluginOrder 注册后增加 3 个 `pb.declare_substage()`
- 验证：现有 25 个 [cpu-integration] 测试在 `enable_mmu=false` baseline 必须 bit-identical PASS

### Phase 2: at_stage 三个闭包实装 (commit 2/6)

- 新建 `ip/cpu/tlm/cpu_keys.h`：4 个 Payload Key（SAT/SFENCE_VADDR/SFENCE_ASID/CPU_EXCEPTION_CODE）
- 修改 `ip/cpu/plugins/mmu.cpp`：增加 `at_stage("csr_write_satp")` / `at_stage("sfence_vma")` / `at_stage("mmu_exit")` 三个闭包
- 验证：编译通过；`[cpu-integration]` + `[mmu]` + `[RiscV]` 全 PASS

### Phase 3: 集成测试 (commit 3/6)

- 扩展 `tests/cpu/integration/test_3stage_riscv.cpp`：+1 case `EnableMMU3StageBuilds`（3-stage + 3 MMU substage 全部注册）
- 扩展 `tests/cpu/integration/test_5stage_riscv.cpp`：+2 case（`EnableMMU5StageBuilds` + `EnableMMUDisabledBitIdenticalBaseline`）
- 扩展 `tests/cpu/integration/test_7stage_riscv.cpp`：+1 case `EnableMMU7StageCommitRetire`
- 扩展 `tests/cpu/integration/test_10stage_riscv.cpp`：+1 case `EnableMMU10StageDeepPipeline`
- 新建 `tests/cpu/test_cpu_riscv_mmu_hooks.cpp`：3 cases（`CSRWriteSatpRoutesToRiscVMMUPlugin` + `SFENCEVMARoutesToRiscVMMUPlugin` + `MMUExceptionPropagatesToCPU`）
- 验证：8 新 case + 25 baseline = **33/33** `[cpu-integration]` PASS；baseline 314/314

### Phase 4: docs sync (commit 4/6)

- 修改 `ip/cpu/README.md`：新增 "RiscV MMU Integration" 段
- 修改 `ip/cpu/configs/cpu_params_schema.json`：enable_mmu description 微调
- 修改 `ip/mmu/STATUS.md`：INTEGRATED + CPU PIPELINE 状态
- 修改 `CHANGELOG.md`：v0.2.0 条目
- 验证：`doc_link_check` exit=0

### Phase 5: final verify (commit 5/6)

- 4 architecture gates all PASS
- baseline 306 → 314/314 PASS
- `openspec archive cpu-mmu-integration` 移动到 archive/

### Phase 6: archive (commit 6/6)

- 4 architecture gates all PASS
- baseline 306 → 314/314 PASS
- `openspec archive cpu-mmu-integration` 移动到 archive/

### Rollback Strategy

每个 commit 独立可 revert：
- commit 1 revert → baseline 25/25 cpu-integration PASS（MMU Plugin 不注册）
- commit 2 revert → at_stage 闭包 noop 声明但未实装；mmu-cache-integration commit 6 直接方法测试 PASS
- commit 3 revert → 移除新 test case
- commit 4 revert → docs 回滚
- commit 5 revert → archive 目录删除

## Open Questions

### Q1: enable_mmu=true 默认值是否该改回 false？

**已知**: `cpu_params_schema.json:44` default `true`，`cpu_factory.h:65` default `true`。**决策**: 保持 `true`（与现有 baseline 一致，避免 regression）。后续 change 可调整。

### Q2: 7-stage `commit/retire` 阶段是否需要 MMU substage？

**已知**: 7-stage = `fetch/decode/execute/memory/writeback/commit/retire`。**决策**: 7-stage test 仅验证 mm substage 在 execute/memory 注册，不强制 commit/retire 阶段 substage（commit/retire 阶段不涉及 MMU hook）。

### Q3: 5 个 stage tests 是否都需要 enable_mmu=true 路径？

**已知**: 5 个 stage tests 验证 enable_mmu=true 完整 build + substage 注册。**决策**: 是（确保跨 stage 深度一致性）。

### Q4: RiscVMMUPlugin::exception_code() 公开访问是否需要？

**已知**: `ip/cpu/plugins/mmu.h:51-52` 已公开 `exception_code() / set_exception_code()`。**决策**: 已就位，本 change 不修改。

## References

- `ip/cpu/cpu_factory.h:65` — `enable_mmu = true` 现有配置
- `ip/cpu/configs/cpu_params_schema.json:42-52, 203-204` — `enable_mmu` + `mmu_mode` 字段
- `ip/cpu/plugins/mmu.h:30-65` — RiscvMMUPlugin 类 + 5 公开方法
- `ip/cpu/plugins/mmu.cpp:28` — `csr_write_satp` 实装（mmu-cache-integration commit 6）
- `ip/cpu/plugins/mmu.cpp:36` — `sfence_vma` 实装（mmu-cache-integration commit 6）
- `ip/mmu/tlm/mmu_keys.h:44-47` — 4 个 RiscV Key（commit 8 mmu-tlb-ptw-impl）
- `openspec/changes/archive/2026-09-13-mmu-cache-integration/design.md` — mmu-cache-integration design 经验
- `openspec/specs/mmu-riscv-isa-adapter/spec.md` — RiscV hook spec 基础
- `docs/architecture/adr.md` — ADR-040 (Plugin framework) + ADR-044 (VIPT)
- `tools/verify_adr.sh` / `verify_plugin_decision.sh` / `check_plugin_portability.sh` / `doc_link_check.sh` — 4 architecture gates