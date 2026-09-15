# Phase 1.5：Stall 兑现 + 端到端验证（2026-09-15 ~ 2026-10）

> **Status**: 🚧 Active (2026-09-15 启动)
> **Phase 触发**: `plugin-framework-stall` v0.1.3 归档（声明式控制流原语首次落地，HazardPlugin 兑现 M4 TODO）
> **目标版本**: ChipForge 0.2.0
> **上承**: Phase 1 核心完成 + MMU/VIPT/CPU Pipeline 4 change 归档 + plugin-framework-stall
> **下启**: Phase 2 Bare-metal（riscv-tests RV64GC + 完整 SoC 装配）

## 0. 战略判断

**当前能力**（v0.1.3 完成态）：

- ✅ CPU Pipeline 真跑 RV32I add.elf → `tohost=1`（5 cycles，cpu-pipeline-stubs-replace）
- ✅ MMU PTW 真走 Sv39 walk + SFENCE.VMA + VIPT dual-write
- ✅ IBus fetch 在 PTW busy 时自动 stall（plugin-framework-stall commit B）
- ✅ HazardPlugin RAW → execute stall（同 commit B）
- ❌ **5 个 RISC-V 仿真测试 pre-existing fail**（add.elf 之外的 RISC-V 程序是否能跑通，无客观基线）
- ❌ 端到端 SoC demo 仍用 `traffic_gen` → mmu → l1 → mem（假流量），不是真 CPU 程序
- ❌ DSE 仍只能扫基线噪声（无完整平台）

**Phase 1.5 的目标不是"做更多 feature"，而是"把已有 feature 验证透"**：
1. **建立客观 ISA 合规基线**（riscv-tests）
2. **基于验证结果修复隐含 bug**（pre-existing 5 个 fail）
3. **首次端到端 SoC demo 真跑 RISC-V 程序**
4. **DSE 在稳定平台上扫**

**关键认识**：跳过 riscv-tests 直接做 SoC demo 是"先建房子再验收"——等到 demo 跑不动某个 ELF 时回溯根因，定位时间 × N。

## 1. 战略目标

| 目标 | 量化 | 验收 |
|------|------|------|
| **RV32I ISA 合规基线** | riscv-tests rv32ui-p-* 套件 ≥60% 通过率（不含 CSR/trap 子集） | ctest `[riscv-tests]` family tag |
| **SoC 真跑 RISC-V** | `soc/riscv_virt.json` 重建，`cpu_sim --elf` → tohost=1 端到端 | soc/ JSON schema 校验 + smoke test |
| **Dimming pre-existing 失败** | 5 → ≤2（接受 2 个为"待实施 feature"如 fence.i） | ctest 全量 PASS |
| **DSE 数据** | cache-dse-sweep 12-case Pareto 输出 CSV | soc/cpu/docs/dse/cache-dse-v1.csv |
| **D4 试金石** | DSE/SoC 跨多个 commit 仍 4 gates 全绿 | CI 阻塞门禁 |

## 2. Wave 序列

> 设计原则：**验证先于堆叠**。Wave 1 建立基线，Wave 2-3 修补 + 扩展，Wave 4 是"毕业 demo"。

### Wave 1（立即，1 周）：建立客观验收门槛

| Change | 目标 | 估时 | 依赖 |
|--------|------|------|------|
| `riscv-tests-rv32ui` | 接入 rv32ui-p-* prebuilt ELFs 到 ctest，量化当前通过率 | 3 天 | 无（复用 `cpu_sim --elf` 机制） |

**核心内容**：
- `tests/cpu/riscv_tests/` 放 prebuilt ELFs（git submodule 或 build-time download）
- `tests/cpu/integration/test_riscv_tests_runner.cpp`：catch2 fixture 按 `[riscv-tests][rv32ui-p-add]` 等 tag 跑每个 ELF，断言 `cpu_sim` 输出含 `tohost=1`
- 基线快照：N 个用例 pass / M 个用例 fail / 失败原因分类（CSR/trap/branch CSR 依赖 vs 真 bug）
- `tests/CMakeLists.txt` 添加 `[riscv-tests]` family 注册（已经 GLOB_RECURSE，无需改）

