# Design — mmu-paddr-consume-and-real-memory

> **来源**: `openspec/changes/mmu-paddr-consume-and-real-memory/proposal.md`
> **本文档定位**: 技术设计文档, 解释 **HOW** 实装 (proposal 解释 **WHY**, specs 定义 **WHAT**)
> **依赖**: P0#1 `cpu-pipeline-canonical-ordering-assert` (已 archive)

## Context

当前 SoC demo 的 MMU 在 CPU 流水线中是**装饰性**的 (`v0.2.3 R3 已知限制 + v0.1.3 §11.2 风险`):
1. `IBusPlugin`/`DBusPlugin` `at_stage("fetch"/"memory", NORMAL)` 直接用 `pl::PC`/`pl::MEM_ADDR` (vaddr) 读内存, 跳过 MMU 翻译结果 `pl::PADDR`。
2. `MMUPlugin::ptw_->advance_from_stub()` 从 `pte_stub_memory_` 读 PTE (测试 mock), 不是真实物理内存 (`ip/mmu/STATUS.md:38` 显式 deferred 承诺)。
3. P0#1 锁定 `MMUPlugin::build()` 先于 `IBusPlugin::build()` 注册 (canonical ordering), 否则 stall 晚 1 cycle 失效。

**约束**:
- canonical ordering 必须在 register 阶段验证 (P0#1 已实装 `register_early_plugins()` 内 `MMU_REG_ORDER = ++PLUGIN_SEQ`)
- 必须向后兼容 (现有 47 个 `[mmu]` 测试 + 6 个 `[cpu-l1-mmu-demo]` 测试 + 40 个 `[riscv-tests]` 0 回归)
- TLM baseline 兼容 (ADR-040 v2.0 §3)
- CH_MEM 模式自动兼容 (`pl::PADDR` 是 `uint_t<64>` POD)

**Stakeholders**:
- Phase 1.5 Wave 4 P2#6 (依赖 mmu_exit hook 真生效)
- Phase 6d 6d.6 (MMU/PTW FSM) (已 ship, 本 change 集成)
- Phase 6d 6d.4 (riscv-tests ELF) (5 ELF baseline, 本 change 0 回归)

## Goals / Non-Goals

**Goals**:
- IBusPlugin `at_stage("fetch", NORMAL)` 优先用 `pl::PADDR`, fallback `pl::PC`
- DBusPlugin `at_stage("memory", NORMAL)` 同样改造 (LOAD + STORE)
- PTW `advance_from_real_memory(MemoryInterface* mem)` 替代 `advance_from_stub()`
- `MemoryInterface` 抽象 (`ip/mmu/lib/memory_interface.h`) + `PicolibcHostMemory` 继承实现
- SoC JSON `soc/cpu_l1_mmu_demo.json` 加 `mmu.memory_interface: "picolibc_host_memory"` 字段
- `[cpu-l1-mmu-demo]` 6 → 8 用例 (加 2 个 PADDR 翻译链路验证)
- ADR-049 `mmu-paddr-consumption-contract` Accepted

**Non-Goals**:
- 多周期 PTW FSM 重构 (Phase 6d 6d.6 已 ship, 本 change 集成而非重写)
- MMU TLB 算法/替换策略实现 (那是 mmu-tlb-ptw-impl scope)
- MMU 配置 JSON 化 (拆分到 `mmu-config-json-driven` 独立 change)
- Cache 配置 JSON 化 (wave4 P2#7 scope)
- CSR/exception 路由 (wave4 P2#6 scope)

## Decisions

### D1: `paddr_valid` 标志 vs 直接用 `pl::PADDR != 0`

**决策**: 引入显式 `paddr_valid` Payload key (`mmu_keys::PADDR_VALID`), 而不是隐式用 `pl::PADDR != 0`。

**理由**:
- 物理地址 0 是合法值 (例如 MMIO 寄存器), 隐式检测会误判
- `paddr_valid` 标志由 `RiscvMMUPlugin::at_stage("tlb_lookup_*", LATE)` 显式写 `true` (翻译成功) 或 `false` (miss/fault)
- IBus/DBus 读取 `paddr_valid=true` 才用 PADDR, 否则 fallback vaddr

**替代方案**:
- ❌ 用 `pl::PADDR != 0` 检测: 物理 0 地址合法, 误判
- ❌ 用单独 `pl::PC_VALID` flag: 增加 PayloadStore 复杂度, 与现有 mmu_keys 不一致

### D2: MemoryInterface 抽象 vs 直接传 PicolibcHostMemory*

**决策**: 在 `ip/mmu/lib/memory_interface.h` 定义抽象类 (`read_word` / `write_word`), `PicolibcHostMemory` 继承实现。

**理由**:
- 解耦 PTW 与具体内存实现 (未来可能接 L2Cache, DRAM 模型, mmap 等)
- lib/ 层 purity: `MemoryInterface` 纯 C++ 抽象, 零 `cf::plugin::*` 依赖
- 测试友好: Mock 实现 (`tests/mmu/test_ptw_real_memory.cpp` 可注入 mock memory)

**替代方案**:
- ❌ 直接传 `PicolibcHostMemory*`: 强耦合, 无法 mock 测试
- ❌ 用 C++20 `std::span` + 模板: 增加模板复杂度, lib/ 层不友好

### D3: MMUPlugin 构造函数接 `MemoryInterface* mem = nullptr`

**决策**: `MMUPlugin(SvMode, vector<TLBConfig>, PTWConfig, MemoryInterface* mem = nullptr, uint64_t satp_value = 0)`。

**理由**:
- 向后兼容: 默认 `nullptr` 走 `advance_from_stub()` (现有 47 测试 0 回归)
- 生产路径: `CpuFactory::register_early_plugins()` 注入 `PicolibcHostMemory*` 实例
- 测试路径: `tests/mmu/test_ptw_real_memory.cpp` 注入 mock memory

**ABI 风险 (R3)**: 加默认参数不破坏 ABI (C++ 默认参数在调用点填充), 既有 `RiscvMMUPlugin(sv_mode, levels, ptw, satp=0)` 调用 0 改动。

### D4: SoC JSON `mmu.memory_interface` 字段路径

**决策**: 字段路径 `soc/cpu_l1_mmu_demo.json::mmu.memory_interface = "picolibc_host_memory"`, 与 `mmu.config.sv_mode` 同级。

**理由**:
- 与 P1#3 `mmu.memory_interface` 同源 (proposal §6)
- `CpuFactory` 解析该字段时, 在 `components` 数组查找 `"type": "cf::cpu::PicolibcHostMemory"` 条目获取实例指针
- 与 wave4 Cache DSE 命名约定一致 (P2#7 proposal 待起草)

### D5: TDD 5-step discipline (Write failing test → Verify fail → Implement → Verify pass → Commit)

**决策**: 严格按 tasks.md §1 (TDD red) → §3-6 (Implement) → §7 (Verify pass) → §10 (Commit) 顺序。

**理由**:
- tasks.md 已是结构化 TDD 5-step (commit `b82af0f` 历史教训: 7stage superscalar `lane_counters` use-after-free 修复时 TDD 抓出)
- `[cpu]` 新测试 1.1 期望 PADDR 实际 vaddr → fail 验证"装饰性"问题确实存在
- `[mmu]` 新测试 1.2 期望真 PTE 实际 stub → fail 验证 stub 路径是当前默认

## Risks / Trade-offs

- **R1 (TLM 测试回归)**: PicolibcHostMemory 当前用 vaddr 0x80001000 直接寻址, 若 IBus 切到 PADDR 但 MMU 翻译未生效, 测试会读错地址
  → **Mitigation**: `paddr_valid` 标志显式 (D1), fallback `pl::PC`/`pl::MEM_ADDR` 路径保留

- **R2 (canonical ordering 必需)**: 若 P0#1 未落地, 本 change IBus 读 stale PADDR=0
  → **Mitigation**: P0#1 已 archive (`commit cba5e53`), runtime 断言 `check_canonical_ordering()` 在 `register_early_plugins()` 验证

- **R3 (MemoryInterface ABI 破坏)**: 抽象类加新方法可能影响现有 MMUTLMBridge 集成
  → **Mitigation**: 默认 `nullptr` 走 stub 路径 (D3), 向后兼容

- **R4 (SoC JSON 字段)**: `mmu.memory_interface` 字段是新增, 现有 soc-demo JSON 需要更新
  → **Mitigation**: 1 文件改动 (`soc/cpu_l1_mmu_demo.json`), 加 1 行, 风险可控

- **R5 (真实内存读 PTE 慢)**: PTW 2 级 walk (Sv32, Phase 6d 6d.6 已 ship 5 状态 FSM) 每级 1 cycle 内存读, 真实物理内存读延迟与 stub 不同
  → **Mitigation**: Phase 6 cycle-precision 框架配合 (P1#4 `plugin-framework-cycle-precision`), 本 change 不引入新延迟

## Migration Plan

**Phase A — failing test (tasks 1.1-1.3)**: 写新测试, 验证 fail (装饰性问题确认存在)
**Phase B — ADR + MemoryInterface (tasks 2-3)**: ADR-049 + 抽象类 + PicolibcHostMemory 改造
**Phase C — PTW real memory (tasks 4)**: `advance_from_real_memory()` 实装 + MMUPlugin 构造扩展
**Phase D — IBus/DBus PADDR consumption (tasks 5)**: 改 at_stage 闭包
**Phase E — SoC JSON (tasks 6)**: 字段 + CpuFactory 解析
**Phase F — verify pass (tasks 7)**: 所有 `[cpu]`/`[mmu]`/`[soc]`/`[riscv-tests]`/`[cpu-integration]` 0 回归
**Phase G — CI 门禁 (tasks 8)**: 3 个 verify 脚本 + doc_link_check PASS
**Phase H — 文档 (tasks 9)**: CSV + architecture.md §3 + STATUS.md 划掉 deferred
**Phase I — commit + archive (tasks 10)**: 1 个原子 commit + CHANGELOG + archive

**Rollback 策略**:
- `MMUPlugin` 构造默认 `mem=nullptr` 走 stub → 删除 `mmu.memory_interface` SoC JSON 字段即可回退
- IBus/DBus `paddr_valid` 标志由 MMU 写 → fallback `pl::PC` 路径始终保留
- 风险等级: 低 (向后兼容, 默认 stub 路径不变)

## Open Questions

- **Q1**: `mmu_keys::PADDR_VALID` 是否新增 Payload key, 还是复用 `mmu_keys::EXCEPTION_CODE`? 当前 stub 写 EXCEPTION_CODE=0 表示 success, 但语义重叠
  → **当前决策**: 新增 `mmu_keys::PADDR_VALID` (避免语义混淆, 与 `paddr_valid` 标志同名)
  → **待 P1#3 实装时定**

- **Q2**: PTW 真实内存读延迟建模 (R5): 是用 CppTLM 事务延迟 (1 cycle), 还是 Phase 6d 6d.6 FSM 5 状态 (IDLE/L0_WAIT/L1_WAIT/DONE/FAULT)
  → **当前决策**: 沿用 6d.6 FSM 5 状态, 真实内存读延迟由 `MemoryInterface::read_word()` 返回 cycle 数 (默认 1)
  → **待 P1#4 cycle-precision 框架落地后细化**

- **Q3**: `PicolibcHostMemory` 当前 `read_word`/`write_word` 签名是否与 `MemoryInterface` 1:1 匹配?
  → **假设**: 是 (`ip/cpu/picolibc_host_memory.h` 已声明), 待实装时验证
  → **风险**: 低 (PicolibcHostMemory 已有 ~30 LOC 接口)