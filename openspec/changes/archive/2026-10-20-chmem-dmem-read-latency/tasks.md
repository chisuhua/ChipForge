# Tasks — chmem-dmem-read-latency

> **Path**: rdd-builder（本 change = 归位 + 根因固化 + 决策材料；**不实施** A/B/C 任一修复方案）
> **Owner**: ip/cpu
> **Spec refs**: `specs/chmem-dmem-read-latency/spec.md`
> **Related**: `openspec/changes/archive/2026-10-20-chmem-alu-unit-semantics/`（同批补的 ALU 语义锚）、上游 CppHDL `openspec/specs/mem-read-timing/spec.md`

## 0. Pre-flight（已完成，作为证据链）

- [x] 0.1 确认 10 个 FAIL 的精确集合与 tohost/cycle（`lb`/`lbu`=11/102、`lh`=5/62、`lhu`=5/64、`lw`=3/57、`sb`=3/51、`sh`=3/51、`sw`=3/53、`ld_st`=3/55、`st_ld`=3/47）
- [x] 0.2 确认失败面精确等于"读内存"（其余 29 个无 load 的 rv32ui 全 PASS）
- [x] 0.3 反汇编 `rv32ui-p-lw` test_2 ⇒ 纯 load 读 `begin_signature`（ELF 预载静态数据），排除"store→load"假设
- [x] 0.4 确认 ELF 预载/读地址一致（预载 word offset = 读端 `bits<15,2>` = 0x800 = 2048）⇒ 排除寻址错位
- [x] 0.5 回退对照实验 ⇒ 与 M 扩展 change 无关（pre-existing）
- [x] 0.6 确认 `[verilator]` 同批 ELF 全 PASS ⇒ 差异在后端，不在插件逻辑
- [x] 0.7 读上游契约 R3/R4 + `docs/simulation/ARCHITECTURE.md` "Memory read port timing"
- [x] 0.8 **实测** aread 1-cycle 滞后（drift pin 探针，逐拍打印）
- [x] 0.9 **独立佐证**（2026-10-21）：对全部 47 个 vendored ELF 逐个 `objdump -d` 统计
      load / store 指令条数 ⇒ FAIL 集合与"含 load"集合逐个精确重合（FAIL 组 load 数
      19～193；PASS 组 29 个 + rv32um 8 个 load 数**全为 0**）⇒ 失败面独立于 tohost
      观测被确证为"读内存"。
- [x] 0.10 **复核 tohost 观测可信性**（2026-10-21）：`tohost_probe` 亦走 `aread`，但其
      地址为常量（word offset `0x400`）⇒ 滞后只影响"读到新值晚一拍"不改稳态值；且
      观测值与 harness `gp|1` 协议自洽（`lb`→11⇒gp=10、`lh`→5⇒gp=4、其余→3⇒gp=2），
      逐条与反汇编 `li gp,N` 吻合。

## 1. 固化（根因可执行锚）

