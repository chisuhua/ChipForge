---
id: feat-rv64-isa-declaration-integrity
kind: feature
status: active
scope: cpu
phase_refs: [phase-4]
主题: RV64 ISA 声明真实性 + XLEN 就绪度门禁 (ADR-087 实施)
---

## 概述

**目标**：消除 `isa` 字段的虚假声明（宣称 RV64GC / 实现全 RV32），并把"未来切 XLEN=64 会静默算错"的风险从隐性变为显性门禁。

**审查结论（2026-10-23，architect 阶段）**：

| 维度 | 判定 |
|------|------|
| 实现层 | **RV32IM + Zicsr(subset)**，与 roadmap 一致（RV64 已推迟至 v1.x 之后，`.rddf/roadmap/archive/phase-2-baremetal.md:5`） |
| 声明层 | **全线宣称 RV64GC** —— README / `CPUConfig::isa` 默认 / schema enum / 4 个 JSON / blueprint / status / SoC integration-guide |
| 隐患层 | 骨架层真参数化 + ISA 语义层写死 RV32 ⇒ `CpuFactory<uint64_t>` **静默算错**，无异常无断言 |

**声明层越界点（实测）**：

- `ip/cpu/README.md:13` 「支持 RV32I/RV64GC 指令集」
- `ip/cpu/cpu_factory.h:127` `CPUConfig::isa = "rv64gc"` ← **默认值即谎言**
- `ip/cpu/cpu_factory.h` 全文**无任何 `isa ==` dispatch** ⇒ `isa` 是装饰字段；`T` 由调用方定（`:310` 默认 `uint32_t`）
- `ip/cpu/configs/{cpu_default,cpu_deep_pipeline,cpu_superscalar}.json:5` `"isa": "rv64gc"`
- `ip/cpu/configs/cpu_params_schema.json:18-23` enum 含 rv64i/rv64gc + default rv64gc，**且无 `xlen` 属性**
- `soc/docs/integration-guide.md:17` `"isa": "rv64imac"` ← 第四种取值，**连 schema enum 都不在**
- `tools/cpu_sim/main.cpp:16,188` 显式 pin `cfg.isa = "rv32i"`，注释自认「修复既有 `CpuFactory<uint32_t>` vs config.isa="rv64gc" 不一致」← **代码自证**

**语义层静默错误点（TLM 侧，切 XLEN=64 后结果错但不报错）**：

| 位置 | 问题 |
|------|------|
| `ip/cpu/arch/riscv/int_alu.h:92,97,100` | 移位量 `& 0x1F`（RV64 需 6-bit） |
| `ip/cpu/arch/riscv/int_alu.h:93-94` / `branch.h:87-90,115-118` | 有符号比较用 `int32_t` |
| `ip/cpu/arch/riscv/mul.h:102-119` | MULH 族 = `uint32_t(r >> 32)`（RV64 需 128-bit 高 64 位） |
| `ip/cpu/arch/riscv/mul_div_fsm.h:148,231,241,244` | radix-2 除法写死 32 拍 |
| `ip/cpu/arch/riscv/payload_riscv.h:55` / `decoder_table.h:181-214` | 立即数 `int32_t` |
| `ip/cpu/arch/riscv/decoder_table.h:283` | 仅 `decode_rv32`，无 `decode_rv64` |

**CH_MEM 侧** 9 处 `kXlenBits = 32` 定义点各自硬编码：`reg_file_chmem.h:96-101` / `decode_chmem.h:96-98` / `int_alu_chmem.h:98-103` / `branch_chmem.h:97-99` / `ibus_chmem.h:36` / `dmem_chmem.h:34-35` / `mmu_ptw_chmem.h:151-152` / `cpu_factory_chmem.h:61-63`（上述 8 处在 `ip/cpu/`）+ **`ip/mmu/plugins/MMUPlugin_chmem.h:81`**（跨 IP 目录，`ip/cpu/` 前缀 grep 漏记典型案例）

**平台层缺失**：无 `misa`/`MXL`（全仓零命中，CPU 无法自述 RV32/RV64）· 无 `sstatus` · CSR 全 32-bit（`csr_chmem.h:517-524`，`mepc = PC+4` 32-bit 截断，`:472`）· ELF32-only（`tools/cpu_sim/elf_loader.h:9`）· 无 LD/SD · 无 W 指令 · 无 C/A/F/D

## 跨阶段拆分

### phase-4（v0.11.0 窗口）— ADR-087 D1/D2/D7 落地

