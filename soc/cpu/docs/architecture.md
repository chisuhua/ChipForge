# RISC-V CPU SoC 系统架构

> **本文件定位**：CPU SoC = RISC-V CPU + MMU + L1 Cache + Memory 的系统级连线与数据流。  
> **不覆盖**：各 IP 内部微架构（见 `ip/{cpu,mmu,cache}/docs/architecture.md`）、Plugin 范式定义（见 `docs/architecture/plugin-framework.md`）。  
> **更新规则**：任一 IP 阶段变更后，同步更新本文档 §3 状态表 + §4 集成缺口。

| 字段 | 值 |
|------|-----|
| 状态 | ✅ Phase 1 核心完成（CPU Pipeline + MMU + L1Cache VIPT 集成，2026-09-14） |
| 下一里程碑 | `soc-cpu-l1-mmu-demo` — CPU+MMU+L1+Memory 完整 SoC + `cache-phase1.5-4way` VIPT 正式化 |
| 最后更新 | 2026-09-14 |

---

## 1. 系统数据流

### 1.1 取指路径（Fetch → MMU → L1I Cache → IBus）

```
┌────────────────────────────────────────────────────────────────────────┐
│  fetch stage (取指)                                                     │
│                                                                         │
│  ┌──────────┐     ┌───────────────────┐     ┌─────────────────┐       │
│  │IBusPlugin│────▶│MMUPlugin::at_stage │────▶│  IBusPlugin 读  │       │
│  │ 写 pl::PC│     │("tlb_lookup_ifetch"│     │  pl::PADDR →    │       │
│  │ (vaddr)  │     │ vaddr→paddr 翻译)  │     │  L1I Cache 命中  │       │
│  └──────────┘     └───────┬───────────┘     └────────┬────────┘       │
│                           │                           │                │
│                    ┌──────▼──────┐              ┌─────▼─────────────┐  │
│                    │ MultiLevelTLB│             │ L1CachePlugin     │  │
│                    │ ┌──────────┐│             │ lookup("fetch",…) │  │
│                    │ │ L0 8/8   ││             │                   │  │
│                    │ │ ~1 cycle ││  VIPT 并行   │ 从 pl::PADDR tag  │  │
│                    │ └──────────┘│◄────────────│ 比对 (物理 tag)   │  │
│                    │ ┌──────────┐│             │ 从 vPC 索引 (VIPT)│  │
│                    │ │ L1 64/4  ││             └───────────────────┘  │
│                    │ └──────────┘│                                     │
│                    └──────┬──────┘                                     │
│                           │ miss                                       │
│                    ┌──────▼──────┐                                     │
│                    │ PTW (Sv39)  │→ L2/memory                          │
│                    │ ptw_l0/l1/l2│                                     │
│                    └─────────────┘                                     │
└────────────────────────────────────────────────────────────────────────┘
```

**关键路径**：vPC → TLB hit (1 cycle) → L1I hit (1 cycle) = **2 cycle fetch hit**。  
**未实装**：TLB hit 和 Cache lookup 的真正并行（Phase 0 `pb.run()` 串行回调限制，Phase 6 框架升级解耦）。

### 1.2 访存路径（Memory → MMU → L1D Cache → DBus）

```
┌──────────────────────────────────────────────────────────────────────┐
│  memory stage (load/store)                                            │
│                                                                       │
│  ┌───────────┐     ┌──────────────────────┐     ┌──────────────┐    │
│  │DBusPlugin │────▶│MMUPlugin::at_stage    │────▶│ DBusPlugin 读│    │
│  │ 写 pl::   │     │("tlb_lookup_loadstore"│     │ pl::PADDR →  │    │
│  │ MEM_ADDR  │     │ vaddr→paddr 翻译)     │     │ L1D Cache    │    │
│  └───────────┘     └──────────────────────┘     └──────────────┘    │
└──────────────────────────────────────────────────────────────────────┘
```

**与取指路径同构**，但使用 `MEM_ADDR` 作为 vaddr 源 + `tlb_lookup_loadstore` 子阶段。

### 1.3 MMU miss → PTW 路径

