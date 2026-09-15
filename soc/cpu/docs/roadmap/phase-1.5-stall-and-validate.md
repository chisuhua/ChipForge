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
- **PTW 真实内存接线**（兑现 `ip/mmu/STATUS.md:38` 的 deferred 承诺）：`MMUPlugin::ptw_->read_pte(vaddr)` 从 PicolibcHostMemory 而非 `pte_stub_memory_` 读 PTE — 不做此步，demo 里的 MMU 仍在跑测试 stub，"端到端真跑"名不副实
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
| `mmu-sv32-ext` | Sv32 PTW decode + megapage（**Sv48 砍掉**：Sv48 是 RV64-only 格式，RV32-only CPU 无消费者，scope creep） | 1 周 | Wave 2 |
| `cpu-pipeline-exception` | throw_when 真实消费者（trap delivery + mcause/mepc/mtvec CSR + mstatus/sstatus） | **1.5 周** | exception CSR 落地（并入） |
| `ip-cpu-csr-minimal` | mstatus/mtvec/mepc/mcause/mtval/sstatus CSR 写实装 | 并入 exception | 无 |
| `cpu-pipeline-mispredict` | flush_when 真实消费者（branch recovery + flush ROB） | 1 周 | exception CSR 落地后 |
| `phase-2-baremetal-kickoff` | Phase 2 启动文档 + riscv-tests RV64GC 接入计划 | 0.5 周 | Wave 4 完成 |

> **Wave 4 依赖解耦说明**：cycle-precision 与 exception 是**正交**的——throw_when 的语义正确性在单遍 run 下完全可实现，cycle-precision 只影响时序精确度。原计划断言"cycle-precision 是 exception 的基础"无证据，且会把 1.5 周 exception 工作阻塞在 Wave 3 后。解耦后 exception 可与 Wave 3 并行（不同 IP 区域）。`cpu-pipeline-mispredict` 依赖 exception 的 CSR 基础设施（非 cycle-precision）。

**Phase 1.5 毕业标准**：
- ✅ RV32I ISA 合规 ≥85%（CSR/trap 子集除外）
- ✅ 端到端 SoC demo 真跑 **≥5** 个 riscv-tests 用例（**门槛**；**≥10** 为 stretch goal）
- ✅ cache-dse-sweep CSV 数据落盘
- ✅ D4 + ADR-040 + ADR-044 + ADR-045全合规
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

### 6.1 进度 / 资源风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| Wave 1 riscv-tests 大面积 fail（>50% 用例 fail） | 中 | 高 | 立即聚焦 Wave 2 为"CPU 真 bug 修"，不强行推 Phase 1.5 |
| Wave 2 fix 后 rv32ui 通过率卡在 60-70%（CSR 依赖） | 中 | 中 | 接受，标记 CSR 为 Phase 2+ work；Phase 1.5 以 RV32I 整数子集毕业 |
| SoC demo 的 memory 模型不够（cpptlm MemoryTLM 限制） | 低 | 中 | Wave 2 内用 PicolibcHostMemory 替代（cpu_sim 已在用） + **PTW 实内存接线**（兑现 `ip/mmu/STATUS.md:38`） |
| riscv-tests ELFs 不可获得（submodule / 网络问题） | 低 | 中 | vendor 到 `build/third_party/riscv-tests/` 路径 + CI 缓存 |
| Wave 3 DSE 时间爆炸（>1 周） | 中 | 中 | 缩小维度范围：仅 sweep line_size × replacement，固定 assoc=1 |
| Phase 1.5 总耗时超 7 周 | 中 | 中 | Wave 4 拆分：CSR 单独 change 推迟到 Phase 2；`mmu-sv32-ext` 独立为单个 change；exception 与 cycle-precision 解耦并行 |

### 6.2 技术语义风险（Oracle R2 标记，plugin-framework-stall 评审时识别，未被任何 wave 覆盖）

