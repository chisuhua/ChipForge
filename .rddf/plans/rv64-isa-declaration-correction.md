# rv64-isa-declaration-correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `CPUConfig::isa` 从「装饰性字段 + 虚假默认值」变成三层 fail-fast 可校验契约（ADR-087 §D1/D2），同时把 TLM 语义层模板入口的静默错算最小集（int_alu/branch）关进编译期护栏，并清除唯一 RV64 死代码定时炸弹 `reg_file.cpp`。

**Architecture:** 校验函数 `cf::cpu::detail::validate_isa_xlen(config, sizeof(T)*8)` 是唯一 SSOT，`build_cpu()` 首行接线（单点入口，cpu_sim 无需重复实现）；声明面（schema/JSON/示例）只覆盖可生效取值，reserved 值走 std::cerr warn 不 throw；编译期护栏放模板运算入口函数体首行（实例化点触发），与文件作用域 include 误伤隔离；现存 T64 实例化点按 ADR-087 AC2 迁移到 T32（RV64 真实路径归 phase-5 Change 2）。

**Tech Stack:** C++23（`std::invalid_argument` + `std::cerr`，无新依赖）；Catch2 amalgamated（`tests/catch2/`，`catch_amalgamated.hpp`）；CMake `GLOB_RECURSE` 自动发现新测试文件，**但无 `CONFIGURE_DEPENDS` ⇒ 新增测试文件后必须重跑 `cmake -S . -B build`**。

**上游 ADR / SSOT:** ADR-087（`docs/architecture/adr/ADR-087-isa-declaration-integrity-and-rv64-readiness.md` §D1/D2/D4/D6/R4）· 缺口表 `ip/cpu/docs/rv64_readiness_gap.md`（§2 状态列 = 本 change 验收台账；§5 = hygiene allowlist SSOT）· proposal.md + design.md Decision 1–8 / Risks R1–R8。

---

## 修订记录（critic 审查后修订，2026-10-23）

critic 对本 change 的 P1 产物给出 **REJECT：6 BLOCKER / 7 SHOULD_FIX / 6 NITS**，本计划已逐条修复：

| 编号 | critic 指控 | 本计划处置 | 落点 |
|------|------------|-----------|------|
| B1 | 护栏 `static_assert(sizeof(T)==4)` 会打断 `chipforge_tests` 编译（`IntAlu<T64>`/`Branch<T64>` 今天已被实例化）→ AC2 与 AC9「0 退化」互斥 | **用户裁决：迁移 T64 调用点，保留编译期护栏**；删除 RV64 构建探测 case + 其余改 T32；`[cpu]` 计数随之调整 | Task 4 |
| B2 | Task 5 改 `isa` 后 3 个 JSON 触发 schema `:204` 条件（`rv32* ⇒ mmu_mode const sv32`）而 `mmu_mode` 是 sv39/sv48 → 变 schema 非法 | 3 个 JSON 的 `mmu_mode` 同步纠偏为 `sv32`（32-bit XLEN 下只有 Sv32 合法，schema 约束本身是对的）；`_comment` 撞 `additionalProperties:false` 属 **pre-existing 基线**，登记不修 | Task 6 + design R8 |
| B3 | hygiene 脚本 allowlist 2 条路径写错 → `warn=0` 不成立 | allowlist 改为缺口表 §5 实测的 **7 个真实文件**（9 处定义点分布在 7 文件） | Task 12 |
| B4 | `validate_isa_xlen` 插在 `namespace cf::cpu::detail` 之外 → 落笔即编译失败 | 计划显式给出包裹代码：helper 放在 struct 之后、**只重开 `detail` 一层**（`cf::cpu` 已由 `:115-116` 打开 ⇒ 再写 `namespace cf { namespace cpu {` 会得到 `cf::cpu::cf::cpu::detail`，同名陷阱） | Task 3 Step 2 |
| B5 | 替换后的注释仍含 `isa="rv32i"` 字面量 → AC4 的 grep 自我否定 | 替换文本不含该字面量；AC4 grep 口径同步核对 | Task 7 + Task 13 |
| B6 | Task 12 自检含 `git stash push` → 会抽走实施期改动 | 删除该行，自检改为 `/tmp/opencode` 纯 dry 探针 | Task 12 |
| S1 | 新测试文件缺 `using namespace cf::cpu;` | 测试文件加 `using namespace cf::cpu;` | Task 2 |
| S2 | `GLOB_RECURSE` 无 `CONFIGURE_DEPENDS` → 新文件不进 build | 新增/改测试后强制 `cmake -S . -B build` | Task 2 / Task 3 / Task 4 |
| S3 | catch2 多 positional 参数是「No tests ran + exit 0」假绿 | 全部改为单 tag 或逗号组合（`,` = OR） | 全文 |
| S4 | AC9 数字与实测不符（36/36、130/130 口径混用） | 全部按实测基线重写（见下表） | Task 13 + AC 表 |
| S5 | `cpu_sim` 不解析 `xlen`（schema 声明了但静默丢弃，同 `cpu-pipeline-tick-fix` F1 病） | `load_config()` 补 `cfg.xlen` 解析 | Task 7 |
| S6 | `branch.h` 护栏落在 `evaluate_branch`（仅测试调用）→ 真实分支判定在 `build()` 闭包内，覆盖为 0 | `branch.h` 加**两处**护栏：静态入口 + 真实闭包内 | Task 8 |
| S7 | proposal 残留「`isa` 含 rv64 ⇒ xlen==64，不一致 fail-fast」，与 D1（reserved warn-only）冲突 | proposal 措辞改写为 D1 三层语义 | proposal.md |
| N1–N6 | 行号漂移（`branch.h:111`、`:320`）、命名层级（`cf::cpu::detail::validate_isa_xlen`）、AC3 措辞自相矛盾、design R6 理由错误（CH_MEM 侧**不** include `int_alu.h`/`branch.h`，实测 `nm` 只有 `compute_static`/`safe_divisor_u`）、Task 11 措辞降级、tasks.md 时点 | 逐条对齐（见 design.md / tasks.md 修订） | 全文 |

---

## 基线快照（HEAD `bcc88aa` 实测，2026-10-23）

