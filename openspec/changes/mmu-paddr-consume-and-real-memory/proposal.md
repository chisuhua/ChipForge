---
initiative: wave3-mmu-real-memory-and-cycle
priority: P1
version_target: v0.8.0
depends_on:
  - cpu-pipeline-canonical-ordering-assert
---

# mmu-paddr-consume-and-real-memory — 让 MMU 在 CPU 流水线中真起作用

## Why

v0.2.3 R3 已知限制 + v0.1.3 §11.2 风险：当前 SoC demo 的 MMU 在 CPU 流水线中是**装饰性**的：

1. **IBusPlugin/DBusPlugin 不消费 `pl::PADDR`**: `at_stage("fetch", NORMAL)` 直接用 `pl::PC` (vaddr) 读内存, 跳过 MMU 翻译结果。MMU 翻译完 `pl::PADDR` 后无下游消费者, 翻译等于白做。
2. **PTW 走 stub memory**: `MMUPlugin::ptw_->advance_from_stub()` 从 `pte_stub_memory_` 读 PTE (测试友好接口), 不是真实物理内存。`ip/mmu/STATUS.md:38` 显式 deferred 承诺, Wave 3 实装。
3. **canonical ordering 隐式契约**: P0#1 锁定 `MMUPlugin::build()` 先于 `IBusPlugin::build()` 注册, 否则 stall 晚 1 cycle 失效 —— 本 change 依赖 P0#1 落地。

后果：当前"端到端 CPU+MMU+L1+Memory demo"实为**"MMU 翻译 → 丢结果 → CPU 用 vaddr 走 stub memory"**，"真跑"名不副实。Wave 4 P2#6 phase-1.5-wave-4 (CSR/exception/mispredict) 依赖 mmu_exit hook 真生效，本 change 是其前置。

## What Changes

### 1. IBusPlugin 真消费 `pl::PADDR`

- **位置**: `ip/cpu/plugins/ibus.h` `at_stage("fetch", NORMAL)` 闭包
- **现状**: `mem_->read_word(pl::PC)` —— 直接用 vaddr
- **目标**: 优先用 `pl::PADDR`（MMU 翻译结果），fallback 用 `pl::PC`（无 MMU 模式）
  - `if (mmu_enabled && paddr_valid) { mem_->read_word(pl::PADDR); } else { mem_->read_word(pl::PC); }`
  - `mmu_enabled` 由 `RiscvMMUPlugin` 注入（运行时 flag，非编译时）
  - `paddr_valid` 由 MMU 写 `pl::PADDR != 0` 或显式 valid bit 标志
- **依赖**: P0#1 canonical-ordering 落地（否则 stall 时序错，IBus 读 stale PADDR=0）

### 2. DBusPlugin 真消费 `pl::PADDR`

- **位置**: `ip/cpu/plugins/dbus.h` `at_stage("memory", NORMAL)` 闭包
- **现状**: STORE/LOAD 用 `pl::MEM_ADDR` (vaddr) 操作内存
- **目标**: 同 IBusPlugin, 优先用 `pl::PADDR`, fallback `pl::MEM_ADDR`
- **依赖**: P0#1 canonical-ordering 落地

### 3. PTW 真实内存读 (`advance_from_real_memory`)

- **位置**: `ip/mmu/lib/ptw.cpp::advance_from_stub()` 旁新增 `advance_from_real_memory()`
- **现状**: `pte_stub_memory_` 是测试 mock, 写死 PTE 值
- **目标**: 接收 `MemoryInterface*` 指针（生产 SoC JSON 注入）, 真实读写
  ```cpp
  class MemoryInterface {
   public:
    virtual uint32_t read_word(uint64_t addr) = 0;
    virtual void write_word(uint64_t addr, uint32_t val) = 0;
    virtual ~MemoryInterface() = default;
  };
  ```
- `MMUPlugin` 构造函数接 `MemoryInterface* mem`（默认 nullptr 走 stub, 兼容现有测试）
- `MMUPlugin::ptw_->advance_from_real_memory()` 替代 `advance_from_stub()`
- **SoC JSON 集成**: `soc/cpu_l1_mmu_demo.json` 增加 `mmu.memory_interface: "picolibc_host_memory"` 字段（与现有 `cpu.memory` 同源）

### 4. `[cpu-l1-mmu-demo]` 测试升级

- **现状**: 6 用例仅测结构 (JSON 拓扑 + 5 ELF tohost=1), MMU 翻译结果不验证
- **目标**: 加 1-2 用例验证真 PADDR 翻译链路
  - 新测试 `test_mmu_paddr_propagation.cpp`: 写入 vaddr 0x80001000 → 期望 PADDR = 0x1000 (page offset 不变, base 翻译)
  - 新测试 `test_ptw_real_memory.cpp`: PTE 在真实物理地址 (非 stub), PTW walk 走 MMU
- CSV 升级: `[cpu-l1-mmu-demo]` 6 → 8 用例

### 5. ADR 新增

- **新增** `docs/architecture/adr/ADR-049-mmu-paddr-consumption-contract.md` (~220 LOC):
  - §Context: 解释装饰性问题
  - §Decision: PADDR-first consumption pattern + `MemoryInterface` 抽象
  - §Consequences: 与 ADR-044 (VIPT) + ADR-045 (CtrlLink stall) 集成
- **更新** `docs/architecture/adr.md` 表项 + 章节锚点

### 6. CSV 基线更新

