---
id: tests-soc-topology-restructure
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P2
manual_deps: []
review_by: 2026-12-31
---

# tests-soc-topology-restructure

> 把 `tests/soc/` 顶层 5 cpp 测试按 SoC 拓扑（与 `soc/*.json` 一一对应）拆为 3 个子目录：
> `tests/soc/soc-cpu-l1-mmu-demo/` + `tests/soc/mmu-minimal/` + `tests/soc/l1-cache-minimal/`。
>
> **本文件是 rdd-planner 起的 5 段草稿**（per add-improve HARD-GATE），下一步由 rdd-builder P0 落地 openspec `proposal.md` + `tasks.md`。

## 1. Why

### 项目上下文（v0.11.0 Wave 5 收官窗口 + 后 Wave 6 治理窗口）

本次梳理产出（`/tmp/opencode/code-test-ownership-report.md §C.3` + `§F`）发现：`tests/soc/` 现在是单一平铺目录，**5 个测试文件按时间累积混在一起**，与 `soc/*.json` 失去一一对应：

| 测试文件 | 真正测的 SoC 拓扑 | 对应 SoC JSON | 当前归属 |
|---------|---------------|-------------|---------|
| `tests/soc/test_cpu_l1_mmu_demo.cpp` | CPU + MMU + Memory（3 IP + memory）| `soc/cpu_l1_mmu_demo.json` | soc 涉 cpu |
| `tests/soc/page_table_helpers.h` | helper（被 test_cpu_l1_mmu_demo.cpp include）| n/a | soc 涉 cpu |
| `tests/soc/test_mmu_minimal_json.cpp` | MMU 独立（单 IP） | `soc/mmu_minimal.json` | soc 涉 mmu |
| `tests/soc/test_cache_params_schema_json.cpp` | L1Cache 独立（单 IP） | `soc/l1_cache_minimal.json` (schema 验证) | soc 涉 cache |
| `tests/soc/test_soc_l1_cache_minimal_json.cpp` | L1Cache 独立（单 IP） | `soc/l1_cache_minimal.json` | soc 涉 cache |

### 本 change 要解决的架构债

把 `tests/soc/` 拆为**按 SoC 拓扑**（与 `soc/*.json` 一一对应）的子目录：
- ✅ **意图清晰**：未来加新 SoC JSON 时，建 `soc/<name>.json` + `tests/soc/<name>/test_<name>.cpp` + `tests/soc/<name>/README.md` 三件套。
- ✅ **拓扑归属清晰**：每个子目录自描述 SoC 拓扑，README 直接交叉引用 `soc/<name>.json`。
- ✅ **避免 "tests/soc/cpu/" 的误导方案**：原方案会把 mmu / cache 独立测试强塞 cpu/ 下，违反归属原则。

### 替代方案被拒绝的原因

| 方案 | 拒绝原因 |
|------|---------|
| `tests/soc/cpu/` + `tests/soc/mmu/` + `tests/soc/cache/` (按 IP 拆) | 违反 SoC 集成测试意图——SoC 测试跨多个 IP，按 IP 拆会让 "测试 CPU + MMU + Memory" 的测试散落在多目录 |
| `tests/soc/{cpu,mmu,cache}/<test>.cpp` | 同上，且与 `soc/*.json` 无一一对应 |
| **本 change 方案：** `tests/soc/<soc-name>/` (按 SoC 拓扑名) | ✅ 与 `soc/*.json` 一一对应，对未来扩展友好 |

## 2. What Changes

### 变更范围

- **新建 3 个子目录** + **git mv 6 个文件**：

