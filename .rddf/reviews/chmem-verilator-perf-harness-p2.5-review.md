# P2.5 Review 报告 — chmem-verilator-perf-harness（交付点 A）

> **change**: `chmem-verilator-perf-harness`（TLM 退役 B1 第 1 项，交付点 A）
> **分支**: `feature/chmem-verilator-perf-harness`（base `2bad6b6`）
> **审查时间**: 2026-10-24 · **审查方式**: AC 逐条机械复核 + 变异对照证据 + commit 卫生 + 变更面核对 + 计划一致性核对

---

## 1. 结论

**PASS —— 建议进入 P3 归档**（交付点 A 可独立归档；交付点 B = CoreMark 移植为独立 change）。

---

## 2. AC 核对（12/12 通过）

| AC | 实测证据 | 结论 |
|----|---------|------|
| AC-1 `TOHOST=1` | before `TOHOST=1498564676 … TIMEOUT` → after **`TOHOST=1 CYCLES=914544 PASS`** | ✅ |
| AC-2 恰好 2 个 PT_LOAD（RX+RW），无纯 R 段 | `readelf -l`：`LOAD 0x80000000 R E` + `LOAD 0x80001000 RW` | ✅ |
| AC-3 `tohost`=0x80001000；`.rodata` ≥ tohost+4 | `readelf -s`：tohost=80001000；`.tohost`@80001000(4B) / `.rodata`@80001004 | ✅ |
| AC-4 可复现构建 | CMake 目标 `dhrystone_elf`（`src/cf_plugin/CMakeLists.txt:136-146`）；`nm -u` **空**；同工具链手工构建**字节一致** | ✅ |
| AC-5 结果正确性硬闸 | 判据 = `TOHOST==1 且非 TIMEOUT`；**实测 `cpu_verilator_sim` 超时也 exit 0** ⇒ 脚本显式判 TIMEOUT 为失败；负例：缺 elf ⇒ exit 1 | ✅ |
| AC-6 cycles 口径 | 脚本只解析 `CYCLES=%u`（`sim_main.cpp` 计数器）；源码内注释显式排除 wall-clock / elaboration / 编译时间 | ✅ |
| AC-7 精确相等 + CI 门禁 | 篡改基线 cycles ⇒ 报出差异 + exit 1；还原 ⇒ PASS；CI step 已接入（steps 11 → **12**） | ✅ |
| AC-8 新 CSV 只含 verilator 行；TLM CSV 零改动 | 新 CSV 1 行 `mode=verilator`；既有 `dhrystone-baseline-matrix.csv` **git diff 空**；TLM 侧实测 **794478 / 1.43277 逐位一致** ⇒ 旧基线仍有效 | ✅ |
| AC-9 CSV 数值实跑产出 + notes 三项标注 | notes 含「裁剪版 workload」+「cycle 与 TLM 不可比（ADR-088 §3）」+「旧单 RWE 段 elf 不可比」 | ✅ |
| AC-10 8 门禁 + 全量回归 0 退化 + 零 `ip/` | 8 门禁全绿；全量 743.36s，失败集合**恰 1 个 pre-existing**（`test_7stage_riscv.cpp:124`）；变更面 **0 个 `.h/.cpp`、0 个 `ip/`** | ✅ |
| AC-11 不改门禁判据值 + ADR owner 登记 | 1.4 / 1.9 / 2.3 原样保留；owner=architect + 窗口（本 change 合入后、② 开工前）已登记（index 注 3 + tasks 留证） | ✅ |
| AC-12 交付点 A 可独立归档 | 本 change **不含** CoreMark ⇒ 不阻塞 ② | ✅ |

---

## 3. 变异对照（3/3 被检出）

| 变异 | 期望 | 实测 |
|------|------|------|
| 缺 elf | 失败 + exit 1 | ✅ `FAIL: 缺少 …/nonexistent.elf` + exit 1 |
| 基线 cycles 篡改 914544→999999 | 报出差异 + exit 1 | ✅ `基线漂移: cycles 实测 914544 != 基线 999999` + exit 1 |
| 还原 | PASS | ✅ `基线一致 (cycles=914544 dmips=1.24467) => PASS` |

---