```
MMUPlugin::tlb_lookup_ifetch (miss)
    │ 写 PTW_ACTIVE=1, PTW_VADDR=vaddr, PTW_ASID=asid
    │ 启动 PTW::start_walk()
    ▼
ptw_l0 ──▶ 读 L0 PTE (ptw-walk-bridge-fix 实装: advance_from_stub() 自动读 stub memory)
    ▼
ptw_l1 ──▶ 读 L1 PTE
    ▼
ptw_l2 ──▶ 读 L2 leaf PTE → 确定 paddr + perms
    │     写 pl::PADDR, 清 PTW_ACTIVE
    │     (PTW walk 完成回调写 BOTH pl::PADDR + pl::MMU_VADDR — 防 stale vaddr)
    ▼
下游 (IBus/DBus) 重新查 TLB (此时 L0 hit)
```

**当前实现**：`pb.run()` 单次遍历所有 `at_stage` 回调（无 cycle 精度）。PTW 三级是**逻辑阶段**，单 cycle 串行走完（`at_stage("ptw_l0/l1/l2")` 均调 `advance_from_stub()`）。Phase 6 框架升级后变为周期精确调度。stub memory (`pte_stub_memory_`) 是测试友好接口，真实内存读推迟到 `soc-cpu-l1-mmu-demo` (memory 模型接入)。

---

## 2. SoC 级通信接口

### 2.1 Payload Key（CPU 内部跨 Plugin 通信）

CPU 内部 12 个 Plugin（11 套件 + `enable_mmu=true` 时的 RiscvMMUPlugin）通过 `pl::*` (Payload Key) 共享数据。MMU 通过以下 Key 插入翻译流程：

| Key | 类型 | 生产者 | 消费者 | 说明 |
|-----|------|--------|--------|------|
| `pl::PC` | `uint64_t` | IBusPlugin | MMUPlugin | 虚 PC（fetch 阶段写） |
| `pl::PADDR` | `uint64_t` | MMUPlugin | IBusPlugin/DBusPlugin | 物理地址（TLB 命中或 PTW 完成后写） |
| `pl::MMU_VADDR` | `uint64_t` | MMUPlugin | L1CachePlugin | VIPT 索引源（ADR-044 §3.2，TLB hit + PTW 完成都写） |
| `pl::MEM_ADDR` | `uint64_t` | DBusPlugin | MMUPlugin | 虚地址（load/store 阶段写） |
| `pl::PTW_ACTIVE` | `bool` | MMUPlugin | CtrlLink | PTW 进行中 stall 下游 |
| `pl::PTW_FAULT` | `uint8_t` | MMUPlugin | ExceptionPlugin | page fault → mcause 12/13/15 |
| `pl::SAT` | `uint64_t` | CPU execute | RiscvMMUPlugin | satp CSR 写入拦截（cpu-mmu-integration, `ip/cpu/tlm/cpu_keys.h`） |
| `pl::SFENCE_VADDR` | `uint64_t` | CPU execute | RiscvMMUPlugin | SFENCE.VMA rs1（cpu-mmu-integration） |
| `pl::SFENCE_ASID` | `uint16_t` | CPU execute | RiscvMMUPlugin | SFENCE.VMA rs2（cpu-mmu-integration） |
| `pl::MMU_EXCEPTION` | `uint8_t` | RiscvMMUPlugin | CPU exception path | exception 12/13/15 路由（cpu-mmu-integration `at_stage("mmu_exit")`） |

见 `ip/mmu/tlm/mmu_keys.h`（14 个 MMU 专属 Key）+ `ip/cpu/tlm/cpu_keys.h`（CPU→MMU IPC 4 Key，cpu-mmu-integration 新增）+ `ip/mmu/docs/integration.md` §1。

### 2.2 Bundle（IP 间硬件级通信）

MMU 不直接暴露 Bundle — 翻译结果通过 `pl::PADDR` Payload Key 传递。L1Cache 接收 `CacheReq(address=paddr)` 进行查找。

| Bundle | 方向 | 说明 |
|--------|------|------|
| `CacheReq` | CPU → L1Cache | 含物理地址（MMU 翻译后）、读/写标记、id |
| `CacheResp` | L1Cache → CPU | 含数据、hit/miss、error |
| `MemReq` | L1Cache → MainMemory | L1 miss → 下级存储请求 |
| `MemResp` | MainMemory → L1Cache | 下级存储响应（cache line fill） |

见 `bundles/mem_bundles.h`（6 个 Bundle）+ `bundles/tlb_bundles_extension.h`（TlbReq/TlbResp，Phase 1.5+）。

---

## 3. IP 集成状态（SoC 视角）

