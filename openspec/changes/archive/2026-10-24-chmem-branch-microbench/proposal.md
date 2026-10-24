---
initiative: wave6-linux-and-productization
priority: P1
version_target: v0.11.0-prep
status: ready
created_from: .rddf/plans/chmem-branch-microbench.md
adr:
  - ADR-088
  - ADR-040
---

# chmem-branch-microbench

## Why

**B1 ②（`chmem-bp-btb-gshare`）目前没有可判定的性能验收手段**：

| 现状 | 问题 |
|------|------|
| ② 的原判据 = 「CoreMark/MHz ≥1.9」 | ① CoreMark 资产**零**（48 个 vendored ELF 全是 rv32ui/rv32um）⇒ 需 EEMBC 全移植（含 `portme.h` / `ee_printf` / CRC / 内存预算，而 CH_MEM dmem 只有 **64KB**）⇒ 工作量大且**不确定** |
| CoreMark 绝对分数在 CH_MEM 后端 | ADR-088 v1.2 §5.3.2 **P-5** 已裁定：绝对值判据待 ADR，且 CH_MEM 的绝对值**本身受缺 BP 影响**（P-6：差距根因 = 无 BP）⇒ 用绝对分数判 BP 会**自证失败** |
| 缺 BP 时的**参照基线** | ② 需要「启用 BP vs 禁用 BP」的**同负载**对比，而 Dhrystone 是**计算密集**（不敏感于分支预测）⇒ 无法度量 BP 收益 |

⇒ 需要一个**分支敏感**、**可控预测率**、**结果可校验**、**构建可复现**的微基准：它同时提供 ② 的**中间验收**手段与**无 BP 参照值**。

## What Changes

### In Scope（5 项）

1. **分支微基准程序** —— `tests/cpu/manual_elf/branchbench/branchbench.c`（+ `branchbench_crt0.S`）：4 档**可控预测率**负载（`always_taken` / `always_not_taken` / `pattern_6of8` / `pseudo_random`），**4 档的 `.text` 逐字节相同**（唯一差异 = 数据里的 mode 字）⇒ 每档执行的指令数完全一致 ⇒ **cycle 差只来自分支方向**
2. **链接脚本** —— `tests/cpu/manual_elf/link_branchbench.ld`：**PHDRS 双段**（RX `.text` + RW `tohost`+data）—— 必需，非可选（CH_MEM 段路由 `is_exec && !is_writable → IBus`，单 RWE 段 ⇒ IBus 为空 ⇒ 取指失败；纯 R 段被**整段丢弃** ⇒ 分数静默错）
3. **可复现构建** —— CMake 目标 `branchbench_elfs`（4 个 ELF，**与 dhrystone 同构建路径**：`riscv32-unknown-elf-g++ -O2 -fno-inline -march=rv32im_zicsr`）—— 刻意**不走** `build_manual_elf.sh`（避免第二条构建路径 ⇒ 字节不一致）
4. **测量脚本 + 基线** —— `tools/measure_branchbench.sh`（4 档测量 + **宿主侧校验和比对** + 基线漂移**精确相等**检测）+ `soc/cpu/docs/dse/branchbench-baseline-matrix.csv`（**无 BP 参照值**）
5. **CI 门禁 + ctest** —— `architecture-gates.yml` 新增 warn 级 step + `tests/CMakeLists.txt` 新增 `branchbench_baseline` entry

### Out of Scope（5 项）

- ❌ **不实现分支预测器**（那是 ② `chmem-bp-btb-gshare` 的范围）—— 本 change 只交付**测量能力**与**无 BP 参照值**
- ❌ **不移植 CoreMark**（交付点 B，`chmem-verilator-coremark-harness`，未立项）
- ❌ **不改 `ip/cpu/**`** —— 零生产代码改动
- ❌ **不改 `enable_cache` / hazard / 任何数据通路**
- ❌ **不引入绝对值判据**（本基线只做**相对**比较；绝对值待 ADR-088 v1.2 §5.3.2 P-6）

### 变更清单（7 个文件）

