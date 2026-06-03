# Phase 5：RTL 协同验证 + Verilog 生成

> **Status**: Not Started
> **Milestone**: M6 - RTL 协同验证 / M7 - Verilog 生成 / M8 - 多芯片扩展 / M10 - 多 ISA 支持
> **Depends on**: Phase 4

**目标**：TLM 模型与 CppHDL RTL 模型协同验证；输出可综合的 Verilog

---

## 任务清单

### 1. CppHDL RTL 开发

以 `L1CacheRtl` 为第一个 RTL 模型：

- [ ] 用 `Component` + `LogicNode` 描述缓存逻辑
- [ ] 用 `Simulator::run()` 直接 C++ 仿真，对标 `L1CacheTlm`
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

- [ ] 确保所有 CPU IP（RiscvIssTlm、ArmIssTlm 等）暴露统一 `ch_stream<MemReqBundle>` 接口，实现 SoC 层零修改切换 ISA
- [ ] DSE 扩展至 RTL 参数空间（Pipeline 深度、Buffer 大小等）

### 6. 可选：GPU SoC 扩展

复用 `cache/`, `memory/`, `interconnect/` 组件库，新建 `soc/GpuSoC.cpp`，验证组件库跨芯片形态的可复用性。

- [ ] 实现 `GpuSoC` 组合配置
- [ ] 验证 IP 组件库跨芯片形态可复用性

