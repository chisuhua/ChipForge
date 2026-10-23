# rv64-isa-declaration-correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `CPUConfig::isa` 从"装饰性字段 + 虚假默认值"变成三层 fail-fast 可校验契约（ADR-087 §D1/D2），同时把 TLM 语义层模板入口的静默错算最小集（int_alu/branch）关进编译期护栏，并清除唯一 RV64 死代码定时炸弹 `reg_file.cpp`。

**Architecture:** 校验函数 `cf::cpu::detail::validate_isa_xlen(config, sizeof(T)*8)` 是唯一 SSOT，`build_cpu()` 首行接线（单点入口，cpu_sim 无需重复实现）；声明面（schema/JSON/示例）只覆盖可生效取值，reserved 值走 std::cerr warn 不 throw（保证 6 处既有测试 `rv64gc` 零改动）；编译期护栏放模板运算入口函数体首行（实例化点触发），与文件作用域 include 误伤隔离。

**Tech Stack:** C++23（`std::invalid_argument` + `std::cerr`，无新依赖）；Catch2 amalgamated（`tests/catch2/`，`catch_amalgamated.hpp`）；CMake GLOB_RECURSE 自动发现新测试文件，**无需改 CMakeLists**。

**上游 ADR / SSOT:** ADR-087（`docs/architecture/adr/ADR-087-isa-declaration-integrity-and-rv64-readiness.md` §D1/D2/D4/D6/R4）· 缺口表 `ip/cpu/docs/rv64_readiness_gap.md`（§2 状态列是本 change 的验收台账）· proposal.md + design.md Decision 1–6 / Risks R1–R7。

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `ip/cpu/cpu_factory.h` | `CPUConfig` 加 `xlen` 字段；`namespace detail` 加 `validate_isa_xlen()`；`build_cpu()` 首行接线 |
| `ip/cpu/configs/cpu_params_schema.json` | `xlen` 属性；`isa` enum 收窄 + default；reserved 值入 description |
| `ip/cpu/configs/cpu_default.json` / `cpu_deep_pipeline.json` / `cpu_superscalar.json` | `isa` → `rv32im` |
| `ip/cpu/configs/cpu_embedded.json` | `rv32imac` → `rv32im`（A 扩展零实现） |
| `tools/cpu_sim/main.cpp` | 删 `cfg.isa = "rv32i"` pin（:187-188）+ 头注释 :16 |
| `ip/cpu/arch/riscv/int_alu.h` | `static T compute()`（:88）函数体首行 `static_assert(sizeof(T)==4)` |
| `ip/cpu/arch/riscv/branch.h` | `static bool evaluate_branch()`（:112）函数体首行同 |
| `ip/cpu/plugins/reg_file.cpp` | **删除**（死代码，无 CMake 引用） |
| `ip/cpu/docs/rv64_readiness_gap.md` | §2 状态列翻转（0.1–0.6 / 0.9 / 0.10 → ✅） |
| `tools/check_xlen_hygiene.sh` | **新建** CI 软门禁（warn-only） |
| `.github/workflows/architecture-gates.yml` | 接入 hygiene 脚本（`continue-on-error: true`） |
| `tools/dse/sweep_config.example.json` | sweep grid `rv64i` → `rv32im` |

### Tests

| File | Responsibility |
|---|---|
| `tests/cpu/test_cpu_factory_isa_xlen.cpp` | **新建**（[cpu][isa-xlen] 5 用例）：默认值契约 / unsupported throw / implemented 冲突 throw / T↔xlen 交叉 throw / reserved warn 单发 |

---

### Task 1: 基线快照（零退化锚）

**Files:**
- Test: 无新文件（只跑既有基线）

- [ ] **Step 1: 跑 TLM 快返基线**

Run: `./build/bin/chipforge_tests 2>&1 | tail -3`
Expected: `All tests passed`（当前基线 481 case / 67026 assertions 量级）

