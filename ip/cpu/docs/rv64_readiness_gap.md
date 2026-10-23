# RV64 就绪度缺口表（RV64 Readiness Gap Table）

| 字段 | 值 |
|------|-----|
| 状态 | Active — **本表是 RV64 缺口的 SSOT**（SSOT = Single Source of Truth） |
| 日期 | 2026-10-23 |
| 归属 ADR | [ADR-087](../../../docs/architecture/adr/ADR-087-isa-declaration-integrity-and-rv64-readiness.md) |
| 审查基线 | commit `3775224`（docs(ip/mmu): ADR-086 rev.2） |
| 审查范围 | `ip/cpu/` + `bundles/` + `ip/mmu/` + `tools/cpu_sim/` + 声明层文档 |
| 结论 | **当前实现 = RV32IM + Zicsr(subset)，不符合 RV64**；与 roadmap 一致（RV64 已推迟），**真正的缺陷是声明层宣称 RV64GC** |

---

## 0. 判定口径

| 符号 | 含义 | 判定标准 |
|------|------|---------|
| 🔴 **阻塞** | RV64 硬阻塞 | 该项不修，RV64 无法跑通 |
| 🟡 **静默错误** | 切到 XLEN=64 后**结果错但不报错** | 最危险：没有任何异常/断言提示 |
| ✅ **就绪** | 已参数化，无需改动 | 仅需在 CH_MEM 侧收敛常量 |
| ⚪ **N/A** | RV32 也缺（与 RV64 无关但一并记录） | 独立 change 处理 |

三列口径：

- **TLM**：`ip/cpu/plugins/*` + `ip/cpu/arch/riscv/*` 的 C++ 直仿路径
- **CH_MEM**：`*_chmem.h` 路径（Phase 6d / Verilator 主战场）
- **ISA 覆盖**：RISC-V ISA 子集覆盖度（与 XLEN 无关的扩展缺失也记在此列）

---

## 1. 结论摘要：声明 vs 实现

| 声明位置 | 宣称 | 实际 | 判定 |
|----------|------|------|------|
| `ip/cpu/README.md:13` | 「支持 RV32I/RV64GC 指令集」 | RV32IM + Zicsr(subset) | 🔴 虚假声明 |
| `ip/cpu/README.md:56` | `isa` 默认 `"rv64gc"` | 同上 | 🔴 虚假默认 |
| `ip/cpu/cpu_factory.h:127` | `CPUConfig::isa = "rv64gc"` | 同上；且 `isa` **完全不参与 dispatch** | 🔴 虚假默认 + 死字段 |
| `ip/cpu/configs/cpu_default.json:5` / `cpu_deep_pipeline.json:5` / `cpu_superscalar.json:5` | `"isa": "rv64gc"` | 同上 | 🔴 虚假声明 |
| `ip/cpu/configs/cpu_embedded.json:5` | `"isa": "rv32imac"` | RV32IM（C/A 缺失） | 🟡 部分虚假 |
| `ip/cpu/configs/cpu_params_schema.json:18-23` | enum 含 rv64i/rv64gc、default rv64gc | 无 `xlen` 属性；`:204` 的 sv32 约束无代码消费 | 🔴 虚假声明 |
| `soc/docs/integration-guide.md:17` | `"isa": "rv64imac"` | 第四种取值，**连 schema enum 都不在** | 🔴 虚假声明 |
| `ip/cpu/docs/blueprint.md:92,156` | 「RV32I/RV64I 整数运算」 | RV32 语义 | 🟡 表述含混 |
| `ip/cpu/docs/blueprint.md:455` | 「RegFilePlugin 多 xlen (32/64) → 模板参数化, 编译期选」（风险登记表） | 模板化成立，但 ISA 语义层未参数化 ⇒ 切 XLEN=64 会静默算错 | 🟡 缓解措施不完整 |
| `ip/cpu/docs/status.md:109` | 「已有 (5 级 RV64GC)」 | RV32IM | 🔴 虚假声明 |
| `ip/cpu/docs/multi_isa_architecture.md:930-931` | `cpu_default.json` = 5 级 RV32IM_Zicsr / `cpu_superscalar.json` = 7 级 RV64IMAFD | 与 JSON 实际内容矛盾（两者都写 `rv64gc`） | ⚪ 文档自相矛盾 |
| `ip/cpu/docs/multi_isa_architecture.md:734,1204` | `isa` 是「唯一权威字段 (字符串, factory dispatch)」 | **无 dispatch 实现** | 🔴 虚假声明 |
| `ip/cpu/docs/dse_architecture.md:126,231,251` | DSE 方案以 `isa` 为可探索维度 | 无 dispatch；`rv64*` 属规划态 | 🟡 表述含混 |
| `tools/cpu_sim/main.cpp:16,188` | 显式 `cfg.isa = "rv32i"` | 注释自认「修复既有 `CpuFactory<uint32_t>` vs config.isa="rv64gc" 不一致」 | ✅ 代码自证 → **已修复**（rv64-isa-declaration-correction：pin 移除，声明跟随 config） |

