# RISC-V CPU SoC 系统架构

> **本文件定位**：CPU SoC = RISC-V CPU + MMU + L1 Cache + Memory 的系统级连线与数据流。  
> **不覆盖**：各 IP 内部微架构（见 `ip/{cpu,mmu,cache}/docs/architecture.md`）、Plugin 范式定义（见 `docs/architecture/plugin-framework.md`）。  
> **更新规则**：任一 IP 阶段变更后，同步更新本文档 §3 状态表 + §4 集成缺口。

| 字段 | 值 |
|------|-----|
| 状态 | 🟡 Phase 1.3（CPU 核心 Plugin 套件完成 + L1Cache Plugin 完成 + MMU 骨架） |
| 下一里程碑 | `mmu-tlb-ptw-impl` — TLB/PTW 算法实装 + MMU↔Cache 集成 |
| 最后更新 | 2026-07-01 |

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
ptw_l0 ──▶ 读 L0 PTE (stub: pte=0, mmu-tlb-ptw-impl 实装内存读)
    ▼
ptw_l1 ──▶ 读 L1 PTE (stub)
    ▼
ptw_l2 ──▶ 读 L2 leaf PTE → 确定 paddr + perms
    │     写 pl::PADDR, 清 PTW_ACTIVE
    │     multi_tlb_->refill_from_ptw() 回填多级 TLB
    ▼