| 口径 | 实测值 | 备注 |
|------|--------|------|
| `[cpu]` | **129 cases / 461 assertions PASS** | 本 change 后 = 129 − 1（删 `build_cpu_rv64`）+ 5（新用例）= **133** |
| `[cpu-integration]` | **96 cases / 95 PASS / 1 FAIL** | 唯一失败 = `tests/cpu/integration/test_7stage_riscv.cpp:124 REQUIRE(pb->node_count() == 17)` → 展开 `9 == 17`，**pre-existing（HEAD 即红）**，与本 change 无关；口径 = **不新增失败**，该 FAIL 保持 1 个 |
| `[riscv-tests]` | 48/48 PASS | — |
| `[mmu]` | 59/59 PASS | — |
| `chipforge_tests_chmem` | binary 总 **130 cases**；`[chmem]` tag **89** | 两个口径不可混用；`run_chipforge_tests.sh --all` 全量实测 **>120s 未完**（AGENTS.md 记载 833s），本 change 收口用 `timeout 1200` |

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `ip/cpu/cpu_factory.h` | `CPUConfig` 加 `xlen` 字段；`cf::cpu::detail` 加 `validate_isa_xlen()`（后置重开 namespace）；`build_cpu()` 首行接线 |
| `ip/cpu/configs/cpu_params_schema.json` | `xlen` 属性；`isa` enum 收窄 + default；reserved 值入 description |
| `ip/cpu/configs/cpu_default.json` / `cpu_deep_pipeline.json` / `cpu_superscalar.json` | `isa` → `rv32im`；`mmu_mode` → `sv32` |
| `ip/cpu/configs/cpu_embedded.json` | `rv32imac` → `rv32im`（A 扩展零实现） |
| `tools/cpu_sim/main.cpp` | 删 `cfg.isa = "rv32i"` pin；`load_config()` 补 `xlen` 解析；头注释更新 |
| `ip/cpu/arch/riscv/int_alu.h` | `static T compute()` 函数体首行 `static_assert(sizeof(T)==4)` |
| `ip/cpu/arch/riscv/branch.h` | `static bool evaluate_branch()` **与** `build()` 闭包内各一处 `static_assert(sizeof(T)==4)` |
| `ip/cpu/plugins/reg_file.cpp` | **删除**（死代码，无 CMake 引用） |
| `ip/cpu/docs/rv64_readiness_gap.md` | §2 状态列翻转（0.1–0.6 / 0.9 / 0.10 → ✅） |
| `tools/check_xlen_hygiene.sh` | **新建** CI 软门禁（warn-only） |
| `.github/workflows/architecture-gates.yml` | 接入 hygiene 脚本（`continue-on-error: true`） |
| `tools/dse/sweep_config.example.json` | sweep grid `rv64i` → `rv32im` |

### Tests

| File | Responsibility |
|---|---|
| `tests/cpu/test_cpu_factory_isa_xlen.cpp` | **新建**（`[cpu][isa-xlen]` 5 用例）：默认值契约 / unsupported throw / implemented 冲突 throw / T↔xlen 交叉 throw / reserved warn 单发 |
| `tests/cpu/test_cpu_factory.cpp` | 删 `build_cpu_rv64` case；余下 T64 → T32；`isa` 断言纠偏 |
| `tests/cpu/test_branch_predictor_runtime_btb.cpp` | 3 个 case 的 `uint64_t` → `uint32_t`（保留 BTB 覆盖） |
| `tests/cpu/integration/test_7stage_riscv.cpp` / `test_10stage_riscv.cpp` / `test_3stage_riscv.cpp` | `cfg.isa` 值纠偏（`rv64gc`/`rv32imac` → `rv32im`；`cfg.isa = p["isa"]` 两处随 JSON 自动生效，不改） |

---

### Task 1: 基线快照（零退化锚）

**Files:**
- Test: 无新文件（只跑既有基线）

- [ ] **Step 1: 跑 TLM 快返基线**

Run: `./build/bin/chipforge_tests 2>&1 | tail -3`
Expected: `All tests passed (461 assertions in 129 test cases)`（`[cpu]` 口径见基线快照表）

- [ ] **Step 2: 跑 `[cpu-integration]` 基线并记录 pre-existing FAIL**

Run: `./build/bin/chipforge_tests "[cpu-integration]" 2>&1 | tail -4`
Expected: `test cases: 96 | 95 passed | 1 failed`，唯一 FAIL = `test_7stage_riscv.cpp:124`（node_count `9 == 17`）——**本 change 不修它**，口径 = 不新增失败

- [ ] **Step 3: 跑其余 family 基线**

Run: `./build/bin/chipforge_tests "[riscv-tests],[mmu]" 2>&1 | tail -3`（逗号 = OR）
Expected: `48 + 59 = 107 cases` 全 PASS

- [ ] **Step 4: 跑 CH_MEM 抽查基线**

Run: `./build/bin/chipforge_tests_chmem "[chmem]" 2>&1 | tail -3`
Expected: 89 cases 全 PASS（`[chmem]` tag 口径；全量 130 在 Task 13 收口）

- [ ] **Step 5: Defer commit**（execute 阶段统一提交）

---

### Task 2: 写失败测试（新 `test_cpu_factory_isa_xlen.cpp`，5 用例）

**Files:**
- Create: `tests/cpu/test_cpu_factory_isa_xlen.cpp`

- [ ] **Step 1: Write the failing test**（完整文件，照抄即可）