| 新位置（目标） | 老位置（来源） |
|---------------|---------------|
| `tests/soc/soc-cpu-l1-mmu-demo/test_cpu_l1_mmu_demo.cpp` | `tests/soc/test_cpu_l1_mmu_demo.cpp` |
| `tests/soc/soc-cpu-l1-mmu-demo/page_table_helpers.h` | `tests/soc/page_table_helpers.h` |
| `tests/soc/soc-cpu-l1-mmu-demo/README.md` | **新建** |
| `tests/soc/mmu-minimal/test_mmu_minimal_json.cpp` | `tests/soc/test_mmu_minimal_json.cpp` |
| `tests/soc/mmu-minimal/README.md` | **新建** |
| `tests/soc/l1-cache-minimal/test_cache_params_schema_json.cpp` | `tests/soc/test_cache_params_schema_json.cpp` |
| `tests/soc/l1-cache-minimal/test_soc_l1_cache_minimal_json.cpp` | `tests/soc/test_soc_l1_cache_minimal_json.cpp` |
| `tests/soc/l1-cache-minimal/README.md` | **新建** |
| `tests/soc/README.md` | **更新**（顶层索引指向 3 个 SoC 子目录） |

- **索引文件**：每个新子目录 `README.md` 含 SoC 拓扑说明 + `soc/<name>.json` 链接 + Family tag 列表。

### 引用分类与处理（精确扫描结果）

| # | 引用类型 | 数量 | 处理动作 |
|---|---------|------|---------|
| 1 | 真实 `#include "page_table_helpers.h"` | 1 处 (test_cpu_l1_mmu_demo.cpp, 相对路径) | include 路径随 git mv 自动跟随，**0 动作** |
| 2 | `tests/CMakeLists.txt` 路径引用 | 0 处（用 `file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*/test_*.cpp")`） | **0 动作** |
| 3 | `tools/migrate_to_catch2/migrate.py` 映射规则 | 1 处 (`'tests/soc': 'soc'`) | 改 dir 映射为 `'tests/soc/<name>': 'soc/<name>'` 模板（如未来还要用，否则忽略） |
| 4 | 活 openspec changes/ (ACTIVE) | 4 处 (mmu-chmem-pipeline-integration + mmu-chmem-phase-b-c-launch + mmufault-verilator-sv32-e2e-flip + cpu-factory-satp-mapping) | 批量改 `tests/soc/test_xxx.cpp` 路径 |
| 5 | openspec/changes/archive/ (HISTORICAL) | 23 处（archived 不可改） | **保留 + 在 doc_link_check KNOWN_LEGACY 加 list** |
| 6 | openspec/specs/ (跨版本 SSOT spec) | 6 处 | 改 spec 路径 |
| 7 | `.rddf/plans/` + `.rddf/roadmap/archive/` + `.rddf/roadmap/strategy.md` | 9 处 | 改路径 |
| 8 | `docs/superpowers/plans/` | 1 处 | 改路径 |
| 9 | `soc/cpu_l1_mmu_demo.json` description 引用 | 1 处 | 改 JSON 描述中的路径字符串 |
| 10 | `AGENTS.md` + `CHANGELOG.md` | 大量 | 改路径 |
| 11 | `ip/cache/README.md` 等 ip/ 内 README | ~3 处 | 改路径（仅涉及 cache reference） |

### 非目标（Out of Scope）

- **不动业务 .cpp 语义**（仅 include 相对路径跟随 git mv 自动消解）。
- **不改 SoC JSON 命名**（`soc/cpu_l1_mmu_demo.json` 等保持原名，不变 `soc/cpu/l1_mmu_demo.json`）。
- **不动 `tests/{cpu,cache,mmu}/` IP 测试目录**（它们已按 IP 同构）。
- **不动 `openspec/changes/archive/`** 内容（archive 不可变）。
- **不增删任何 ctest / Catch2 test case**。

## 3. Acceptance

### 必达（Hard）

