# P2.5 Review 报告 — rv64-isa-declaration-correction

> **change**: `rv64-isa-declaration-correction`（ADR-087 Change 1）
> **分支**: `feature/rv64-isa-declaration-correction`（base `efab12b`）
> **审查时间**: 2026-10-24
> **审查范围**: 实施结果（13 Task + 追加 Task 14 测试完备性补全）vs plan / proposal / spec / ADR-087
> **审查方式**: AC 逐条机械复核 + commit 卫生核对 + 变更面核对 + 变异对照证据复核

---

## 1. 结论

**PASS —— 建议进入 P3 归档**（唯一未闭合项：critic 第 4 轮 plan 复审尚未回，结论到达后若报 BLOCKER 需先修再归档）。

---

## 2. AC 逐条核对（10/10 通过）

| AC | 判据 | 实测证据 | 结论 |
|----|------|---------|------|
| AC1 | 三层 fail-fast 语义生效 | `tests/cpu/test_cpu_factory_isa_xlen.cpp` **9 cases / 20 assertions** PASS（默认值契约 / unsupported throw / implemented 冲突 throw / T↔xlen 交叉 throw / reserved 单发 warn + 3 个 helper 直调用例） | ✅ |
| AC2 | `CpuFactory<uint64_t>` 编译失败 | 3 处 `static_assert(sizeof(T)==4)`（int_alu 1 + branch 2）；T64 调用点已迁移（`grep` = 0）；`nm` 扫全部 `.o` 无 `unsigned long` 实例化；变异对照：改 8 → `static assertion failed: RV32 TLM path assumes 32-bit XLEN` → 还原计数 2 → 编译恢复 | ✅ |
| AC3 | 声明面无虚假 rv64 | `grep -rn '"rv64' ip/cpu/configs/ soc/ tools/dse/sweep_config.example.json` = **0**；schema enum = 3 值（不含 rv64*）+ default `rv32im` + description 文档化 rv64i；3 JSON `mmu_mode=sv32` 满足 `params.allOf` 条件约束；Draft7 校验 4×OK | ✅ |
| AC4 | cpu_sim 无硬编码 pin | `grep -n 'isa="rv32i"' tools/cpu_sim/main.cpp` = **0**（含注释）；无 `--isa` flag；`load_config()` 已解析 `xlen`（e2e 证明：xlen=64 ⇒ 非 0 退出 / xlen=32 ⇒ 退出 0） | ✅ |
| AC5 | 死代码归零 | `ip/cpu/plugins/reg_file.cpp` 已删（commit `7e494a2`）；`grep -rn "RegFilePlugin<std::uint64_t>" ip/` = 0；双 binary build 通过 | ✅ |
| AC6 | 缺口表状态列一致 | §2 的 0.1–0.6 / 0.9 / 0.10 → ✅（含具体修复说明）+ §1 `cpu_sim` 行标注 + 行号口径 blockquote 更新 | ✅ |
| AC7 | CI 软门禁上线 | `tools/check_xlen_hygiene.sh`（基线 `warn=0` / 探针 `warn=1` / `STRICT_XLEN_GATE=yes` exit 1）+ `architecture-gates.yml` 接入（warn 级）+ `tools/README.md` 登记 | ✅ |
| AC8 | 6 门禁全 PASS | verify_adr ✅ · verify_plugin_decision 8/8 ✅ · check_plugin_portability 13/13 ✅ · doc_link_check 0 broken ✅ · roadmap-r9-check 11 fragments ✅ · check_xlen_hygiene warn=0 ✅ | ✅ |
| AC9 | 0 退化 | `[cpu]` **137**（480 assertions）· `[cpu-integration]` 96 = 95 + 1 pre-existing · `[riscv-tests],[mmu],[cache]` 132 PASS · `[cpu-configs]` 13/13 · chmem binary **130**（`[chmem]` tag 89）PASS · TLM 全量 493 = 492 + 1 pre-existing | ✅ |
| AC10 | change 合法 | `openspec change validate rv64-isa-declaration-correction` = valid（0 error；warning 均为 RFC 2119 措辞，历史风格） | ✅ |

---

## 3. commit 卫生核对（8 批，逐批核对文件清单）