```cpp
// tests/cpu/test_cpu_factory_isa_xlen.cpp
// Family: [cpu] — ADR-087 §D1/D2 isa/xlen 契约 (rv64-isa-declaration-correction)
// 覆盖: 三层 fail-fast + declared-reserved 单发 warn (design.md Decision 1/2)
// 变异对照: 删 warn / cerr 改 cout / 删 throw → 本文件至少 1 用例 FAIL
#include "catch_amalgamated.hpp"
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ip/cpu/cpu_factory.h"

using namespace cf::cpu;  // CPUConfig / CpuFactory

namespace {

// 捕获 std::cerr (design D2: declared-reserved warn 走 cerr)
class CerrCapture {
 public:
  CerrCapture() : old_(std::cerr.rdbuf(buf_.rdbuf())) {}
  ~CerrCapture() { std::cerr.rdbuf(old_); }
  std::string str() const { return buf_.str(); }

 private:
  std::ostringstream buf_;
  std::streambuf* old_ = nullptr;
};

}  // namespace

TEST_CASE("CPUConfig default isa/xlen contract", "[cpu][isa-xlen]") {
  CPUConfig cfg;
  REQUIRE(cfg.isa == "rv32im");
  REQUIRE(cfg.xlen == 32);
}

TEST_CASE("unsupported isa fail-fast", "[cpu][isa-xlen]") {
  CPUConfig cfg;
  cfg.isa = "rv128i";
  REQUIRE_THROWS_AS(CpuFactory<std::uint32_t>::build_cpu(cfg),
                    std::invalid_argument);
}

TEST_CASE("implemented isa xlen mismatch fail-fast", "[cpu][isa-xlen]") {
  CPUConfig cfg;  // isa=rv32im (implemented), 显式 xlen=64 冲突
  cfg.xlen = 64;
  REQUIRE_THROWS_AS(CpuFactory<std::uint32_t>::build_cpu(cfg),
                    std::invalid_argument);
}

TEST_CASE("kernel T vs xlen cross-check fail-fast (reserved path)",
          "[cpu][isa-xlen]") {
  CerrCapture cap;
  CPUConfig cfg;  // isa=rv64gc (declared-reserved) + 显式 xlen=64 + T=uint32
  cfg.isa = "rv64gc";
  cfg.xlen = 64;
  REQUIRE_THROWS_AS(CpuFactory<std::uint32_t>::build_cpu(cfg),
                    std::invalid_argument);
  REQUIRE(cap.str().find("[declared-reserved]") != std::string::npos);
}

TEST_CASE("declared-reserved accepted with single warning", "[cpu][isa-xlen]") {
  CerrCapture cap;
  CPUConfig cfg;  // isa=rv32imac (declared-reserved), xlen 默认 32 一致
  cfg.isa = "rv32imac";
  REQUIRE_NOTHROW(CpuFactory<std::uint32_t>::build_cpu(cfg));
  REQUIRE(cap.str().find("[declared-reserved]") != std::string::npos);
  // 单发语义: 本用例仅一次装载, warn 只应出现一次
  REQUIRE(cap.str().find("[declared-reserved]") ==
          cap.str().rfind("[declared-reserved]"));
}
```

- [ ] **Step 2: 重新生成构建系统（GLOB 无 CONFIGURE_DEPENDS，新文件必须先 re-cmake）**

Run: `cmake -S . -B build >/dev/null && echo "re-cmake ok"`
Expected: `re-cmake ok`（**漏掉这步 ⇒ 新测试文件不进 `chipforge_tests`，后续会得到「0 tests ran」假绿**）

- [ ] **Step 3: Run test to verify it fails（编译失败也算“红”）**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -8`
Expected: **编译失败** — `CPUConfig` 无 `xlen` 字段（这是 TDD 红；无需运行期失败）

- [ ] **Step 4: Defer commit**

---

### Task 3: `cpu_factory.h` — `xlen` 字段 + `validate_isa_xlen` + `build_cpu` 接线

**Files:**
- Modify: `ip/cpu/cpu_factory.h:127`（isa 默认值 + 新增 xlen 字段）、`:176` 之后（**后置重开 `cf::cpu::detail` 放 helper**）、`:320`（build_cpu 首行接线）
- Test: `tests/cpu/test_cpu_factory_isa_xlen.cpp`（Task 2 文件转绿）

- [ ] **Step 1: 改 `CPUConfig` 两个字段（:127 附近）**

```cpp
// 旧:
  std::string isa = "rv64gc";       // rv32i/rv32im/rv32imac/rv64i/rv64gc
// 新:
  // ADR-087 §D1: 三态语义 implemented=rv32i/rv32im, declared-reserved=rv32imac/rv64i/rv64gc
  // 之外的取值 = unsupported (validate_isa_xlen fail-fast)。默认 = 实际实现水平。
  std::string isa = "rv32im";
  // ADR-087 §D2: XLEN 数据字段 (默认 32)。与 isa 的关联校验见
  // cf::cpu::detail::validate_isa_xlen。xlen 与 ISA 扩展字母正交
  // (rv32imac 的 A/C 与 xlen 无关)。
  std::uint8_t xlen = 32;
```

- [ ] **Step 2: 后置重开 namespace 放校验 helper（唯一 SSOT，design D1/D6）**

> `namespace cf::cpu::detail` 的原块在 `cpu_factory.h:113` 闭合（`}}}  // namespace cf::cpu::detail`），**早于** `struct CPUConfig`（:124）的定义 ⇒ helper 不能放回原块。但 `namespace cf { namespace cpu {` 在 `:115-116` **已重新打开**（直到 `:520-521` 才闭合）⇒ 此处**只需重开 `detail` 一层**；若再写 `namespace cf { namespace cpu {` 会产生 `cf::cpu::cf::cpu::detail` ⇒ 落笔即错（同名陷阱）。

在 `struct CPUConfig { ... };`（:176 的 `};`）之后插入：

```cpp
// ----------------------------------------------------------------------------
// ADR-087 §D1/D2 — isa/xlen 契约校验 (rv64-isa-declaration-correction)
// 唯一入口: build_cpu() 首行调用; 三层语义 (design.md Decision 1):
//   ① unsupported isa 值 ⇒ throw
//   ② implemented (rv32i/rv32im) 与 xlen=32 冲突 ⇒ throw
//   ③ build_cpu<T> 交叉校验 xlen != sizeof(T)*8 ⇒ throw
//   ④ declared-reserved (rv32imac/rv64i/rv64gc) ⇒ 接受 + 一次 cerr warning
// kernel_xlen = sizeof(T)*8 (build_cpu 泛化入口的运行期兜底;
// CpuFactory<uint64_t> 主防线是 int_alu.h/branch.h 的编译期 static_assert)
// ----------------------------------------------------------------------------
namespace detail {

inline void validate_isa_xlen(const CPUConfig& config, unsigned kernel_xlen) {
  const std::string& isa = config.isa;
  if (isa == "rv32i" || isa == "rv32im") {
    if (config.xlen != 32) {
      throw std::invalid_argument(
          "CpuFactory: isa=\"" + isa + "\" (implemented) requires xlen=32, got " +
          std::to_string(config.xlen) + " (ADR-087 §D1)");
    }
    if (kernel_xlen != 32) {
      throw std::invalid_argument(
          "CpuFactory: xlen=32 (rv32i/rv32im) but kernel T is " +
          std::to_string(kernel_xlen) +
          "-bit; instantiate CpuFactory<uint32_t> (ADR-087 §D1)");
    }
    return;
  }
  if (isa == "rv32imac" || isa == "rv64i" || isa == "rv64gc") {
    std::cerr << "[declared-reserved] isa=\"" << isa
              << "\" 未实现（预案 v1.x），配置被接受但不参与 dispatch（ADR-087 "
                 "§D1）"
              << std::endl;
    if (kernel_xlen != config.xlen) {
      throw std::invalid_argument(
          "CpuFactory: xlen=" + std::to_string(config.xlen) +
          " mismatch with kernel T (" + std::to_string(kernel_xlen) +
          "-bit); instantiate CpuFactory<uint32_t> (ADR-087 §D1)");
    }
    return;
  }
  throw std::invalid_argument(
      "CpuFactory: unsupported isa=\"" + isa +
      "\"; implemented=rv32i/rv32im, declared-reserved=rv32imac/rv64i/rv64gc "
      "(ADR-087 §D1)");
}

}  // namespace detail
```