下游 (IBus/DBus) 重新查 TLB (此时 L0 hit)
```

**当前限制**：`pb.run()` 单次遍历所有 `at_stage` 回调（无 cycle 精度）。PTW 三级是**逻辑阶段**，不是真实 3-cycle pipeline。Phase 6 框架升级后变为周期精确调度。

---

## 2. SoC 级通信接口

### 2.1 Payload Key（CPU 内部跨 Plugin 通信）

CPU 内部 11 个 Plugin 通过 `pl::*` (Payload Key) 共享数据。MMU 通过以下 Key 插入翻译流程：

| Key | 类型 | 生产者 | 消费者 | 说明 |
|-----|------|--------|--------|------|
| `pl::PC` | `uint64_t` | IBusPlugin | MMUPlugin | 虚 PC（fetch 阶段写） |
| `pl::PADDR` | `uint64_t` | MMUPlugin | IBusPlugin/DBusPlugin | 物理地址（TLB 命中后写） |
| `pl::MEM_ADDR` | `uint64_t` | DBusPlugin | MMUPlugin | 虚地址（load/store 阶段写） |
| `pl::PTW_ACTIVE` | `bool` | MMUPlugin | CtrlLink | PTW 进行中 stall 下游 |
| `pl::PTW_FAULT` | `uint8_t` | MMUPlugin | ExceptionPlugin | page fault → mcause 12/13/15 |

见 `ip/mmu/tlm/mmu_keys.h`（10 个 MMU 专属 Key）+ `ip/mmu/docs/integration.md` §1。

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
| **CPU Core** | 🟡 M4/M5（11 Plugin 套件完整） | RV64IMACZicsrZifencei decode/execute/load-store/branch/csr | MMU/FPU/Exception Plugin 未注册到 CpuFactory（ADR-042）；5 个 RISC-V 仿真测试预存失败 | ✅ CpuFactory 可用 |
| **L1 Cache** | 🟡 Phase 1.3（16KB direct-mapped） | lookup + refill 两阶段；Bridge + Adapter e2e | Phase 0 offset=4 简化；VIPT 需升 4-way（ADR-044）；无 L2 | ⚠️ 仅 TLM 验证 |
| **MMU** | 🟡 骨架（mmu-ip-skeleton） | TLB 模板 + MultiLevelTLB + 4 策略 + PTW stub | TLB/PTW 算法实装、satp/sfence hook、VPN 切分 3 处 bug（见附录） | ❌ 未集成 |
| **Memory** | 🔴 规划中 | — | 全部 | ❌ |
| **Interconnect** | 🔴 规划中 | — | 全部 | ❌ |
| **Peripheral** | 🔴 规划中 | — | PLIC/CLINT/UART/Timer（Phase 3+） | ❌ |

---

## 4. 集成缺口（跨 IP）

### 4.1 阻塞项

| # | 缺口 | 涉及 IP | 阻塞目标 | 状态 |
|---|------|---------|----------|:---:|
| 1 | MMU TLB/PTW 算法实装 | mmu | 虚实地址翻译功能 | `mmu-tlb-ptw-impl` 待 |
| 2 | `RiscvMMUPlugin` satp/sfence.vma hook | mmu ↔ cpu | CSR 写入翻译到 TLB flush | 同上 |
| 3 | MMU → L1Cache 集成（MMU 写 `pl::PADDR` → Cache 读 `pl::PADDR` 查 tag） | mmu ↔ cache | 虚实翻译生效 | 同上 + ADR-044 |
| 4 | PTW 内存读（替换 `pte=0` stub） | mmu | Page table walk | 同上 |

### 4.2 设计已锁但未实装

| # | 缺口 | 涉及 IP | 锁定文档 | 状态 |
|---|------|---------|----------|:---:|
| A | L1Cache VIPT 升级（256×1 → 64×4） | cache | ADR-044 | Phase 1.5 |
| B | CtrlLink halt_when PTW stall | mmu | `MMUPlugin.cpp:130` | `mmu-tlb-ptw-impl` |
| C | MMUTLMBridge（cpptlm 适配） | mmu ↔ CppTLM | 推迟 | Phase 1.5 以后 |
| D | SoC JSON 拓扑（CPU+MMU+Cache+Mem） | soc | 推迟 | Phase 2+ |

### 4.3 RISC-V 规范合规缺口（骨架 bug）

| Bug | 文件 | 修复方案 |
|-----|------|---------|
| VPN 提取错（`tag=vpn>>ASID_BITS`） | `ip/mmu/lib/tlb.h:74` | 按 Sv39 VPN[2]/VPN[1]/VPN[0] 九位三字段切分 |
| PTE.PPN 字段提取错（`raw>>10` 一刀切） | `ip/mmu/lib/ptw.cpp:91` | 按 Sv39/Sv48 PPN[0:2] (9+9+26 bits) 分字段拼合，大页判定 |
| PTW 下一级地址错（`pte_ppn<<12` stub） | `ip/mmu/lib/ptw.cpp:68` | 按 vaddr[level_idx:9*(level+1)+12] 索引 + PPN << 12 |
| 无 megapage/gigapage 支持 | `ip/mmu/lib/ptw.cpp:52` | leaf PTE 检测（V/R/W/X 组合）+ PPN[0] 和 PPN[1] 在大页时位置改变 |

---

## 5. 建议后续计划

### Phase 1.4: MMU 实装（`mmu-tlb-ptw-impl`）

**优先级：最高。** 阻塞所有虚实地址相关功能。

- [ ] 修复 3 处 RISC-V spec 合规 bug（§4.3）
- [ ] 实装 TLB lookup/insert 算法（VPN 精确化 + PPN 拼合）
- [ ] 实装 PTW Sv32/Sv39/Sv48 解码（含 leaf PTE + megapage/gigapage）
- [ ] 实装 `RiscvMMUPlugin` 4 个 hook（satp/sfence/mstatus/fault mapping）
- [ ] PTW 内存读（替换 `pte = 0` stub）
- [ ] CtrlLink halt_when PTW stall 接线
- [ ] MMU 5 个测试恢复编译 + 通过

### Phase 1.5: L1Cache VIPT 升级 + MMU↔Cache 集成

- [ ] L1Cache 从 256×1 升到 64×4（ADR-044）
- [ ] `extract_idx_from_vaddr` + `extract_idx_from_paddr` 双 helper
- [ ] L1Cache 集成测试：MMU 翻译 → Cache lookup → hit/miss → refill
- [ ] 性能基线：fetch hit 路径 2-cycle（TLB L0 1-cycle + Cache 1-cycle）

### Phase 2: SoC 完整装配

- [ ] L2 Cache PIPT（256KB-1MB，8-16 way）
- [ ] Memory IP（SRAM/DRAM 模型）
- [ ] Interconnect IP（交叉开关/NoC）
- [ ] SoC JSON 拓扑 `soc/cpu/riscv_virt.json`（CPU + MMU + L1I + L1D + L2 + Memory + CLINT/PLIC）
- [ ] Bare-metal 固件启动（`firmware.elf` → tohost 退出）

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
