> **⚠️ ARCHIVED 2026-09-26**：本文档作为**历史参考**保留（CppHDL RTL + co-simulation 任务清单溯源）。当前主控路线图是 [`./execution-roadmap.md`](./execution-roadmap.md)（v0.10.0 → v1.3.0）。原 Phase 5 内容覆盖映射：
>
> | 原 Phase 5 任务 | 对应 execution-roadmap.md 章节 |
> |-----------------|-------------------------------|
> | CppHDL RTL + VerilogCodeGen | Phase 6c 已完成（`pb.elaborate(ctx)` 一次性发射 DAG） |
> | Verilator 编译 + VL1Cache 替换 | Phase 6d.5 已完成（0d30d64 + CppHDL 7f7da88 lint clean + 1a99ed3 sim cycle-identical） |
> | ImplMode::COMPARE/SHADOW + ComparisonEngine | 双模对拍 ADR-080（v1.2 试点 → v1.3 硬门禁）天然等同 COMPARE 模式 |
> | Spike Co-simulation | v1.2.0 PoC-9 Spike lockstep（1M 条零分歧） |
> | 测试矩阵（ISA/中断/MMU/Cache/冒险/随机） | riscv-tests 40/40 + riscv-dv 随机指令流（v1.2.0 PoC-9 内） |
> | GPU SoC 跨芯片扩展 | 推迟到 v1.x 之后远期 |
>
> **不再作为执行依据。**

# Phase 5：RTL 协同验证 + Verilog 生成

> **Status**: Not Started
> **Milestone**: M6 - RTL 协同验证 / M7 - Verilog 生成 / M8 - 多芯片扩展 / M10 - 多 ISA 支持
> **Depends on**: Phase 4

**目标**：TLM 模型与 CppHDL RTL 模型协同验证；输出可综合的 Verilog

---

## 任务清单

### 1. CppHDL RTL 开发

以 `L1CacheRtl`（CppHDL Component）为第一个 RTL 模型：

- [ ] 用 `Component` + `LogicNode` 描述缓存逻辑
- [ ] 用 `Simulator::run()` 直接 C++ 仿真，对标 `L1CachePlugin`
- [ ] 调用 `VerilogCodeGen::generate("l1_cache.v")` 输出 Verilog
- [ ] Verilator 编译 -> 生成 `VL1Cache` C++ 模型 -> 替换 `L1CacheRtl`

### 2. COMPARE 模式验证流程

```
riscv-dv 生成随机测试程序
         |
         v
  编译为 ELF
         |
         v
RiscvVirtSoC (ImplMode::COMPARE)
          +-- TLM 执行路径 ------+
          +-- RTL 执行路径 ------+
                                 v
                           ScoreBoard
                     （逐条指令对比寄存器写回）
                                 v
                          PASS / FAIL 报告
                          （含执行迹 JSON）
```

> 注：上述 `RiscvVirtSoC` 是 Phase 2 计划的 SoC 装配类（待实施），不是 `soc/riscv_virt.json`（已删除）。

- [ ] 实现 `ImplMode::COMPARE` 模式：TLM + RTL 并行执行 + ScoreBoard 逐周期对比
- [ ] 实现 `ImplMode::SHADOW` 模式：RTL 跟踪 TLM 主路径
- [ ] 实现 `ComparisonEngine`：自动对比两路执行迹，生成差异报告

### 3. Spike Co-simulation

```cpp
// verification/SpikeBridge.h
class SpikeBridge {
public:
    // 驱动 Spike 执行一条指令，获取参考寄存器状态
    SpikeState step();

    // 与 TLM 执行迹对比
    bool compare(const TlmState& tlm, const SpikeState& ref);
};
```

- [ ] 实现 SpikeBridge co-simulation 接口
- [ ] 与 TLM 执行迹自动对比

### 4. 测试矩阵

| 测试类型 | TLM 模型 | RTL (CppHDL) | RTL (Verilator) | 覆盖目标 |
|----------|----------|--------------|-----------------|---------|
| ISA 指令 | v | v | v | 100% |
| 中断/异常 | v | v | v | 100% |
| 虚拟内存 | v | v | v | 95% |
| Cache 一致性 | v | v | v | 90% |
| 流水线冒险 | - | v | v | 95% |
| riscv-dv 随机 | v | v | v | - |

- [ ] ISA 指令测试全模式通过
- [ ] 中断/异常测试全模式通过
- [ ] 虚拟内存测试全模式通过
- [ ] Cache 一致性测试
- [ ] 流水线冒险测试
- [ ] riscv-dv 随机测试集成

### 5. ISA 抽象层与 ImplMode 完善

- [ ] 确保所有 CPU IP 暴露统一 `ch_stream<MemReqBundle>` 接口，实现 SoC 层零修改切换 ISA（ISA 无关性详细见 [overview.md §"ch_stream 接口即 ISA 无关层"](../../../../docs/architecture/overview.md)）
- [ ] DSE 扩展至 RTL 参数空间（Pipeline 深度、Buffer 大小等）

### 6. 可选：GPU SoC 扩展

复用 `cache/`, `memory/`, `interconnect/` 组件库，新建 `soc/GpuSoC.cpp`，验证组件库跨芯片形态的可复用性。

- [ ] 实现 `GpuSoC` 组合配置
- [ ] 验证 IP 组件库跨芯片形态可复用性

