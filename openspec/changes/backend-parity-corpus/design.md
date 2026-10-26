# backend-parity-corpus — 设计

> 契约正文（`docs/architecture/parity-gate-contract.md` v1.5）是唯一引用来源。

---

## 1. 语料集合：两侧 `kCases` 必须相等

| 侧 | 文件 | 数量 |
|---|---|---|
| Simulator | `tests/cpu/test_cpu_chmem_vendored_elf.cpp` | 47 → **48**（补 `rv32ui-p-simple`） |
| Verilator | `tests/cpu/test_cpu_verilator_sim.cpp` | 48 |

⇒ 交集 **48**，**零漂移**。门禁的 `check_corpus_against_harnesses()` **方向性断言**：
清单 ⊄ 某侧 ⇒ `ERROR`（真漏检）；某侧 ⊄ 清单 ⇒ `WARNING`（覆盖超集）。
★ 本 change 后两侧都 ⊆ 清单 ⇒ **两个方向都干净**（此前是「Verilator 侧覆盖超集 ⇒ WARNING」）。

## 2. ★★ 跑批前抓到的 3 个真缺陷（都会让首跑结果**不可信**）

### 缺陷 1：partial 语义的**假绿通道**（★ 最严重）

branchbench 在 Simulator 侧 cap ≤ 20k 拍 ⇒ **跑不到 done** ⇒ 两侧 `tohost` **恒为 0**。
原 `judge()` 不知道 case 是否 `done` ⇒ 「两侧 `tohost` 相等（0 == 0）」会被计为**通过证据**
⇒ ★ **假绿**：门禁会因为「两边都没写 tohost」而报 PASS。

⇒ **修**：新增 **S-9a partial 语义** —— 两侧都 `done=false` ⇒ `tohost`/`cycles` 标 **vacuous**
（不计入承重判据）⇒ 承重判据 = `regs` + `memsig` + `pchash`；报告 MUST 标 `partial_cases`。
★ 承重判据**未削弱**：`pchash` 是**逐拍 PC 流 hash** ⇒ 仍是 S-9 强度序里的最强判据。
★ 另一侧（两侧 `done` 不一致）判 **FAIL**（不是 `ERROR`）—— 那是一后端跑完、另一后端没跑完 ⇒ **真分歧**。

### 缺陷 2：Verilator 的 `--work-dir` / `--verilog` **全局共享**

`cpu_verilator_sim` 默认 `--work-dir /tmp/cpu_vl_sim` + `--verilog /tmp/cpu.v`
⇒ ★ **并行跑批时后一个 case 会覆盖前一个的 `top.v` 与 `obj_dir/`** ⇒ **静默错测**（测的是上一个 case 的网表）。
⇒ **修**：门禁按 case 分配 `work_dir/vl/<case>/` + `top.v` ⇒ 隔离。
★ 附带修：路径 MUST 是**绝对路径** —— `cpu_verilator_sim` 内部执行 `cd <work-dir> && verilator <verilog_path>`
⇒ 相对路径在 `cd` 后失效（实测：手工跑成功、门禁跑失败 ⇒ 定位到此处）。

### 缺陷 3：**per-case cap 缺失**

ELF 需 ~1k 拍、branchbench 需 ~86k 拍（差 ~30 倍）⇒ 单一全局 cap 必然二选一失败。
⇒ **修**：清单第 5 列 `max_cycles`（可选）+ 按 `kind` 默认 cap（`elf`=3000 · `branchbench`=20000）。

## 3. 并行化

两侧 runner 是**独立子进程**（各自独立 `ch::core::context`、独立 work-dir）⇒ **线程池安全**。
⇒ `--jobs`（默认 4）⇒ 52 case 串行 ≈ 3 h → 并行 ≈ 45~60 min。
★ 报告 MUST 按**清单顺序**排列（并行完成顺序不定 ⇒ 重排以保证报告可 diff）。

## 4. ★ branchbench 4 档的角色（两用）

| 用途 | 数据 |
|---|---|
| **parity** | 跨后端 5 观测量比对（**partial**：承重判据 = `regs`/`memsig`/`pchash`） |
| **BP 量收益** | 同一批负载的 `cycles` ⇒ 与 `branchbench-baseline-matrix.csv` 对照 |

⇒ ★ **这正是用户裁决 #4「A3 跑批与 BP 增量 3 量收益跑批合并」的落点**。
★ **本次未合并**：BP 增量 3 的 1.2 量收益**被架构签批卡住**（1.2 → 1.1 三件套未落地 → 1.0 的 D10 未签）
⇒ 按 ADR-089 R5 的兜底条款「放弃合并、分开跑（机时不省，但门禁不延期）」处理。
⇒ ★ 但**双产物的接口已就位**：门禁的 `cycles` 观测量 + branchbench 的清单条目 = 合并跑批所需的全部数据。

## 5. 首跑 ⇒ 分歧清单的分类法

| 分类 | 判定依据 | 处置 |
|---|---|---|
| **上游待对齐** | 两后端的**语义**不同（不是 bug，是 CppHDL Simulator vs 生成的 Verilog 行为差异） | 向上游提 issue + 记收敛期限 |
| **有意例外** | 语义**等价但表示不同**（如哈希口径、采样点） | 白名单登记（S-11 的 5 必填字段）+ 作用域 |
| **真 bug** | 一侧违反 RISC-V 语义 | 独立修复 change |
