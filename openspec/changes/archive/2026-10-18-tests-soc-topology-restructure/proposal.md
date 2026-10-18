---
initiative: wave5-isa-coverage-and-bp
priority: P2
version_target: v0.11.0+post-launch
revision_history:
  - date: 2026-10-18
    author: rdd-builder (Sisyphus bootstrap)
    summary: |
      Initial draft — same critic-trap risks as cache-tlm-to-plugins-rename:
      include 真站点 / archive 命中数 / doc_link_check KNOWN_LEGACY 不存在 / frontmatter / baseline 数字
      全部需在 P2 execute 前重统计 + 修正. pre-emptive frontmatter added.
---

# tests-soc-topology-restructure — `tests/soc/` 按 SoC 拓扑拆为 3 子目录

## Why

`tests/soc/` 当前是单一平铺目录（5 cpp + 1 helper），与 `soc/*.json` 失去一一对应：

| 当前 | 测的 SoC 拓扑 | 对应 JSON |
|------|---------------|----------|
| `tests/soc/test_cpu_l1_mmu_demo.cpp` | CPU + MMU + Memory | `soc/cpu_l1_mmu_demo.json` |
| `tests/soc/page_table_helpers.h` | helper for demo | n/a |
| `tests/soc/test_mmu_minimal_json.cpp` | MMU 独立 | `soc/mmu_minimal.json` |
| `tests/soc/test_cache_params_schema_json.cpp` | L1Cache schema | `soc/l1_cache_minimal.json` (schema) |
| `tests/soc/test_soc_l1_cache_minimal_json.cpp` | L1Cache 独立 | `soc/l1_cache_minimal.json` |

**目标**：让每个 SoC 拓扑有自己的子目录 `tests/soc/<soc-name>/`，与 `soc/<name>.json` 一一对应，未来加新 SoC 即建新子目录。

### 替代方案被拒绝

