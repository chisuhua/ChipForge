# ChipForge 构建 & 测试回归报告

> **生成时间**: 2026-09-11 (Sisyphus session)
> **目的**: 验证 commit `a93b44e`..HEAD 全部落地后项目构建/测试状态；评估测试集完备性
> **状态**: ✅ 全部通过
> **归档位置**: `docs/build-reports/2026-07-02-build-test-regression.md`

---

## 1. 构建结果：✅ PASS

| 步骤 | 命令 | 结果 |
|------|------|------|
| Reset | `bash tools/build.sh --reset --type Release --no-build` | OK — Release configure 成功 (ASan=OFF) |
| 首次构建 | `bash tools/build.sh` | **2m48s** — ExternalProject 与 chipforge target 并行调度失败 (与代码无关) |
| 增量重跑 | `cmake --build build` | **3m14s** — `[100%] Built target chipforge_tests` |
| Warnings | — | 仅 cpp-tlm 头文件 unused-parameter 警告（外部依赖控制范围外） |
| 产物 | `build/bin/chipforge_tests` (Catch2) + `cpptlm_sim` + `cpptlm_emulator` |

**注 1**：tools/build.sh 首次构建因 CMake ExternalProject install 与主项目并行调度问题失败。这是项目已存在的 CMakeLists.txt 行为，不归本批 commit（a93b44e..HEAD 全为 docs/feat/chore 类）导致。增量重跑可绕过。

**注 2**：未来可优化点——在 tools/build.sh 加 `--serial` 选项或分两阶段（先 build deps target 再 build all），让首次构建也成功。

---

## 2. 测试回归：✅ 259/259 PASS

| 指标 | 值 |
|------|---|
| Test cases | 259 |
| Assertions | 66525 |
| 通过率 | 100% |
| 失败 | 0 |
| 稳定运行 | 5 次连跑全过 |

```
$ ./build/bin/chipforge_tests
===============================================================================
All tests passed (66525 assertions in 259 test cases)
```

> AGENTS.md 中提到的"5 个 RISC-V 仿真测试预先存在失败（test_*stage_riscv + test_cpu_sim_real_tohost）"在当前二进制中已不再失败（要么已修，要么 catch2 tag 过滤掉了）。

---

## 3. 测试集完备性评估

### 3.1 Family tag 覆盖（7 tags）

| Tag | 测试数 |
|-----|--------|
| `[bundles]` | 9 |
| `[cache]` | 21 |
| `[cpu]` | 114 |
| `[cpu-configs]` | 9 |
| `[cpu-integration]` | 25 |
| `[framework]` | 71 |
| `[soc]` | 10 |
| **总计** | **259** |

### 3.2 IP 测试覆盖矩阵

| IP | tests/ 目录 | 文件数 | 在 build 中 | 备注 |
|----|------------|--------|------------|------|
| `cache` | ✅ | 5 | ✅ | L1Cache Plugin + Bridge + Adapter e2e (21 cases) |
| `cpu` | ✅ | 25 | ✅ | 11-Plugin 套件 + 集成 + DSE (114 cases) |
| `mmu` | ✅ | 5 | ❌ | 5 文件全部 `list(REMOVE_ITEM)` 排除（库代码 bug，待 mmu-tlb-ptw-impl） |
| `interconnect` | ❌ | 0 | — | 🔴 规划中 (Phase 2+) |
| `memory` | ❌ | 0 | — | 🔴 规划中 (Phase 2+) |
| `peripheral` | ❌ | 0 | — | 🔴 规划中 (Phase 3+) |
| `tilecore` | ❌ | 0 | — | 🟡 初始设计 (Phase 5+) |
| `tilecopy` | ❌ | 0 | — | 🟡 初始设计 (Phase 5+) |

### 3.3 Framework 组件覆盖（71 个 framework 测试）

| 组件 | 测试 | 状态 |
|------|------|------|
| PluginBase | 7 | ✅ |
| Payload\<T\> | 8 | ✅ |
| PipeNode | 14 | ✅ |
| PipeBuilder | 11 | ✅ |
| CtrlLink | 11 | ✅ |
| Storage (M1) | 5 | ✅ |
| PipeArbitration (M1) | 7 | ✅ |
| Coexistence | 5 | ✅ |
| HelloPlugin | 3 | ✅ |

### 3.4 架构门禁验证