| IP | 状态 | 核心能力 | 关键缺口 | 集成到 SoC？ |
|----|------|---------|----------|:-----------:|
| **CPU Core** | ✅ Phase 1 完成（11 Plugin 套件 + MMU 注册） | RV64IMACZicsrZifencei decode/execute/load-store/branch/csr + `enable_mmu=true` 时 RiscvMMUPlugin 条件注册 + 3 substage（csr_write_satp/sfence_vma/mmu_exit） | FPU Plugin 未注册；5 个 RISC-V 仿真测试预存失败（工具链） | ✅ CpuFactory 可用 |
| **L1 Cache** | ✅ Phase 1.3 + VIPT（mmu-cache-integration） | lookup + refill 两阶段；Bridge + Adapter e2e；VIPT 索引消费（`pl::MMU_VADDR`）+ PIPT fallback | 16KB direct-mapped 非 4-way（ADR-044 §2.5 待 `cache-phase1.5-4way`）；无 L2 | ⚠️ 仅 TLM 验证 |
| **MMU** | ✅ INTEGRATED + CPU PIPELINE（2026-09-14） | TLB lookup/insert/invalidate + PTW Sv39 walk 端到端 + VIPT 双写 + RiscvMMUPlugin hook + MMUTLMBridge/Adapter 真实工作 | Sv32/Sv48 PTW 解码（`mmu-sv32-sv48-ext`）；PTW 真实内存读（stub memory 是测试接口）；CtrlLink stall 框架（`plugin-framework-stall`） | ✅ soc/mmu_minimal.json |
| **Memory** | 🔴 规划中 | — | 真实 SRAM/DRAM 模型（cpptlm MemoryTLM 暂时服务 SoC JSON） | ❌ |
| **Interconnect** | 🔴 规划中 | — | 全部 | ❌ |
| **Peripheral** | 🔴 规划中 | — | PLIC/CLINT/UART/Timer（Phase 3+） | ❌ |

---

## 4. 集成缺口（跨 IP）

### 4.1 已解决（2026-09 归档 4 个 change）

| # | 缺口 | 涉及 IP | 解决 change | 状态 |
|---|------|---------|------------|:---:|
| 1 | MMU TLB/PTW 算法实装 | mmu | `mmu-tlb-ptw-impl` (09-12) | ✅ |
| 2 | `RiscvMMUPlugin` satp/sfence.vma hook | mmu ↔ cpu | `mmu-tlb-ptw-impl` + `cpu-mmu-integration` (09-14) | ✅ |
| 3 | MMU → L1Cache 集成（VIPT 数据流双端） | mmu ↔ cache | `mmu-cache-integration` (09-13) | ✅ |
| 4 | PTW at_stage 接线（walk 实际走通） | mmu | `ptw-walk-bridge-fix` (09-14) | ✅ |
| 5 | MMUTLMBridge/Adapter 真实 issue_request/read_response | mmu ↔ CppTLM | `ptw-walk-bridge-fix` + `mmu-cache-integration` | ✅ |
| 6 | RiscvMMUPlugin 注册到 CpuFactory + 3 substage | cpu | `cpu-mmu-integration` (09-14) | ✅ |
| 7 | CPU→MMU IPC Payload Key（SAT/SFENCE/MMU_EXCEPTION） | cpu ↔ mmu | `cpu-mmu-integration` | ✅ |

### 4.2 剩余缺口

| # | 缺口 | 涉及 IP | 阻塞目标 | 状态 |
|---|------|---------|----------|:---:|
| A | L1Cache VIPT 升级（256×1 → 64×4） | cache | VIPT 正式安全（ADR-044 §2.5） | `cache-phase1.5-4way` 待 |
| B | CtrlLink halt_when PTW stall（框架消费 `should_halt`） | mmu ↔ framework | PTW_ACTIVE RETRY workaround 替换 | `plugin-framework-stall` 待 |
| C | Sv32/Sv48 PTW 解码 | mmu | 多 ISA 支持（当前仅 Sv39） | `mmu-sv32-sv48-ext` 待 |
| D | PTW 真实内存读（替换 stub memory） | mmu ↔ memory | 生产 page table walk | `soc-cpu-l1-mmu-demo` 待 |
| E | CPU+MMU+L1+Memory 完整 SoC JSON demo | soc | 真 RISC-V 程序 tohost 退出 | `soc-cpu-l1-mmu-demo` 待 |
| F | 真实 Memory IP（SRAM/DRAM 模型） | memory | 替换 cpptlm MemoryTLM | Phase 2 待 |
| G | L2 Cache PIPT + Interconnect + Peripheral | cache/interconn/periph | SoC 完整装配 | Phase 2 待 |
| H | DSE 配置扫描（12-case Pareto） | cache | 参数空间验证 | `cache-dse-sweep` 待 |