**预期收益**：
- **客观红/绿矩阵**替代"自造 add.elf 拍脑袋"
- 失败用例 = 后续 milestone 的 work list（如 `jalr` fail → 修 branch/JALR、`auipc` fail → 修 U-type immediate、`fence` fail → 修 fence opcode）
- 为 Wave 2 修补提供精确靶位

**不做**（防 scope creep）：
- ❌ 不修任何 fail 用例（仅记录，留给 Wave 2）
- ❌ 不实现 rv32mi（machine mode，依赖 mstatus/mtvec 等 CSR，CPU 当前无 CSR 写）
- ❌ 不实现 rv32si（supervisor mode，依赖 mmu + satp CSRs）
- ❌ 不下载 riscv64 套件（CPU 当前 RV32 only）

### Wave 2（Wave 1 后，2 周）：修补 high-impact 失败 + 首次 SoC demo

**前置**：Wave 1 输出的红/绿矩阵作为优先级排序依据

| Change | 目标 | 估时 | 触发条件 |
|--------|------|------|---------|
| `cpu-pipeline-fix-rv32ui-N` | 修 X 个 high-impact rv32ui fail 用例 | 1 周 | Wave 1 矩阵显示 ≥3 fail 属于真 bug（非 feature stub） |
| `soc-cpu-l1-mmu-demo` | `soc/riscv_virt.json` 重建：CPU + MMU + L1 + Memory 端到端 tohost=1 | 1 周 | Wave 1 通过率 ≥40%（基本整数 ALU OK） |

**`soc-cpu-l1-mmu-demo` 范围**：
- 新建 `soc/cpu_l1_mmu_demo.json`：CPU(`cf::cpu::plugins`)+ MMU(`cf::ip::mmu::MMUPlugin` + `cf::ip::mmu::RiscvMMUPlugin`) + L1(`cf::ip::cache::L1CachePlugin`) + Memory(PicolibcHostMemory 64KB)
- `tests/soc/test_cpu_l1_mmu_demo.cpp`：catch2 集成测试，跑 `add.elf`（已有）+ 选 3-5 个 riscv-tests 用例通过子集 → 端到端 tohost=1
- 删除 `riscv_virt.json` 删除警告（v0.0.2 删除记录）→ 重建
- 文档：`soc/README.md` 更新、Phase 1.5 → 1 推进状态

**`cpu-pipeline-fix-rv32ui-N` 范围（典型候选）**：
- `jalr` fail → 修 branch.h (RISC-V spec §2.5: rs1+imm 4-byte aligned, lowest bit clear)
- `auipc/lui` U-type fail → 修 int_alu.h (U-type uses imm upper 20 bits shifted left 12)
- `fence`/`fence.i` fail → 标记为"feature stub"（Wave 3+）
- 其他识别出的真 bug（如 hazard stall 实际破坏某类指令）

### Wave 3（Wave 2 后，1-2 周）：DSE + framework 精化

| Change | 目标 | 估时 | 依赖 |
|--------|------|------|------|
| `cache-dse-sweep` | 12-case Pareto 扫描（size × assoc × replacement × line_size） | 1 周 | Wave 1+2 稳定 |
| `cpu-pipeline-multi-cycle` | MUL/DIV LATENCY>1 真多周期 stall（复用 plugin-framework-stall 原语） | 1 周 | Wave 1+2 |
| `plugin-framework-cycle-precision` | `pb.run(cycle_count=N)` 真 cycle 精度（cpu_sim 主循环接入） | 0.5 周 | Wave 1+2 |