| 方案 | 拒绝原因 |
|------|---------|
| `tests/soc/cpu/`（用户原方案）| 会把 mmu / cache 独立测试强塞 cpu/ 下，违反归属原则 |
| `tests/soc/{cpu,mmu,cache}/` 按 IP | 违反 SoC 跨 IP 集成测试意图 |
| **本方案：** `tests/soc/<soc-name>/` 按 SoC 拓扑 | ✅ 与 soc/*.json 一一对应 |

详见 `.rddf/improvements/tests-soc-topology-restructure.md` §1。

## What Changes

### In Scope

- **新建 3 子目录**：

| 子目录 | 来源 git mv |
|-------|-----------|
| `tests/soc/soc-cpu-l1-mmu-demo/` | test_cpu_l1_mmu_demo.cpp + page_table_helpers.h |
| `tests/soc/mmu-minimal/` | test_mmu_minimal_json.cpp |
| `tests/soc/l1-cache-minimal/` | test_cache_params_schema_json.cpp + test_soc_l1_cache_minimal_json.cpp |

- **新建 3 个 README**（每个子目录）+ **更新 `tests/soc/README.md` 索引**。
- **路径字符串同步**：

| # | 引用类型 | 数量 | 动作 |
|---|---------|------|------|
| 1 | `#include "page_table_helpers.h"` (相对) | 1 处 | 跟随 git mv 自动消解 |
| 2 | `tests/CMakeLists.txt` 路径 | 0 | `GLOB_RECURSE` 自动发现 |
| 3 | `tools/migrate_to_catch2/migrate.py` | 1 | 改 dir 映射（可选）|
| 4 | 活 openspec changes/ (ACTIVE) | 4 | 改路径字符串 |
| 5 | openspec/changes/archive/ | 23 | 保留，加 KNOWN_LEGACY |
| 6 | openspec/specs/ | 6 | 改路径字符串 |
| 7 | .rddf/plans/ + .rddf/roadmap/ | 9 | 改路径字符串 |
| 8 | docs/superpowers/plans/ | 1 | 改路径字符串 |
| 9 | soc/cpu_l1_mmu_demo.json description | 1 | 改 JSON 描述 |
| 10 | AGENTS.md + CHANGELOG.md | 大量 | 改路径字符串 |
| 11 | ip/cache/README.md 等 | ~3 | 改路径字符串 |

### Out of Scope

- **不改任何 .cpp 业务代码语义**（仅 include 路径跟随 git mv）。
- **不改 SoC JSON 命名**（`soc/cpu_l1_mmu_demo.json` 等保持原名）。
- **不动 tests/{cpu,cache,mmu}/** IP 测试目录。
- **不动 openspec/changes/archive/** 内容。
- **不增删 ctest / Catch2 test case**。

## Capabilities

### MUST

- `tests/soc/{soc-cpu-l1-mmu-demo,mmu-minimal,l1-cache-minimal}/` 3 子目录存在。
- 6 文件通过 `git mv` 迁移到新位置（git history 保留）。
- 3 个新 README.md 含 SoC 拓扑说明 + `soc/<name>.json` 链接 + family tag 表。
- 顶层 `tests/soc/README.md` 重写。
- 活 openspec/CHANGELOG/AGENTS/.rddf 全部路径字符串同步。

### MUST NOT

- 不动业务 .cpp 语义。
- 不改 SoC JSON 命名。
- 不增删 catch2 test case。
- 不动 tests/{cpu,cache,mmu}/。

详见 `.rddf/improvements/tests-soc-topology-restructure.md` §4。

## Impact

### 受影响文件

| 文件 / 目录 | 动作 |
|------|------|
| `tests/soc/soc-cpu-l1-mmu-demo/` | 新建 + git mv 2 文件 + README |
| `tests/soc/mmu-minimal/` | 新建 + git mv 1 文件 + README |
| `tests/soc/l1-cache-minimal/` | 新建 + git mv 2 文件 + README |
| `tests/soc/README.md` | 重写 |
| 活 openspec/changes/ 4 文件 | 改路径 |
| openspec/specs/ 6 文件 | 改路径 |
| .rddf/{plans,roadmap}/ 9 文件 | 改路径 |
| AGENTS.md + CHANGELOG.md | 改路径 |
| soc/cpu_l1_mmu_demo.json | 改 description |
| ip/README 类似 README ~3 | 改路径 |
| tools/doc_link_check.sh | KNOWN_LEGACY 加 23 行 |

总估时：**40+ 文件路径字符串改**，**0 行业务语义改**。

### 风险

- **R-DOC-1**：doc_link_check false positive → 缓解：KNOWN_LEGACY pattern。
- **R-LINK-1**：include 路径漂移 → 缓解：git mv 保留相对路径。
- **R-CMAKE-1**：`GLOB_RECURSE` cache 失效 → 缓解：`cmake --build build --target rebuild_cache`。
- **R-TEST-1**：`[soc]` family 过滤失效 → 缓解：跑 `[soc]` 验证。
- **R-DOC-2**：活 openspec 遗漏 → 缓解：`grep -rn 'tests/soc/test_' openspec/changes/ | grep -v archive` 必须 0 hits。

详见 `.rddf/improvements/tests-soc-topology-restructure.md` §5。

## Acceptance

### 必达（Hard）

- [ ] `tests/soc/` 3 子目录建立。
- [ ] `git mv` 6 文件完成。
- [ ] 3 个新 README.md 创建。
- [ ] 顶层 `tests/soc/README.md` 更新。
- [ ] `grep -rn 'tests/soc/test_' openspec/ AGENTS.md CHANGELOG.md .rddf/ | grep -v /archive/` 返回 0 hits。
- [ ] `grep -rn 'tests/soc/test_' openspec/changes/archive/` 仍命中 23 处 + doc_link_check KNOWN_LEGACY 已加。
- [ ] `bash tools/doc_link_check.sh` 0 fail。
- [ ] `bash tools/run_chipforge_tests.sh` 0 regression。
- [ ] `cmake --build build -j$(nproc)` 0 error。

### 应达（Soft）

- [ ] `soc/cpu_l1_mmu_demo.json` description 同步。
- [ ] AGENTS.md `tests/soc/test_cpu_l1_mmu_demo.cpp:113` 引用同步。
- [ ] CHANGELOG.md 多处 `tests/soc/test_cpu_l1_mmu_demo.cpp:124` 引用同步。

### 治理（Meta）

- [ ] openspec archive `tests-soc-topology-restructure` 成功。
- [ ] `.rddf/improvements/tests-soc-topology-restructure.md` 状态 draft → approved/archived。
- [ ] `openspec/specs/soc-cpu-mmu-demo-topology/` 等 6 个 spec 文件路径同步。