> **⚠️ 行号口径**：本节及 §2–§7 的 file:line 引用**审查基线 commit `3775224`**。2026-10-23 的声明纠偏（§2 的 0.7 / 0.8 两项）与 **rv64-isa-declaration-correction**（§2 的 0.1–0.6 / 0.9 / 0.10 八项：schema enum 收窄 + `xlen` 属性、4 个 JSON `isa`/`mmu_mode` 纠偏、`cpu_sim` 去 pin、`reg_file.cpp` 删除、`CPUConfig::isa` 默认值与三态 fail-fast）已使部分行号漂移 —— **判断某项是否已修复时以 §2 的「状态」列为准，不以行号为准**。
>
> **反例说明**：`ip/cpu/docs/blueprint.md:469`（「支持 RV32I/M/A/C/F/D + Linux」）**不是虚假声明** —— 该行位于 §8.1「VexRiscv 简介」，描述的是上游 VexRiscv 项目的能力，与 ChipForge 实现无关。审查初稿曾误判此项，此处更正。

> **路线图事实**（判定依据，不是缺陷）：`.rddf/roadmap/archive/phase-2-baremetal.md:5` 明确记载「rv64ui/um/ua/uf/ud/uc/si/mi 8 子集 … **rv64 子集推迟到 v1.x 之后远期**」；`docs/architecture/roadmap-evolution.md` 全部版本节点（v0.10.0 ~ v1.3.0）均为 RV32IMAC / RV32C / RV32A 形态。**⇒ 实现层是 RV32 与 roadmap 一致；声明层是唯一越界项。**

---

## 2. Layer 0 — 声明层缺口（🔴 最高优先级，零代码风险）

| # | 缺口 | 位置 | 归属 | 状态 |
|---|------|------|------|------|
| 0.1 | `isa` 字段无三态语义（implemented / declared-reserved / unsupported） | `ip/cpu/cpu_factory.h:127` | builder | ✅ 三态语义 + 三层 fail-fast（`cf::cpu::detail::validate_isa_xlen`，ADR-087 §D1） |
| 0.2 | `isa` 不参与 dispatch，`T` 由调用方决定（默认 `uint32_t`） | `ip/cpu/cpu_factory.h:310` | builder | ✅ 契约化（dispatch 留 phase-5）：`isa`→契约校验已落地；`isa → T` 真 dispatch 属 ADR-087 Change 2 |
| 0.3 | schema 缺 `xlen` 属性（32/64） | `ip/cpu/configs/cpu_params_schema.json:18-23` | builder | ✅ `xlen` 属性（enum 32/64，default 32）+ `isa` enum 收窄 `[rv32i,rv32im,rv32imac]` |
| 0.4 | 3 个 JSON 默认 `rv64gc` | `ip/cpu/configs/cpu_default.json:5` / `cpu_deep_pipeline.json:5` / `cpu_superscalar.json:5` | builder | ✅ `isa` → `rv32im`；`mmu_mode` → `sv32`（XLEN=32 下只有 Sv32 合法，满足 schema `:210` 条件约束） |
| 0.5 | `cpu_embedded.json` 宣称 `rv32imac`（A 扩展缺失） | `ip/cpu/configs/cpu_embedded.json:5` | builder | ✅ `rv32imac` → `rv32im`（A 扩展零实现） |
| 0.6 | integration-guide 第四种 isa 取值 | `soc/docs/integration-guide.md:17` | builder | ✅ 已修（architect 批 `11225a3`：`rv64imac` → `rv32im`），此处追认 |
| 0.7 | README / blueprint / status / DSE 文档宣称 RV64GC 或「isa 已有 dispatch」 | `ip/cpu/README.md:13,56` / `ip/cpu/docs/blueprint.md:92,156,455` / `ip/cpu/docs/status.md:109` / `ip/cpu/docs/multi_isa_architecture.md:734,1204` / `ip/cpu/docs/dse_architecture.md:126,231,251` | **architect（文档部分，已在 ADR-087 落地 ✅）** | ✅ |
| 0.8 | `multi_isa_architecture.md` 目录树与 `isa` 权威性表述 | `ip/cpu/docs/multi_isa_architecture.md:928-933` | **architect（文档部分，已在 ADR-087 落地 ✅）** | ✅ |
| 0.9 | `tools/cpu_sim` 靠 pin `rv32i` 打补丁 | `tools/cpu_sim/main.cpp:16,87,188` | builder | ✅ pin 已移除（声明跟随 config）+ `load_config()` 补 `xlen` 解析（不再静默丢弃 schema 声明字段） |
| 0.10 | `ip/cpu/plugins/reg_file.cpp` 显式实例化 `RegFilePlugin<uint64_t>` 却用 `keys_rv32::*`（**死代码**，一旦进 CMakeLists 即破坏 RV64） | `ip/cpu/plugins/reg_file.cpp`（**已删除**，0 includer） | builder | ✅ 文件已删除（`git rm`；实测 0 CMake 引用 + 0 `#include` 命中） |