## 4. 实施期发现并修正的 6 处 plan 缺陷（已回写 plan）

| # | plan v2 的判断 | 实测事实 |
|---|---------------|---------|
| 1 | 「`build_manual_elf.sh` 无 dhrystone 目标 ⇒ 须新增」 | **错** —— 仓内已有 CMake 目标 `dhrystone_elf` ⇒ **不新增**（新增会造成第二条构建路径 ⇒ 字节不一致风险） |
| 2 | 「替换旧 elf」 | 旧 elf **不在 git**（`.gitignore` 的 `*.elf`）⇒ 无仓库产物可替换；权威产物 = `build/dhrystone.elf`；**TLM 基线 CSV 无需改**（逐位一致） |
| 3 | 「结果正确性用 checksum」 | `dhrystone.c` **无 checksum 比对** ⇒ 判据改为 **TOHOST==1 且非 TIMEOUT** |
| 4 | 「CH_MEM 全组合 ⇒ cycle 远小于 TLM」 | **反了** —— CH_MEM **多 15%**（无 load-use 转发）⇒ DMIPS 1.24467 **< 1.4** ⇒ 判据重校准 ADR **从「预防性」变「实测必需」** |
| 5 | 工具链前缀无关 | **有关** —— `riscv32-`（CMake 用）`.text` 0x494 ≡ 旧 elf；`riscv64-` 0x3d4 ⇒ 必须用 CMake 的 riscv32- |
| 6 | 「需要 `--tohost-addr`」 | **不需要** ⇒ Task 4 关闭（前提实测成立：tohost=0x80001000 = `kToHostWord` 0x400，三处一致） |

---

## 5. 已知遗留（不阻塞归档）

| # | 事项 | 归属 |
|---|------|------|
| 1 | **CH_MEM 后端 DMIPS 1.24467 < 1.4 硬门禁** ⇒ 判据如何处置未决 | **architect 显式 ADR**（owner=architect，窗口 = 本 change 合入后、② 开工前）；三选项：① 重校准门禁值 ② **补 CH_MEM 侧 load-use 转发**（reviewer 倾向此项 —— 是架构缺陷而非判据问题，且同时改善 CoreMark）③ 保留 1.4 并标注 CH_MEM 暂不达标 |
| 2 | `dhrystone.c` **无 checksum 比对** ⇒ 「结果正确性」只能到「程序跑完」级 | 后续项（需改 `dhrystone.c` ⇒ 超本 change scope） |
| 3 | 仓库内 `tests/cpu/manual_elf/dhrystone/dhrystone.elf`（gitignored 手工副本）与 CMake 产物**双份** | TLM 测试仍读仓库路径 ⇒ 暂保留并同步为权威字节；**根治**（测试改读 `build/dhrystone.elf` + compile definition）留给后续 change |
| 4 | **CI 不跑 ctest** ⇒ 本 change 的 ctest entry 只是开发者辅助；真正满足 ADR-088 §5.2 判据 4 的是 **CI 门禁 step**（warn 级，不阻塞 PR） | 已按「判据 4 部分满足」登记；升 blocking 需独立 change（与仓库「测试脚本 warn 级」策略冲突） |
| 5 | 交付点 B（CoreMark 移植）未做 | 独立 change `chmem-verilator-coremark-harness`（未立项，含放弃判据 + 降级路径）⇒ **不阻塞 ②**（② 的中间验收可用分支 microbenchmark） |
| 6 | ① 的 `auto_insert_stage_regs` 等 CH_MEM 组合通路的结构性特征未在本 change 处理 | 归 B1 ②/③（BP 的 GShare 历史、Cache 的 tag 区分度） |

---

## 6. 建议

1. **可进 P3 归档**（交付点 A）
2. 归档后**立即**启动 **② `chmem-bp-btb-gshare`**（硬前置已解除；BP 的 AC-8 达标判定依 ① 的实测 + ADR 结论）
3. **architect 需出判据重校准 ADR**（见 §5.1）—— 建议选 ②（补 load-use 转发），因它是架构缺陷而非判据问题
4. **⑤ `chmem-ptw-walk` 批 1**（单元级 PTW/TLB/`FsmBase` 封装）**无前置**，可与 ② 并行启动
