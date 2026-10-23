# Design — rv64-isa-declaration-correction

> 上游：ADR-087（§D1/D2/D4/D6）· proposal.md · spec `cpu-isa-xlen-contract`。
> 本文件记录实施层决策；语义裁决冲突由本 design 归档（proposal/spec 已同步）。

## Decision 1 — fail-fast 语义的三层分解（修正 proposal/spec 初稿口径）

初稿把「isa 含 rv64 且 xlen==32」直接 fail-fast，与 ADR-087 §D1 三态表（declared-reserved = 允许但 warn）矛盾：`rv64gc` 同时落进两个验收场景。**裁决**（以 ADR-087 §D1 为准）：

| 判定 | 输入 | 行为 |
|------|------|------|
| `unsupported` | `isa` ∈ schema enum∪reserved 全集（`rv32i/rv32im/rv32imac/rv32imac*/rv64i/rv64gc` 之外，例 `rv128i`） | **throw**（`load_config()` / `build_cpu()` 均抛 `std::invalid_argument`） |
| `implemented`（`rv32i/rv32im`） | 与 `xlen == 32` 冲突（显式 `xlen==64`） | **throw** |
| `declared-reserved`（`rv32imac/rv64i/rv64gc`） | 任意 | **接受 + emit 一次 warning**（`std::cerr`，每条配置装载 ≤1 行） |
| 交叉校验（模板侧） | `build_cpu<T>` 中 `xlen != sizeof(T)*8` | **throw**（第二层 fail-fast；`CpuFactory<uint64_t>` 主要由 int_alu/branch 的 `static_assert` 编译期拒绝，此运行时层兜底非模板泛化入口） |

副产品（**已按 Decision 7 修正**）：初稿曾设想"reserved warn 设计 ⇒ 现存 6 处测试 `cfg.isa = "rv64gc"` 零改动"。critic 实测（`nm -C` 扫 `.o`）证明 `RiscvIntAluPlugin<unsigned long>` / `RiscvBranchPlugin<unsigned long>` **今天已被实例化** ⇒ 护栏与既有 T64 测试互斥，零改动不成立。处置见 Decision 7。

## Decision 2 — 警告形态

`std::cerr` 单行：`[declared-reserved] isa="rv64gc" 未实现（预案 v1.x），配置被接受但不参与 dispatch（ADR-087 §D1）`。不做去重缓存（同一进程重复装载同配置会重复 warn——可观测、确定性优先，避免静态缓存引入线程问题）。

## Decision 3 — 编译期护栏落点（D4 提前部分）

- `ip/cpu/arch/riscv/int_alu.h:88` `static T compute(OpCode op, T rs1, T rs2)` 函数体首行（`build()` 内的真实 ALU 运算经此入口 ⇒ 覆盖真实路径）
- `ip/cpu/arch/riscv/branch.h:111` `static bool evaluate_branch(std::uint8_t funct3, T rs1, T rs2)` 函数体首行 **+ `branch.h` 的 `build()` 闭包体内首行**（真实分支判定路径；只护静态入口 ⇒ 真实 BLT/BGE 判定不受覆盖，critic SHOULD_FIX 6）
- 消息串固定：`"RV32 TLM path assumes 32-bit XLEN; RV64 requires the phase-5 block gate see ADR-087 G3"`
- **禁止**文件作用域（文件被 include 即失败的误伤）；既有 `static_assert(std::is_unsigned<T>...)`（int_alu.h:35 / branch.h:35）保留，新护栏与旧断言是**正交的两条约束**（unsigned 语义 vs XLEN 预算）

## Decision 4 — schema 策略（修正 AC3 自相矛盾）

- schema `isa.enum` 收窄至 `["rv32i","rv32im","rv32imac"]`；`rv64i/rv64gc` 从 enum **移到 description** 文档化（"declared-reserved 态运行时可接受并 warn，见 ADR-087 §D1"）
- 理由：schema 是配置的**机器校验层**，只该覆盖可正常生效的取值；reserved 属运行时宽容语义，写入 description 后 AC 的 `grep '"rv64' ip/cpu/configs/`（0 命中）可机械化验证
- `:204` 既有条件约束（rv32i/rv32im/rv32imac ⇒ mmu_mode const sv32）**本身正确**（XLEN=32 下只有 Sv32 合法）—— 但初稿误判为"保持不变即自洽"。**事实**：3 个 JSON 的 `mmu_mode` 是 sv39/sv48（继承自 rv64gc 时代的虚假声明）⇒ 只改 `isa` 不改 `mmu_mode` 会让这 3 个配置**变成 schema 非法**。处置：3 个 JSON 的 `mmu_mode` 同步纠偏为 `sv32`（`enable_mmu=false` 下路径 inert，行为不变，回归 `[mmu]`/`[cpu-l1-mmu-demo]` 兜底）
- **连带纠偏**：`tools/dse/sweep_config.example.json:8` 的 sweep grid `["rv32i","rv64i"]` → `["rv32i","rv32im"]`（工具链示例不再诱导生成 reserved 值；运行时语义不变）

## Decision 5 — `xlen` 字段与 `isa` 的正交性