---

## 3. Layer 1 — 骨架层（✅ 已参数化，无需改动）

| 能力 | 位置 | 判定 |
|------|------|------|
| Payload key 模板化 `keys<T, XLEN>` + `static_assert(XLEN == 32 \|\| 64)` | `ip/cpu/core/payload_common.h:112-120` | ✅ |
| `keys_rv64` 别名（**生产代码 0 引用；测试 7 处实例化覆盖**：`tests/cpu/test_payload_common.cpp:53,56,60,169,170,192,193`，复现 `grep -rn "keys_rv64" tests/cpu/test_payload_common.cpp`） | `ip/cpu/core/payload_common.h:174` | ✅ |
| PC / RS1 / RS2 / RD_DATA / RESULT / MEM_ADDR / MEM_DATA / IID_PC = `Payload<T>` | `ip/cpu/core/payload_common.h:126-167` | ✅ |
| 指令字固定 32-bit（**RV64 基础指令本就是 32-bit，正确**） | `ip/cpu/core/payload_common.h:129` | ✅ |
| `DecodePayload::branch_target = uint64_t` | `ip/cpu/core/payload_common.h:95` | ✅ |
| 传播层完全参数化 | `ip/cpu/plugins/stage_link.h:55` | ✅ |
| 寄存器堆模板化（`N_REGS=32` 合规范，RV64 不变） | `ip/cpu/plugins/reg_file.h:68,186` / `reg_file_chmem.h:92` | ✅ |
| 内存/缓存 bundle 地址与数据 64-bit | `bundles/mem_bundles.h:45-46,60,77-78` | ✅ |
| satp 编码已支持 Sv39 / Sv48 | `ip/cpu/cpu_factory.h:79-112` | ✅ |
| PTW 层级表含 Sv39 / Sv48（缺 Sv57） | `ip/mmu/lib/ptw.cpp:15-18` | ✅（Sv57 记 §6） |

---

## 4. Layer 2 — ISA 语义层（TLM 侧）缺口

> **本层是最高技术风险**：所有条目在 RV32 下全绿，切到 XLEN=64 后**静默产出错误结果**，无异常、无断言、无日志。