### 4.3 RISC-V 规范合规缺口（残留）

| Bug | 文件 | 状态 |
|-----|------|------|
| Sv32/Sv48 PTE 解码（当前仅 Sv39 `decode_pte` 完整） | `ip/mmu/lib/ptw.cpp:100` | `mmu-sv32-sv48-ext` 待 |
| megapage/gigapage 大页支持（leaf PTE 检测 + PPN[0]/PPN[1] 大页位移） | `ip/mmu/lib/ptw.cpp:69` | `mmu-sv32-sv48-ext` 待 |

---

## 5. 建议后续计划

### 里程碑 1: SoC 完整 demo（`soc-cpu-l1-mmu-demo`）— 最高优先级

**解锁**：真 RISC-V 程序跑通（CPU → MMU 翻译 → L1Cache → Memory → tohost 退出）。cpu-mmu-integration 已把 RiscvMMUPlugin 注册进 CPU pipeline，条件成熟。

- [ ] 重建 `soc/cpu/riscv_virt.json`（CPU + MMU + L1Cache + Memory 4 模块全链）
- [ ] PTW 真实内存读（替换 stub memory，Memory IP 或 cpptlm MemoryTLM 对接）
- [ ] 最小 ELF 加载 → tohost 退出验证（替换当前 5 个预存失败的仿真测试）
- [ ] 默认 `enable_mmu=false` 裸机启动路径（PIPT direct）

### 里程碑 2: VIPT 正式化（`cache-phase1.5-4way`）

- [ ] L1Cache 256×1 direct-mapped → 64×4 set-associative（ADR-044 §2.5 正式 VIPT 安全）
- [ ] `kIdxBits=6` + `kOffsetBits=6`（64B line, Phase 0 offset=4 简化移除）
- [ ] LRU 4-way 替换策略接入（policy 层已有）
- [ ] 21 baseline + 3 MMU integration tests 零回归（PIPT fallback 保持）

### 里程碑 3: Framework stall 补全（`plugin-framework-stall`）

- [ ] `PipeBuilder::run()` 消费 `CtrlLink::should_halt()`（框架级，任何 Plugin 可用）
- [ ] PTW busy → `halt_when` 注册（替换 PTW_ACTIVE RETRY workaround）
- [ ] 审计现有 317 tests 对"非 stall"行为的依赖，零回归

### 里程碑 4: ISA 扩展 + DSE

- [ ] `mmu-sv32-sv48-ext`：Sv32/Sv48 PTW 解码 + megapage/gigapage 大页（§4.3 残留）
- [ ] `cache-dse-sweep`：12-case Pareto DSE 配置扫描（4-way 落地后）

### Phase 2: SoC 完整装配（Bare-metal 测试套件前置）

- [ ] Memory IP（真实 SRAM/DRAM 模型，替换 cpptlm MemoryTLM）
- [ ] Interconnect IP（交叉开关/NoC）
- [ ] L2 Cache PIPT（256KB-1MB，8-16 way）
- [ ] SoC JSON 拓扑 `soc/cpu/riscv_virt.json`（+CLINT/PLIC）
- [ ] Bare-metal 固件启动（`firmware.elf` → tohost 退出）
- [ ] riscv-tests RV64GC 集成（见 [roadmap/phase-2-baremetal.md](roadmap/phase-2-baremetal.md)）

---

## 6. 参考文档

| 文档 | 内容 |
|------|------|
| `ip/mmu/docs/architecture.md` | MMU 微架构（多级 TLB + PTW 逻辑阶段图） |
| `ip/mmu/docs/integration.md` | MMU Plugin 与 CPU 集成契约 |
| `ip/cache/docs/adr/ADR-044-*.md` | L1Cache VIPT 决策 |
| `ip/cpu/README.md` | CPU IP 总览 + Plugin 套件清单 |
| `ip/cpu/docs/multi_isa_architecture.md` | 多 ISA + PipeBuilder 架构 |
| `docs/architecture/plugin-framework.md` | Plugin 框架完整设计 |
| `docs/architecture/adr.md` | 全局 ADR 注册表 |
| `bundles/README.md` | Bundle 定义 + 字段位宽依据 |
| `openspec/specs/mmu-tlb-coherence-protocol/spec.md` | MultiLevelTLB shadow fill 协议 |
| `openspec/changes/archive/2026-06-29-mmu-ip-skeleton/` | 本 change 完整 artifacts |
