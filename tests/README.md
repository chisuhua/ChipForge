# 项目测试入口 (Tests)

> **状态**: 🟢 Catch2 迁移完成 (2026-06-30, [plan](../docs/superpowers/plans/2026-06-30-catch2-test-framework.md))
> **架构**: 按家族 (family) 划分子目录, 而非按物理位置 (src/) 或 IP
> **框架**: Catch2 v3.7.0 (vendored amalgamated, 与 CppTLM/CppHDL 完全一致)
> **规模**: 测试文件数 / case 数**以实测为准** —— 本文件不写死快照数字 (每次改动都会漂移);
>   要当前数字就跑 `./build/bin/chipforge_tests --list-tests` (脚本会自行打印总数)
> **构建模式**: 两个测试 binary —— `chipforge_tests` (TLM, 默认) + `chipforge_tests_chmem` (CH_MEM, 需 `-DCF_PLUGIN_USE_CH_MEM`)

## 1. 子目录组织

| 目录 | 家族 | 测的是什么 | 归属 binary | 增长预测 |
|------|------|------------|------|----------|
| `framework/` | cf_plugin 框架层 | Plugin 基础设施 (PluginBase/Payload/PipeNode/PipeBuilder/CtrlLink/Storage/Coexistence/PipeArbitration) | TLM + CH_MEM | 稳定 (框架冻结) |
| `cache/` | L1Cache IP 业务 | L1CachePlugin 单元测试 + Bridge + Adapter e2e | TLM + CH_MEM | 稳定 |
| `cpu/` | CPU IP 业务 (基础) | RISC-V 解码/ALU/分支/寄存器/CSR 等 | TLM + CH_MEM | 稳定 |
| `cpu/integration/` | CPU 集成 | 多 stage RISC-V 集成测试 (3/5/7/10-stage) | TLM | 稳定 |
| `cpu/schema/` | CPU 配置 | JSON Schema 验证 (含 CH_MEM 配置面契约) | TLM | 慢增 |
| `soc/` | SoC 集成层 | SoC JSON 拓扑 + JSON Schema 验证 | TLM | 慢增 |
| `bundles/` | 共享 Bundle 定义 | `bundles/mem_bundles.h` | TLM | 稳定 |
| `mmu/` | MMU IP 业务 | TLB / PTW / Plugin / Config | TLM + CH_MEM | 慢增 |

> **历史**：`cpu/configs/` 已更名为 `cpu/schema/`（治理 rename）；`mmu/` 曾在 `mmu-tlb-ptw-impl`
> commit 10 之前被临时排除，现已恢复并常态纳入构建。

## 2. 测试框架 (Catch2 v3.7.0)

Catch2 是 header-only 的 C++ 单元测试框架, 与 CppTLM/CppHDL 完全对齐。

### 2.1 运行方式

```bash
# 配置 + 构建 (CppTLM/CppHDL 通过 ExternalProject 自动构建并 install)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DCHIPFORGE_BUILD_TESTS=ON
cmake --build build -j$(nproc)

# 跑全部测试 (走 baseline-aware 判定, 详见 §3)
bash tools/run_chipforge_tests.sh          # TLM binary
bash tools/run_chipforge_tests.sh --all    # 全量: TLM + CH_MEM + Verilator smoke + gate

# 直接跑可执行文件
./build/bin/chipforge_tests                    # 全部
./build/bin/chipforge_tests "[framework]"      # 按 family tag 过滤
./build/bin/chipforge_tests "[cache]"          # 单 family
./build/bin/chipforge_tests "[cpu-integration]" # 集成测试
./build/bin/chipforge_tests "~[mmu]"           # 排除某 family
./build/bin/chipforge_tests_chmem "[chmem]"    # CH_MEM binary

# 列表
./build/bin/chipforge_tests --list-tests
```

### 2.2 编写测试

```cpp
// tests/<family>/test_<name>.cpp
#include "catch_amalgamated.hpp"
#include "your/header.h"

TEST_CASE("test description", "[<family>]") {
    // setup
    REQUIRE(condition);      // 失败中止当前 test case
    CHECK(condition);         // 失败继续
    INFO("context for next assertion");
}
```

### 2.3 新增测试

1. 在 `tests/<family>/` 下创建 `test_<name>.cpp`
2. 使用 `TEST_CASE(name, "[<family>]")` 模式
3. **无需修改 CMake** — `file(GLOB_RECURSE)` 自动发现 `tests/*/test_*.cpp`
   （例外：`chipforge_tests_chmem` 的源文件是 `CHMEM_TEST_SOURCES` **显式列表**，新增 CH_MEM-only 测试需手工加入）

## 3. 已知失败与「回归是否通过」的判定

> **真源** = `tools/baseline_known_failures.txt` —— 按 binary 分节；每条附**根因 + owner + 收敛期限**。
> 清单本身也要被 review，**不得**靠扩张清单来消红。

### 3.1 判定机制（不要直接用 ctest 退出码判「全量通过」）

- **baseline-aware**：`tools/run_catch2_with_baseline.sh` 判定「是否有**新**失败」而非「是否有任何失败」
  —— 失败集合 ⊆ 清单 ⇒ 退出码 `0`；⊋ 清单 ⇒ 非 `0`（fail-closed）
- **产物新鲜度 fail-closed**：`--no-build` 时校验 binary 的 mtime 晚于全部编译输入，否则**拒绝运行**
  —— 堵死「改动没编译进去 → 旧 binary 仍输出 All tests passed」
- **默认先 build**：编译失败即退出，不跑旧产物

### 3.2 当前登记的 baseline-known 失败

- TLM binary：`7stage_topology_from_config`（infra drift —— 期望值过时，非实现 bug）
- CH_MEM binary：`cpphdl_poc_chbool_contextual_conversion`（上游 CppHDL context-pollution flake，单跑 PASS）
- CH_MEM binary：`mmu_bare_plumbing_tohost1_baseline_5_elf`（顺序敏感 flake，单跑 PASS）

### 3.3 会让人误判 FAIL 的坑

- **CWD**：`test_7stage_riscv.cpp` 用相对路径调 `cpu_sim`，ctest 以仓库根为 `WORKING_DIRECTORY`
  ⇒ **从非仓库根直接跑 binary 会误报 FAIL**
- **`[vendored-elf]` 全量跑极慢**：`chmem_vendored_elf_tohost1` 的 DYNAMIC_SECTION 在同一进程内累积 CPU 网表
  ⇒ 全量是十分钟量级；本地回归用 `--tag` 过滤跳过

## 4. CMake 集成

- **两个 binary**：`chipforge_tests` (TLM，`file(GLOB_RECURSE)` 收集 `tests/*/test_*.cpp`) + `chipforge_tests_chmem` (CH_MEM，`CHMEM_TEST_SOURCES` 显式列表)
- **vendored 框架**：`tests/catch2/catch_amalgamated.{hpp,cpp}` 来自 CppTLM 同步
- **CF Plugin INTERFACE 库**: cache 测试需要的实现 .cpp (`L1CachePlugin.cpp` 等) 显式列入 `CACHE_IMPL_SOURCES`
- **CppTLM/CppHDL 依赖**: 通过 `cf_plugin_link_cpptlm/cpphdl` helper 函数附加

完整迁移历史见 [实施计划](../docs/superpowers/plans/2026-06-30-catch2-test-framework.md) 与 [结果报告](../docs/superpowers/plans/2026-06-30-catch2-migration-results.md)。