- **Change 1 `rv64-isa-declaration-correction`（P0）**：✅ **已完成并归档**（`openspec/changes/archive/2026-10-24-rv64-isa-declaration-correction/`，2026-10-24；AC 10/10 通过；实施要点：`isa` 默认 `rv32im` + `xlen` 字段 + `cf::cpu::detail::validate_isa_xlen` 三层 fail-fast + 3 处模板入口编译期护栏 + 4 个死代码删除 + `check_xlen_hygiene.sh` 软门禁）
  - `CPUConfig::isa` 默认 `rv64gc` → `rv32im`；新增 `xlen` 字段（默认 32）
  - schema 加 `xlen` 属性（enum 32/64）；`isa` enum 三态标注（implemented / declared-reserved / unsupported）
  - 3 个 JSON 的 `isa` 改 `rv32im`；`cpu_embedded.json` 的 `rv32imac` → `rv32im`（A 扩展未实现）
  - `tools/cpu_sim` 移除 `isa="rv32i"` pin（改为跟随 config）
  - `ip/cpu/plugins/reg_file.cpp`（死代码，显式实例化 `RegFilePlugin<uint64_t>` 却用 `keys_rv32::*`）删除或修正
  - `isa` → `xlen` 映射 + **运行时 fail-fast** 一致性检查（`isa` 为运行期 `std::string`，`cpu_factory.h:127`，编译期校验不可行 —— 与 D1 unsupported 同形态）**本 change 不做 dispatch**（dispatch 留到 B1 之后）
  - **D4 提前落地**：`int_alu.h` / `branch.h` 模板运算入口加 `static_assert(sizeof(T) == 4)`（与 TLM 退役决策正交，phase-4→5 窗口期不裸奔；`mul.h` / `mul_div_fsm.h` 归 Change 2）
- **Change 3 `rv64-readiness-gap-tracker`（P1）**：缺口表入 `ip/cpu/docs/README.md` 索引 + ADR-087 注册 + "新增 XLEN 假设必须登记缺口表"的 CI 提示

### phase-5（v1.0.0+，与 `feat-tlm-backend-retirement` B3 同步）— D3/D4 改造

- **Change 2 `rv64-xlen-width-guardrails`**：
  - D3：`xlen_bits<T>` 单一来源，CH_MEM 9 处 `kXlenBits = 32` 定义点收敛（含 `ip/mmu/` 的 1 处；需 TLM 类型 → CH_MEM 位宽显式映射层，`ch_uint<32>` 的 `sizeof` ≠ 4）
  - D4 护栏**形态需随 TLM 退役调整**：
    - **TLM 未删时** ⇒ `int_alu.h` / `branch.h` 已随 Change 1 落地；Change 2 补 `mul.h` / `mul_div_fsm.h` 的 `static_assert(sizeof(T) == 4)`，把"静默算错"变"编译失败"
    - **TLM 已删后** ⇒ 护栏对象消失，改为 **CH_MEM 侧 CI grep 门禁**（禁止 `kXlenBits` 硬编码 / 禁止 `isa` 含 rv64 的配置被接受）+ **ADR-087 G1–G9 准入清单**人工判定
    - ⇒ **此交叉点需 architect 出 ADR-087 v1.1 修订**，本 feature 记录依赖

## 验收标准

- [ ] `CPUConfig{}` 默认 `isa == "rv32im"`；`xlen == 32`；`isa` 含 rv64 与 `xlen` 不一致 ⇒ **运行时 fail-fast 生效**（抛异常/报错，形态同 D1 unsupported）
- [ ] `CpuFactory<uint64_t>` 实例化**编译失败**（`int_alu.h` / `branch.h` 的 `static_assert(sizeof(T)==4)` 生效；变异对照：改 8 应编译失败 `[cpu]` 用例）
- [ ] `grep -rn '"rv64' ip/cpu/configs/ soc/` 返回 0；schema 无 `rv64i`/`rv64gc` 默认值
- [ ] `tools/cpu_sim` 不再 pin `isa="rv32i"`，配置跟随行为一致
- [ ] `ip/cpu/plugins/reg_file.cpp` 的 `uint64_t` 显式实例化消失
- [ ] 架构门禁 5 脚本全 PASS；`[cpu]` / `[cpu-integration]` / `[riscv-tests]` / `[chmem]` 0 退化
- [ ] 缺口表 `ip/cpu/docs/rv64_readiness_gap.md` 入索引；ADR-087 注册进 `docs/architecture/adr.md`
- [ ] CI 提示上线："新增 XLEN 假设必须登记缺口表"（软门禁，warn）
- [ ] **phase-5**：`grep -rnE "kXlenBits *=" --include='*.h' ip/ | grep -v payload_common.h` 返回 0（9 处定义全部收敛单一来源）；CH_MEM 侧 CI grep 门禁上线
- [ ] **phase-5**：ADR-087 v1.1 修订完成（D4 护栏形态随 TLM 退役切换已落定）

## 关联

- **ADR-087**（ISA 声明真实性与 RV64 就绪度门禁，Proposed）— 本 feature 是其实施载体；**需 v1.1 修订**（D4 护栏形态）
- **ADR-083**（Plugin-style 单一 source of truth）— TLM/CH_MEM 双模下 `xlen` 单一来源的可行性前提
- **ADR-047**（Result 范式）— `declared-reserved` 降级 warn 的自然载体
- `feat-tlm-backend-retirement` — **交叉依赖**：其 B3 删除 TLM 后，本 feature 的 D4 护栏对象消失 ⇒ 护栏形态切换
- `ip/cpu/docs/rv64_readiness_gap.md` — 缺口表 SSOT（含全部 file:line 证据 + G1–G9 准入清单）
- `objective-ip-cpu-tlm-backend-retirement` — 共享 objective（Go/No-Go gate 覆盖本 feature 的 P0 项）
