# Tasks — chmem-dmem-read-latency

> **Path**: rdd-builder（本 change = 归位 + 根因固化 + 决策材料；**不实施** A/B/C 任一修复方案）
> **Owner**: ip/cpu
> **Spec refs**: `specs/chmem-dmem-read-latency/spec.md`
> **Related**: `openspec/changes/chmem-alu-unit-semantics/`（同批补的 ALU 语义锚）、上游 CppHDL `openspec/specs/mem-read-timing/spec.md`

## 0. Pre-flight（已完成，作为证据链）

- [x] 0.1 确认 10 个 FAIL 的精确集合与 tohost/cycle（`lb`/`lbu`=11/102、`lh`=5/62、`lhu`=5/64、`lw`=3/57、`sb`=3/51、`sh`=3/51、`sw`=3/53、`ld_st`=3/55、`st_ld`=3/47）
- [x] 0.2 确认失败面精确等于"读内存"（其余 29 个无 load 的 rv32ui 全 PASS）
- [x] 0.3 反汇编 `rv32ui-p-lw` test_2 ⇒ 纯 load 读 `begin_signature`（ELF 预载静态数据），排除"store→load"假设
- [x] 0.4 确认 ELF 预载/读地址一致（预载 word offset = 读端 `bits<15,2>` = 0x800 = 2048）⇒ 排除寻址错位
- [x] 0.5 回退对照实验 ⇒ 与 M 扩展 change 无关（pre-existing）
- [x] 0.6 确认 `[verilator]` 同批 ELF 全 PASS ⇒ 差异在后端，不在插件逻辑
- [x] 0.7 读上游契约 R3/R4 + `docs/simulation/ARCHITECTURE.md` "Memory read port timing"
- [x] 0.8 **实测** aread 1-cycle 滞后（drift pin 探针，逐拍打印）

## 1. 固化（根因可执行锚）

- [x] 1.1 新增 `tests/cpu/test_cpu_chmem_mem_read_drift.cpp`：drift pin（断言当前 6/6 滞后）
- [x] 1.2 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` 加入该文件（显式列表）
- [x] 1.3 文件头注释写明上游契约引用 + "上游若修则本测试 FAIL 是期望行为"

## 2. 归位（消除"无台账"）

- [x] 2.1 `AGENTS.md` known-issues 段新增该条（含根因 + owner + 方案 A/B/C 指针 + 10 个 ELF 清单）
- [x] 2.2 明确 owner 归属：**违约方是 ChipForge CH_MEM dmem**（上游行为 by-contract）

## 3. 决策材料

- [x] 3.1 proposal.md 记录方案 A（`sread` backend 同构，代价 = 数据通路级改动）
- [x] 3.2 记录方案 B（上游把 async 读端口归 combinational；上游 R3 是 hard requirement ⇒ 需上游重新决策）
- [x] 3.3 记录方案 C（上游提供显式 backend 语义开关）

## 4. 验证

- [x] 4.1 `[mem-drift]` 1 case PASS
- [ ] 4.2 `[chmem]` 全套零新增 FAIL（family 66 → 67 case，10 个 pre-existing FAIL 不变）
- [ ] 4.3 5 架构门禁 exit 0
- [ ] 4.4 `openspec change validate chmem-dmem-read-latency` → 0 error
- [ ] 4.5 `openspec archive` + `openspec validate --specs --strict` 新 spec PASS

## 5. 后续（不在本 change）

- [ ] 5.1 方案 A/B/C 决策（需评审；A 涉及 CH_MEM 数据通路级改动）
- [ ] 5.2 决策后开实施 change，把 10 个 ELF 转绿
- [ ] 5.3 同步更新 `feat-phase-6d-rtl-verification`（该 feature 是这 10 个 FAIL 的既有 owner）

---

**总计**: ≈ 16 tasks（本 change 只做归位与固化；10 个 ELF 转绿在 5.x）
