# MMU 集成契约

MMU 不直接暴露 Bundle，而是**通过 CPU 的 fetch/memory 阶段 Payload Key** 读写。

## 1. CPU Pipeline 集成

### 1.1 fetch 阶段集成（指令翻译）

```
┌─────────────────────────────────────────────────────────────┐
│  fetch stage                                                │
│  ┌──────────┐   ┌──────────────┐   ┌──────────────────┐  │
│  │IBusPlugin│──▶│MMUPlugin::at_│──▶│  IBusPlugin 读   │  │
│  │ 写 pl::PC│   │stage("tlb_   │   │  pl::PADDR →     │  │
│  │          │   │lookup_ifetch")│   │  L1I Cache lookup │  │
│  └──────────┘   └──────────────┘   └──────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

**数据流**:
1. `IBusPlugin::at_stage("fetch", ...)` 写 `pl::PC` (虚地址)
2. `MMUPlugin::at_stage("tlb_lookup_ifetch", ...)`:
   - 读 `pl::PC` (虚地址)
   - 调 `multi_tlb_->lookup(vpc, asid)` 查 TLB
   - **hit**: 写 `pl::PADDR` = paddr, 清 `pl::PTW_ACTIVE`
   - **miss**: 写 `pl::PTW_ACTIVE=1`, 写 `pl::PTW_VADDR=vpc`, 写 `pl::PTW_ASID=asid`
3. `IBusPlugin` 后续 `at_stage` 读 `pl::PADDR`, 用 paddr 查 L1I

### 1.2 memory 阶段集成（数据翻译）

类似 fetch，DBusPlugin 在 `memory` 阶段写 `pl::MEM_ADDR` (虚地址)，MMUPlugin 在 `tlb_lookup_loadstore` 阶段翻译为 paddr，DBusPlugin 后续阶段读 `pl::PADDR`。

### 1.3 PTW 3 级子流水（miss 路径）

```
MMUPlugin::at_stage("ptw_l0"):  读 L0 PTE from memory → 写 pl::PTW_L0_RAW
MMUPlugin::at_stage("ptw_l1"):  读 L1 PTE from memory → 写 pl::PTW_L1_RAW
MMUPlugin::at_stage("ptw_l2"):  读 L2 PTE from memory → 写 pl::PADDR + 清 PTW_ACTIVE
                                  + 调 multi_tlb_->refill_from_ptw(...)
```

**Phase 0 框架限制**：`pb.run()` 单 cycle 遍历所有 `at_stage` 回调。PTW 3 级是**逻辑阶段拆分**，不是 cycle 精确调度（Phase 6 框架升级后会变成真实 3 cycle）。

### 1.4 PTW 期间 stall 下游（CtrlLink）

```
MMUPlugin 通过 PipeBuilder::ctrl_link_between("fetch", "memory")
  ->halt_when([&] { return pto_->is_busy(); })