- [ ] **Step 2: 跑 CH_MEM 抽查基线**

Run: `./build/bin/chipforge_tests_chmem "[chmem]" 2>&1 | tail -3`
Expected: All passed（130 case 全绿口径，抽查即可，全量收口在 Task 12）

- [ ] **Step 3: 记录 `[cpu]` 当前计数**

Run: `./build/bin/chipforge_tests "[cpu]" 2>&1 | grep -E "passed|assertions" | tail -2`
Expected: 129 case（+ 本次新文件后 134，任务 3 的期望值以此为准）

- [ ] **Step 4: Defer commit**（execute 阶段统一提交）

---

### Task 2: 写失败测试（新 `test_cpu_factory_isa_xlen.cpp`，5 用例）

**Files:**
- Create: `tests/cpu/test_cpu_factory_isa_xlen.cpp`

- [ ] **Step 1: Write the failing test**（完整文件，照抄即可）

```cpp
// tests/cpu/test_cpu_factory_isa_xlen.cpp
// Family: [cpu] — ADR-087 §D1/D2 isa/xlen 契约 (rv64-isa-declaration-correction)
// 覆盖: 三层 fail-fast + declared-reserved 单发 warn (design.md Decision 1/2)
// 变异对照: 删 warn /cerr 改 cout / 删 throw → 本文件至少 1 用例 FAIL
#include "catch_amalgamated.hpp"
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ip/cpu/cpu_factory.h"

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
  REQUIRE_THROWS_AS(cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg),
                    std::invalid_argument);
}

TEST_CASE("implemented isa xlen mismatch fail-fast", "[cpu][isa-xlen]") {
  CPUConfig cfg;  // isa=rv32im (implemented), 显式 xlen=64 冲突
  cfg.xlen = 64;
  REQUIRE_THROWS_AS(cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg),
                    std::invalid_argument);
}

TEST_CASE("kernel T vs xlen cross-check fail-fast (reserved path)",
          "[cpu][isa-xlen]") {
  CerrCapture cap;
  CPUConfig cfg;  // isa=rv64gc (declared-reserved) + 显式 xlen=64 + T=uint32
  cfg.isa = "rv64gc";
  cfg.xlen = 64;
  REQUIRE_THROWS_AS(cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg),
                    std::invalid_argument);
  REQUIRE(cap.str().find("[declared-reserved]") != std::string::npos);
}

TEST_CASE("declared-reserved accepted with single warning", "[cpu][isa-xlen]") {
  CerrCapture cap;
  CPUConfig cfg;  // isa=rv32imac (declared-reserved), xlen 默认 32 一致
  cfg.isa = "rv32imac";
  REQUIRE_NOTHROW(cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg));
  REQUIRE(cap.str().find("[declared-reserved]") != std::string::npos);
  // 单发语义: 本用例仅一次装载, warn 只应出现一次
  REQUIRE(cap.str().find("[declared-reserved]") ==
          cap.str().rfind("[declared-reserved]"));
}
```

- [ ] **Step 2: Run test to verify it fails（编译失败也算"红"）**

Run: `cmake --build build -j$(nproc) && ./build/bin/chipforge_tests "[isa-xlen]" 2>&1 | tail -5`
Expected: **FAIL** — `CPUConfig` 无 `xlen` 字段 → 编译错误（这是 TDD 红；无需运行期失败）

- [ ] **Step 3: Defer commit**

---

### Task 3: `cpu_factory.h` — `xlen` 字段 + `validate_isa_xlen` + `build_cpu` 接线

**Files:**
- Modify: `ip/cpu/cpu_factory.h:127`（isa 默认值）、`:134` 前（插 xlen 字段）、struct 之后（detail 命名空间 helper）、`:322`（build_cpu 首行接线）
- Test: `tests/cpu/test_cpu_factory_isa_xlen.cpp`（Task 2 文件转绿）

- [ ] **Step 1: 改 `CPUConfig` 两个字段（:127 附近）**

