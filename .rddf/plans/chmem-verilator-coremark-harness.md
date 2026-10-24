# ⑤-交付点B chmem-verilator-coremark-harness — 执行文件

> **change**: `chmem-verilator-coremark-harness`（B1 执行图 §5.1 顺序 5 = **交付点 B**）
> **规模**: **大**（CoreMark 全移植）· **前置**: 0c `chmem-branch-microbench` ✅（已归档）· **② `chmem-bp-btb-gshare`**（BP 存在才有意义）
> **优先级**: **最低**（ADR-088 v1.2 §5.3.2 **P-5**：CoreMark 移植**允许顺延**，中间验收改用分支 microbenchmark ⇒ **② 不被本 change 阻塞**）
> **上游**: ADR-088 v1.2 §5.3.2 **P-5**（中间验收口径）· §5.2 判据 2 **BP 专项删除前置**（② + 本 change 是 B3 删除 TLM BP 的**双前置**）· `feat-bp-btb-gshare` 的绝对值验收标准
> **姊妹**: ① `chmem-verilator-perf-harness`（交付点 A = Dhrystone harness，**本 change 复用其全部脚手架**）· 0c `chmem-branch-microbench`

---

## Goal

把 **EEMBC CoreMark** 移植到 CH_MEM/Verilator 后端，产出 **`CoreMark/MHz` 实测值**。

### 为什么它排在最后（ADR-088 v1.2 P-5）

| 项 | 说明 |
|---|------|
| **不是 ② 的前置** | ② 的性能验收已改用 **microbench 中间验收**（0c 已交付）⇒ **② 不被本 change 阻塞** ⇒ 本 change 可**任意延后** |
| **是 B3 删除 TLM BP 的双前置之一** | ADR-088 §5.2 判据 2：删除 `branch_predictor.{h,cpp}` 需同时满足 ① CH_MEM 版存在且已注册（= ②）② **CoreMark harness 跑通并产出实测值**（= 本 change）⇒ **本 change 阻塞 B3，但不阻塞 B1 其余项** |
| **绝对值门禁的载体** | `CoreMark/MHz ≥1.9`（v0.10.0 软门禁）· v1.0.0 `≥2.3` · **HARD 门禁 PoC-14**（FPGA CoreMark vs VexiiRiscv 偏差 ≤10%） |

---

## Architecture

```
tests/cpu/manual_elf/coremark/            (新)
  ├─ coremark.c              (EEMBC 移植: portme + core_list + core_main)
  ├─ coremark_portme.h       (RV32 移植层: 计时器 / 端口 / CRC16 / 内存)
  ├─ coremark_crt0.S         (栈初始化 + tohost 协议, 同 dhrystone_crt0.S)
  └─ coremark_core.h         (EEMBC core 主体, 多文件拆分或单文件)
tests/cpu/manual_elf/link_coremark.ld    (PHDRS 双段, 同 link_dhrystone.ld)
src/cf_plugin/CMakeLists.txt              (改: coremark_elfs 目标)
tools/measure_coremark.sh                 (新: 测量 + 校验 + 漂移检测)
soc/cpu/docs/dse/coremark-baseline-matrix.csv  (新: 基线)
tests/CMakeLists.txt + .github/workflows/architecture-gates.yml  (改: ctest + CI step)
```

### 复用 ① 交付点 A 的全部脚手架（**不要重造**）

| 资产 | 复用方式 |
|------|---------|
| `tests/cpu/manual_elf/link_dhrystone.ld` | **照抄结构** ⇒ `link_coremark.ld`（**PHDRS 双段** —— 必需非可选：CH_MEM 段路由 `is_exec && !is_writable → IBus`，单 RWE 段 ⇒ IBus 为空 ⇒ 取指失败；纯 R 段被**整段丢弃** ⇒ 分数静默错） |
| `src/cf_plugin/CMakeLists.txt:136-146`（dhrystone 目标） | **同构建路径**（同一 `RISCV_GPP` + 同一 flags）⇒ **不走** `build_manual_elf.sh`（避免第二条构建路径 ⇒ 字节不一致，见 ① 实施期修正 #1） |
| `tools/measure_perf.sh` | **照抄结构** ⇒ `tools/measure_coremark.sh`（但判据不同，见下） |
| `soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv` | 同构 ⇒ `coremark-baseline-matrix.csv` |
| `tools/verilator_runner/sim_main.cpp` 的 `tohost==1` 约定 | CoreMark 用**同款**协议（`tohost_pass()` 写 1 后死循环） |

