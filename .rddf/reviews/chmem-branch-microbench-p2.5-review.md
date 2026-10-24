# P2.5 Review 报告 — chmem-branch-microbench

> ⚠️ **档名已互换重命名**（`chmem-branch-microbench-fix`，2026-10-24）：
> 本文件出现的 `always_taken` / `always_not_taken` 是**旧名**（与硬件方向相反）⇒ 对应关系：
> 旧 `always_taken`(=66034) ⇒ 新 **`always_not_taken`**(=86034)；旧 `always_not_taken`(=68034) ⇒ 新 **`always_taken`**(=88034)。
> ★ 旧 cycles 数值（66034/68034/79534/73012）**已作废**（工作量污染）⇒ 新值见 `soc/cpu/docs/dse/branchbench-baseline-matrix.csv`。
> ★ 本文件**不是**执行依据（0c 已归档）；前置1 的执行依据 = `.rddf/plans/chmem-branch-microbench-fix.md`。


> **change**: `chmem-branch-microbench`（B1 的 **0c** 项：② 的中间验收载体 + 无 BP 参照值）
> **分支**: ⚠️ **实施直接在 `main` 上完成**（builder 疏漏：建 change 时**忘记 `git checkout -b`**，与 2026-10-24 早先那次 C change 同型疏漏）
> —— 已补建 ref `feature/chmem-branch-microbench` 指向实施终态 `ca53fa9` 供追溯；4 个 commit 线性落在 main（无分叉）⇒ 技术无害，但**工作流惯例未遵守**
> **审查时间**: 2026-10-24 · **审查方式**: AC 逐条机械复核 + 变异对照证据 + commit 卫生 + 变更面核对 + 计划一致性核对

---

## 1. 结论

**PASS —— 建议进入 P3 归档**。

---

## 2. AC 核对（7/7 通过）

| AC | 实测证据 | 结论 |
|----|---------|------|
| AC-1 4 ELF 两段 + tohost + nm -u 空 + 字节一致 | `readelf -l`：`LOAD 0x80000000 R E` + `LOAD 0x80001000 RW`；`tohost`=0x80001000；`nm -u` 空；同参数两次构建 `cmp` 一致 | ✅ |
| AC-2 4 档 `.text` sha256 一致 | **4 档全部 `5a7c0b2c4265c156`** ⇒ 指令数相同 ⇒ cycle 差只来自分支方向 | ✅ |
| AC-3 4 档跑完 + 宿主侧校验和逐位相等 | `TOHOST` = 校验和\|1（103/183/147/101，LSB 均为 1 ⇒ done 位成立 ⇒ cycle 计数真实）；校验和不符 ⇒ exit 1 且不产出数值 | ✅ |
| AC-4 基线入库 + 精确相等漂移检测 | 4 档**无 BP 参照值**；变异 B（cycles 篡改）⇒ 报出差异 + exit 1 | ✅ |
| AC-5 ctest + CI 门禁 | `branchbench_baseline` PASS（82.16s，TIMEOUT 300）；CI steps 12 → **13**，缺 binary/elf 显式 SKIP。⚠️ **CI 不构建项目 ⇒ 该 step 恒 SKIP ⇒ 挂载点 ≠ 已生效门禁**（判据 4 仍未满足，已登记 B3 前置） | ✅（含诚实限定） |
| AC-6 8 门禁 + 全量回归 0 退化 + 零 `ip/` | 8 门禁全绿；全量回归见 §4；变更面 `ip/` 改动数 = **0** | ✅ |
| AC-7 不依赖 CoreMark | 本 change 不含任何 CoreMark 资产 ⇒ ② 的中间验收不被交付点 B 阻塞（ADR-088 v1.2 P-5） | ✅ |

---

## 3. 变异对照（2/2 被检出）

| 变异 | 期望 | 实测 |
|------|------|------|
| 基线期望校验和 103 → 999 | 检出「结果错」+ exit 1 | ✅ `校验和不符 —— 实测 TOHOST=103 != 基线期望 999` + exit 1 |
| 基线 cycles 66034 → 99999 | 检出「基线漂移」+ exit 1 | ✅ `基线漂移: 基线 99999 != 实测 66034` + exit 1 |
| 还原 | 4 档 PASS | ✅ `基线一致 (4 档) => PASS` + exit 0 |

---

## 4. 实测留证