```cpp
// 旧:
  std::string isa = "rv64gc";       // rv32i/rv32im/rv32imac/rv64i/rv64gc
// 新:
  // ADR-087 §D1: 三态语义 implemented=rv32i/rv32im, declared-reserved=rv32imac/rv64i/rv64gc
  // 之外的取值 = unsupported (validate_isa_xlen fail-fast)。默认 = 实际实现水平。
  std::string isa = "rv32im";
  // ADR-087 §D2: XLEN 数据字段 (默认 32)。与 isa 关联校验见 detail::validate_isa_xlen。
  // xlen 与 ISA 扩展字母正交 (rv32imac 的 A/C 与 xlen 无关)。
  std::uint8_t xlen = 32;
```

- [ ] **Step 2: struct 之后加校验 helper（唯一 SSOT，design D1/D6）**

在 `struct CPUConfig` 结束后（`};` 下一个空行）插入：

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
          std::to_string(kernel_xlen) + "-bit; instantiate CpuFactory<uint32_t> "
                                        "(ADR-087 §D1)");
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
```

（若文件已 include `<iostream>` 则跳过；否则补 `#include <iostream>`，位于既有 `<stdexcept>`/`<string>` 附近按字母序插入。）

- [ ] **Step 3: `build_cpu()` 首行接线（:320 `reset_canonical_counters();` 之后）**

```cpp
    // ADR-048: reset canonical ordering counters at each build_cpu entry
    cf::cpu::detail::reset_canonical_counters();
    cf::cpu::detail::validate_isa_xlen(config, sizeof(T) * 8);  // ADR-087 §D1/D2
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) && ./build/bin/chipforge_tests "[isa-xlen]" 2>&1 | tail -4`
Expected: **5 passed**（warning "单发"用例若失败 → 检查 cerr 只输出一次）

- [ ] **Step 5: 回归既有 `[cpu]` 與 e2e**

Run: `./build/bin/chipforge_tests "[cpu]" "[cpu-integration]" "[riscv-tests]" 2>&1 | tail -3`
Expected: 全 PASS（count = Task 1 Step 3 记录值 + 5 新 case；test_7stage/test_10stage 的 `rv64gc` 走 reserved-warn，cerr 可见但不影响 assertion）

- [ ] **Step 6: Defer commit**

---

### Task 4: schema — `xlen` 属性 + `isa` enum 收窄 + DSE 示例纠偏

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
          "description": "XLEN 位宽 (ADR-087 §D2)。与 isa 的关联校验在 CpuFactory::validate_isa_xlen（build_cpu 入口）。"
        },
```

- [ ] **Step 2: 修 `tools/dse/sweep_config.example.json:8`**

```json
  "isa":                 ["rv32i", "rv32im"],
```

- [ ] **Step 3: 验证 schema 语法 + AC 前置**

Run: `python3 -c "import json; json.load(open('ip/cpu/configs/cpu_params_schema.json')); json.load(open('tools/dse/sweep_config.example.json')); print('json ok')"`
Expected: `json ok`

Run: `grep -rn '"rv64' ip/cpu/configs/ || echo "AC-clean: 0 hits"`
Expected: `AC-clean: 0 hits`（schema enum 收窄后此 grep 可机械化验证 = proposal AC3 前半）

- [ ] **Step 4: Defer commit**

---

### Task 5: JSON ×4 isa 纠偏

**Files:**
- Modify: `ip/cpu/configs/cpu_default.json:5`、`cpu_deep_pipeline.json:5`、`cpu_superscalar.json:5`、`cpu_embedded.json:5`

- [ ] **Step 1: 四个文件 `isa` 值改写**

`cpu_default.json` / `cpu_deep_pipeline.json` / `cpu_superscalar.json`:
```json
    "isa": "rv32im",
```
`cpu_embedded.json`:
```json
    "isa": "rv32im",   // 原 rv32imac: A 扩展零实现, 起 declared-reserved 误导 (ADR-087 §D1)