---

## Tech Stack

C++23（宿主侧）· RISC-V RV32IM（`riscv32-unknown-elf-g++`）· 无 libc（自供栈 + 自校验）· CppHDL/Verilator 后端

---

## Tasks

### Task 1: 事实复核 + 移植可行性评估（**先评估，后动手**）

- [ ] **Step 1: EEMBC CoreMark 源码获取与许可核对** —— CoreMark **EEMBC 许可要求**：须保留版权声明 + 标明「在 EEMBC 授权下使用」⇒ **入库前须确认许可条款**（不得整目录照搬而不标注）
- [ ] **Step 2: 内存预算核对（★实测约束）** —— CH_MEM dmem 窗口 = `kMemWords = 16 * 1024` words = **64KB**（`ip/cpu/plugins/dmem_chmem.h:35`）⇒ CoreMark 官方默认 `TOTAL_DATA_SIZE = 2000`（2KB）+ 栈（crt0 设 `sp = 0x8000F000` ⇒ 60K 可用）⇒ **内存不是瓶颈** ⇒ 若移植后溢出，先调 `TOTAL_DATA_SIZE`（CoreMark 官方支持缩减迭代数，**分数按官方规则折算**）
- [ ] **Step 3: 代码体积核对** —— CoreMark 移植后 `.text` 体积 vs 64KB 窗口（`link_dhrystone.ld` 的 `.text` 从 `0x80000000` 起，`.tohost` 段从 `0x80001000` 起 ⇒ **代码区仅 4KB**！）⇒ ⚠️ **实测 `.text` 体积**：若 >4KB ⇒ 须调整 `link_coremark.ld` 的段布局（`.tohost` 的段起始地址可后移，但 `tohost` 须是 RW 段**第一个** output section 且 `set_tohost_probe_word(0x400)` 对齐 ⇒ **该偏移是硬约束**）
- [ ] **Step 4: ISA 需求核对** —— CoreMark 用到 `mul`（`core_list.c` 的乘法）⇒ 需 `rv32im`（现有 dhrystone 用的就是 `rv32im_zicsr`）⇒ **无需新 ISA 扩展**
- [ ] **Step 5: 输出可行性结论（★门禁）** —— 若 Step 3 判定「代码区 4KB 不够」且无法在 `set_tohost_probe_word(0x400)` 约束下解决 ⇒ **出 ADR**（改 `tohost` 探测位置属架构变更）⇒ 不得擅自改 `cpu_factory_chmem.h:313`

### Task 2: 移植实现

- [ ] **Step 1: `portme.h` → `coremark_portme.h`** —— RV32 移植层：`portme_init` / `portme_tick` / `portme_stop` / `portme_read_time` / `portme_write_reg` / `portme_get_time` / 内存初始化 / `ee_printf`（**最小实现**：`putchar` + 十六进制打印，**不引 libc**）
- [ ] **Step 2: CRC16 实现** —— CoreMark 的结果校验依赖 CRC16 ⇒ 必须**逐位正确**（移植最常见的错点 ⇒ **用官方已知向量自测**）
- [ ] **Step 3: `core_list.c` + `core_main.c` 移植** —— 保留 CoreMark 的**算法结构**（`core_bench_list` / `core_bench_matrix` / `core_bench_state`）⇒ **不改算法**（改了就不是 CoreMark）
- [ ] **Step 4: 自校验 + tohost 协议** —— CoreMark 跑完后写 `tohost`：
  - **成功** ⇒ 写 **1**（`tohost==1` ⇒ runner 判 PASS）⇒ 但 **CoreMark 分数本身也要回传** ⇒ 见 Step 5
  - **失败** ⇒ 写非 1 值（runner 判 FAIL，且宿主可从 `TOHOST=` 读出实际值定位）⇒ **结果错时绝不静默**