| # | 缺口 | 位置 | TLM | 判定 | RV64 正确语义 |
|---|------|------|-----|------|-------------|
| 2.1 | 移位量掩码 `& 0x1F`（SLL/SRL/SRA） | `ip/cpu/arch/riscv/int_alu.h:92,97,100` | ✓ | 🟡 | RV64 需 6-bit（`& 0x3F`） |
| 2.2 | SLT 用 `static_cast<int32_t>` | `ip/cpu/arch/riscv/int_alu.h:93-94` | ✓ | 🟡 | 需 int64 有符号比较 |
| 2.3 | BLT / BGE 用 `static_cast<int32_t>` | `ip/cpu/arch/riscv/branch.h:87-90,115-118` | ✓ | 🟡 | 需 int64 有符号比较 |
| 2.4 | MULH / MULHSU / MULHU = `uint32_t(r >> 32)` | `ip/cpu/arch/riscv/mul.h:102-119` | ✓ | 🔴 | 需 **128-bit 乘积的高 64 位** |
| 2.5 | DIV / REM 用 `int32_t` + `INT32_MIN` 溢出特判 | `ip/cpu/arch/riscv/mul.h:121-140` | ✓ | 🔴 | 需 INT64_MIN 特判 |
| 2.6 | radix-2 除法 32 拍写死（`DIV_CYCLES=33` / `iter_<32` / `31-iter_`） | `ip/cpu/arch/riscv/mul_div_fsm.h:148,231,241,244,406` | ✓ | 🔴 | 需 64 拍 ⇒ **必须重写，不是放开模板参数** |
| 2.7 | 符号判定 + `0x80000000u` / `0xFFFFFFFFu` 特判 | `ip/cpu/arch/riscv/mul_div_fsm.h:375-390` | ✓ | 🔴 | 值域需按 XLEN 参数化 |
| 2.8 | 立即数字段 `std::int32_t imm` | `ip/cpu/arch/riscv/payload_riscv.h:55` | ✓ | 🔴 | 需 XLEN 宽符号扩展立即数 |
| 2.9 | 译码立即数提取全部返回 `int32_t` | `ip/cpu/arch/riscv/decoder_table.h:181,187,196,209,214` | ✓ | 🔴 | 同上 |
| 2.10 | 仅 `decode_rv32(std::uint32_t)`，无 `decode_rv64` | `ip/cpu/arch/riscv/decoder_table.h:283` / `ip/cpu/arch/riscv/decode.h:66` | ✓ | 🔴 | 需按 XLEN 分派 |
| 2.11 | LUI / AUIPC 依赖 `int32 → uint64` **隐式**符号扩展 | `ip/cpu/arch/riscv/int_alu.h:64-68` | ✓ | 🟡 | RV64 下结果正确，但属隐式副作用，非显式设计 ⇒ 脆弱 |
| 2.12 | LD（funct3=011）落入 `OpCode::UNKNOWN`，STORE 无 SD | `ip/cpu/arch/riscv/decoder_table.h:309-319,322-329` | ✓ | 🔴 | RV64 必须支持 LD/SD |
| 2.13 | 未识别编码静默返回 0（`default: result = T{0}`），**无非法指令异常机制** | `ip/cpu/plugins/dbus.h:112-114`；`ip/cpu/` 全域 grep `illegal` 零命中 | ✓ | ⚪ | RV32 亦不合规，RV64 更依赖 |

---

## 5. Layer 3 — CH_MEM 后端缺口（9 处 `kXlenBits = 32` 定义点各自硬编码）