```

PTW 进行中下游 IBus/DBus 阶段 stall，等待 PTW 完成。骨架阶段仅声明接口（推迟到 `mmu-tlb-ptw-impl`）。

## 2. RISC-V 适配器集成

`ip/cpu/plugins/mmu.h` 改为 `RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin`，处理 RISC-V 特定 hook：

| Hook | 说明 | 推迟到 |
|------|------|--------|
| satp CSR 写入拦截 | Trap satp 写入 → 更新 PTW 配置 | mmu-tlb-ptw-impl |
| SFENCE.VMA 指令拦截 | 触发 `multi_tlb_->invalidate_vaddr/asid/all()` | mmu-tlb-ptw-impl |
| Exception code 12/13/15 | page fault / access fault 映射 | mmu-tlb-ptw-impl |
| mstatus.MXR/SUM 行为 | 影响 permission check | mmu-tlb-ptw-impl |

骨架阶段仅声明接口 + 类型别名，向后兼容：
```cpp
// ip/cpu/plugins/mmu.h
class RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin {
  // RISC-V 特定 hook 推迟
};
using MMUPlugin = RiscvMMUPlugin;  // 向后兼容
```

## 3. SoC JSON 集成样例（推迟到 TLB/PTW 算法稳定后）

`ip/mmu/` 骨架阶段**不**创建 cpptlm `MMUTLMBridge`（与 `L1CacheTLMBridge` 同构），推迟到 `mmu-tlb-ptw-impl` 之后。

**未来 SoC JSON 样例**（mmu-tlb-ptw-impl 之后）:
```json
{
  "modules": [
    {"name": "cpu",   "type": "RiscVCpuTLM",     "params": {...}},
    {"name": "mmu",   "type": "MMUTLMBridge",    "params": {"sv_mode":"sv39", "asid_bits":9, ...}},
    {"name": "l1i",   "type": "L1CacheTLMBridge", "params": {"num_sets":256, "tag_bits":20, ...}},
    {"name": "l1d",   "type": "L1CacheTLMBridge", "params": {"num_sets":256, "tag_bits":20, ...}},
    {"name": "mem",   "type": "MemoryTLM",       "params": {"size_kb":1024}}
  ],
  "connections": [
    {"src": "cpu", "dst": "mmu", "port": "fetch_vaddr"},
    {"src": "mmu", "dst": "l1i", "port": "fetch_paddr"},
    {"src": "cpu", "dst": "mmu", "port": "loadstore_vaddr"},
    {"src": "mmu", "dst": "l1d", "port": "loadstore_paddr"},
    {"src": "l1i", "dst": "cpu", "port": "fetch_instr"},
    {"src": "l1d", "dst": "cpu", "port": "loadstore_data"}
  ]
}
```

## 4. 测试集成入口

```cpp
// tests/mmu/test_mmu_plugin.cpp
#include "ip/mmu/tlm/MMUPlugin.h"

TEST(MMUPlugin, TLBHit1Cycle) {
  cf::plugin::PipeBuilder pb;
  pb.register_plugin(std::make_unique<cf::ip::mmu::MMUPlugin>(
      cf::ip::mmu::SvMode::Sv39,
      {cf::ip::mmu::MMUPlugin::TLBConfig{"L0", 8, 8},
       cf::ip::mmu::MMUPlugin::TLBConfig{"L1", 64, 4}},
      {max_inflight: 2}));
  pb.build();
  
  // 设置 vPC, 验证 pl::PADDR 正确
  auto* node = pb.node_of_logic_stage("tlb_lookup_ifetch");
  (*node)(cf::ip::mmu::payload::mmu_keys<uint64_t>::VADDR) = 0xDEADBEEFULL;
  
  pb.run();
  
  EXPECT_EQ((*node)(cf::ip::mmu::payload::mmu_keys<uint64_t>::PADDR), ...);
  EXPECT_EQ((*node)(cf::ip::mmu::payload::mmu_keys<bool>::PTW_ACTIVE), false);
}
```

## 5. 集成检查清单

| 检查项 | 命令 | 期望 |
|--------|------|------|
| lib/ 0 引用 Plugin 框架 | `grep -rn "cf/plugin/plugin_base.h\|cf/plugin/pipe_builder.h\|cf/plugin/payload.h" ip/mmu/lib/` | 0 匹配 |
| MMUPlugin 派生 PluginBase | `grep "class MMUPlugin : public cf::plugin::PluginBase" ip/mmu/tlm/MMUPlugin.h` | 1 匹配 |
| MMUPlugin 0 业务 tick | `grep "::tick()" ip/mmu/tlm/MMUPlugin.cpp` | 0 匹配 |
| RiscvMMUPlugin 兼容旧名 | `grep "using MMUPlugin = RiscvMMUPlugin" ip/cpu/plugins/mmu.h` | 1 匹配 |
| 既有 cpu 测试 0 破坏 | `tools/run_chipforge_tests.sh` | 21/21 PASS |