- [ ] **Step 5: 分数回传机制（★设计要点）** —— CoreMark 的**分数不是整数**（= 迭代数 × 频率 ÷ 总时间）⇒ 无法像 Dhrystone 那样塞进 1 个 word。**方案**：把分数**写进 `tohost` 后面的 dmem word**（`tohost` 之后有 64KB 空间）⇒ runner 需新增一个「读 dmem 指定 word」的选项（`tools/verilator_runner/`）⇒ ⚠️ **这会改 runner** ⇒ 须评估对既有 `verilator_perf_baseline` / `branchbench_baseline` 的**零影响**（新增选项默认关闭 ⇒ 旧路径不变）
- [ ] **Step 6: `coremark_crt0.S`** —— 照抄 `dhrystone_crt0.S`（栈初始化 + `jal _cstart` + `tail tohost_pass`）⇒ ⚠️ `tohost_pass` 必须 `extern "C"`（否则 C++ 名字修饰 ⇒ crt0 的 `tail tohost_pass` 链接失败 —— **实测踩过**）
- [ ] **Step 7: `link_coremark.ld`** —— PHDRS 双段（见 Task 1 Step 3）

### Task 3: 构建 + 测量

- [ ] **Step 1: CMake 目标 `coremark_elfs`** —— 与 dhrystone **同构建路径**（同一 `RISCV_GPP` + flags）
- [ ] **Step 2: `tools/measure_coremark.sh`** —— 照抄 `measure_perf.sh` 结构，但：
  - **结果正确性**判据 = `tohost == 1`（**同 dhrystone**）+ **分数与基线期望值逐位相等**（CoreMark 分数需转成可比整数（如 ×1000 取整）⇒ 逐位可比）
  - **`CoreMark/MHz` 计算** = 官方公式（`iterations × 频率 / 总 ticks`）⇒ **须记录频率假设**（Verilator 无真实频率 ⇒ 用 **cycles/MHz** 口径：`CoreMark/MHz = iterations × 1e6 / cycles`，与 DMIPS 的 `N*1e6/(1757*cycles)` **同构** ⇒ 口径一致可比）
  - **精确相等**漂移检测（无容差，同 ①）
- [ ] **Step 3: 基线 CSV** —— `soc/cpu/docs/dse/coremark-baseline-matrix.csv`（含 `cores_per_mhz` / `cycles` / `expected_tohost` / `expected_score` + notes 标注「BP 前后须分别录」）
- [ ] **Step 4: ctest + CI step** —— `coremark_baseline` entry + warn 级 CI step（★**记住**：CI **不构建项目** ⇒ 该 step 在 CI 中**恒 SKIP** ⇒ 挂载点 ≠ 已生效门禁 ⇒ notes 须如实标注）

### Task 4: 收口

- [ ] **Step 1: 8 架构门禁全 PASS**
- [ ] **Step 2: 全量回归 0 退化**（失败集合恰 1 个 pre-existing）
- [ ] **Step 3: 台账更新** —— `[dhrystone]` 3 条 / `[performance]` / `[cycle-parity]` 中与 CoreMark 相关的条目重判（+ **族级 §2 同步**）
- [ ] **Step 4: ADR-088 §5.2 BP 专项前置状态更新** —— 「② CoreMark harness 跑通并产出实测值」⇒ 勾上 ⇒ **B3 删除 TLM BP 的双前置全部就位**
- [ ] **Step 5: `feat-bp-btb-gshare` 的绝对值验收标准更新** —— 原「CoreMark/MHz ≥1.9」⇒ 以本 change 的**实测值**替换 ⇒ **若实测未达 1.9** ⇒ ⚠️ **不得改判据**（ADR-088 v1.2 **P-1**：不重校准 DMIPS ≥1.4 判据，同理适用于 CoreMark）⇒ 记为**未达标 + 根因分析**，不调标
- [ ] **Step 6: commit**

---

## Acceptance