（`cf::cpu` 在 `:115-116` 已打开，故**只需**闭合 `detail` 一层；`cf` / `cpu` 由 `:520-521` 统一闭合）

（若文件未 include `<iostream>` 需补：实测 `cpu_factory.h` 只 include 了 `<stdexcept>`/`<string>` 等，**没有 `<iostream>`** ⇒ 必须补 `#include <iostream>`，按字母序插在既有 include 区）

- [ ] **Step 3: `build_cpu()` 首行接线（:320 `reset_canonical_counters();` 之后）**

```cpp
    // ADR-048: reset canonical ordering counters at each build_cpu entry
    cf::cpu::detail::reset_canonical_ordering_counters_placeholder();
```

> 落笔时**照抄实际代码**：紧接既有 `cf::cpu::detail::reset_canonical_counters();` 之后新增一行：

```cpp
    cf::cpu::detail::validate_isa_xlen(config, sizeof(T) * 8);  // ADR-087 §D1/D2
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -4 && ./build/bin/chipforge_tests "[isa-xlen]" 2>&1 | tail -4`
Expected: **5 passed**（“单发”用例失败 ⇒ 检查 cerr 只输出一次）

- [ ] **Step 5: 回归既有 family（逗号 = OR，单 tag 写法在 catch2 里是 AND，多 positional 会假绿）**

Run: `./build/bin/chipforge_tests "[cpu],[cpu-integration],[riscv-tests]" 2>&1 | tail -4`
Expected: `[cpu]` 129 PASS（尚无新用例被计入？——新用例已存在 ⇒ 实为 **134**：129 + 5，本步断言口径见 Task 4 后统一校准）；`[cpu-integration]` 96 cases / 95 PASS / 1 pre-existing FAIL；`[riscv-tests]` 48 PASS。**若 `[cpu]` 出现失败** ⇒ 大概率是 Task 4 未做（`test_cpu_factory.cpp:28` 仍断言 `isa == "rv64gc"`）→ 先做 Task 4 再复跑

- [ ] **Step 6: Defer commit**

---

### Task 4: 迁移 T64 实例化点（用户裁决：保留编译期护栏）

> 现状实测（`nm -C` 扫 `build/tests/CMakeFiles/**/*.o`）：`RiscvIntAluPlugin<unsigned long>` / `RiscvBranchPlugin<unsigned long>` 已被实例化于 `test_cpu_factory.cpp.o` 与 `test_branch_predictor_runtime_btb.cpp.o`。ADR-087 AC2 要求 `CpuFactory<uint64_t>` **编译失败** ⇒ 必须先把这些实例化点迁走，否则 Task 8 的护栏会让整个 `chipforge_tests` 编不过。

**Files:**
- Modify: `tests/cpu/test_cpu_factory.cpp`、`tests/cpu/test_branch_predictor_runtime_btb.cpp`
- Modify: `tests/cpu/integration/test_7stage_riscv.cpp`、`test_10stage_riscv.cpp`、`test_3stage_riscv.cpp`

- [ ] **Step 1: `test_cpu_factory.cpp` — 删 RV64 探测 case**

删除整个 case（原 `:43-51`）：

```cpp
TEST_CASE("build_cpu_rv64", "[cpu]") {
  CPUConfig cfg;
  cfg.isa = "rv64gc";
  cfg.pipeline_stages = 5;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  auto pb = CpuFactory<T64>::build_cpu(cfg);
  REQUIRE(pb != nullptr);
}
```

同时把文件头注释（原 `:7-12` 的“测试覆盖 (5 用例)”清单）改为 4 用例口径，删掉 “5. 不同 xlen (uint32/uint64) 实例化” 条目；并删除已无引用的 `using T64 = std::uint64_t;`（原 `:24`）。

- [ ] **Step 2: `test_cpu_factory.cpp` — 剩余 3 处 T64 → T32 + isa 断言纠偏**

| 行 | 现状 | 改为 |
|----|------|------|
| `:28` | `REQUIRE(cfg.isa == "rv64gc");` | `REQUIRE(cfg.isa == "rv32im");` |
| `:56` | `cfg.isa = "rv32imac";`（`embedded_config`） | `cfg.isa = "rv32im";`（与 Task 6 的 `cpu_embedded.json` 纠偏对齐） |
| `:69` | `cfg.isa = "rv64gc";`（`default_json_config`） | `cfg.isa = "rv32im";` |
| `:79` | `auto pb = CpuFactory<T64>::build_cpu(cfg);` | `auto pb = CpuFactory<T32>::build_cpu(cfg);` |
| `:93` | `auto pb = cf::cpu::CpuFactory<std::uint64_t>::build_cpu(cfg);` | `auto pb = cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg);` |

- [ ] **Step 3: `test_branch_predictor_runtime_btb.cpp` — 3 个 case 的 `uint64_t` → `uint32_t`（保留 BTB 覆盖，BTB 与 XLEN 无关）**

