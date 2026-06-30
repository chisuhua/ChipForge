# 项目测试入口 (Tests)

> **状态**: 🟢 Catch2 迁移完成 (2026-06-30, [plan](docs/superpowers/plans/2026-06-30-catch2-test-framework.md))
> **架构**: 按家族 (family) 划分子目录, 而非按物理位置 (src/) 或 IP
> **框架**: Catch2 v3.7.0 (vendored amalgamated, 与 CppTLM/CppHDL 完全一致)
> **总数**: 47 个测试文件, 259 个 test cases (M1-M5 累计)
> **构建模式**: 单二进制 `chipforge_tests` (CppTLM 风格 file(GLOB))

## 1. 子目录组织

| 目录 | 家族 | 测的是什么 | 当前数量 | 增长预测 |
|------|------|------------|----------|----------|
| `framework/` | cf_plugin 框架层 | Plugin 基础设施 (PluginBase/Payload/PipeNode/PipeBuilder/CtrlLink/Storage/Coexistence/PipeArbitration) | 9 | 稳定 (框架冻结) |
| `cache/` | L1Cache IP 业务 | L1CachePlugin 单元测试 + Bridge + Adapter e2e | 5 | 稳定 (Phase 1.3 全完结) |
| `cpu/` | CPU IP 业务 (基础) | RISC-V 解码/ALU/分支/寄存器/CSR 等 | 19 | 稳定 |
| `cpu/integration/` | CPU 集成 | 多 stage RISC-V 集成测试 (3/5/7/10-stage) | 4 | 稳定 |
| `cpu/configs/` | CPU 配置 | JSON Schema 验证 | 1 | 慢增 |
| `soc/` | SoC 集成层 | SoC JSON 拓扑 + JSON Schema 验证 | 2 | 慢增 |
| `bundles/` | 共享 Bundle 定义 | `bundles/mem_bundles.h` | 1 | 稳定 |
| `mmu/` | MMU IP 业务 | TLB / MultiLevelTLB / Plugin / Config (mmu-ip-skeleton 阶段) | 5 | 暂排除 (mmu 库代码待完成) |
| **总计** | | | **47** | |

## 2. 测试框架 (Catch2 v3.7.0)

Catch2 是 header-only 的 C++ 单元测试框架, 与 CppTLM/CppHDL 完全对齐。

### 2.1 运行方式

```bash
# 配置 + 构建 (CppTLM/CppHDL 通过 ExternalProject 自动构建并 install)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DCHIPFORGE_BUILD_TESTS=ON
cmake --build build -j$(nproc)

# 跑全部测试 (单二进制, 254/259 通过, 5 个 RISC-V 仿真失败是预先存在的)
ctest --test-dir build --output-on-failure

# 直接跑可执行文件
./build/bin/chipforge_tests                    # 全部
./build/bin/chipforge_tests "[framework]"      # 按 family tag 过滤
./build/bin/chipforge_tests "[cache]"          # 单 family
./build/bin/chipforge_tests "[cpu-integration]" # 集成测试
./build/bin/chipforge_tests "~[mmu]"           # 排除某 family

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

## 3. 已知问题

### 3.1 mmu/ 测试临时排除 (mmu 库代码未完成)

5 个 mmu/ 测试因 `ip/mmu/lib/tlb.h` 等库代码 bug (TLBEntry::tag_type 缺失, hits_/misses_ 在 const 方法中修改) 暂从构建中排除 (`tests/CMakeLists.txt` 中用 `list(REMOVE_ITEM)`)。
等 mmu-tlb-ptw-impl 完成后恢复。

### 3.2 5 个 RISC-V 仿真测试失败 (预先存在)

`test_*stage_riscv` (4) + `test_cpu_sim_real_tohost` (1) 失败因 RISC-V 工具链配置 (tohost=1 字符串缺失, riscv64 assembler 不在 PATH), 与 Catch2 迁移无关。

## 4. CMake 集成

- **单二进制模式**: `tests/CMakeLists.txt` 用 `file(GLOB_RECURSE)` 收集所有 `tests/*/test_*.cpp`
- **vendored 框架**: `tests/catch2/catch_amalgamated.{hpp,cpp}` 来自 CppTLM 同步
- **CF Plugin INTERFACE 库**: cache 测试需要的实现 .cpp (`L1CachePlugin.cpp` 等) 显式列入 `CACHE_IMPL_SOURCES`
- **CppTLM/CppHDL 依赖**: 通过 `cf_plugin_link_cpptlm/cpphdl` helper 函数附加

完整迁移历史见 [实施计划](../docs/superpowers/plans/2026-06-30-catch2-test-framework.md) 与 [结果报告](../docs/superpowers/plans/2026-06-30-catch2-migration-results.md)。