| # | 缺口 | 位置 | 判定 |
|---|------|------|------|
| 3.1 | 寄存器堆 `kXlenBits = 32` | `ip/cpu/plugins/reg_file_chmem.h:96-101` | 🔴 |
| 3.2 | 译码 `kXlenBits = 32` + `imm` 锁 `uint_t<32>` + `imm_u = inst & ~0xFFF`（**无 64-bit 符号扩展**） | `ip/cpu/plugins/decode_chmem.h:77,96-98,201` | 🔴 |
| 3.3 | ALU `kXlenBits = 32` + 移位 `bits<4,0>` + MULH 高位抽取 `bits<63,32>`（**放开 kXlenBits 后 MULH 依旧错**） | `ip/cpu/arch/riscv/int_alu_chmem.h:98-103,221,276-284` | 🔴 |
| 3.4 | 分支 `kXlenBits = 32` | `ip/cpu/plugins/branch_chmem.h:97-99` | 🔴 |
| 3.5 | PC = `ch_reg<ch_uint<32>>` | `ip/cpu/plugins/ibus_chmem.h:36,59,77-80` | 🔴 |
| 3.6 | 数据存储地址空间仅 64KB（`kMemWords = 16K`，`word_addr = bits<15,2>`） | `ip/cpu/plugins/dmem_chmem.h:34-35,151` | 🔴 |
| 3.7 | PTW FSM `kXlenBits = 32` / `kPtrBits = 22` ⇒ **sv32 2 级**（5 状态 `IDLE/L0_WAIT/L1_WAIT/DONE/FAULT`） | `ip/cpu/plugins/mmu_ptw_chmem.h:151-152,131-137` | 🔴 |
| 3.8 | 工厂 `kXlenBits = 32; // RV32 PoC` 传给全部 7 个 plugin | `ip/cpu/cpu_factory_chmem.h:61-63` | 🔴 |
| 3.9 | Hazard 显式 TODO：「仅 RV32 PoC (XLEN=32 硬编码), RV64 需 M4/W8 泛化」 | `ip/cpu/plugins/hazard_chmem.h:39` | 🟡（**全仓唯一显式 RV64 TODO**） |
| 3.10 | MMU 插件 `kXlenBits = 32`（与 3.1–3.8 同型硬编码，**位于 `ip/mmu/` 跨 IP 目录、最易被 `ip/cpu/` 前缀 grep 漏记**） | `ip/mmu/plugins/MMUPlugin_chmem.h:81` | 🔴 |

> **结构性风险**：9 处各自定义 ⇒ 未来改 XLEN **必然漏改**。ADR-087 D3 要求收敛为单一来源。
> **复现**（两条都要跑 —— 多行 `#ifdef` 定义只能被宽松正则命中）：
> `grep -rnE "kXlenBits *= *(32\b)" --include='*.h' ip/`（单行形式，5 处）+ `grep -rnE "kXlenBits *=" --include='*.h' ip/`（**合计 9 处定义点**，与 3.1–3.10 一一对应；不得用 `ip/cpu/` 前缀 —— 会结构性漏掉 3.10）

---

## 6. Layer 4 — 平台 / 外设层缺口

| # | 缺口 | 位置 | 判定 |
|---|------|------|------|
| 4.1 | 8 个 M-mode CSR 全 `ch_reg<ch_uint<32>>`；`keys<T,32>` 硬编码 | `ip/cpu/plugins/csr_chmem.h:136,429,517-524` | 🔴 |
| 4.2 | `mepc = pc_val + ch_uint<32>(4)` ⇒ **32-bit 截断 PC**（RV64 要求 mepc/mtvec/mcause 均 XLEN 宽） | `ip/cpu/plugins/csr_chmem.h:472` | 🔴 |
| 4.3 | TLM 端 CSR 是纯 stub（`setup`/`build` 全空） | `ip/cpu/arch/riscv/csr.h:26-36` | 🔴 |
| 4.4 | **全仓无 `misa` / `MXL`** ⇒ CPU 无法自述 RV32 还是 RV64 | 全仓 grep 零命中 | 🔴 |
| 4.5 | 无 `sstatus`（0x100），无 mstatus 的 RV64 `SD` 位 | 全仓 grep 零命中 | 🟡 |
| 4.6 | ELF 加载器仅支持 ELF32（「RV32 only」）⇒ RV64 程序加载不进来 | `tools/cpu_sim/elf_loader.h:9` | 🔴 |
| 4.7 | 无 W 指令（ADDIW / ADDW / SLLW / …） | `ip/cpu/arch/riscv/decoder_table.h` 无对应项 | 🔴 |
| 4.8 | 无 A 扩展（AMO / LR / SC） | 同上 | 🔴 |
| 4.9 | 无 C 扩展（16-bit 取指 / `opcode[1:0] != 11` 判别全缺） | 同上 | 🔴 |
| 4.10 | 无 F / D 扩展（FPU 是空 stub） | `ip/cpu/arch/riscv/fpu.h:25-35` | 🔴 |
| 4.11 | L1 cache tag = `uint_t<20>` ⇒ tag 位宽远小于 Sv39 PPN(44-bit)/Sv48 PPN(56-bit) 的物理地址需求（位宽口径见 `bundles/mem_bundles.h:108` 现状） | `bundles/mem_bundles.h:108` | 🟡 |
| 4.12 | 真内存 PTE 读仅 Sv32（「MemoryInterface 32-bit 接口, design.md D2 scope」） | `ip/mmu/lib/ptw.cpp:122-123` | 🔴 |
| 4.13 | 无 Sv57（`max_levels` 最大 Sv48） | `ip/mmu/lib/ptw.cpp:15-18` | 🟡（RV64 可选） |