| 风险 | 来源 | 影响 | 缓解 |
|------|------|------|------|
| **HazardPlugin scoreboard 生命周期 vs 单 pass-per-run 语义** | plugin-framework-stall R2 盲点：mark/clear 同 run 发生，stall 可能永不触发或永久 stall | cpu-integration 4 个 RISC-V 测试最高危；riscv-tests 中 load-use 序列（`lb`/`sb` 数据依赖）会**被动暴露** | **Wave 2 预设** `hazard-scoreboard-lifecycle` 验证任务：落地 scoreboard 跨 cycle 持久化方案（每次 `pb.run()` 前 snapshot） |
| **stall ≠ bubble**（下游 stage 重执行陈旧 payload，非幂等指令被破坏） | plugin-framework-stall R2：fetch stall 时 decode/execute/memory 继续在 stale payload 上跑，同条指令被反复 decode+execute | `cpu-pipeline-multi-cycle`（Wave 3）的正确性前提；store/CSR 类非幂等指令直接被破坏 | **Wave 3 multi-cycle 前置条件**：在 commit B 前 spike 一个"非幂等指令破坏"测试（`sw x_n, 0(x0)` 跑 N 次 → 验证只写一次） |
| **`tlb_lookup_ifetch` 在 PTW 期间不 stall / 无 TLB refill** | plugin-framework-stall R2：每 cycle 重复 `do_lookup` → TLB 未 refill → 重复 `start_walk` | demo 与 rv32ui 的 MMU 路径有持续空转开销 | Wave 2 demo 范围：**MMUPlugin 完成回调需 refill TLB**（除写 PADDR 外）—— 与 PTW 实内存接线合并实现 |
| **canonical stage ordering**（MMUPlugin 必须先于 IBusPlugin 注册） | plugin-framework-stall R2 盲点：MMUPlugin 晚于 IBusPlugin 注册 → stall 晚 1 cycle，stale-read bug 残留 | 沉默 bug（demo 可能偶发失败） | Wave 2 demo 验证：`CpuFactory` 调用顺序断言 + CI 检查 |
| **commit_storages 与 throw 路径交互**（throw 跳过 commit_storages → 1-cycle 假 stall 残留） | plugin-framework-stall R2 | Wave 4 exception 引入 throw_when 真触发后影响 real consumers | Wave 4 exception 内部处理（throw 时强制 reset hazard cache） |

### 6.3 战略风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| RV64GC 跳跃（Phase 2 声明范围 vs Phase 1.5 RV32-only） | 中 | 中 | Phase 2 启动文档显式说明 XLEN=64 切换工作量为新 change |
| Sv48 无 RV32 消费者 | — | — | 已砍（Wave 4 仅 Sv32） |
| PLIC/CLINT 缺口（Phase 3 RTOS 依赖 timer interrupt） | 高 | 中 | `phase-2-baremetal.md` 显式立项 CLINT 子项，不推迟到 Phase 3 |
| 6.1 资源风险与 6.2 技术风险耦合（Wave 1 暴露 scoreboard bug → Wave 2 fix 时间爆炸） | 中 | 高 | 见 §6.2 表第 1 行：预设验证任务；Wave 2 N > 5 时转 Phase 2 |

## 7. 决策可追溯

### 7.1 与 CHANGELOG Pending 顺序的有意偏离

`CHANGELOG.md` v0.1.3 §Pending 列出 6 个后续 change，**`soc-cpu-l1-mmu-demo` 第一、`riscv-tests-rv32ui` 第二**。本计划在 Wave 1 把 `riscv-tests-rv32ui` 提前、`soc-cpu-l1-mmu-demo` 延后到 Wave 2。

**偏离理由**（防止 `verify_adr` 文档漂移检查视为不一致）：
1. **CHANGELOG Pending 顺序基于"feature 价值"**（先解锁 demo 解锁多 capability）
2. **本计划基于"先验证后堆叠"**（先建客观基线，避免 demo 跑不动时回溯根因）
3. CHANGELOG Pending 与本 Wave 1 的目标**互补非冲突**：riscv-tests 是工具，soc-demo 是结果。先有工具再判结果可信度
4. CHANGELOG 在 Wave 1 完成后应同步更新为 `riscv-tests-rv32ui` (done) + `soc-cpu-l1-mmu-demo` (in progress)

### 7.2 其他决策来源

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
| [`../../../../docs/roadmap/README.md`](../../../../docs/roadmap/README.md) | 全局路线图入口 |
| [`../../../../CHANGELOG.md`](../../../../CHANGELOG.md) | v0.0.x → v0.2.x 变更历史 |
| [`../../architecture.md`](../architecture.md) | SoC 系统架构 |

## 10. 变更日志

| 日期 | 变更 |
|------|------|
| 2026-09-15 | 初版：plugin-framework-stall 归档后启动 Phase 1.5，4-wave 计划 |
