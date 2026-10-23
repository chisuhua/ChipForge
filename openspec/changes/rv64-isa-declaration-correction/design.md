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

副产品：现存 6 处测试 `cfg.isa = "rv64gc"`（test_7stage×3 / test_10stage×3）与 1 处 `rv32imac`（test_3stage）**零改动**——reserved warn 设计保证 0 退化自动成立。

## Decision 2 — 警告形态

`std::cerr` 单行：`[declared-reserved] isa="rv64gc" 未实现（预案 v1.x），配置被接受但不参与 dispatch（ADR-087 §D1）`。不做去重缓存（同一进程重复装载同配置会重复 warn——可观测、确定性优先，避免静态缓存引入线程问题）。

## Decision 3 — 编译期护栏落点（D4 提前部分）

- `ip/cpu/arch/riscv/int_alu.h:88` `static T compute(OpCode op, T rs1, T rs2)` 函数体首行
- `ip/cpu/arch/riscv/branch.h:112` `static bool evaluate_branch(std::uint8_t funct3, T rs1, T rs2)` 函数体首行
- 消息串固定：`"RV32 TLM path assumes 32-bit XLEN; RV64 requires the phase-5 block gate see ADR-087 G3"`
- **禁止**文件作用域（文件被 include 即失败的误伤）；既有 `static_assert(std::is_unsigned<T>...)`（int_alu.h:35 / branch.h:35）保留，新护栏与旧断言是**正交的两条约束**（unsigned 语义 vs XLEN 预算）

## Decision 4 — schema 策略（修正 AC3 自相矛盾）

- schema `isa.enum` 收窄至 `["rv32i","rv32im","rv32imac"]`；`rv64i/rv64gc` 从 enum **移到 description** 文档化（"declared-reserved 态运行时可接受并 warn，见 ADR-087 §D1"）
- 理由：schema 是配置的**机器校验层**，只该覆盖可正常生效的取值；reserved 属运行时宽容语义，写入 description 后 AC 的 `grep '"rv64' ip/cpu/configs/`（0 命中）可机械化验证
- `:204` 既有条件约束（rv32i/rv32im/rv32imac ⇒ mmu_mode const sv32）**保持不变且自洽**
- **连带纠偏**：`tools/dse/sweep_config.example.json:8` 的 sweep grid `["rv32i","rv64i"]` → `["rv32i","rv32im"]`（工具链示例不再诱导生成 reserved 值；运行时语义不变）

## Decision 5 — `xlen` 字段与 `isa` 的正交性

`std::uint8_t xlen = 32;`。`xlen` 与 ISA 扩展字母**正交**（`rv32imac` 的 `ac` 与 `xlen` 无关）；单一来源（ADR-087 §D3 的 `xlen_bits<T>`）归 Change 2，本 change 只提供**数据字段** + **交叉校验**，不做 dispatch、不改任何 plugin 签名。

## Decision 6 — 校验函数位置

`ip/cpu/cpu_factory.h` 内 `namespace cf::cpu::detail { inline void validate_isa_xlen(const CPUConfig&, unsigned kernel_xlen); }`，`build_cpu()` 首行调用（`kernel_xlen = sizeof(T) * 8`）。`tools/cpu_sim/main.cpp` 的 `load_config()` 不重复实现校验（单一 SSOT 在 build_cpu 入口）；cpu_sim 仅删除 `cfg.isa = "rv32i"` pin（`:187-188`）与头注释 `:16`（:85 的 JSON 读取保持）。

## Risks（继承 proposal R1–R4 + 实施新增）

- **R5（warning 噪声）**：test 全家在 reserved 值下每 build 一行 warn。可接受：ctest 输出尾部可见但不进 assertion；若 CI 噪声超标，后续小改 → per-process dedupe（YAGNI 现在不做）
- **R6（双 binary 编译面）**：int_alu.h/branch.h 亦被 CH_MEM 路径 include（T=ch_uint<32> 非 4 字节整型，`is_unsigned` 已可编译），护栏条件 `sizeof(T)==4` 对 CH_MEM 侧实例化点是否触发需要在 Task 8 的双 binary 全量 build 实测确认（预期：不触发，因 CH_MEM 侧不实例化 T=整型的这些模板入口；若触发则 CH_MEM 侧本身已在 Change 2 处理范围）
- **R7（`--isa` CLI flag 不存在）**：cpu_sim 无 `--isa` 参数（AC 只约束"若存在"→ 不存在则验收自动满足，plan 中 grep 证实）