```

- [ ] **Step 2: 验证**

Run: `python3 -c "import json; [json.load(open(f)) for f in ['ip/cpu/configs/cpu_default.json','ip/cpu/configs/cpu_deep_pipeline.json','ip/cpu/configs/cpu_superscalar.json','ip/cpu/configs/cpu_embedded.json']]; print('ok')"`
Expected: `ok`

- [ ] **Step 3: Defer commit**

---

### Task 6: cpu_sim 去 pin

**Files:**
- Modify: `tools/cpu_sim/main.cpp:16`、`:184-188`

- [ ] **Step 1: 删 pin 与其注释**

`HEader 注释 :16`（`//   - isa="rv32i" 显式 pin (修复既有 CpuFactory<uint32_t> vs config.isa="rv64gc" 不一致)`）改为：
```cpp
//   - isa 跟随 config (rv64-isa-declaration-correction; config 见 --config 默认 ip/cpu/configs/cpu_default.json)
```

`:184-188`:
```cpp
  // --------------------------------------------------------------------------
  // 5.2: 加载 JSON 配置
  //  (isa="rv32i" pin 已移除 — rv64-isa-declaration-correction; 校验 SSOT 在
  //   CpuFactory::build_cpu → detail::validate_isa_xlen, ADR-087 §D1/D2)
  // --------------------------------------------------------------------------
  cf::cpu::CPUConfig cfg = load_config(config_path);
```

- [ ] **Step 2: 证实无 `--isa` CLI flag（AC4 前置）**

Run: `grep -n '"--isa"' tools/cpu_sim/main.cpp || echo "no --isa flag"`
Expected: `no --isa flag`（不存在 ⇒ AC4"若存在"分支自动满足）

- [ ] **Step 3: 回归 5 个 e2e 用例**

Run: `./build/bin/chipforge_tests "[cpu-integration]" "[cpu-sim]" 2>&1 | tail -3`（或显式 filter `"{3,5,7,10}stage_add_elf_end_to_end","[test]test_cpu_sim_real_tohost"`）
Expected: 全 PASS（0 退化记录进 Task 12 终验表）

- [ ] **Step 4: Defer commit**

---

### Task 7: int_alu.h / branch.h 编译期护栏

**Files:**
- Modify: `ip/cpu/arch/riscv/int_alu.h:88`、`ip/cpu/arch/riscv/branch.h:112`

- [ ] **Step 1: `int_alu.h` — `static T compute(OpCode op, T rs1, T rs2)` 函数体首行**

```cpp
  // ADR-087 §D4 (Change 1 提前): RV32 TLM 语义 XLEN 预算 — 实例化点触发
  static_assert(sizeof(T) == 4,
                "RV32 TLM path assumes 32-bit XLEN; RV64 requires the "
                "phase-5 block gate see ADR-087 G3");
```

- [ ] **Step 2: `branch.h` — `static bool evaluate_branch(...)` 函数体首行**（同消息串、同注释）