| 行 | 现状 | 改为 |
|----|------|------|
| `:25` / `:36` / `:46` | `std::make_unique<BranchPredictorPlugin<std::uint64_t>>(cfg.btb_entries)` | `std::make_unique<BranchPredictorPlugin<std::uint32_t>>(cfg.btb_entries)` |
| `:28` / `:38` / `:48` | `CpuFactory<std::uint64_t>::build_cpu(cfg)` | `CpuFactory<std::uint32_t>::build_cpu(cfg)` |

- [ ] **Step 4: integration 侧 `cfg.isa` 值纠偏（`T` 已是 uint32，非 T64 风险点）**

- `tests/cpu/integration/test_7stage_riscv.cpp`：`:38` / `:71` / `:139` 的 `cfg.isa = "rv64gc";` → `"rv32im"`（`:105` 的 `cfg.isa = p["isa"];` 随 JSON 自动生效，**不动**）
- `tests/cpu/integration/test_10stage_riscv.cpp`：`:37` / `:104` / `:120` 的 `cfg.isa = "rv64gc";` → `"rv32im"`（`:76` 的 `p["isa"]` 不动）
- `tests/cpu/integration/test_3stage_riscv.cpp`：`:34` 的 `cfg.isa = "rv32imac";` → `"rv32im"`

- [ ] **Step 5: 重新生成构建系统 + 编译**

Run: `cmake -S . -B build >/dev/null && cmake --build build -j$(nproc) 2>&1 | tail -4`
Expected: 编译成功（此刻护栏尚未加，T64 已全部迁走 ⇒ 仍应成功）

- [ ] **Step 6: 验证迁移结果（`nm` 应无 T64 实例化）**

Run: `for f in $(find build/tests/CMakeFiles -name '*.o'); do nm -C "$f" 2>/dev/null | grep -q "RiscvIntAluPlugin<unsigned long>\|RiscvBranchPlugin<unsigned long>" && echo "T64 残留: $f"; done; echo "scan done"`
Expected: 仅 `scan done`（**无 T64 残留输出**）

Run: `grep -rn "uint64_t>::build_cpu\|CpuFactory<T64>\|BranchPredictorPlugin<std::uint64_t>" tests/ || echo "AC-clean: 0"`
Expected: `AC-clean: 0`

- [ ] **Step 7: 回归（口径：`[cpu]` = 129 − 1 + 5 = **133**）**

Run: `./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -3`
Expected: `All tests passed (… assertions in 133 test cases)`

- [ ] **Step 8: Defer commit**

---

### Task 5: schema — `xlen` 属性 + `isa` enum 收窄 + DSE 示例纠偏

**Files:**
- Modify: `ip/cpu/configs/cpu_params_schema.json:18-23`、`tools/dse/sweep_config.example.json:8`

- [ ] **Step 1: 改 `isa` 节点 + 插 `xlen` 属性**

```json
        "isa": {
          "type": "string",
          "enum": ["rv32i", "rv32im", "rv32imac"],
          "default": "rv32im",
          "description": "指令集架构。implemented: rv32i/rv32im；declared-reserved: rv32imac, 以及规划值 rv64i/rv64gc（后者 schema 不收录, 运行时可接受并 emit warn, 不参与 dispatch, 见 ADR-087 §D1）；其余取值运行时 fail-fast。"
        },
        "xlen": {
          "type": "integer",
          "enum": [32, 64],
          "default": 32,
          "description": "XLEN 位宽 (ADR-087 §D2)。与 isa 的关联校验在 cf::cpu::detail::validate_isa_xlen（build_cpu 入口）。"
        },
```

- [ ] **Step 2: 核查 schema `:204` 条件约束（**不修改**，但必须确认它与 Task 6 的 JSON 改法自洽）**

Run: `sed -n '200,210p' ip/cpu/configs/cpu_params_schema.json`
Expected: 条件为 `isa ∈ {rv32i,rv32im,rv32imac} ⇒ mmu_mode const "sv32"` —— 该约束**正确**（XLEN=32 下只有 Sv32 合法），Task 6 因此必须把 3 个 JSON 的 `mmu_mode` 一并纠偏为 `sv32`

- [ ] **Step 3: 修 `tools/dse/sweep_config.example.json:8`**

```json
  "isa":                 ["rv32i", "rv32im"],
```

- [ ] **Step 4: 验证 JSON 语法 + AC3 前半**

Run: `python3 -c "import json; json.load(open('ip/cpu/configs/cpu_params_schema.json')); json.load(open('tools/dse/sweep_config.example.json')); print('json ok')"`
Expected: `json ok`

Run: `grep -rn '"rv64' ip/cpu/configs/ || echo "AC-clean: 0 hits"`
Expected: `AC-clean: 0 hits`（schema enum 收窄后此 grep 可机械化验证 = proposal AC3 前半）

- [ ] **Step 5: Defer commit**

---

### Task 6: JSON ×4 纠偏（`isa` + `mmu_mode`）

**Files:**
- Modify: `ip/cpu/configs/cpu_default.json:5,10`、`cpu_deep_pipeline.json:5,19`、`cpu_superscalar.json:5,17`、`cpu_embedded.json:5`

- [ ] **Step 1: 4 个文件 `isa` 改写**

`cpu_default.json` / `cpu_deep_pipeline.json` / `cpu_superscalar.json`:
```json
    "isa": "rv32im",
```
`cpu_embedded.json`:
```json
    "isa": "rv32im",   // 原 rv32imac: A 扩展零实现, 起 declared-reserved 误导 (ADR-087 §D1)
```

- [ ] **Step 2: 3 个文件 `mmu_mode` 改写（XLEN=32 ⇒ 只有 Sv32 合法；与 schema `:204` 条件自洽）**

| 文件 | 现状 | 改为 |
|------|------|------|
| `cpu_default.json:10` | `"mmu_mode": "sv39"` | `"mmu_mode": "sv32"` |
| `cpu_deep_pipeline.json:19` | `"mmu_mode": "sv48"` | `"mmu_mode": "sv32"` |
| `cpu_superscalar.json:17` | `"mmu_mode": "sv39"` | `"mmu_mode": "sv32"` |

> 行为影响说明：3 个 JSON 的 `enable_mmu` 均为 `false`（`cpu-pipeline-tick-fix` F2 的 Bare smoke 语义）⇒ MMU 路径 inert，`sv39/sv48 → sv32` 不改变运行行为；**仍必须回归** `[mmu]` / `[cpu-l1-mmu-demo]`（Task 6 Step 4）。