- `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` 保留 (40/40 PASS 不变)
- 新增 `soc/cpu/docs/dse/mmu-paddr-propagation-matrix.csv` (真 PADDR 翻译矩阵)

## Capabilities

### Modified Capabilities

- `cpu-real-fetch-and-memory`: 新增 "IBusPlugin MUST prefer `pl::PADDR` over `pl::PC` when MMU is enabled and `paddr_valid`" requirement
- `cpu-real-fetch-and-memory`: 同样规则应用于 DBusPlugin 的 LOAD/STORE 路径
- `mmu-tlb-coherence-protocol`: 新增 "PTW SHALL read PTE from real memory via `MemoryInterface` (not stub) when SoC JSON specifies `mmu.memory_interface`" requirement

### New Capabilities

- `mmu-paddr-consumption`: 定义 IBusPlugin/DBusPlugin 真消费 PADDR 的 contract, 含 canonical ordering 依赖

## Impact

**真实工作量估算** (Oracle #4 修订, 跨 IP + SoC JSON + 测试 + ADR):
- **修改代码**:
  - `ip/cpu/plugins/ibus.h` (+15 LOC: PADDR 优先读取 + `paddr_valid` 标志)
  - `ip/cpu/plugins/dbus.h` (+25 LOC: PADDR 优先 + LOAD/STORE 对称)
  - `ip/mmu/lib/ptw.h/.cpp` (+60 LOC: `advance_from_real_memory()` 多 cycle 延迟 vs TLM stub 同步)
  - `ip/mmu/tlm/MMUPlugin.h/.cpp` (+40 LOC: `MemoryInterface* mem_` 注入 + 切换逻辑)
  - `ip/mmu/lib/memory_interface.h` (新建, ~50 LOC: 抽象类 + 字节序/对齐注释)
  - `ip/cpu/picolibc_host_memory.h` (修改: 继承 `MemoryInterface`, 加 override 声明)
- **新增测试**:
  - `tests/cpu/test_mmu_paddr_propagation.cpp` (~120 LOC)
  - `tests/mmu/test_ptw_real_memory.cpp` (~120 LOC: Sv32 2-level walk + PTW state 兼容)
  - `tests/soc/test_cpu_l1_mmu_demo.cpp` 升级 (+100 LOC, 6 → 8 用例)
- **SoC JSON + 工厂方法**:
  - `soc/cpu_l1_mmu_demo.json` (+10 字段: `mmu.memory_interface` + `paddr_valid` flag)
  - `CpuFactory` 解析新字段 + 注入 MemoryInterface* (~30 LOC)
- **文档**:
  - `docs/architecture/adr/ADR-049-*.md` (新建, ~250 LOC)
  - `docs/architecture/adr.md` (+5 LOC)
  - `soc/cpu/docs/dse/mmu-paddr-propagation-matrix.csv` (新建)
  - `soc/cpu/docs/architecture.md` §3 MMU 状态更新（"装饰性" → "真集成"）

**估算总代码变更**: ~700 LOC (比原始 ~130 LOC 多 5 倍, 因跨 5 个文件 + 3 个新测试 + ADR + SoC schema)

## Acceptance

- [ ] IBusPlugin 真消费 PADDR (新测试 `test_mmu_paddr_propagation` PASS)
- [ ] DBusPlugin 真消费 PADDR (新测试 PASS)
- [ ] PTW `advance_from_real_memory()` 实装（`test_ptw_real_memory` PASS）
- [ ] `[cpu-l1-mmu-demo]` 8/8 PASS（升级 2 用例）
- [ ] `[mmu]` 47 + 2 新测试 = 49/49 PASS
- [ ] `[riscv-tests]` 40/40 PASS 不回归
- [ ] `[cpu-integration]` 4/4 PASS 不回归
- [ ] 3 门禁全 PASS
- [ ] ADR-049 Accepted + adr.md 注册
- [ ] CSV `mmu-paddr-propagation-matrix.csv` 落盘
- [ ] `soc/cpu/docs/architecture.md` §3 MMU 状态更新
- [ ] TLM baseline 0 回归
- [ ] CHANGELOG v0.8.0 段本 change 条目
- [ ] `openspec archive mmu-paddr-consume-and-real-memory -y`

## Risk

- **R1 (TLM 测试回归)**: PicolibcHostMemory 当前用 vaddr 0x80001000 直接寻址, 若 IBus 切到 PADDR 但 MMU 翻译未生效, 测试会读错地址 → 必须 `paddr_valid` 标志显式, fallback 路径保留 PicolibcHostMemory 当前用 vaddr 0x80001000 直接寻址, 若 IBus 切到 PADDR 但 MMU 翻译未生效, 测试会读错地址 → 必须 `paddr_valid` 标志显式, fallback 路径保留
- **R2 (canonical ordering 必需)**: 若 P0#1 未落地, 本 change IBus 读 stale PADDR=0 → 强依赖 P0#1 archive
- **R3 (MemoryInterface ABI 破坏)**: 抽象类加新方法可能影响现有 MMUTLMBridge 集成 → 默认 `nullptr` 走 stub 路径, 向后兼容
- **R4 (SoC JSON 字段)**: `mmu.memory_interface` 字段是新增, 现有 soc-demo JSON 需要更新 → 1 文件改动, 风险可控
- **R5 (真实内存读 PTE 慢)**: PTW 2 级 walk (Sv32, Phase 6d 6d.6 已 ship 5 状态 FSM) 每级 1 cycle 内存读, 真实物理内存读延迟与 stub 不同 → Phase 6 cycle-precision 框架配合 (P1#4)