- [ ] **Step 3: 双 binary 全量 build（R6 编译面验证）**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -3 && cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -3`
Expected: 两 binary 均编译成功（CH_MEM 侧不实例化 T=4 字节整型的这两个入口 ⇒ 护栏不触发；若意外触发，停下改走 R6 缓解——在 Occurrence 点补 `if constexpr` 前先 oracle 咨询，**不得静默删除护栏**）

- [ ] **Step 4: 回归 `[cpu-integration]`（护栏承载用例）**

Run: `./build/bin/chipforge_tests "[cpu-integration]" "[cpu]" 2>&1 | tail -3`
Expected: 全 PASS（护栏是纯编译期断言，RV32 实例化恒通过）

- [ ] **Step 5: Defer commit**

---

### Task 8: 变异对照 — 证明护栏在实例化点生效（ADR-087 R4 证据）

**Files:**
- Test: 无新文件（mutation check）

- [ ] **Step 1: 临时把 `branch.h` 护栏改 `sizeof(T) == 8`**

Run: `sed -i 's/static_assert(sizeof(T) == 4/static_assert(sizeof(T) == 8/' ip/cpu/arch/riscv/branch.h`

- [ ] **Step 2: 编译必须失败**

Run: `cmake --build build -j4 2>&1 | tail -6`
Expected: **编译失败**，错误含 `static assertion failed` + "RV32 TLM path assumes 32-bit XLEN"（向 ADR-087 R4 的变异对照登记）

- [ ] **Step 3: 还原**

Run: `sed -i 's/static_assert(sizeof(T) == 8/static_assert(sizeof(T) == 4/' ip/cpu/arch/riscv/branch.h && cmake --build build -j$(nproc) 2>&1 | tail -2`
Expected: 编译成功

- [ ] **Step 4: Defer commit**

---

### Task 9: 删 `reg_file.cpp` 死代码

**Files:**
- Delete: `ip/cpu/plugins/reg_file.cpp`

- [ ] **Step 1: 终次确认 0 引用（防止采纳期新引用）**

Run: `grep -rn "reg_file.cpp" --include=CMakeLists.txt . | grep -v build || echo "0 CMake refs"`；`grep -rln 'ip/cpu/plugins/reg_file.cpp' ip/ src/ tests/ tools/ | grep -v "reg_file.cpp:" || echo "0 includes"`
Expected: 两处 0

- [ ] **Step 2: 删除与验证**

Run: `git rm ip/cpu/plugins/reg_file.cpp && cmake --build build -j$(nproc) 2>&1 | tail -2 && ./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -2`
Expected: build 成功 + `[cpu]` 全 PASS

Run: `grep -rn "RegFilePlugin<std::uint64_t>" ip/ || echo "AC-clean: 0"`
Expected: `AC-clean: 0`（AC5 后半）

- [ ] **Step 3: Defer commit**

---

### Task 10: 缺口表 §2 状态列翻转

**Files:**
- Modify: `ip/cpu/docs/rv64_readiness_gap.md` §2 表（0.1–0.6 / 0.9 / 0.10）

- [ ] **Step 1: 状态列 `⬜` → `✅ (rv64-isa-declaration-correction)`（8 行）**

0.1（三态语义）/ 0.2（`isa` 装饰字段→契约校验，"dispatch 不做"口径已在状态注解：`✅ 契约化（dispatch 留 phase-5）`）/ 0.3（schema xlen）/ 0.4（3 JSON）/ 0.5（cpu_embedded）/ 0.6（integration-guide）/ 0.9（cpu_sim 去 pin）/ 0.10（reg_file.cpp 删除）。0.7 / 0.8 保持 ✅ 不动。

- [ ] **Step 2: 行号口径注记自检**

Run: `bash tools/doc_link_check.sh`
Expected: 0 broken（§1 表的行号引用如已漂移，状态列注解已豁免——§2 "行号口径" blockquote 已声明以状态列为准，无需逐行改）

- [ ] **Step 3: Defer commit**

---

### Task 11: CI 软门禁 `check_xlen_hygiene.sh`

**Files:**
- Create: `tools/check_xlen_hygiene.sh`
- Modify: `.github/workflows/architecture-gates.yml`（portability 步骤后插一段，`continue-on-error: true`）
- Test: 脚本自检（人为加一处硬编码 → warn 可见）

- [ ] **Step 1: 新脚本（全量内容）**

```bash
#!/usr/bin/env bash
# tools/check_xlen_hygiene.sh — ADR-087 §D6 软门禁 (warn-only, rv64-isa-declaration-correction)
# 新增 XLEN 硬编码假设必须登记 ip/cpu/docs/rv64_readiness_gap.md (缺口表 §5)。
# allowlist = 缺口表 §5 已登记的 9 处 kXlenBits 定义点。
# 出现新定义点 → WARN (exit 0); STRICT_XLEN_GATE=yes 时升级 BLOCK (exit 1)。
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