- [ ] **Step 3: schema 有效性核验（过滤已知 pre-existing 基线）**

```bash
python3 - <<'EOF'
import json, jsonschema
s = json.load(open('ip/cpu/configs/cpu_params_schema.json'))
v = jsonschema.Draft7Validator(s)
for f in ['cpu_default','cpu_deep_pipeline','cpu_superscalar','cpu_embedded']:
    d = json.load(open(f'ip/cpu/configs/{f}.json'))
    errs = [e.message for e in v.iter_errors(d)
            if 'Additional properties are not allowed' not in e.message]  # 已知基线: 顶层 _comment
    print(f, 'OK' if not errs else errs)
EOF
```
Expected: 4 行全 `OK`（**`_comment` 撞 `additionalProperties:false` 属 pre-existing 基线**，登记在 design.md R8，不在本 change 修）

- [ ] **Step 4: 回归 MMU 相关 family（`mmu_mode` 改动的安全网）**

Run: `./build/bin/chipforge_tests "[mmu],[cpu-l1-mmu-demo]" 2>&1 | tail -3`
Expected: 59 + 6 = 65 cases 全 PASS

- [ ] **Step 5: Defer commit**

---

### Task 7: `tools/cpu_sim` 去 pin + 解析 `xlen`

**Files:**
- Modify: `tools/cpu_sim/main.cpp:16`、`:85-90`（`load_config` 内 `params` 解析区）、`:180-190`（pin 删除）

- [ ] **Step 1: 头注释 `:16` 改写（**替换文本不得含 `isa="rv32i"` 字面量**，否则 AC4 的 grep 自我否定）**

原行：
```cpp
//   - isa="rv32i" 显式 pin (修复既有 CpuFactory<uint32_t> vs config.isa="rv64gc" 不一致)
```
改为：
```cpp
//   - ISA 声明跟随 config (rv64-isa-declaration-correction; 校验 SSOT 见
//     CpuFactory::build_cpu → cf::cpu::detail::validate_isa_xlen, ADR-087 §D1/D2)
```

- [ ] **Step 2: `load_config()` 补 `xlen` 解析（schema 声明了但被静默丢弃 = `cpu-pipeline-tick-fix` F1 同型 bug）**

在 `cfg.isa = p.value("isa", cfg.isa);` 之后新增：
```cpp
    cfg.xlen = p.value("xlen", cfg.xlen);
```

- [ ] **Step 3: 删 pin 及其注释（`:180-190` 区）**

改为：
```cpp
  // --------------------------------------------------------------------------
  // 5.2: 加载 JSON 配置
  //  (ISA 声明 pin 已移除 — rv64-isa-declaration-correction; 校验 SSOT 在
  //   CpuFactory::build_cpu → cf::cpu::detail::validate_isa_xlen, ADR-087 §D1/D2)
  // --------------------------------------------------------------------------
  cf::cpu::CPUConfig cfg = load_config(config_path);
```
（**不得**保留任何含 `isa="rv32i"` 字面量的行）

- [ ] **Step 4: 证实无 `--isa` CLI flag（AC4 前置）**

Run: `grep -n '"--isa"' tools/cpu_sim/main.cpp || echo "no --isa flag"`
Expected: `no --isa flag`（不存在 ⇒ AC4“若存在”分支自动满足）

- [ ] **Step 5: AC4 grep 自检（**必须 0 命中**）**

Run: `grep -n 'isa="rv32i"' tools/cpu_sim/main.cpp || echo "AC4-clean: 0 hits"`
Expected: `AC4-clean: 0 hits`

- [ ] **Step 6: 回归 5 个 e2e 用例（`[cpu-integration]` 内，`[cpu-sim]` tag 不存在）**

Run: `./build/bin/chipforge_tests "[cpu-integration]" 2>&1 | tail -4`
Expected: 96 cases / 95 PASS / **1 pre-existing FAIL**（`test_7stage_riscv.cpp:124`，与本 change 无关，基线已登记）

- [ ] **Step 7: Defer commit**

---

### Task 8: int_alu.h / branch.h 编译期护栏

**Files:**
- Modify: `ip/cpu/arch/riscv/int_alu.h:88`（`static T compute`）、`ip/cpu/arch/riscv/branch.h:111`（`static bool evaluate_branch`）**与** `branch.h` 的 `build()` 闭包内（真实分支判定路径）

- [ ] **Step 1: `int_alu.h` — `static T compute(OpCode op, T rs1, T rs2)` 函数体首行**

```cpp
  // ADR-087 §D4 (Change 1 提前): RV32 TLM 语义 XLEN 预算 — 实例化点触发
  static_assert(sizeof(T) == 4,
                "RV32 TLM path assumes 32-bit XLEN; RV64 requires the "
                "phase-5 block gate see ADR-087 G3");
```
（`build()` 内的真实 ALU 运算经 `compute()`（`int_alu.h` 的 `result = compute(op, rs1_val, op2)`）⇒ 该落点覆盖真实路径）

- [ ] **Step 2: `branch.h` — 两处护栏（真实路径 + 静态入口）**

真实路径（`build()` 的 `at_stage` 闭包体首行，闭包体即函数体 ⇒ 同样在实例化点触发）：
```cpp
      // ADR-087 §D4 (Change 1 提前): RV32 TLM 语义 XLEN 预算 — 实例化点触发
      static_assert(sizeof(T) == 4,
                    "RV32 TLM path assumes 32-bit XLEN; RV64 requires the "
                    "phase-5 block gate see ADR-087 G3");
```
静态入口（`static bool evaluate_branch(std::uint8_t funct3, T rs1, T rs2)` 函数体首行）：同一条 `static_assert`（测试可直接命中该入口）

> 两处都要加：只加 `evaluate_branch` ⇒ 真实分支判定（`build()` 闭包内的 `int32_t` 强转 BLT/BGE）不受护栏覆盖（critic SHOULD_FIX 6）。