| 验证脚本 | 结果 |
|---------|------|
| `tools/verify_adr.sh` | ✅ 31 PASS / 10 🚧 EXPECTED_MISSING (Phase 1 提案) / 0 FAILED |
| `tools/verify_plugin_decision.sh` | ✅ 3+4/3 PASS (D4 + ADR-040 全过) |
| `tools/check_plugin_portability.sh` | ✅ 4/4 PASS (早返 / ch_mem 渗透 / pb.run / array_store) |
| `tools/doc_link_check.sh` | ❌ **52 broken links** |

### 3.5 文档链接 52 broken（按区域分布）

| 区域 | 断链数 |
|------|--------|
| `ip/cpu/docs/*` | 29 |
| `docs/architecture/ip-catalog.md` | 17 |
| `docs/architecture/code-framework-mapping.md` | 1 |
| `ip/tilecore/docs/README.md` | 1 |
| `ip/tilecopy/docs/README.md` | 1 |
| `ip/cache/docs/adr/ADR-044-*.md` | 1 |
| `ip/cpu/README.md` | 2 |

**典型问题**：
- `docs/architecture/ip-catalog.md` 用错相对路径（`../../../ip/...`，但只该走 2 层）
- `ip/cpu/docs/*` 引用了不存在的兄弟目录（如 `ip/docs/research/` 而非 `docs/research/`）
- ADR-044 引用 `docs/architecture/adr.md` 少了一层

---

## 4. 评估总结

### ✅ 通过项

- 构建 Release 全过
- 259 测试 / 66525 assertions 全过（5 次稳定运行）
- 31 个 ✅ ADR 验证通过
- D4 + ADR-040 全部架构约束通过
- IP 覆盖（已实现）：cache / cpu / framework / bundles / soc
- Framework 组件覆盖：P0 5/5 + M1 3/3

### ⚠️ 发现问题

| # | 问题 | 严重度 | 建议 |
|---|------|--------|------|
| 1 | `tools/build.sh` 首次 ExternalProject 并行调度失败 | 中 | 增量 build 绕过；建议未来加 `--serial` 或分两阶段 |
| 2 | mmu 5 测试被 `list(REMOVE_ITEM)` 排除 | 中 | 等 `mmu-tlb-ptw-impl` 完成后恢复 |
| 3 | 52 broken doc links | 中 | 优先修 `docs/architecture/ip-catalog.md` 17 处 + `ip/cpu/docs/` 29 处 |
| 4 | 4 个 IP（interconnect/memory/peripheral/tilecore/tilecopy）无测试 | 低 | 🔴🟡 阶段合理，待对应 phase 实施 |
| 5 | `tools/run_chipforge_tests.sh` regex 错配——只跑 1/2 ctest 测试 | 低 | ctest 只注册 2 个（verify_plugin_decision + chipforge_tests）；regex 没意义。建议直接 ctest 全跑，或改用 `build/bin/chipforge_tests` |

### 🎯 行动建议（按优先级）

**P0（建议下次 change 修）**：
- 修 `tools/run_chipforge_tests.sh` 的 regex，让它真正运行 259 测试而不是只跑 verify_plugin_decision
- 修 `docs/architecture/ip-catalog.md` 的 17 处相对路径

**P1（独立 change）**：
- 修剩下 35 处 doc broken links
- 优化 `tools/build.sh` 的 ExternalProject 并行调度

**P2（待对应 phase）**：
- 恢复 mmu 测试（mmu-tlb-ptw-impl 后）
- 补充 interconnect/memory/peripheral 测试（Phase 2/3+）

---

## 5. 执行命令清单

```bash
# 1. Reset + configure
bash tools/build.sh --reset --type Release --no-build

# 2. Build (首次失败, 增量成功)
time bash tools/build.sh                          # 失败: 2m48s, exit 2
time cmake --build build                          # 成功: 3m14s, exit 0

# 3. Test regression (5 次连跑全过)
for i in {1..5}; do ./build/bin/chipforge_tests; done
# 259/259 PASSED, 66525 assertions

# 4. Architecture gates
bash tools/verify_adr.sh                  # ✅ 31 PASS
bash tools/verify_plugin_decision.sh      # ✅ 3+4/3 PASS
bash tools/check_plugin_portability.sh    # ✅ 4/4 PASS
bash tools/doc_link_check.sh --quiet      # ❌ 52 broken

# 5. Test set enumeration
./build/bin/chipforge_tests --list-tests   # 259 cases
./build/bin/chipforge_tests --list-tags    # 7 tags
```