**DSE 优先级理由**：
- cache-dse-sweep 现在有真实平台基线（Wave 1+2 后）才有意义
- 当前 cache 是 256×1 direct-mapped，4-way 是真实扫描对象 — 但若 Wave 1+2 发现 CPU 端 bug 多，建议先修真 bug
- `cache-phase1.5-4way` 不在此 wave（4-way 是大改造单独立 change）；DSE 可在 256×1 上跑 size/replacement/line_size 三个维度先行

### Wave 4（Wave 3 后，2 周）：Phase 1.5 → Phase 2 毕业

| Change | 目标 | 估时 | 依赖 |
|--------|------|------|------|
| `mmu-sv32-sv48-ext` | Sv32/Sv48 PTW decode + megapage/gigapage | 1 周 | Wave 2 |
| `cpu-pipeline-exception` | throw_when 真实消费者（trap delivery + mcause/mepc/mtvec CSR） | 1 周 | Wave 3 cycle-precision |
| `ip-cpu-csr-minimal` | mstatus/mtvec/mepc/mcause CSR 写实装 | 并入 exception | Wave 3 |
| `cpu-pipeline-mispredict` | flush_when 真实消费者（branch recovery + flush ROB） | 并入 mispredict | Wave 3 |
| `phase-2-baremetal-kickoff` | Phase 2 启动文档 + riscv-tests RV64GC 接入计划 | 0.5 周 | Wave 4 完成 |

**Phase 1.5 毕业标准**：
- ✅ RV32I ISA 合规 ≥85%（CSR/trap 子集除外）
- ✅ 端到端 SoC demo 真跑 ≥10 个 riscv-tests 用例
- ✅ cache-dse-sweep CSV 数据落盘
- ✅ D4 + ADR-040 + ADR-044 + ADR-045 全合规
- ✅ CHANGELOG v0.2.0 发布

## 3. Wave 间依赖图

```
Wave 1 (riscv-tests-rv32ui)
        │
        ▼
Wave 2 (cpu-pipeline-fix-rv32ui-N) ←─┐
        │                            │
        ├──────────────┐             │
        ▼              ▼             │
   (soc-demo)    (cpu-pipeline-     │
                 fix sub-changes)    │
        │                            │
        ▼                            │
Wave 3 (cache-dse-sweep + cpu-pipeline-multi-cycle + plugin-framework-cycle-precision)
        │
        ▼
Wave 4 (mmu-sv32-sv48 + cpu-pipeline-exception + cpu-pipeline-mispredict)
        │
        ▼
Phase 2 Bare-metal Kickoff
```

**关键观察**：
- Wave 2 的 fix sub-changes 与 soc-demo **可并行**（不同 issue 跟踪）
- Wave 4 必须等 Wave 3（cycle-precision 是 exception/mispredict 的基础）

## 4. 与已归档 / 待归档 Change 的关系

### 已归档（基线）
- v0.0.9 mmu-tlb-ptw-impl
- v0.1.0 mmu-cache-integration
- v0.1.1 ptw-walk-bridge-fix
- v0.1.2 cpu-pipeline-stubs-replace
- v0.2.0 cpu-mmu-integration（chipforge version）
- **v0.1.3 plugin-framework-stall**

### 推迟到 Phase 2+（不在 Phase 1.5 范围）
- `cache-phase1.5-4way`（Wave 4 或 Phase 2，需独立 change 范围，4-way 替换 + VIPT 安全重新验证）
- `soc-riscv-virt-full`（Phase 2+，含 PLIC/CLINT/UART）
- `ip-memory`（独立 IP 实现，Phase 2+）

## 5. Wave 1 优先级论证

**为什么不直接做 `soc-cpu-l1-mmu-demo` 而要先做 `riscv-tests-rv32ui`？**

| 选项 | 优点 | 缺点 |
|------|------|------|
| **直接做 SoC demo** | 立即看到 CPU 真跑 demo 程序 | "能跑 add.elf" 不证明"对"；Wave 2 后会有大量隐含 bug 在 demo 里爆炸 |
| **先 riscv-tests（A）** | 客观红/绿矩阵指导所有后续 change；rv32ui ELFs 是现成的；3-5 天小投入 | 推迟 1 周才有 demo；Phase 1.5 整体完成时间 +1 周 |