- [ ] **Step 3: 双 binary 全量 build（CH_MEM 侧不 include `int_alu.h`/`branch.h`，实测 `nm` 只有 `compute_static`/`safe_divisor_u` ⇒ 护栏不触发）**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -3 && cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -3`
Expected: 两 binary 均编译成功（**若意外失败** ⇒ 说明 CH_MEM 侧存在 T=4 字节整型实例化，停下报告，**不得静默删除护栏**）

- [ ] **Step 4: 回归**

Run: `./build/bin/chipforge_tests "[cpu],[cpu-integration]" 2>&1 | tail -4`
Expected: `[cpu]` 133 PASS；`[cpu-integration]` 96 / 95 PASS + 1 pre-existing FAIL

- [ ] **Step 5: Defer commit**

---

### Task 9: 变异对照 — 证明护栏在实例化点生效（ADR-087 R4 证据）

**Files:**
- Test: 无新文件（mutation check）

- [ ] **Step 1: 预演 sed 命中范围（避免误伤既有 `static_assert(std::is_unsigned<T>...)`）**

Run: `grep -n "static_assert" ip/cpu/arch/riscv/branch.h`
Expected: 只命中本 change 新增的 `sizeof(T) == 4` 断言与既有 `is_unsigned` 断言（后者文本为 `is_unsigned`，与 sed 模式不匹配 ⇒ 安全）

- [ ] **Step 2: 临时把 `branch.h` 护栏改 `sizeof(T) == 8`**

Run: `sed -i 's/static_assert(sizeof(T) == 4/static_assert(sizeof(T) == 8/' ip/cpu/arch/riscv/branch.h && grep -n "sizeof(T) == 8" ip/cpu/arch/riscv/branch.h`
Expected: 命中 2 行（两处护栏）

- [ ] **Step 3: 编译必须失败**

Run: `cmake --build build -j4 2>&1 | tail -8`
Expected: **编译失败**，错误含 `static assertion failed` + “RV32 TLM path assumes 32-bit XLEN”（登记为 ADR-087 R4 变异对照证据）

- [ ] **Step 4: 还原并确认编译恢复**

Run: `sed -i 's/static_assert(sizeof(T) == 8/static_assert(sizeof(T) == 4/' ip/cpu/arch/riscv/branch.h && grep -c "static_assert(sizeof(T) == 4)" ip/cpu/arch/riscv/branch.h && cmake --build build -j$(nproc) 2>&1 | tail -2`
Expected: 计数 `2` + 编译成功

- [ ] **Step 5: Defer commit**

---

### Task 10: 删 `reg_file.cpp` 死代码

**Files:**
- Delete: `ip/cpu/plugins/reg_file.cpp`

- [ ] **Step 1: 终次确认 0 引用（防止采纳期新引用）**

Run: `grep -rn "reg_file.cpp" --include=CMakeLists.txt . | grep -v build || echo "0 CMake refs"`
Run: `grep -rln 'ip/cpu/plugins/reg_file.cpp' ip/ src/ tests/ tools/ | grep -v "^ip/cpu/plugins/reg_file.cpp:" || echo "0 includes"`
Expected: 两处 0

- [ ] **Step 2: 删除与验证**

Run: `git rm ip/cpu/plugins/reg_file.cpp && cmake --build build -j$(nproc) 2>&1 | tail -2 && ./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -2`
Expected: build 成功 + `[cpu]` 133 PASS

Run: `grep -rn "RegFilePlugin<std::uint64_t>" ip/ || echo "AC-clean: 0"`
Expected: `AC-clean: 0`（AC5 后半）

- [ ] **Step 3: Defer commit**

---

### Task 11: 缺口表 §2 状态列翻转

**Files:**
- Modify: `ip/cpu/docs/rv64_readiness_gap.md` §2 表（0.1–0.6 / 0.9 / 0.10）

- [ ] **Step 1: 状态列 `⬜` → `✅ (rv64-isa-declaration-correction)`（8 行）**

0.1（三态语义）/ 0.2（`isa` 装饰字段 → 契约校验，注解写 `✅ 契约化（dispatch 留 phase-5）`）/ 0.3（schema xlen）/ 0.4（3 JSON isa）/ 0.5（cpu_embedded）/ 0.6（integration-guide — **architect 批已修为 rv32im**，此处仅追认）/ 0.9（cpu_sim 去 pin）/ 0.10（reg_file.cpp 删除）。0.7 / 0.8 保持 ✅ 不动。

- [ ] **Step 2: 行号口径核验（**不假设，实跑**）**

Run: `bash tools/doc_link_check.sh`
Expected: 0 broken（§2 的“行号口径” blockquote 已声明以状态列为准；若本步报出行号漂移，按实际漂移逐条修正 §2 单元格中的行号引用）

- [ ] **Step 3: Defer commit**

---

### Task 12: CI 软门禁 `check_xlen_hygiene.sh`

**Files:**
- Create: `tools/check_xlen_hygiene.sh`
- Modify: `.github/workflows/architecture-gates.yml`（portability 步骤后插一段，`continue-on-error: true`）
- Test: 脚本自检（dry 探针，**不得 stash / 不得写进 `ip/`**）

- [ ] **Step 1: 新脚本（全量内容；allowlist = 缺口表 §5 实测的 7 个真实文件 / 9 处定义点）**

```bash
#!/usr/bin/env bash
# tools/check_xlen_hygiene.sh — ADR-087 §D6 软门禁 (warn-only, rv64-isa-declaration-correction)
# 新增 XLEN 硬编码假设必须登记 ip/cpu/docs/rv64_readiness_gap.md (缺口表 §5)。
# allowlist = 缺口表 §5 登记的 9 处 kXlenBits 定义点（分布于 7 个文件）:
#   3.1 ip/cpu/plugins/reg_file_chmem.h
#   3.2 ip/cpu/plugins/decode_chmem.h
#   3.3 ip/cpu/arch/riscv/int_alu_chmem.h
#   3.4 ip/cpu/plugins/branch_chmem.h
#   3.7 ip/cpu/plugins/mmu_ptw_chmem.h
#   3.8 ip/cpu/cpu_factory_chmem.h
#   3.10 ip/mmu/plugins/MMUPlugin_chmem.h
# 出现新定义点 → WARN (exit 0); STRICT_XLEN_GATE=yes 时升级 BLOCK (exit 1)。
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

ALLOW="ip/cpu/plugins/reg_file_chmem.h
ip/cpu/plugins/decode_chmem.h
ip/cpu/arch/riscv/int_alu_chmem.h
ip/cpu/plugins/branch_chmem.h
ip/cpu/plugins/mmu_ptw_chmem.h
ip/cpu/cpu_factory_chmem.h
ip/mmu/plugins/MMUPlugin_chmem.h"