- [x] 1.1 新增 `tests/cpu/test_cpu_chmem_mem_read_drift.cpp`：drift pin（断言当前 6/6 滞后）
- [x] 1.2 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` 加入该文件（显式列表）
- [x] 1.3 文件头注释写明上游契约引用 + "上游若修则本测试 FAIL 是期望行为"
- [x] 1.4 **断言强度加固**（2026-10-21）：除 `obs != 当前地址`（6/6 失配）外，额外断言
      `obs == 上一拍地址的内容`（6/6）。仅断言 `!=` 无法区分 1-cycle 与 N-cycle 滞后
      （2-cycle 滞后同样满足 `!=`），故必须钉死漂移**幅度**才是真正的 "one-tick skew" pin。
- [x] 1.5 **受控实验确证顺序成因**（2026-10-21）：同一 CppHDL 下仅交换"读端口"与
      "消费寄存器"的创建先后，实测消费方捕获值分别为 `mem[areg-1]`（当拍）与
      `mem[areg-2]`（上一拍）⇒ 证明 node id / 创建顺序**确实**决定滞后是否传导，
      而非推断。CH_MEM CPU 属后者（regfile id 702 < `load_read` id 1707）。

## 2. 归位（消除"无台账"）

- [x] 2.1 `AGENTS.md` known-issues 段新增该条（含根因 + owner + 方案 A/B/C 指针 + 10 个 ELF 清单）
- [x] 2.2 明确 owner 归属：**违约方是 ChipForge CH_MEM dmem**（上游行为 by-contract）

## 3. 决策材料

- [x] 3.1 proposal.md 记录方案 A（`sread` backend 同构，代价 = 数据通路级改动）
- [x] 3.2 记录方案 B（上游把 async 读端口归 combinational；上游 R3 是 hard requirement ⇒ 需上游重新决策）
- [x] 3.3 记录方案 C（上游提供显式 backend 语义开关）

## 4. 验证

- [x] 4.1 `[mem-drift]` 1 case PASS（3 assertions，含 1.4 的漂移幅度断言）
- [x] 4.2 `[chmem]` 全套零新增 FAIL（family **72 → 73 case**，实测 73 case / 72 PASS / 1 failed，
      10 个 pre-existing FAIL 不变；15706 assertions / 15696 PASS / 10 failed）
- [x] 4.3 5 架构门禁 exit 0（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability`
      / `doc_link_check` / `roadmap-r9-check`）
- [x] 4.4 `openspec validate --changes`（`openspec change validate` 已 deprecated）→ 0 error
- [x] 4.5 `openspec archive` + `openspec validate --specs --strict` 新 spec PASS
- [x] 4.6 **文档数字与实测对齐**（2026-10-21）：修正 proposal/tasks 的 `[chmem]` family
      计数 `66→67` → 实测 `72→73`（与 commit `4a06f8c` 自身 AGENTS.md diff 的 72→73 一致）
- [x] 4.7 **断链修复**（2026-10-21）：`openspec/changes/chmem-alu-unit-semantics/` →
      `openspec/changes/archive/2026-10-20-chmem-alu-unit-semantics/`（该 change 已 archive；
      `doc_link_check.sh` 不扫 `openspec/`，故 CI 门禁未捕获）

## 5. 后续（不在本 change）

- [x] 5.1 方案 A/B/C 决策（2026-10-21 **实测裁决 → 方案 B**）：
      方案 A（dmem 改 `sread`）单独实施实测**恶化**（`[chmem]` 10 → 11 FAIL，
      10 个 ELF 依旧全红）；方案 B（上游 `async_read` 归组合 + 数据流边）
      实测 ChipForge `[chmem]` **10 → 0 FAIL**、47 ELF 全 PASS、`[verilator]` 不变、
      **ChipForge 业务代码零改动** ⇒ 采纳 B，A 放弃，C 保留为上游只能小步改时的备选。
      依据见本 proposal §选型建议（2026-10-21 修订段）
- [ ] 5.2 决策后开实施 change，把 10 个 ELF 转绿（**转上游实施**：
      `CppHDL/openspec/changes/sim-mem-read-timing-align-2026-10/`，
      commit `cc41a21` 立项 + `bd56b4c` 补实测决策证据；ChipForge 侧待上游落地后
      升级依赖 + 重跑 `[chmem]` 全套 + 把 `[mem-drift]` pin 从"钉死已知缺陷"
      改为"确认 skew 已消除"）
- [x] 5.3 同步更新 `feat-phase-6d-rtl-verification`（该 feature 是这 10 个 FAIL 的既有 owner）
      —— "已知问题（owned）"段 + 修复方向改为方案 B + 上游 change 指针 + ChipForge 侧待办

---

**总计**: ≈ 25 tasks（本 change 只做归位与固化；10 个 ELF 转绿在 5.x）