`std::uint8_t xlen = 32;`。`xlen` 与 ISA 扩展字母**正交**（`rv32imac` 的 `ac` 与 `xlen` 无关）；单一来源（ADR-087 §D3 的 `xlen_bits<T>`）归 Change 2，本 change 只提供**数据字段** + **交叉校验**，不做 dispatch、不改任何 plugin 签名。

## Decision 6 — 校验函数位置

`ip/cpu/cpu_factory.h` 内 `namespace cf::cpu::detail { inline void validate_isa_xlen(const CPUConfig&, unsigned kernel_xlen); }`，`build_cpu()` 首行调用（`kernel_xlen = sizeof(T) * 8`）。`tools/cpu_sim/main.cpp` 的 `load_config()` 不重复实现校验（单一 SSOT 在 build_cpu 入口）；cpu_sim 仅删除 `cfg.isa = "rv32i"` pin（`:187-188`）与头注释 `:16`（:85 的 JSON 读取保持），并**补 `cfg.xlen` 解析**（schema 已声明 `xlen`，`load_config` 静默丢弃 = `cpu-pipeline-tick-fix` F1 同型 bug，critic SHOULD_FIX 5）。

⚠️ **落点约束（critic B4）**：`namespace cf::cpu::detail` 的原块在 `cpu_factory.h:113` 闭合，**早于** `struct CPUConfig`（:124）的定义 ⇒ helper 无法放回原块；但 `namespace cf { namespace cpu {` 已在 `:115-116` 重新打开（`:520-521` 才闭合）⇒ helper 落点只需**重开 `detail` 一层**（写 `namespace cf { namespace cpu { namespace detail {` 会产生 `cf::cpu::cf::cpu::detail` ⇒ 落笔即错，同名陷阱）。

## Decision 7 — T64 实例化点迁移（用户裁决：保留编译期护栏）

ADR-087 AC2 要求 `CpuFactory<uint64_t>` **编译失败**；critic 实测（`nm -C` 扫 `build/tests/CMakeFiles/**/*.o`）证明 `RiscvIntAluPlugin<unsigned long>` / `RiscvBranchPlugin<unsigned long>` 今天已被 `tests/cpu/test_cpu_factory.cpp` 与 `tests/cpu/test_branch_predictor_runtime_btb.cpp` 实例化 ⇒ AC2 与「0 退化」在现状下**数学上不可同时成立**。

**裁决（用户，2026-10-23）**：迁移 T64 调用点，保留编译期护栏。

| 位置 | 处置 | 理由 |
|------|------|------|
| `test_cpu_factory.cpp` `build_cpu_rv64` case | **删除** | 它测的是"能否构建 64-bit 内核"= 未实现能力的虚假探测，与本 change 目标冲突 |
| `test_cpu_factory.cpp` 其余 2 处 T64 | 改 T32 | 保留"11 plugins 注册"等断言价值 |
| `test_branch_predictor_runtime_btb.cpp` 3 case | `uint64_t` → `uint32_t` | BTB 与 XLEN 无关，**保留 BTB 覆盖**（不删测试） |
| integration 侧 7 处 `cfg.isa` | `rv64gc`/`rv32imac` → `rv32im` | 仓库内不留 reserved 值；reserved 语义由新增专用测试覆盖 |

计数影响：`[cpu]` 129 → 128（删 1 case）+ 5（契约用例）+ 4（测试完备性补全）= **137**（非退化，AC9 已按此校准）。RV64 真实路径归 ADR-087 Change 2（phase-5）。

## Risks（继承 proposal R1–R6 + 实施新增）

- **R5（warning 噪声）**：test 全家在 reserved 值下每 build 一行 warn。可接受：ctest 输出尾部可见但不进 assertion；若 CI 噪声超标，后续小改 → per-process dedupe（YAGNI 现在不做）
- **R6（双 binary 编译面）**：**实测 CH_MEM 侧不 include `int_alu.h`/`branch.h`**（`cpu_factory_chmem.h` 只 include `*_chmem.h`；`nm chipforge_tests_chmem` 只有 `compute_static`/`safe_divisor_u`）⇒ 护栏**不触发**。初稿理由（"CH_MEM 下 T=ch_uint<32>"）不成立，已更正——真实风险是 `int_alu_chmem.h` 为**同名类**（`#ifdef` 隔离），护栏落点若误改到 chmem 变体才会打断 CH_MEM 编译。处置：Task 8 双 binary 全量 build 实测
- **R7（`--isa` CLI flag 不存在）**：cpu_sim 无 `--isa` 参数（AC 只约束"若存在"→ 不存在则验收自动满足，plan 中 grep 证实）
- **R8（schema 有效性基线）**：4 个配置 JSON 中 3 个因**顶层 `_comment`** 撞 schema `additionalProperties: false` 而**今天已非法**（pre-existing；`tools/test_doc_checker.py:129-132` 捕获 `ValidationError` 后 pass ⇒ CI 从不报）。本 change 只保证**不新增**违规（`isa`/`mmu_mode` 组合满足 `:204` 条件），`_comment` 问题登记为基线、不在本 change 修