| 文件 | 动作 | 具体 |
|------|------|------|
| `tests/cpu/manual_elf/branchbench/branchbench.c` | **新增** | 4 档负载 + 校验和回传 |
| `tests/cpu/manual_elf/branchbench/branchbench_crt0.S` | **新增** | 栈初始化 + tohost 协议 |
| `tests/cpu/manual_elf/link_branchbench.ld` | **新增** | PHDRS 双段 |
| `src/cf_plugin/CMakeLists.txt` | 改 | 4 个 ELF 目标（`branchbench_elfs`） |
| `tools/measure_branchbench.sh` | **新增** | 测量 + 校验和 + 漂移检测 |
| `soc/cpu/docs/dse/branchbench-baseline-matrix.csv` | **新增** | 无 BP 参照基线 |
| `.github/workflows/architecture-gates.yml` + `tests/CMakeLists.txt` | 改 | CI step + ctest entry |

## Capabilities

- **BP 收益可量化**：同一负载下「启用 BP vs 禁用 BP」的 Δcycles ⇒ ② 的性能 AC 有**可机械判定**的锚（不依赖 CoreMark 绝对分数）
- **结果正确性宿主判定**：程序回传 32-bit 校验和（LSB 置 1 使 runner 的 done 位恒成立 ⇒ cycle 计数 = 真实完成时刻）⇒ 结果错时**立即失败**，不产出数值
- **构建可复现**：CMake 单一路径 + 4 档 `.text` 逐字节一致 ⇒ 负载差异**只有**分支方向一个变量

## Impact

### 风险

1. **R1 workload 差异污染对比** —— 各档若生成的指令不同 ⇒ cycle 差混入工作量 ⇒ **已消除**：mode 放在 `volatile` 全局（不可常量折叠）⇒ 4 档 `.text` sha256 一致（实测 `5a7c0b2c4265c156`）
2. **R2 伪随机档被简单预测器学到** ⇒ 下界参照失真 ⇒ 用固定 seed 的 LCG + 2000 迭代；且判据是**相对**（BP 前后同档对比），不依赖绝对下界
3. **R3 「启用/禁用 BP」缺开关** ⇒ ② 须提供**可关闭 BP 的配置面**（已登记为 ② 的接口需求，写入 ② plan）
4. **R4 微基准绝对值不代表真实应用** ⇒ 基线 notes 显式标注；只用于相对比较
5. **R5 双构建路径** ⇒ 与 dhrystone 同走 CMake（不走 `build_manual_elf.sh`）

## Acceptance

- [ ] AC-1 4 个 ELF 均为 **RX+RW 两段**、`tohost`=0x80001000、`nm -u` 空、两次构建**字节一致**
- [ ] AC-2 4 档 `.text` **sha256 完全一致** ⇒ 指令数相同 ⇒ cycle 差只来自分支方向
- [ ] AC-3 4 档均跑完（`TOHOST` = 校验和|1，LSB=1 ⇒ done 位成立 ⇒ cycle 计数真实）+ **宿主侧校验和与基线逐位相等**（不符 ⇒ 失败且不产出数值）
- [ ] AC-4 基线 CSV 入库（4 档 = **无 BP 参照值**）+ `--check-baseline` **精确相等**检测（漂移 ⇒ exit 1）
- [ ] AC-5 ctest entry + CI 门禁 step（warn 级，缺 binary/elf 时显式 SKIP 不误判通过）
- [ ] AC-6 8 架构门禁全 PASS + 全量回归 0 退化 + **零 `ip/` 改动**
- [ ] AC-7 **不依赖 CoreMark** ⇒ ② 的中间验收不被交付点 B 阻塞（ADR-088 v1.2 §5.3.2 P-5）

## References

- ADR-088 **v1.2** §5.3.2 **P-5**（中间验收改用分支 microbenchmark）· §5.3.2 **P-6**（绝对值门禁恢复条件 = ② 落地）· §5.3.1（根因 = 无 BP）
- ADR-040 v3.0（payload key 模态 / 顺序契约）· ADR-083（双文件分离）
- plan: `.rddf/plans/chmem-branch-microbench.md` · 姊妹: `chmem-verilator-perf-harness`（① 交付点 A，已归档）