WARN=0
while IFS= read -r hit; do
  [ -z "$hit" ] && continue
  f="${hit%%:*}"
  case "$f" in *.h) ;; *) continue ;; esac
  if ! grep -qx "$f" <<< "$ALLOW"; then
    echo "WARN: 未登记的 kXlenBits 定义点（须入缺口表 §5）: $hit"
    WARN=$((WARN + 1))
  fi
done < <(grep -rnE "kXlenBits *=" --include='*.h' ip/ || true)

echo "check_xlen_hygiene: warn=${WARN} (ADR-087 §D6, allowlist 口径见缺口表 §5)"
if [ "$WARN" -gt 0 ] && [ "${STRICT_XLEN_GATE:-}" = "yes" ]; then
  echo "STRICT_XLEN_GATE=yes → BLOCK"
  exit 1
fi
exit 0
```

- [ ] **Step 2: gates.yml 接入（`Run ADR-040 Portability check` 步骤之后）**

```yaml
      - name: Check XLEN hygiene (check_xlen_hygiene.sh, warn-only per ADR-087 D6)
        run: |
          echo "=== Running check_xlen_hygiene.sh ==="
          bash tools/check_xlen_hygiene.sh
        continue-on-error: true
```

- [ ] **Step 3: 基线自检（必须 0 warn）**

Run: `chmod +x tools/check_xlen_hygiene.sh && bash tools/check_xlen_hygiene.sh`
Expected: `check_xlen_hygiene: warn=0`

- [ ] **Step 4: 变异自检（dry 探针，**只读复制到 `/tmp/opencode`，不写 `ip/`、不 stash**）**

Run:
```bash
rm -rf /tmp/opencode/xlen_probe && mkdir -p /tmp/opencode/xlen_probe \
  && cp -r ip /tmp/opencode/xlen_probe/ip \
  && printf '\nstatic constexpr std::size_t kXlenBits = 32;\n' >> /tmp/opencode/xlen_probe/ip/cpu/plugins/ibus_chmem.h \
  && grep -rnE "kXlenBits *=" /tmp/opencode/xlen_probe/ip/cpu/plugins/ibus_chmem.h
```
Expected: 探针文件命中（证明 grep 口径有效）；**仓库 `ip/` 未被改动**（`git status --porcelain ip/` 应为空）

Run: `bash tools/check_xlen_hygiene.sh && echo "warn-only confirmed" && git status --porcelain ip/ && echo "(ip/ clean)"`
Expected: `warn-only confirmed` + `(ip/ clean)`

- [ ] **Step 5: Defer commit**

---

### Task 13: 全门禁收口 + AC 终验

**Files:**
- Test: 全部门禁（终验表落在 change tasks.md）

- [ ] **Step 1: 双 binary 全量测试（CH_MEM 全量实测 >120s，用 timeout 上限）**

Run: `timeout 1200 bash tools/run_chipforge_tests.sh --all 2>&1 | tail -8`
Expected: 4 ctest entries 全 PASS

- [ ] **Step 2: 5 架构门禁 + hygiene**

Run:
```bash
bash tools/verify_adr.sh | tail -1
bash tools/verify_plugin_decision.sh | tail -1
bash tools/check_plugin_portability.sh | tail -1
bash tools/doc_link_check.sh | tail -1
bash tools/roadmap-r9-check.sh | tail -1
bash tools/check_xlen_hygiene.sh | tail -1
```
Expected: 6 项全绿（AC8；hygiene 必须 `warn=0`）

- [ ] **Step 3: AC 逐条机械复核（grep 落证）**

```bash
grep -rn '"rv64' ip/cpu/configs/ soc/                                   || echo "AC3-a OK: 0"   # 期望 0
grep -n 'isa="rv32i"' tools/cpu_sim/main.cpp                            || echo "AC4 OK: 0"     # 期望 0
grep -rn "RegFilePlugin<std::uint64_t>" ip/                             || echo "AC5 OK: 0"     # 期望 0
grep -n 'std::string isa = "rv32im"' ip/cpu/cpu_factory.h                || echo "AC1-a OK"      # 期望命中
grep -n "xlen = 32" ip/cpu/cpu_factory.h                                || echo "AC1-b OK"      # 期望命中
grep -rn "uint64_t>::build_cpu\|CpuFactory<T64>" tests/                 || echo "AC2 OK: 0"     # 期望 0（T64 已迁走）
```
Expected: 6 条全符合

- [ ] **Step 4: 计数终验（与基线快照表对照）**

Run: `./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -2` → 期望 `133 test cases`（129 − 1 + 5）
Run: `./build/bin/chipforge_tests "[cpu-integration]" 2>&1 | tail -3` → 期望 `96 | 95 passed | 1 failed`（1 failed = pre-existing）
Run: `./build/bin/chipforge_tests "[riscv-tests],[mmu]" 2>&1 | tail -2` → 期望 `107 test cases` 全 PASS
Run: `./build/bin/chipforge_tests_chmem --list-tests 2>&1 | tail -1` → 期望 `130 test cases`

- [ ] **Step 5: `openspec change validate`**

Run: `openspec change validate rv64-isa-declaration-correction`
Expected: valid（0 error）

- [ ] **Step 6: 分批 commit（按 4 类改动拆，不做单 mega-commit）**

```bash
git add tests/ ip/cpu/cpu_factory.h && git commit -m "feat(cpu): isa/xlen 三层 fail-fast 契约 + T64 实例化点迁移 (ADR-087 Change 1)"
git add ip/cpu/configs/ tools/dse/sweep_config.example.json && git commit -m "fix(cpu): 声明面纠偏 — isa enum 收窄 + mmu_mode 归位 sv32"
git add ip/cpu/arch/riscv/ tools/cpu_sim/main.cpp ip/cpu/plugins/reg_file.cpp && git commit -m "feat(cpu): TLM 模板入口 XLEN 编译期护栏 + 死代码清除"
git add tools/check_xlen_hygiene.sh .github/workflows/architecture-gates.yml ip/cpu/docs/rv64_readiness_gap.md && git commit -m "ci: XLEN 假设 hygiene 软门禁 + 缺口表状态列翻转"
```
（message 按当次实际改动行数调整；**每批只含本类文件**，不混批）
