# ADR-049: MMU PADDR Consumption Contract + MemoryInterface Abstraction（MMU PADDR 真消费契约 + 内存抽象）

> **Status**: ✅ Accepted (v0.8.0, 2026-09-25)
> **类别**: Plugin / IP 架构 / 数据通路
> **关联 ADR**: ADR-040 v2.0 (TLM→HDL 移植性) · ADR-044 (VIPT) · ADR-045 (CtrlLink Stall Loop) · ADR-048 (Plugin 注册规范序)
> **关联变更**: `openspec/changes/mmu-paddr-consume-and-real-memory/`

---

## Context

### 背景

v0.2.3 R3 已知限制 + v0.1.3 §11.2 风险：当前 SoC demo 的 MMU 在 CPU 流水线中是**装饰性**的：

1. **IBusPlugin/DBusPlugin 不消费 `pl::PADDR`**: `at_stage("fetch", NORMAL)` 直接用 `pl::PC` (vaddr) 读内存, 跳过 MMU 翻译结果。MMU 翻译完 `pl::PADDR` 后无下游消费者, 翻译等于白做。
2. **PTW 走 stub memory**: `MMUPlugin::ptw_->advance_from_stub()` 从 `pte_stub_memory_` 读 PTE (测试友好接口), 不是真实物理内存。`ip/mmu/STATUS.md:38` 显式 deferred 承诺, Wave 3 实装。
3. **canonical ordering 隐式契约**: ADR-048 锁定 `MMUPlugin::build()` 先于 `IBusPlugin::build()` 注册, 否则 stall 晚 1 cycle 失效。

### 缺口

1. **数据通路错配**：MMU 翻译结果 `pl::PADDR` 是孤儿 payload key，没有 IBus/DBus 消费端。
2. **PTW 测试 mock 是生产路径**：`pte_stub_memory_` (4096 项 array) 是 TDD 友好的，但 production SoC 必须读真实物理内存（`soc/cpu_l1_mmu_demo.json` 中 `cpu.memory` 已 vendor 的 `PicolibcHostMemory`）。
3. **Wave 4 P2#6 阻塞**：`phase-1.5-wave-4` (CSR/exception/mispredict) 依赖 `mmu_exit` hook 真生效。

### 约束

- **依赖 ADR-048**：MMU before IBus 注册序必须先实装 (v0.7.0 已 archive, ✅)。
- **TLM baseline 兼容**：47 `[mmu]` 测试 + 6 `[cpu-l1-mmu-demo]` 测试 + 40 `[riscv-tests]` 测试零回归（ADR-040 v2.0 §3）。
- **CH_MEM 模式兼容**：`pl::PADDR` 是 `uint_t<64>` POD, 不引入运行期状态。
- **向后兼容**：`MMUPlugin` 构造加 `MemoryInterface* mem = nullptr` 默认参数, `nullptr` 走 stub 路径（旧行为）。
- **lib/ 层 purity**：`MemoryInterface` 必须纯 C++, 零 `cf::plugin::*` 依赖（与 `ptw.h` 同层）。
- **错误范式**：复用 `std::invalid_argument` + `PluginError::BuildFailed` 现有错误流（ADR-047），不新建 `ConfigError`。

---

## Decision

**D1: PADDR-first consumption pattern（IBus/DBus 优先读 PADDR）**

### 机制

1. `IBusPlugin::at_stage("fetch", NORMAL)` 闭包改造：
   ```cpp
   // 优先 PADDR (MMU 翻译结果), fallback vaddr (无 MMU 模式)
   if (paddr_valid) {
     mem_->read_word(pl::PADDR);
   } else {
     mem_->read_word(pl::PC);
   }
   ```
2. `DBusPlugin::at_stage("memory", NORMAL)` 同样改造 (LOAD + STORE 对称)。
3. **`paddr_valid` 显式 Payload key**（新增 `mmu_keys::PADDR_VALID`）由 `RiscvMMUPlugin::at_stage("tlb_lookup_*", LATE)` 写 `true` (翻译成功) 或 `false` (miss/fault)。

### 与 D1=B（隐式 `PADDR != 0`）的比较

| 维度 | D1=A 显式 flag（选中） | D1=B 隐式检测 |
|------|---------------------|--------------|
| 物理地址 0 合法性 | ✅ 区分 | ❌ 误判 (PADDR=0 是合法的 MMIO 地址) |
| 与现有 mmu_keys 命名一致 | ✅ | ❌ |
| PayloadStore 复杂度 | +1 key | 0 |
| 可读性 | 高 | 低 |

D1=A 被选中因为：物理 0 地址合法, 隐式检测误判。

---

**D2: `MemoryInterface` 抽象类（解耦 PTW 与具体内存）**

### 机制

