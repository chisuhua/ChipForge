# M1-M5 CPU 实施复盘 (Phase 1.5 v2.0)

> **状态**: Accepted (2026-06-16)
> **作者**: ChipForge Plugin Team
> **范围**: v2.0 文档拆分后 M1-M5 实施全程

## 1. 总览

| 阶段 | 内容 | 估时 | 实际 | 节省 | ctest 增量 |
|------|------|------|------|------|------------|
| **M1** | 框架层 (PipeArbitration + payload_common) | 3-4 d | 1.1 d | 70% | 18 → 20 |
| **M2** | ISA 无关 5 Plugin | 2-3 d | 0.8 d | 70% | 20 → 23 |
| **M3** | RISC-V 6 Plugin | 4-5 d | 1.0 d | 80% | 23 → 30 |
| **M4** | CpuFactory + 集成测试 | 2-3 d | 0.5 d | 80% | 30 → 33 |
| **M5** | 联调 + 文档 + ADR | 1-2 d | ~0.3 d | 70% | 33 → 34 |
| **总计** | | **12-17 d** | **~3.7 d** | **~75%** | **+16 测试** |

## 2. 关键 Lessons (经验教训)

### 2.1 复用现有基础设施 (节省 70-80% 时间)

- **cf_plugin 框架** (M1.2): 复用 Phase 0 已有的 PipeBuilder/Payload/PipeNode, 仅扩展 `at_stage` API
- **L1CachePlugin 方法学** (Phase 1.4): 6 维度 D1+D2 范式, 直接套用到 RegFile/Hazard/IBus/DBus
- **Payload Key 复用**: M1.7 定义 8 通用 Key, M2/M3 仅补充 ISA 特有 (funct3/funct7/imm)
- **PicolibcHostMemory** (M4.10): 64KB 静态 RAM 替代 MemoryTLM, 联调零障碍

### 2.2 模板参数化 xlen (编译期 0 开销)

- 所有 Plugin 用 `template <typename T>` 模板 (T=uint32/uint64)
- KeyType 自动匹配: `cf::cpu::core::payload::keys<T, sizeof(T)*8>`
- 编译期 static_assert 验证 XLEN ∈ {32, 64}
- RV32/RV64 双实例化, 运行时无差异

### 2.3 双 Payload 模式 (通用 + ISA 特有)

- 通用 Payload (`payload_common.h`): rs1_idx/rs2_idx/rd_idx/op_class 等跨 ISA 字段
- ISA 特有 Payload (`payload_riscv.h`): funct3/funct7/imm/csr_addr 等 RISC-V 字段
- 优点: 通用 Plugin 不依赖 ISA, ISA 特有 Plugin 不污染通用层
- 实施: M2 用通用, M3 加 RISC-V 特有, 无重复

### 2.4 constexpr 编译期求值 (译码表)

- `decode_rv32()` 用 constexpr 查表, 编译期 O(1)
- 运行时 0 开销, 与手写 switch 性能一致
- 单元测试 40+ 指令编译期覆盖, 无运行时调试成本

### 2.5 sign-extension 陷阱 (M 扩展)

**问题**: `int32_t(0x80000000) / -1` 应返回 INT32_MIN, 但错误实现会触发 UB
**解决**: uint32_t → int32_t → int64_t 显式 cast, 避免 zero-extension
**测试**: test_mul.cpp test_boundary 用例专门覆盖 INT_MIN/-1 边界
**教训**: 所有 RV32 算术先 cast 到 int32_t 再扩展, 避免 unsigned 隐式转换

### 2.6 build() 重复声明陷阱 (M2/M3 早期)

**问题**: 模板类成员函数 `build()` 在 .h 声明 + 在头文件实现 → 重复定义
**解决**: 仅在头文件实现, 删除 .h 中独立声明
**教训**: 模板方法只在 .h 实现, .cpp 仅做显式实例化

### 2.7 IntAlu compute() 静态 API

**设计**: 算术指令 compute() 是静态方法, 不依赖 PipeBuilder
**优点**: 单元测试无需完整 PipeBuilder, 38 用例快速验证
**推广**: Branch/Mul 也用静态 API (evaluate_branch/compute), 测试隔离