- [ ] **AC-1** CoreMark 在 CH_MEM/Verilator **跑通**（`tohost == 1` ⇒ runner 判 PASS）且 **TIMEOUT 一律判失败**（不因进程退出码 0 而误判）
- [ ] **AC-2** **CoreMark/MHz 实测值**入库基线 CSV（**含 BP 启用/禁用两行** ⇒ 可量化 BP 增益）
- [ ] **AC-3** **结果正确性由宿主判定** —— 分数回传 + 与基线**逐位相等**；不符 ⇒ **失败且不产出数值**（与 0c `measure_branchbench.sh` 同纪律）
- [ ] **AC-4** **CRC16 逐位正确**（用官方已知向量自测）⇒ 分数不是「算了个数」而是「CoreMark 的数」
- [ ] **AC-5** ELF **PHDRS 双段**（RX `.text` + RW `tohost`+data）· `tohost` = RW 段首个 output section · `nm -u` 空 · 同参数两次构建**字节一致**
- [ ] **AC-6** **构建路径单一** —— 走 CMake `coremark_elfs`（**不走** `build_manual_elf.sh`）
- [ ] **AC-7** ctest entry + CI step（warn 级，**显式标注 CI 不构建 ⇒ 恒 SKIP** ⇒ 挂载点 ≠ 已生效门禁）
- [ ] **AC-8** 8 架构门禁全 PASS + 全量回归 0 退化（失败集合恰 1 个 pre-existing）
- [ ] **AC-9** ADR-088 §5.2 BP 专项前置状态更新 + `feat-bp-btb-gshare` 绝对值验收标准以实测值替换（**未达标则记未达标，不改判据** —— P-1 纪律）

---

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | **代码区仅 4KB**（`.text` 从 `0x80000000` 起，`.tohost` 段从 `0x80001000` 起 ⇒ 中间 4KB）⇒ CoreMark 移植后可能溢出 | Task 1 Step 3 **先实测** `.text` 体积；溢出则调整 `link_coremark.ld` 段布局；**`tohost` 的 `0x400` 对齐是硬约束**（`cpu_factory_chmem.h:313` + `sim_main.cpp:35`）⇒ 若需改 ⇒ **出 ADR** |
| R2 | **CRC16 移植错误** ⇒ 分数「看着对」实则不是 CoreMark 的数 | AC-4 用官方已知向量自测；**不得**只靠「跑完了」判定 |
| R3 | **分数非整数** ⇒ 无法像 Dhrystone 塞进 1 个 word | Task 2 Step 5：写进 `tohost` 后的 dmem word + runner 新增**默认关闭**的读取选项 ⇒ 对既有 2 个基线 ctest **零影响** |
| R4 | **改 runner** ⇒ 影响既有 `verilator_perf_baseline` / `branchbench_baseline` | 新增选项**默认关闭**；AC-8 的全量回归是硬闸 |
| R5 | **EEMBC 许可**未标注 ⇒ 合规风险 | Task 1 Step 1 核对；入库文件头保留版权 + 标注授权 |
| R6 | **规模大 + 优先级最低** ⇒ 长期顺延 ⇒ B3 删除 TLM BP 被卡 | ★这是**已知且接受**的代价（ADR-088 v1.2 P-5 明确允许顺延）⇒ 但须在 B1 收口时**显式登记「B3 阻塞项：本 change 未启动」**，不得假装 B3 可放行 |
| R7 | **绝对值未达标** ⇒ 触发「改判据」压力 | ★**P-1 纪律：不重校准**（同 DMIPS ≥1.4）⇒ 记未达标 + 根因，**不改标**（AC-9） |
| R8 | CoreMark 官方 `TOTAL_DATA_SIZE` 若需缩减 ⇒ **分数须按官方规则折算** | Task 1 Step 2；**不得**直接改数字充数 |

---

## 迁移说明

- **执行顺序**：Task 1（**先评估** —— 尤其 Step 3 的 `.text` 体积）→ Task 2（移植）→ Task 3（构建 + 测量）→ Task 4（收口）
- **与其它 change 的关系**：前置 = 0c ✅ + ②（BP 才有意义）；**产出解除** B3 删除 TLM BP 的第二前置
- **不在本 change 范围**：BP 实现（⇒ ②）· Dhrystone harness（⇒ ① 已归档）· 分支 microbenchmark（⇒ 0c 已归档）· **改判据**（⇒ 需显式 ADR，不属本 change）