新文件 `ip/mmu/lib/memory_interface.h` (~40 LOC)：

```cpp
namespace cf::ip::mmu {
class MemoryInterface {
 public:
  virtual std::uint32_t read_word(std::uint64_t addr) = 0;
  virtual void write_word(std::uint64_t addr, std::uint32_t val) = 0;
  virtual ~MemoryInterface() = default;
};
}  // namespace cf::ip::mmu
```

### 设计要点

- **lib/ 层 purity**：纯 C++ 抽象, 零 `cf::plugin::*` 依赖, 与 `ptw.h` 同层。
- **接口最小化**：仅 `read_word` / `write_word`, 不暴露字节读写接口（PTW 32-bit PTE 粒度足够）。
- **`PicolibcHostMemory` 继承实现**（D3）。

---

**D3: `PicolibcHostMemory` 继承 `MemoryInterface`（具体实现）**

### 机制

`ip/cpu/picolibc_host_memory.h` 修改：

```cpp
#include "ip/mmu/lib/memory_interface.h"
class PicolibcHostMemory : public cf::ip::mmu::MemoryInterface {
  // 既有 read_word / write_word 签名匹配 (PicolibcHostMemory.h:101,116)
  // 移除 read_word 的 const 限定 (与 MemoryInterface 非 const 一致)
  std::uint32_t read_word(std::uint64_t mem_address) override;
  void write_word(std::uint64_t mem_address, std::uint32_t val) override;
};
```

### 向后兼容

- 既有 `read_byte` / `read_half` / `write_byte` / `write_half` 保留（PicolibcHostMemory 字节级接口是 riscv-tests ELF 加载必需）。
- 4 个 callers (`dbus.h:84`, `ibus.h:76`, `test_picolibc_memory_base_window.cpp:35,84`) 全部通过非 const 指针调用, 移除 const 不破坏。

---

**D4: `MMUPlugin` 构造函数接 `MemoryInterface* mem = nullptr`**

### 机制

```cpp
MMUPlugin(SvMode mode, std::vector<TLBConfig> levels_cfg, PTWConfig ptw_cfg,
          MemoryInterface* mem = nullptr);
```

- `mem == nullptr` → 走 `advance_from_stub()` (现有 47 `[mmu]` 测试 + 6 `[cpu-l1-mmu-demo]` 测试 0 回归)。
- `mem != nullptr` → `MMUPlugin::at_stage` 闭包优先用 `advance_from_real_memory(mem_)`, fallback `advance_from_stub()` (向后兼容路径, 仅当 real memory read fail 时降级)。

### ABI 影响 (R3 mitigation)

- 加默认参数不破坏 ABI（C++ 默认参数在调用点填充）。
- 既有 `RiscvMMUPlugin(sv_mode, levels, ptw, satp=0)` 调用 0 改动。

---

**D5: SoC JSON `mmu.memory_interface` 字段路径**

### 机制

`soc/cpu_l1_mmu_demo.json` 加：

```json
{
  "components": [
    {"type": "cf::cpu::CpuFactory", "config": "ip/cpu/configs/cpu_default.json"},
    {"type": "cf::cpu::PicolibcHostMemory", "base": "0x80000000", "size": "64KB"},
    {"type": "cf::ip::mmu::MMUPlugin", "sv": "sv32", "memory_interface": "picolibc_host_memory"}
  ]
}
```

`CpuFactory::build_cpu()` 解析 `memory_interface` 字段时，在 `components` 数组查找 `"type": "cf::cpu::PicolibcHostMemory"` 条目获取实例指针，注入 `MMUPlugin` 构造。

---

## Consequences

### 收益

1. **数据通路真起作用**：MMU 翻译结果 `pl::PADDR` 真消费, "端到端真跑" 名副其实。
2. **PTW 走真实内存**：production SoC 不依赖 stub, ELF 程序能真访问页表。
3. **Wave 4 P2#6 解锁**：`phase-1.5-wave-4` (CSR/exception) 依赖的 `mmu_exit` hook 真生效。
4. **未来扩展友好**：`MemoryInterface` 抽象可注入 L2Cache / DRAM 模型 / mmap，未来 SoC 配置无需改 PTW 代码。

### 成本

1. **API 扩展**：`MMUPlugin` 构造加 `MemoryInterface*` 参数（旧调用 0 改动）。
2. **payload key +1**：`mmu_keys::PADDR_VALID` 新增（已有 11 个 key, +1）。
3. **`PicolibcHostMemory` 移除 `const` on read_word**（4 callers 兼容性已验证）。

---

## Open Questions

- Q1: `mmu_keys::PADDR_VALID` 是否合并到 `mmu_keys::EXCEPTION_CODE`? 当前 stub 写 EXCEPTION_CODE=0 表示 success, 但语义重叠。
  → **当前决策**: 新增独立 `PADDR_VALID`（避免语义混淆）。