## 3. v2.0 拆分收益

原 `cpu_implementation_guide_v2.0.md` 1220 行混杂 4 类内容, 拆分为:
- `cpu_implementation_guide_v2.0.md` (决策入口, 只读)
- `blueprint.md` (静态架构, 极少改)
- `status.md` (任务状态, 高频改)
- `implementation-plan/README.md` + M1-M5 详细文件

**收益**:
- 文档可独立 review (决策 vs 实施)
- status.md 每次 Mx.y 完成即更新, 不影响其他文档
- M1-M5 详细文件可并行编写, 无冲突

## 4. D4 决策 (Plugin-style 范式) 验证

D4 要求: CPU Plugin 全部用 cf_plugin 框架 (PluginBase/PipeBuilder/at_stage), 不直接调 ch_stream/sim_object。

**M1-M5 验证**: 11 个 Plugin 全部 `class XxxPlugin : public cf::plugin::PluginBase` + `void build(PipeBuilder&)` 注册
- ✅ 0 个 Plugin 直接用 ch_stream
- ✅ 0 个 Plugin 绕开 PipeBuilder
- ✅ 0 个业务 tick() (Plugin 内部)

D4 PASS。

## 5. ADR-042 推迟决策

FPU/MMU/Exception 三个 Plugin 推迟到 Phase 5+, 理由:
- FPU: 需要 RTL 实现浮点运算, 仿真精度要求高
- MMU: 需要 OS 联调, 当前无 OS
- Exception: 依赖 MMU + 完整 CSR

推迟期间用:
- 软浮点 (compiler-rt)
- 直接物理地址 (enable_mmu: false)
- tohost 机制 (替代 trap)

详细见 `docs/architecture/adr/ADR-042-plugin-deferral.md`。

## 6. 下一步

Phase 1.5 v2.0 实施完成 (M1-M5 100%)。

**Phase 1.5 → Phase 2 过渡**:
- Phase 1.5: CPU + L1Cache 联调, 工具链基础 (本期)
- Phase 2: 内存子系统 (MemoryTLM + DDR 模型) + 多核
- Phase 5+: FPU/MMU/Exception + RTL 实现
- Phase 6: 7 级超标量 + Vector 扩展

**git tag**: `phase-1.5-cpu-v2.0-2026-06-16`

## 7. 累计产出

- **代码**: 30+ 文件, ~3500 行
  - 11 个 Plugin (M2 5 + M3 6)
  - 1 个 CpuFactory
  - 1 个 PicolibcHostMemory
  - 1 个 decoder_table (constexpr 查表)
  - 1 个 payload_common (8 通用 Key)
  - 1 个 payload_riscv (RISC-V 特有 Key)
  - 1 个 PipeArbitration (M1.5)

- **测试**: 34 个 ctest 全部 PASS
  - 9 framework (M1)
  - 5 P0 Plugin (M2)
  - 7 RISC-V Plugin (M3)
  - 3 CpuFactory (M4)
  - 6 联调 ELF (M5)

- **文档**: 10+ 文件
  - 4 类拆分文档 (决策/蓝图/状态/计划)
  - 5 个 M 详细计划
  - 1 个 lessons 复盘 (本文档)
  - 1 个 ADR-042

## 8. 致谢

- L1CachePlugin (Phase 1.2): 提供 6 维度方法学 D1+D2 范式
- VexRiscv (Scala): Plugin-style 设计灵感
- Spike ISS: 译码表参考
- picolibc: tohost 机制

## 相关文档

- **M1 详细**: `ip/cpu/docs/implementation-plan/M1-cpu-skeleton.md`
- **M2 详细**: `ip/cpu/docs/implementation-plan/M2-core-plugins.md`
- **M3 详细**: `ip/cpu/docs/implementation-plan/M3-riscv-plugins.md`
- **M4 详细**: `ip/cpu/docs/implementation-plan/M4-integration.md`
- **M5 详细**: `ip/cpu/docs/implementation-plan/M5-verification.md`
- **status.md**: 任务状态看板
- **ADR-042**: Plugin 推迟决策