---

## 7. 跨模块不一致（当前无任何 ADR 记录）

| 不一致 | 甲方 | 乙方 | 影响 |
|--------|------|------|------|
| MMU 侧已按 Sv39/Sv48 规划（ADR-085 / ADR-086 已归档设计），CPU 侧 CH_MEM PTW 仍是 sv32 2 级 | `ip/mmu/docs/adr/ADR-085-mmu-ptw-architecture.md` / `ADR-086-l0-l1-tlb-architecture.md` | `ip/cpu/plugins/mmu_ptw_chmem.h:151-152` | MMU 架构已跑到 RV64 形态，CPU 消费端还在 RV32 |
| `CPUConfig::mmu_mode` 默认 `"sv39"` | `ip/cpu/cpu_factory.h:138` | `ip/cpu/plugins/mmu_ptw_chmem.h:151`（sv32） | 默认配置与硬件实现不匹配 |
| `CPUConfig::isa` 默认 `"rv64gc"` | `ip/cpu/cpu_factory.h:127` | `ip/cpu/cpu_factory.h:310`（`T=uint32_t`） | 声明与实现不符 |

---

## 8. RV64 实施准入清单（Gate Checklist）

> 路线图把 RV64 定在 v1.x 之后（§1 路线图事实）。本清单定义**将来真做 RV64 时必须全部满足**的条件，作为 ADR-087 D5 的验收依据。**当前状态：0/9 满足。**

| # | 准入条件 | 当前 | 归属层 |
|---|----------|------|--------|
| G1 | `isa` 真正驱动 dispatch（`isa → xlen → T`），且 `isa` 含 rv64 ⇒ 编译期断言 `xlen == 64` | ⬜ | Layer 0 |
| G2 | `xlen_bits<T>` 单一来源（消除 9 处 `kXlenBits = 32` 定义点，缺口表 §5 的 3.1–3.10） | ⬜ | Layer 3 |
| G3 | ISA 语义层全部 XLEN 参数化（§4 的 2.1–2.10 全部转绿） | ⬜ | Layer 2 |
| G4 | M 扩展重写为真 128-bit 乘积 + 64 拍 radix-2 除法 | ⬜ | Layer 2 |
| G5 | `misa` 存在且 `MXL = 2`（RV64）；`mstatus` 含 `SD` 位；`sstatus` 存在 | ⬜ | Layer 4 |
| G6 | `mepc` / `mtvec` / `mcause` / `mstatus` 宽度 = XLEN | ⬜ | Layer 4 |
| G7 | 支持 LD / SD + W 指令族（ADDIW / ADDW / SLLW / SRLW / SRAW / SUBW） | ⬜ | Layer 2 / ISA 覆盖 |
| G8 | MMU 侧支持 Sv39 真内存 walk（当前仅 Sv32）+ CPU CH_MEM PTW 扩到 3 级 | ⬜ | Layer 4 / 跨模块 |
| G9 | ELF64 little-endian 加载器 + RV64 测试 ELF vendor | ⬜ | 工具链 |

**附加（非准入必需，但影响可用性）**：C 扩展 / A 扩展 / F-D 扩展（README 宣称 `rv64gc` 的 "gc" 三项）、非法指令异常机制、L1 tag 宽度扩到 ≥48-bit。

---

## 9. 维护规则

1. 本表是 RV64 缺口的**唯一登记处**。任何新发现的 XLEN 假设（无论 TLM / CH_MEM / 工具链）必须登记到本表，不得只写在代码注释里。
2. 每条目带 `file:line` 证据。**代码移动行号时需同步本表**（与 ADR-086 §维护规则同约定）。
3. §8 准入清单的勾选状态由 ADR-087 D5 判定，不得在代码合入时单方面打勾。
4. 本表由 `ip/cpu/docs/README.md` 索引；ADR-087 只引用本表，不复制内容。