- [ ] `tests/soc/` 下建 3 个子目录：`soc-cpu-l1-mmu-demo/` + `mmu-minimal/` + `l1-cache-minimal/`。
- [ ] `git mv` 6 文件到新位置（保留 git history）。
- [ ] 新建 3 个子目录 README.md（含 SoC 拓扑说明 + `soc/<name>.json` 链接 + family tag 表）。
- [ ] 更新 `tests/soc/README.md` 顶层索引。
- [ ] 全仓 `grep -rn 'tests/soc/test_' | grep -v '/archive/'` 返回 **0 hits**（即活 openspec + docs + AGENTS + CHANGELOG + .rddf + soc/*.json 全部已改）。
- [ ] 全仓 `grep -rn 'tests/soc/' | grep '/archive/'` 仍命中 23 处（归档不可变 + doc_link_check KNOWN_LEGACY 已添加）。
- [ ] `bash tools/doc_link_check.sh` 0 fail（除 openspec/changes/archive/ legacy 例外）。
- [ ] `bash tools/run_chipforge_tests.sh` 0 regression（`[soc]` 5 cpp + `[cpu-l1-mmu-demo]` 6 case + `[mmu]` 59 + `[cache]` 7 全 PASS）。
- [ ] `cmake --build build -j$(nproc)` 0 error 0 warning 新增（catch2 测试 CMake 重 configure 应一次过，因为 `file(GLOB_RECURSE)` 自动发现）。

### 应达（Soft）

- [ ] `soc/cpu_l1_mmu_demo.json` description 引用路径同步。
- [ ] AGENTS.md 内 1 处 `tests/soc/test_cpu_l1_mmu_demo.cpp:113` 引用同步。
- [ ] CHANGELOG.md 多处 `tests/soc/test_cpu_l1_mmu_demo.cpp:124` 路径同步。

### 治理（Meta）

- [ ] openspec change 提案 `proposal.md` 含 frontmatter `initiative: wave5-isa-coverage-and-bp`。
- [ ] `openspec archive tests-soc-topology-restructure` 成功（仅 1 个 change）。
- [ ] 本 improvement 草稿状态从 `draft` → `approved` / `archived`。
- [ ] `openspec/specs/soc-cpu-mmu-demo-topology/spec.md` 等 6 个 spec 文件路径同步。

## 4. Capabilities

新增能力：

- **SoC 拓扑测试可独立演进**：未来新增 `soc/foo.json` 时，建 `tests/soc/foo/test_foo.cpp`，每 SoC 拓扑独立 git history、独立 release notes。
- **测试发现零手工**：保持 `file(GLOB_RECURSE)` 自动发现，子目录下测试自动被 ctest 收编（与现有模式兼容）。
- **Family tag 命名稳定**：所有 SoC 测试仍 `[soc]` + 细分 tag（如 `[cpu-l1-mmu-demo]`），Catch2 family tag 不变，**二进制 rebuild 不破坏 `[soc]` 过滤器使用**。
- **为 mmufault-verilator-sv32-e2e-flip 等活跃 change 铺路**：它们都引用 `tests/soc/test_cpu_l1_mmu_demo.cpp:113`，本 change 把路径挪到子目录后，未来 mmufault-flip 加 `TEST_CASE 7 cpu_l1_mmu_demo_sv32_translation_flipped` 直接落在新子目录，路径自描述。
- **README 三件套自文档化**：每个 SoC 子目录的 README 都是新人 onramp（点开一个 `tests/soc/<name>/README.md` 即知 SoC 拓扑、JSON 位置、family tag 列表、上下游 spec 引用）。

## 5. Impact

### 受影响文件

| 文件 / 目录 | 动作 | 影响 |
|------|------|------|
| `tests/soc/soc-cpu-l1-mmu-demo/` | 新建 + git mv 2 文件 | test_cpu_l1_mmu_demo.cpp + page_table_helpers.h + 新建 README |
| `tests/soc/mmu-minimal/` | 新建 + git mv 1 文件 | test_mmu_minimal_json.cpp + 新建 README |
| `tests/soc/l1-cache-minimal/` | 新建 + git mv 2 文件 | test_cache_params_schema_json.cpp + test_soc_l1_cache_minimal_json.cpp + 新建 README |
| `tests/soc/README.md` | 顶层更新 | 索引重写指向 3 个子目录 |
| `openspec/changes/<active>/` 4 文件 | 改路径 | ~6 行 |
| `openspec/specs/` 6 文件 | 改路径 | ~12 行 |
| `.rddf/plans/` + `.rddf/roadmap/` 9 文件 | 改路径 | ~15 行 |
| `AGENTS.md` + `CHANGELOG.md` | 改路径 | ~10 行 |
| `soc/cpu_l1_mmu_demo.json` | 改 description 引用 | 1 行 |
| `ip/cache/README.md` 等 ip README | 改路径 | ~3 行 |
| `tools/migrate_to_catch2/migrate.py` | 改映射规则 | 1 行（未来 catch2 重迁移工具时才用得到，可选） |
| `tools/doc_link_check.sh` | KNOWN_LEGACY 加 23 行 | 例外化 openspec/changes/archive 旧路径 |

总估时：**40+ 文件路径字符串改动**，**0 行业务语义改动**，**5 cpp 测试代码** 完全不动。

### 风险（per `.rddf/roadmap/strategy.md` §5）

- **R-DOC-1（doc_link_check false positive）**：openspec archive 23 处无法改 → **缓解**：本次 change tasks §D 加 `KNOWN_LEGACY_REGEX` pattern，行格式 `^openspec/changes/archive/.*:.*ip/cache/plugins/`，doc_link_check 自动 skip。
- **R-LINK-1（include 路径相对漂移）**：`test_cpu_l1_mmu_demo.cpp` 用 `#include "page_table_helpers.h"`（**相对**），git mv 同步迁移 → **缓解**：git mv 保留 relative path，0 动作。
- **R-CMAKE-1（`file(GLOB_RECURSE)` cache 失效）**：CMake 在某些版本对 GLOB 的 RELOAD 需要 `cmake --build build --target rebuild_cache` → **缓解**：本 change 必达项加 `cmake --build build --target rebuild_cache` 后再 build。
- **R-TEST-1（[soc] family 过滤失效）**：Catch2 family tag 不变（`[soc]` 仍在），但新文件路径深一层 → **缓解**：用 `./build/bin/chipforge_tests [soc]` 跑全 family 确认。
- **R-DOC-2（活动 openspec 路径遗漏）**：活 openspec changes 4 处遗漏 1 处 → **缓解**：tasks §C.3 `grep -rn 'tests/soc/test_' openspec/changes/ | grep -v archive` 必须 0 hits，否则 archive 前自我阻断。
- **回退**：单 git revert 即可（git mv + 路径字符串同步全部在 1 个 commit）。

### 下游依赖

- **上游**：无（无 manual_deps）。
- **下游**：
  - `mmufault-verilator-sv32-e2e-flip` (`openspec/changes/2026-10-08-mmufault-verilator-sv32-e2e-flip/`) — TEST_CASE 7 新增预计落在 `tests/soc/soc-cpu-l1-mmu-demo/test_cpu_l1_mmu_demo.cpp`（路径已迁移）。
  - `mmu-chmem-pipeline-integration` (D.2 task) — 同上。
  - `mmu-chmem-phase-b-c-launch` (D.2 task) — 同上。
  - `cpu-factory-satp-mapping` (已 archive，但 spec.md 引用 `tests/soc/test_cpu_l1_mmu_demo.cpp:113` workaround 注释) — 路径改后 spec 内注释同步。

### 不解决

- `ip/cache/tlm/` → `ip/cache/plugins/` 改名 → 单独的 `cache-tlm-to-plugins-rename` change。
- `ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md` → 独立 ADR 治理。
- `tests/bundles/` 补 csr/tlb bundle 测试 → 独立测试补全 change。
- 占位 IP 治理 → 独立 cleanup change。
- `tests/cpu/configs/` 命名 → 独立 rename change。