| commit | 内容 | 路径限定核对 |
|---|---|---|
| `0ae4826` | 契约 + T64 迁移 | `tests/` + `ip/cpu/cpu_factory.h`（8 文件）✅ |
| `c08aa37` | 声明面纠偏 | `ip/cpu/configs/` + `tools/dse/sweep_config.example.json`（6 文件）✅ |
| `3f226ef` | 护栏 + cpu_sim 去 pin | `ip/cpu/arch/riscv/` + `tools/cpu_sim/main.cpp`（3 文件）✅ |
| `7e494a2` | 删 reg_file.cpp | `ip/cpu/plugins/reg_file.cpp`（D）+ plan 口径修正 ✅（**不带 `git add`** —— 已删路径 `git add` 会 exit 128） |
| `a1e4236` | 软门禁 + 缺口表 | 4 文件 ✅ |
| `749ed2c` / `065b66a` / `e0887c1` | 台账 + state | ✅ |
| `81a0fe2` / `9a730ba` | 测试完备性补全 + 计数同步 | ✅ |

**index 无残留**（`git diff --cached` 空）⇒ 批 4 的删除确实进了 commit，未滞留。

---

## 4. 变更面核对（无 plan 外意外改动）

`git diff main...HEAD --name-only` 共 31 个文件。其中不属于 A 的 plan 范围的仅 3 个，均为**流程/并行任务产物**：
- `.rddf/state/builder/rv64-isa-declaration-correction.json`（builder handoff）
- `openspec/changes/chmem-tlm-backend-freeze/{design.md,tasks.md}`（投案 B 的 P1 产物，用户批准的并行任务）

**无意外文件** ✅

---

## 5. 实施过程中被审查发现并修复的问题（4 轮 critic + 2 次自检）

| 轮次 | 发现数 | 典型 |
|------|--------|------|
| critic 第 1 轮 | 6 BLOCKER / 7 SHOULD_FIX | T64 实例化点与编译期护栏互斥（用户裁决：迁移 T64 保留护栏） |
| critic 第 2 轮 | 5 BLOCKER / 6 SHOULD_FIX | hygiene allowlist 漏 2 文件（实测 9 命中 9 文件） |
| critic 第 3 轮 | 5 BLOCKER | 新测试文件含 reserved 输入 vs 残留 grep 自相矛盾；JSON 行内注释会崩解析；中间态 FAIL 实为 7 个（实编译验证） |
| builder 自检 1 | 2 真 bug | allowlist 全等比较在绝对路径探针下全误报；helper 落点同名陷阱（`cf::cpu::cf::cpu::detail`） |
| builder 自检 2 | 1 真 bug | hygiene 脚本 allowlist 匹配口径（后缀匹配） |
| 用户要求「测试完备性」 | 3 覆盖缺口 | helper 直调分支不可达 / cpu_sim xlen 解析无测试 / schema+JSON 声明值无断言 |

**所有变异对照均被抓住**（护栏 1 个、hygiene 3 个、补全测试 3 个）⇒ 测试具备真实检验力，非「跑通即算」。

---

## 6. 已知遗留（不阻塞归档，需登记）

| # | 事项 | 归属 |
|---|------|------|
| 1 | `tests/cpu/integration/test_7stage_riscv.cpp:124` node_count `9 == 17` —— **pre-existing**（HEAD 即红），口径 = 不新增失败 | 独立跟踪项（非本 change） |
| 2 | `mul.h` / `mul_div_fsm.h` 无编译期护栏 | ADR-087 Change 2（phase-5），R4 已登记 |
| 3 | 测试不进 CI（Oracle 2026-10-06 决策）⇒ 本 change 的 137+132 case 仍无 CI 保护 | 既有决策；本 change 新增的 hygiene 门禁已进 CI（warn 级） |
| 4 | `cpu_sim` 走 `build_cpu_impl` ⇒ 异常被收敛为 `PluginError::BuildFailed`（无 message 通道）⇒ 运行时错误信息不含具体原因 | 既有接口约束；如需保留原因需改 `PluginError`（超本 change 范围） |
| 5 | `ip/cpu/plugins/mmu_exception_handler.h` 是 TLM-only（`cpu_factory_chmem.h:54-56` 的 `#ifndef` 包裹） | 已登记进投案 B 的共享清单口径修正 |
| 6 | schema 顶层 `_comment` 撞 `additionalProperties:false`（3 份配置今日已非法） | pre-existing 基线，design R8 登记，不在本 change 修 |

---

## 7. 建议

1. **可进 P3 归档**（待 critic 第 4 轮 plan 复审结论到达；若有 BLOCKER 先修再归档）
2. 归档前先合并 `main`（当前 12 commits 未合、未 push）—— 合并与 push 需用户确认
3. 归档后按 ADR-087 推进 Change 2（phase-5）：`mul.h`/`mul_div_fsm.h` 护栏 + `isa → T` 真 dispatch
4. 投案 B（`chmem-tlm-backend-freeze`）可启动实施：其 baseline 锚点须在 A 合并后采集（否则 A 的 `validate_isa_xlen` 会被判为「新增函数」假阳性）