| 项 | 实测 |
|---|---|
| 4 档 `.text` sha256 | `5a7c0b2c4265c156`（4 档**完全一致**） |
| 无 BP 参照值 | `always_taken` 103 / **66034** · `always_not_taken` 183 / **68034** · `pattern_6of8` 147 / **79534** · `pseudo_random` 101 / **73012** |
| 形态观察 | 方向**恒定**的档（m0/m1）cycle **最少**；方向**变化**的 m2 **最多** ⇒ CH_MEM 分支代价与**方向变化次数**相关 ⇒ **正是 BP 可消除的部分** ⇒ 微基准对 BP 敏感（这修正了 plan v1 的 AC-3 假设方向） |
| ctest | `branchbench_baseline` PASS（82.16s） |
| 8 门禁 | 全绿 |
| 零 `ip/` 改动 | `git status` 中 `ip/` 改动数 = **0** |

---

## 5. 实施期发现并修正的 5 处 plan 缺陷（已回写 plan）

| # | plan v1 的设计 | 实测问题 | 修正 |
|---|---------------|---------|------|
| 1 | 期望校验和做**编译期常量** | ① 折进 `.text` ⇒ **4 档机器码不同** ⇒ cycle 差混入工作量差异（实测 mode2/3 明显更慢）② 比较行为异常 | **期望值移到宿主侧**：程序恒写 `sum\|1`，期望值存基线 CSV |
| 2 | 模式做**编译期常量** | 编译器折叠 select ⇒ 各档指令流不同 | 模式改 `volatile` 全局（运行时读，不可折叠） |
| 3 | 走 `build_manual_elf.sh` | dhrystone 已有 CMake 目标 ⇒ 新增 shell 路径 = **双构建路径**（字节不一致风险） | 改走 CMake（`branchbench_elfs`） |
| 4 | `tohost_pass` 直接定义 | C++ 名字修饰 ⇒ crt0 `tail tohost_pass` **链接失败** | 加 `extern "C"`（与 dhrystone.c 同构） |
| 5 | AC-3 假设「恒 taken 档收益最大」 | 实测形态相反：方向**恒定**的档 cycle **最少** | AC-3 改为**机制化**判据：「方向**变化**的档 Δcycles 显著大于方向恒定的档」（恒定档本就无变化惩罚） |

---

## 6. 已知遗留（不阻塞归档）

| # | 事项 | 归属 |
|---|------|------|
| 1 | **「启用/禁用 BP」的开关面尚未存在** ⇒ ② 落地后需提供（`CPUConfig` 字段或等价）才能做 BP 前后对比 | **② 的接口需求**（已写入 ② plan 的 Task 6 Step 4 + microbench plan R3） |
| 2 | 绝对值判据（CoreMark ≥1.9 / DMIPS ≥1.4）在 CH_MEM 侧**仍未达标** | ADR-088 v1.2 §5.3.2 **P-6**：恢复条件 = ② 落地（同源：差距根因 = 无 BP） |
| 3 | 微基准**不代表真实应用**（裁剪版负载）⇒ 只用于**相对**比较 | 基线 CSV notes 已显式标注 |
| 6 | ★**CI 门禁 step 在 CI 中恒 SKIP** —— CI 的 `architecture-gates.yml` **不构建项目**（唯一 cmake 是构建 Verilator 本身）⇒ 该 step 缺 `build/bin/cpu_verilator_sim` ⇒ 走 SKIP ⇒ **零实际保护** ⇒ **ADR-088 §5.2 判据 4 仍未满足** | 已在 ①/0c 的 tasks + spec + ADR-088 §5.3.3 **诚实更正**（原表述「判据 4 已部分满足」站不住）；「**CI 具备 build 能力**」登记为 **B3 前置** |
| 4 | 伪随机档可能被简单预测器部分学到 ⇒ 下界参照偏乐观 | 判据是**相对**（BP 前后同档）⇒ 不影响结论；notes 已标注 |
| 5 | 本 change 的基线是**无 BP**参照值 ⇒ ② 落地后须**重录**该 CSV（BP 值） | ② 的 Task 6 Step 4 |

---

## 7. 建议

1. **可进 P3 归档**
2. 归档后**立即启动 ② `chmem-bp-btb-gshare`**（硬前置已全解除：① 交付点 A 已归档 + 0c 微基准已就位）；② 须提供 BP 开关面 + 落地后重录 0c 基线
3. ③/④/⑤ 可并行启动（⑤ 批 1 `chmem-ptw-walk-core` 无前置）