- Q2: PTW 真实内存读延迟建模: 用 CppTLM 事务延迟 (1 cycle) 还是 Phase 6d 6d.6 FSM 5 状态 (IDLE/L0_WAIT/L1_WAIT/DONE/FAULT)?
  → **当前决策**: 沿用 6d.6 FSM 5 状态, 真实内存读延迟由 `MemoryInterface::read_word()` 返回 cycle 数（默认 1）。
- Q3: SoC JSON `mmu.memory_interface` 字段路径: 嵌套 `mmu.*` 还是 `components` 数组扁平?
  → **当前决策**: 嵌套 `mmu.memory_interface` (与 P1#6 mmu-config-json-driven §2 字段命名一致)。

---

## 后续项

### MMU 配置 JSON 驱动 (拆分独立 change `mmu-config-json-driven`)

**激活条件**: 本 ADR-049 archive 后 + `mmu-config-json-driven` 实装时, 合入本章节。

**目的**: 将 `MMUPlugin` 构造参数从 C++ 硬编码迁移到 JSON 配置驱动, 消除 `CpuFactory::register_early_plugins()` 中 TLB 几何硬编码。该 capability 体现"配置驱动装配"的 D4 范式 (ADR-040 v2.0 §3)。

**详细草案**: `openspec/changes/mmu-config-json-driven/adr-049-followup-section.md` (commit `19d5107`)。完整 §1-§7 内容（决策概述 / 配置优先级链 / 字段命名约束 / 实施位置 / 错误范式 / 兼容性边界 / 关联文档）见该草案。

#### 8 项集成 checklist (本 ADR-049 实装 owner)

1. 创建 `ADR-049-mmu-paddr-consumption-contract.md` 时, 在文件末尾追加 `## 后续项 — JSON 配置驱动` 章节（已完成, 见上）。
2. ✅ 已复制 `adr-049-followup-section.md` §1-§7 内容到本 ADR-049 §后续项 (上方)。
3. ✅ 已调整 §关联文档引用从 commit hash 改为本 ADR-049 自己的引用。
4. **P1#3 tasks.md §2.1 增加 checkbox**: "ADR-049 §后续项 已含 JSON 配置驱动段落 (引用 `openspec/changes/mmu-config-json-driven/adr-049-followup-section.md`)" — **本 ADR 已完成**
5. **P1#3 archive 验证**: `grep -l "JSON 配置驱动" docs/architecture/adr/ADR-049-mmu-paddr-consumption-contract.md` 必须 1 命中 — **本 ADR 已满足**
6. **通知 P1#6 owner** (`mmu-config-json-driven` 实装 agent): 本 ADR-049 §后续项 已就绪, 实装时可参考 §关联文档引用
7. **ADR-049 §后续项 内容版本控制**: 与 `adr-049-followup-section.md` 草案同步, 任意修订需双向更新
8. **archive 前 grep 验证**: `grep -c "adr-049-followup-section.md" docs/architecture/adr/ADR-049-*.md` 必须 ≥ 1

#### 关键决策摘要（参考 §详细草案）

- **配置优先级链**: SoC JSON `mmu.*` > params_schema.json defaults > 硬编码 fallback
- **sv_mode 单源**: 在 `cpu_default.json::mmu_mode`, SoC JSON 不重复声明
- **字段命名约束**: 复用既有 `params_schema.json` 词汇表（`entries`/`associativity`/`num_lookup_ports`/`lookup_latency_cycles`/`replacement_policy`/`ptw_max_inflight`）, 禁止发明 `mmu.tlb_geometry`/`sets`/`ways`/`block_size`/`parallel_factor` 等新词
- **JsonConfigLoader 落点**: `ip/mmu/lib/mmu_config_loader.{h,cpp}`（lib/ 层允许 nlohmann/json 依赖, 禁 `cf::plugin::*` 依赖）
- **错误范式**: `std::invalid_argument` + `PluginError::BuildFailed`（ADR-047 范式）, 不新建 `ConfigError`

---

## 相关 ADR

- **ADR-040 v2.0** — TLM→HDL 移植性约束（CH_MEM 模式兼容, Tier-2 array_store）
- **ADR-044** — L1 Cache↔MMU VIPT 锁定 + 反别名安全边界
- **ADR-045** — Plugin CtrlLink 消费契约 + PipeBuilder::run() Stall Loop
- **ADR-047** — 静态配置期错误处理 Result 范式 (`std::expected<T, PluginError>`)
- **ADR-048** — Plugin 注册规范序（MMU before IBus/DBus 前提）
- **ADR-049 §后续项** — MMU 配置 JSON 驱动（拆分自 P1#3, 由 `mmu-config-json-driven` 独立 change 实装）