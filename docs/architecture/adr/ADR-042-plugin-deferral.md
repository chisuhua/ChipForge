# ADR-042: Plugin 推迟决策 (FPU/MMU/Exception → Phase 5+)

> **状态**: Accepted (2026-06-16)
> **作者**: ChipForge Plugin Team
> **关联**: `multi_isa_architecture.md` v2.0 §1.1, `cpu_implementation_guide_v2.0.md` §3 议题 1-8

## 1. 背景

ChipForge v2.0 决策的 RISC-V CPU 范围是 **RV32I/RV64I + M + Zicsr + Zifencei** (5 个扩展, 11 个 Plugin 套件)。 但 v2.0 计划中包含 3 个额外的 Plugin, 实施时需要明确推迟:

| Plugin | ISA 扩展 | 推迟原因 |
|--------|----------|----------|
| `RiscvFpuPlugin` | F (单精度) / D (双精度) | 单精度/双精度浮点, 需要 FPU 寄存器堆 (f0-f31) + 浮点运算流水线, 复杂度高 |
| `MMUPlugin` | Sv32/Sv39/Sv48 | 虚拟内存 + TLB + 页表遍历, 需要硬件/OS 配合 |
| `ExceptionPlugin` | — | mcause/mepc/mtvec CSR + trap handler, 需要完整异常/中断机制 |

## 2. 决策

**M2 阶段** 实施 FPU/MMU/Exception 的 **.h 占位文件** (P3+), `.cpp` 写 `// TODO: M3+` 注释, 不在 CpuFactory 中注册。

**M3+ 阶段** 推迟到 **Phase 5+** (RTL 阶段), 理由:
- FPU 需要 RTL 实现浮点运算单元, 仿真精度要求高
- MMU 需要 OS 支持 (Linux/RTOS), 当前无 OS 联调
- Exception 需要完整 CSR + trap 机制, 依赖 MMU

## 3. 推迟的 Plugin 接口

```cpp
// ip/cpu/plugins/fpu.h (P3+ 占位)
class FPUPlugin : public cf::plugin::PluginBase {
 public:
  void setup(cf::plugin::PipeBuilder&) override {}
  void build(cf::plugin::PipeBuilder&) override {}
  // TODO: Phase 5+ 实现 f0-f31 寄存器 + 浮点 ALU
};

// ip/cpu/plugins/mmu.h (P3+ 占位)
class MMUPlugin : public cf::plugin::PluginBase {
 public:
  void setup(cf::plugin::PipeBuilder&) override {}
  void build(cf::plugin::PipeBuilder&) override {}
  // TODO: Phase 5+ 实现 Sv32/Sv39 TLB + 页表遍历
};

// ip/cpu/plugins/exception.h (P3+ 占位)
class ExceptionPlugin : public cf::plugin::PluginBase {
 public:
  void setup(cf::plugin::PipeBuilder&) override {}
  void build(cf::plugin::PipeBuilder&) override {}
  // TODO: Phase 5+ 实现 mcause/mepc/mtvec + trap handler
};
```

## 4. CpuFactory 中的处理

CpuFactory::build_cpu() 当前不注册 FPU/MMU/Exception, 保持 M5 收官时的 11 个核心 Plugin (M2 5 个 + M3 6 个)。

## 5. Phase 5+ 触发条件

推迟到以下条件满足时实施:
- FPU: 用户请求 F/D 扩展支持 + RTL 阶段
- MMU: OS 联调 (Linux/RTOS) 需求 + 虚拟内存测试
- Exception: 完整 CSR 需求 + 中断/异常处理测试

## 6. 替代方案

推迟期间, 用户可使用:
- 浮点运算: 软件仿真 (compiler-rt/libgcc 软浮点)
- 虚拟内存: 直接物理地址 (当前 M5 配置 `enable_mmu: false`)
- 异常: 简化处理 (tohost 机制替代 trap)

## 7. 参考

- v2.0 决策: `cpu_implementation_guide_v2.0.md` §3 议题 2 (Plugin 拆分粒度)
- multi_isa 架构: `multi_isa_architecture.md` §1.1 (项目目标)
- M2 实施: commit `036d769` (M2.6-M2.9 占位)
- M3 实施: commit `5d9967b` (M3.8 RiscvCsrPlugin P2 stub)
- status.md: M2/M3 累计 100%