ALLOW="ip/cpu/plugins/reg_file_chmem.h
ip/cpu/plugins/decode_chmem.h
ip/cpu/plugins/int_alu_chmem.h
ip/cpu/plugins/branch_chmem.h
ip/cpu/plugins/ibus_chmem.h
ip/cpu/plugins/dmem_chmem.h
ip/cpu/plugins/mmu_ptw_chmem.h
ip/cpu/plugins/cpu_factory_chmem.h
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

- [ ] **Step 3: 自检（变异对照门禁本身）**

Run: `chmod +x tools/check_xlen_hygiene.sh && bash tools/check_xlen_hygiene.sh`
Expected: `warn=0`（当前 9 处全在 allowlist）

Run: `echo 'static constexpr int kXlenBits = 32;' >> /tmp/opencode/xlen_probe.h && (cd ip/cpu/plugins && git stash push --quiet 2>/dev/null; cp /tmp/opencode/xlen_probe.h probe_xlen.h)` — **不要真写进 ip/**：改为 `mkdir -p /tmp/opencode/probe && cp -r ip /tmp/opencode/probe_ip && echo 'static constexpr int kXlenBits = 32;' >> /tmp/opencode/probe_ip/cpu/plugins/ibus_chmem.h && grep -rnE "kXlenBits *=" /tmp/opencode/probe_ip/cpu/plugins/ibus_chmem.h`
Expected: 探针文件命中（证明 grep 口径有效）——真门禁变异对照用 dry 方式（临时 untracked 文件不入仓）

Run: `bash tools/check_xlen_hygiene.sh && echo "warn-only confirmed"`
Expected: exit 0 即 warn-only 证实

- [ ] **Step 4: Defer commit**

---

### Task 12: 全门禁收口 + AC 终验

**Files:**
- Test: 全部门禁（终验表落在 change tasks.md）

- [ ] **Step 1: 双 binary 全量测试**

Run: `bash tools/run_chipforge_tests.sh --all 2>&1 | tail -6`
Expected: 4 ctest entries 全 PASS（AC9：`[cpu]`134+=[129+5] · `[cpu-integration]` 36/36 · `[riscv-tests]` 48/48 · `[chmem]` 130/130 · `[mmu]` 59/59）

- [ ] **Step 2: 5 架构门禁字母表**

Run:
```bash
bash tools/verify_adr.sh | tail -1
bash tools/verify_plugin_decision.sh | tail -1
bash tools/check_plugin_portability.sh | tail -1
bash tools/doc_link_check.sh | tail -1
bash tools/roadmap-r9-check.sh | tail -1
bash tools/check_xlen_hygiene.sh | tail -1
```
Expected: 6 项全绿（AC8 + 软门禁 warn=0）

- [ ] **Step 3: AC 逐条机械复核清单（grep 落证）**

```bash
grep -rn '"rv64' ip/cpu/configs/ soc/            # AC3 → 0
grep -n 'isa="rv32i"' tools/cpu_sim/main.cpp     # AC4 → 0
grep -rn "RegFilePlugin<std::uint64_t>" ip/      # AC5 → 0
grep -n "std::string isa = \"rv32im\"" ip/cpu/cpu_factory.h   # AC1 默认值
grep -n "xlen = 32" ip/cpu/cpu_factory.h                      # AC1 xlen
```
Expected: 5 条全符合

- [ ] **Step 4: 标记 change 所有 AC checkbox → P2.5 review 前状态**

Run: `openspec change validate rv64-isa-declaration-correction`
Expected: valid（0 error）

- [ ] **Step 5: 统一 commit（execute 阶段交付点）**

Run: `git add -A && git commit -m "feat(cpu): rv64-isa-declaration-correction — isa/xlen 三层 fail-fast 契约 + TLM 模板护栏 + 死代码清除 (ADR-087 Change 1)"`（message 由 execute 阶段按当次实际改动行数调整）