**结论**：Wave 1 选 A。1 周投资换"全周期可量化"。

**反例**：直接做 SoC demo 然后发现 `jalr` fail → 拆 demo 修 jalr → 重做 demo。**净耗时 >2 周 + 2 次 commit history 污染**。

## 6. 风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| Wave 1 riscv-tests 大面积 fail（>50% 用例 fail） | 中 | 高 | 立即聚焦 Wave 2 为"CPU 真 bug 修"，不强行推 Phase 1.5 |
| Wave 2 fix 后 rv32ui 通过率卡在 60-70%（CSR 依赖） | 中 | 中 | 接受，标记 CSR 为 Phase 2+ work；Phase 1.5 以 RV32I 整数子集毕业 |
| SoC demo 的 memory 模型不够（cpptlm MemoryTLM 限制） | 低 | 中 | Wave 2 内用 PicolibcHostMemory 替代（cpu_sim 已在用） |
| riscv-tests ELFs 不可获得（submodule / 网络问题） | 低 | 中 | vendor 到 `build/third_party/riscv-tests/` 路径 + CI 缓存 |
| Wave 3 DSE 时间爆炸（>1 周） | 中 | 中 | 缩小维度范围：仅 sweep line_size × replacement，固定 assoc=1 |
| Phase 1.5 总耗时超 6 周 | 中 | 中 | Wave 4 拆分：CSR 单独 change 推迟到 Phase 2；mmu-sv32 可推迟 |

## 7. 决策可追溯

- **Wave 1 优先 riscv-tests**：Oracle R1 评估 hidden 4th candidate `riscv-tests-rv32ui`（分 20）作为 follow-up；本次前瞻判断"先做"而非"后做"
- **Wave 2 双轨**：识别 high-impact 真 bug 与 demo 端到端并行，互不阻塞
- **Wave 3 cache-dse 推迟**：Wave 1+2 稳定后再做（避免在错误基线上扫 DSE 数据无意义）
- **Wave 4 csr/exception/mispredict 合流**：三个 use case 共享 CSR 基础 + flush 基础，单 change 内部拆 commit

## 8. 退出标准

**Phase 1.5 → Phase 2 切换条件**：
1. ✅ RV32I rv32ui-p-* 子集通过率 ≥85%（含 jalr/branch/lui/auipc/addi/add/sub/xor/etc）
2. ✅ `soc/cpu_l1_mmu_demo.json` 端到端跑 ≥5 个 riscv-tests 用例 → tohost=1
3. ✅ `cache-dse-sweep` 输出 CSV 可在 60 秒内出
4. ✅ `pb.run(cycle_count=N)` 在 cpu_sim 主循环可用
5. ✅ 全 ctest 332+ pass（pre-existing 5 个 fail 至少消除 3 个）
6. ✅ CHANGELOG v0.2.0 + docs/roadmap 更新到 Phase 1.5 毕业态
7. ✅ 4 architecture gates 全绿

## 9. 相关文档

| 文档 | 内容 |
|------|------|
| [README.md](README.md) | SoC 整体 roadmap 与 Phase 状态 |
| [phase-1-tlm-foundation.md](phase-1-tlm-foundation.md) | Phase 1 详细任务（已完成） |
| [phase-2-baremetal.md](phase-2-baremetal.md) | Phase 2 启动条件 |
| [`../../../docs/roadmap/README.md`](../../../docs/roadmap/README.md) | 全局路线图入口 |
| [`../../../../CHANGELOG.md`](../../../../CHANGELOG.md) | v0.0.x → v0.2.x 变更历史 |
| [`../../architecture.md`](../architecture.md) | SoC 系统架构 |

## 10. 变更日志

| 日期 | 变更 |
|------|------|
| 2026-09-15 | 初版：plugin-framework-stall 归档后启动 Phase 1.5，4-wave 计划 |
