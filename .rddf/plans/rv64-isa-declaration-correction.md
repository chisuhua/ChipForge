# rv64-isa-declaration-correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `CPUConfig::isa` 从「装饰性字段 + 虚假默认值」变成三层 fail-fast 可校验契约（ADR-087 §D1/D2），同时把 TLM 语义层模板入口的静默错算最小集（int_alu/branch）关进编译期护栏，并清除唯一 RV64 死代码定时炸弹 `reg_file.cpp`。

**Architecture:** 校验函数 `cf::cpu::detail::validate_isa_xlen(config, sizeof(T)*8)` 是唯一 SSOT，`build_cpu()` 首行接线（单点入口，cpu_sim 无需重复实现）；声明面（schema/JSON/示例）只覆盖可生效取值，reserved 值走 std::cerr warn 不 throw；编译期护栏放模板运算入口函数体首行（实例化点触发），与文件作用域 include 误伤隔离；现存 T64 实例化点按 ADR-087 AC2 迁移到 T32（RV64 真实路径归 phase-5 Change 2）。

**Tech Stack:** C++23（`std::invalid_argument` + `std::cerr`，无新依赖）；Catch2 amalgamated（`tests/catch2/`，`catch_amalgamated.hpp`）；CMake `GLOB_RECURSE` 自动发现新测试文件，**但无 `CONFIGURE_DEPENDS` ⇒ 新增测试文件后必须重跑 `cmake -S . -B build`**。

**上游 ADR / SSOT:** ADR-087（`docs/architecture/adr/ADR-087-isa-declaration-integrity-and-rv64-readiness.md` §D1/D2/D4/D6/R4）· 缺口表 `ip/cpu/docs/rv64_readiness_gap.md`（§2 状态列 = 本 change 验收台账；§5 = hygiene allowlist SSOT）· proposal.md + design.md Decision 1–8 / Risks R1–R8。

---

## 修订记录（critic 审查后修订，2026-10-23）

### 第 2 轮 critic 复审（session `ses_e98421060ffequMkI7v6JzOtpY`，REJECT：5 BLOCKER / 6 SHOULD_FIX / 4 NITS）

| 编号 | critic 指控 | 本计划处置 | 落点 |
|------|------------|-----------|------|
| B-1 | hygiene allowlist 漏 2 个文件（`ibus_chmem.h` / `dmem_chmem.h`）⇒ 脚本基线 `warn=2`，AC8 永不可达 | allowlist 补至 **9 条**（实测 `kXlenBits =` = **9 命中 / 9 文件**；缺口表 §5 的 3.5/3.6 描述未点名 `kXlenBits` 但文件内确有定义点） | Task 12 Step 1 |
| B-2 | Task 13 Step 1 期望「4 ctest entries 全 PASS」不可达（`chipforge_tests` 条目**基线即红**） | 改为「3 PASS + `chipforge_tests` Failed（失败集合恰为 `{test_7stage_riscv.cpp:124}`）」+ 机判口径 `ctest -E '^chipforge_tests$'` 3 条全绿 | Task 13 Step 1 |
| B-3 | Task 1 Step 1 命令（无 filter）与期望（`[cpu]` 口径）不符 | 拆成 Step 1（无 filter → 481/480/1）+ Step 1b（`"[cpu]"` → 129/461） | Task 1 |
| B-4 | Task 5 的 `grep '"rv64'` 期望 0 命中，但 3 个 JSON 归 Task 6 ⇒ 该时点不可达 | Task 5 期望改为「恰好 3 命中（Task 6 消除）」；0 命中断言移到 Task 6 Step 3 | Task 5 / Task 6 |
| B-5 | Task 3 Step 3 接线代码块含不存在的符号 `reset_canonical_ordering_counters_placeholder`（全仓仅 plan 命中） | 删掉占位块，只留真实上下文两行 + 显式「不要写占位符号」告警 | Task 3 Step 3 |
| S-1 | Task 3 Step 5 Expected 自相矛盾（129 / 134 并列）且该时点 `[cpu]` 必 FAIL | 改为「本步必然 2 处 FAIL（列明 file:line）+ 收敛到 Task 4」+ 计数时序注（134 → 133） | Task 3 Step 5 |
| S-2 | proposal「共 10 处」与「7 处」冲突 | 统一为 **7 处**（integration 3 文件 9 行中 2 行是 `p["isa"]` 不动） | proposal item 13 |
| S-3 | `tests/cpu/test_topology_builder.cpp:35` 残留 `rv64gc` 未覆盖 ⇒ 「无 reserved 残留」为假 | 纳入 Task 4 Step 4b + 增加 `tests/` 残留 grep（Task 4 Step 6 / Task 13 Step 3） | Task 4 / Task 13 |
| S-4 | Task 12 变异探针零检验力（脚本扫仓库 `ip/`，探针在 `/tmp`） | 脚本加 `XLEN_SCAN_ROOT` 环境变量（默认 `ip`）+ 探针指向非 allowlist 文件 | Task 12 Step 1/4 |
| S-5 | Task 13 的 AC2 grep 弱于 Task 4（漏 `BranchPredictorPlugin<std::uint64_t>`） | 两处统一同一 pattern + 新增 tests/ 残留 grep | Task 13 Step 3 |
| S-6 | 分批 commit 机制自破（`git rm` 残留 index ⇒ 第 1 批夹带删除 + 第 3 批 pathspec 失效） | 全部改用 `git commit -- <paths>` 限定 pathspec，并拆成 5 批 | Task 13 Step 6 |
| N-1~N-4 | 134/133 时序读感、缺口表 §1 行过期、`tools/README.md` 未登记、sweep 示例无 AC 覆盖 | 分别：加时序注 / Task 11 Step 1b 补 §1 标注 / Task 12 Step 4b 登记 / Task 13 Step 3 grep 范围补 sweep 示例 | 见各 Task |

### 第 3 轮 critic 终审（session `ses_e97f8b25affe6Pc37MJ0YVnNdm`，REJECT：5 BLOCKER）

| 编号 | critic 指控 | 本计划处置 | 落点 |
|------|------------|-----------|------|
| B-1 | 新测试文件**必然**含 `isa="rv64gc"` / `isa="rv32imac"`（AC1 第 4/5 用例的输入）⇒ 「tests/ 无 reserved 残留」机判永不可达 | 残留 grep **排除** `test_cpu_factory_isa_xlen.cpp`（专用 reserved 语义测试）；AC1 的 5 用例一个不删 | Task 4 Step 6 / Task 13 Step 3 |
| B-2 | Task 9 Step 4 的 `grep -c "static_assert(sizeof(T) == 4)"` 恒 0（落笔行尾是 `,` 不是 `)`）⇒ `&&` 断链 ⇒ 编译恢复验证被静默跳过 | pattern 去掉 `)` | Task 9 Step 4 |
| B-3 | Task 13 Step 6 批 4 对已 `git rm` 的路径做 `git add` ⇒ exit 128 ⇒ **该批永不产生**（删除一直留在 index） | 批 4 去掉 `git add`，直接 `git commit -- <path>`；告警文字的「第 3 批」更正为「第 4 批」 | Task 13 Step 6 |
| B-4 | Task 6 Step 1 往 `cpu_embedded.json` 写**行内 `//` 注释** ⇒ `json.load` / nlohmann 解析崩 ⇒ `test_schema_m5_19.cpp:37` + `test_3stage_riscv.cpp:126` 新增失败 | 注释移出 ```json 围栏（改为围栏外说明句） | Task 6 Step 1 |
| B-5 | Task 3 Step 5 的「2 个 FAIL」实为 **7**，且 `:56` 不可能 FAIL（reserved 路径只 warn） | **实编译验证**（不是照抄推理）：注入 helper + 接线后实跑 ⇒ 实测 **7 个本 change FAIL**（`test_cpu_factory.cpp:28/43/66/88` + `test_branch_predictor_runtime_btb.cpp:20/32/42`）+ 1 pre-existing；`embedded_config` 实测 **PASS** ⇒ Expected 表按实测重写（含「勿误改成 throw」警示） | Task 3 Step 5 |
| 附注 | `nm` pattern `BranchPredictorPlugin<unsigned long>` 匹配不到真实符号（该模板有 5 个实参，demangle 后是 `..., 16ul, 16ul, ...`）⇒ 假阴性 | 三条 pattern 去掉结尾 `>` | Task 4 Step 6 |

### builder 自检（第 2 轮，代码块「逐字跑一次」，已随实施验证）

| 自检 | 方法 | 结果 |
|------|------|------|
| hygiene 脚本三态 | 从 plan 抽出脚本原文（`sed -n '/^#!\/usr\/bin\/env bash$/,/^exit 0$/p'`）实跑 | 基线 `warn=0` ✓；探针 `warn=1`（恰 1 条）✓；`STRICT_XLEN_GATE=yes` → exit 1 ✓ |
| **allowlist 匹配口径** | 探针跑 | 首次实跑暴露真 bug：全等比较在绝对路径探针模式下把 9 个已登记文件全误报（`warn=10`）⇒ 改为**路径后缀匹配**，探针改落非 allowlist 文件 |
| `validate_isa_xlen` helper + 接线 | 把 plan 落笔形态注入 `cpu_factory.h` 副本，`g++ -std=c++23 -fsyntax-only` | **exit 0** ✓（含 `<iostream>` 补充 + 接线行） |
| **落点错误复现** | 把 helper 误插在 `:113` 闭合与 `:115` 重开之间 | 实测报错 `‘CPUConfig’ does not name a type` + `‘validate_isa_xlen’ is not a member of ‘cf::cpu::detail’` ⇒ 落点必须落在 struct 之后（已写入 Task 3 Step 2 告警） |
| 新测试文件（5 用例） | 从 plan 抽出代码块 + 遮蔽 header，`g++ -fsyntax-only` | **exit 0** ✓ |

### 第 1 轮 critic 审查（session `ses_e9879fb37ffeBO2wBIropJMaDe`，REJECT：6 BLOCKER / 7 SHOULD_FIX / 6 NITS）

critic 对本 change 的 P1 产物给出 **REJECT：6 BLOCKER / 7 SHOULD_FIX / 6 NITS**，本计划已逐条修复：

| 编号 | critic 指控 | 本计划处置 | 落点 |
|------|------------|-----------|------|
| B1 | 护栏 `static_assert(sizeof(T)==4)` 会打断 `chipforge_tests` 编译（`IntAlu<T64>`/`Branch<T64>` 今天已被实例化）→ AC2 与 AC9「0 退化」互斥 | **用户裁决：迁移 T64 调用点，保留编译期护栏**；删除 RV64 构建探测 case + 其余改 T32；`[cpu]` 计数随之调整 | Task 4 |
| B2 | Task 5 改 `isa` 后 3 个 JSON 触发 schema `:204` 条件（`rv32* ⇒ mmu_mode const sv32`）而 `mmu_mode` 是 sv39/sv48 → 变 schema 非法 | 3 个 JSON 的 `mmu_mode` 同步纠偏为 `sv32`（32-bit XLEN 下只有 Sv32 合法，schema 约束本身是对的）；`_comment` 撞 `additionalProperties:false` 属 **pre-existing 基线**，登记不修 | Task 6 + design R8 |
| B3 | hygiene 脚本 allowlist 2 条路径写错 → `warn=0` 不成立 | allowlist 改为缺口表 §5 + **实测 grep 复核**：`kXlenBits =` 共 **9 命中 / 9 个文件**（含 `ibus_chmem.h` / `dmem_chmem.h` —— 草案 §5 的 3.5/3.6 虽未在描述里点名 `kXlenBits`，文件内确有定义点），allowlist 补齐 9 条 | Task 12 |
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

## 基线快照（HEAD `9b45fac` 实测，2026-10-23）

| 口径 | 实测值 | 备注 |
|------|--------|------|
| **TLM binary 全量（无 filter）** | **481 cases / 480 PASS / 1 FAIL**（67,135 assertions） | 唯一 FAIL = `tests/cpu/integration/test_7stage_riscv.cpp:124`（node_count `9 == 17`），**pre-existing（HEAD 即红）**；口径 = **不新增失败** |
| `[cpu]` | **129 cases / 461 assertions PASS** | 本 change 后 = 129 − 1（删 `build_cpu_rv64`）+ 5（新用例）= **133**（时序：Task 3 后 = 134 → Task 4 删 case 后 = 133） |
| `[cpu-integration]` | **96 cases / 95 PASS / 1 FAIL** | 唯一失败同上（pre-existing） |
| `[riscv-tests]` | 48/48 PASS | — |
| `[mmu]` | 59/59 PASS | — |
| `[cache]` | 25/25 PASS | — |
| `chipforge_tests_chmem` | binary 总 **130 cases**；`[chmem]` tag **89** | 两个口径不可混用；`run_chipforge_tests.sh --all` 全量实测 **>120s 未完**（AGENTS.md 记载 833s），本 change 收口用 `timeout 1200` |
| **ctest 条目** | **4 条**：`verify_plugin_decision` / `chipforge_tests` / `chipforge_tests_chmem` / `cpu_verilator_sim_add` | `chipforge_tests` 条目**基线即红**（含上述 1 个 pre-existing FAIL）⇒ 终验口径见 Task 13 Step 1 |
| `kXlenBits` 定义点 | **9 命中 / 9 个文件** | 缺口表 §5 的 3.1/3.2/3.3/3.4/3.5/3.6/3.7/3.8/3.10 —— 3.5（`ibus_chmem.h`）/ 3.6（`dmem_chmem.h`）描述里未点名 `kXlenBits`，但文件内确有定义点 ⇒ hygiene allowlist 必须 9 条 |

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

- [ ] **Step 1: 跑 TLM 全量基线（无 filter）**

Run: `./build/bin/chipforge_tests 2>&1 | tail -3`
Expected: `test cases: 481 | 480 passed | 1 failed` + `assertions: 67135 | 67134 passed | 1 failed`（**1 failed = pre-existing** `test_7stage_riscv.cpp:124`，本 change 不修它）

> 注：`All tests passed (461 assertions in 129 test cases)` 是 `"[cpu]"` **带 filter** 的输出口径，不是全量口径 —— 二者不可混用。

- [ ] **Step 1b: 跑 `[cpu]` 过滤基线**

Run: `./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -3`
Expected: `All tests passed (461 assertions in 129 test cases)`

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
>
> ⚠️ **落点必须精确**：插在 **`struct CPUConfig { … };` 的 `};` 之后**（此时 `cf::cpu` 已打开 ⇒ `CPUConfig` 可见 + `detail` 解析为 `cf::cpu::detail`）。若误插在 `:113` 闭合与 `:115` 重开**之间**（全局命名空间）⇒ 报 `‘CPUConfig’ does not name a type` + `‘validate_isa_xlen’ is not a member of ‘cf::cpu::detail’; did you mean ‘detail::validate_isa_xlen’?`（**已实测复现**）。

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

落笔形态（**只加这一行**，前一行是既有的 ADR-048 代码，不要改动）：

```cpp
    // ADR-048: reset canonical ordering counters at each build_cpu entry
    cf::cpu::detail::reset_canonical_counters();
    cf::cpu::detail::validate_isa_xlen(config, sizeof(T) * 8);  // ADR-087 §D1/D2
```

> ⚠️ 不要写 `reset_canonical_ordering_counters_placeholder` 之类占位符号（全仓无此符号 ⇒ 编译失败）。真实符号是 `cf::cpu::detail::reset_canonical_counters`（定义 `ip/cpu/cpu_factory.h:60`，调用点 `:320`）。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -4 && ./build/bin/chipforge_tests "[isa-xlen]" 2>&1 | tail -4`
Expected: **5 passed**（“单发”用例失败 ⇒ 检查 cerr 只输出一次）

- [ ] **Step 5: 回归既有 family（逗号 = OR，单 tag 写法在 catch2 里是 AND，多 positional 会假绿）**

Run: `./build/bin/chipforge_tests "[cpu],[cpu-integration],[riscv-tests]" 2>&1 | tail -4`
Expected（**本步必然有 7 个 FAIL + 1 个 pre-existing FAIL，属 Task 4 未做的已知中间态** —— **实测值**，2026-10-23 于 `feature/rv64-isa-declaration-correction` 上跑出）：

| 用例 | 状态 | 原因 | 收敛点 |
|------|------|------|--------|
| `tests/cpu/test_cpu_factory.cpp:28`（`default_config`） | **FAIL** | Task 3 已把默认改成 `rv32im`，断言仍是 `rv64gc` | Task 4 Step 2 |
| `tests/cpu/test_cpu_factory.cpp:43`（`build_cpu_rv64`） | **FAIL** | `rv64gc` + T64 ⇒ `kernel_xlen(64) != xlen(32)` ⇒ throw | Task 4 Step 1（删 case） |
| `tests/cpu/test_cpu_factory.cpp:66`（`default_json_config`） | **FAIL** | `rv64gc` + T64 ⇒ 同上 | Task 4 Step 2 |
| `tests/cpu/test_cpu_factory.cpp:88`（`build_cpu_registers_11_real_plugins`） | **FAIL** | T64 + 默认 `rv32im`（implemented）⇒ `kernel_xlen(64) != 32` ⇒ throw | Task 4 Step 2 |
| `tests/cpu/test_branch_predictor_runtime_btb.cpp:20/32/42`（3 case） | **FAIL ×3** | 同上（`CpuFactory<std::uint64_t>::build_cpu` + 默认 config） | Task 4 Step 3 |
| `tests/cpu/test_cpu_factory.cpp:53`（`embedded_config`，`isa="rv32imac"`） | **PASS** | declared-reserved 分支：`kernel_xlen(32) == config.xlen(32)` ⇒ **只 warn 不 throw**（design D1 的 reserved 宽容语义）⇒ **不要误改成 throw** | — |
| `tests/cpu/integration/test_7stage_riscv.cpp:124` | **FAIL（pre-existing）** | 基线即红，与本 change 无关 | — |

`[riscv-tests]` 48 PASS。
**计数时序**：`[cpu],[cpu-integration],[riscv-tests]` 合计 **230 cases = 222 passed + 8 failed**（7 本 change + 1 pre-existing）；Task 3 后 `[cpu]` = 129 + 5（新用例）= **134**；Task 4 删 `build_cpu_rv64` case 后 = **133**（终验口径）。

- [ ] **Step 6: Defer commit**

---

### Task 4: 迁移 T64 实例化点（用户裁决：保留编译期护栏）

> 现状实测（`nm -C` 扫 `build/tests/CMakeFiles/**/*.o`）：`RiscvIntAluPlugin<unsigned long>` / `RiscvBranchPlugin<unsigned long>` 已被实例化于 `test_cpu_factory.cpp.o` 与 `test_branch_predictor_runtime_btb.cpp.o`。ADR-087 AC2 要求 `CpuFactory<uint64_t>` **编译失败** ⇒ 必须先把这些实例化点迁走，否则 Task 8 的护栏会让整个 `chipforge_tests` 编不过。

**Files:**
- Modify: `tests/cpu/test_cpu_factory.cpp`、`tests/cpu/test_branch_predictor_runtime_btb.cpp`
- Modify: `tests/cpu/integration/test_7stage_riscv.cpp`、`test_10stage_riscv.cpp`、`test_3stage_riscv.cpp`
- Modify: `tests/cpu/test_topology_builder.cpp`（残留 `rv64gc` 清理，见 Step 4b）

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

⇒ 上述 3 文件 `cfg.isa` 共 **9 行**，其中 2 行是 `cfg.isa = p["isa"];`（不动）⇒ **字面量待改 = 7 处**。

- [ ] **Step 4b: `test_topology_builder.cpp` 残留清理**

`:35` 的 `cfg.isa = "rv64gc";` → `"rv32im"`。该文件 4 个 `[cpu]` case 不调 `build_cpu`（只调 `TopologyBuilder<N>::expand`）⇒ 功能无碍，但**仓库内不留 reserved 值残留**（否则 proposal item 13 的声明为假）。

- [ ] **Step 5: 重新生成构建系统 + 编译**

Run: `cmake -S . -B build >/dev/null && cmake --build build -j$(nproc) 2>&1 | tail -4`
Expected: 编译成功（此刻护栏尚未加，T64 已全部迁走 ⇒ 仍应成功）

- [ ] **Step 6: 验证迁移结果（`nm` 应无 T64 实例化）**

Run: `for f in $(find build/tests/CMakeFiles -name '*.o'); do nm -C "$f" 2>/dev/null | grep -q "RiscvIntAluPlugin<unsigned long\|RiscvBranchPlugin<unsigned long\|BranchPredictorPlugin<unsigned long" && echo "T64 残留: $f"; done; echo "scan done"`
Expected: 仅 `scan done`（**无 T64 残留输出**；三条 pattern 覆盖 IntAlu / Branch / BranchPredictor 三个会被新护栏拦到的模板）

> ⚠️ pattern **不带结尾 `>`**：`BranchPredictorPlugin` 有 5 个模板实参，demangle 后是 `BranchPredictorPlugin<unsigned long, 16ul, 16ul, (unsigned char)8, 1ul>` ⇒ 带 `>` 的 pattern **匹配不到真实符号**（假阴性 ⇒ 残留漏检）。`RiscvIntAluPlugin` / `RiscvBranchPlugin` 只有一个模板实参，带不带 `>` 都能命中。

Run: `grep -rn "uint64_t>::build_cpu\|CpuFactory<T64>\|BranchPredictorPlugin<std::uint64_t>" tests/ || echo "AC-clean: 0"`
Expected: `AC-clean: 0`

Run: `grep -rn 'isa = "rv64\|isa = "rv32imac' tests/ --include='*.cpp' | grep -v 'test_cpu_factory_isa_xlen.cpp' || echo "AC-clean: 0 (no reserved value in tests/)"`
Expected: `AC-clean: 0 (no reserved value in tests/)`

> ⚠️ **必须排除 `test_cpu_factory_isa_xlen.cpp`**：该文件按 AC1 第 4/5 用例**必须**含 `isa="rv64gc"` / `isa="rv32imac"` 两种 reserved 输入（否则 AC1 只剩 3 个用例，与声明矛盾）⇒ 残留判据 = **除该专用测试外** tests/ 无 reserved 值。

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

- [ ] **Step 4: 验证 JSON 语法（**注意：此时 `ip/cpu/configs/` 仍有 3 处 `rv64`，那是 Task 6 的活**）**

Run: `python3 -c "import json; json.load(open('ip/cpu/configs/cpu_params_schema.json')); json.load(open('tools/dse/sweep_config.example.json')); print('json ok')"`
Expected: `json ok`

Run: `grep -rn '"rv64' ip/cpu/configs/`
Expected: **恰好 3 命中**（`cpu_default.json:5` / `cpu_deep_pipeline.json:5` / `cpu_superscalar.json:5` 的 `"isa": "rv64gc"`）—— schema enum 收窄后 **schema 自身已 0 命中**（rv64* 只在 description 文本里，无前引号），3 个 JSON 由 **Task 6** 消除 ⇒ **0 命中的断言放在 Task 6 Step 3**（不要在本步断言 0）

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
`cpu_embedded.json`（**注释写在围栏外，不要写进行内 `//`** —— 行内注释会让 `json.load` / `nlohmann::json::parse` 直接崩 ⇒ `tests/cpu/schema/test_schema_m5_19.cpp:37` 与 `tests/cpu/integration/test_3stage_riscv.cpp:126`（`--config cpu_embedded.json`）会新增失败）：

```json
    "isa": "rv32im",
```

> 原值 `rv32imac` 的理由（A 扩展零实现，起 declared-reserved 误导，ADR-087 §D1）—— **写在上面的围栏外说明句里**，不写进 JSON。

- [ ] **Step 2: 3 个文件 `mmu_mode` 改写（XLEN=32 ⇒ 只有 Sv32 合法；与 schema `:204` 条件自洽）**

| 文件 | 现状 | 改为 |
|------|------|------|
| `cpu_default.json:10` | `"mmu_mode": "sv39"` | `"mmu_mode": "sv32"` |
| `cpu_deep_pipeline.json:19` | `"mmu_mode": "sv48"` | `"mmu_mode": "sv32"` |
| `cpu_superscalar.json:17` | `"mmu_mode": "sv39"` | `"mmu_mode": "sv32"` |

> 行为影响说明：3 个 JSON 的 `enable_mmu` 均为 `false`（`cpu-pipeline-tick-fix` F2 的 Bare smoke 语义）⇒ MMU 路径 inert，`sv39/sv48 → sv32` 不改变运行行为；**仍必须回归** `[mmu]` / `[cpu-l1-mmu-demo]`（Task 6 Step 4）。

- [ ] **Step 3: schema 有效性核验 + AC3 零命中（过滤已知 pre-existing 基线）**

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

Run: `grep -rn '"rv64' ip/cpu/configs/ soc/ || echo "AC3-a-clean: 0 hits"`
Expected: `AC3-a-clean: 0 hits`（**AC3 的零命中断言在本步**——Task 5 结束时仍有 3 命中）

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

Run: `sed -i 's/static_assert(sizeof(T) == 8/static_assert(sizeof(T) == 4/' ip/cpu/arch/riscv/branch.h && grep -c "static_assert(sizeof(T) == 4" ip/cpu/arch/riscv/branch.h && cmake --build build -j$(nproc) 2>&1 | tail -2`
Expected: 计数 `2` + 编译成功

> ⚠️ pattern **不带结尾 `)`**：护栏落笔形态是 `static_assert(sizeof(T) == 4,`（行尾是 `,`，message 换行）⇒ 带 `)` 的 pattern 恒匹配 0 行，且 `&&` 链会**在 `cmake --build` 之前断链** ⇒ 「还原后编译恢复」这一步被静默跳过。

- [ ] **Step 5: Defer commit**

---

### Task 10: 删 `reg_file.cpp` 死代码

**Files:**
- Delete: `ip/cpu/plugins/reg_file.cpp`

- [ ] **Step 1: 终次确认 0 引用（防止采纳期新引用）**

Run: `grep -rn "reg_file.cpp" --include=CMakeLists.txt . | grep -v build || echo "0 CMake refs"`
Run: `grep -rn '#include.*reg_file\.cpp' ip/ src/ tests/ tools/ | grep -v '^ip/cpu/plugins/reg_file.cpp:' || echo "0 includes"`
Expected: 两处 0

> ⚠️ 第 2 条必须用 **include 模式**（`#include.*reg_file\.cpp`）：若用路径子串匹配，会命中 **4 个文档提及**（`ip/cpu/docs/dse_architecture.md` / `implementation-plan/M4-integration.md` / `research/ooo_forward_compat_gap_analysis.md` / `rv64_readiness_gap.md`）与 ADR-087 ⇒ 期望值不再是 0（文档提及是预期的：缺口表 0.10 行正是登记该死代码）。

- [ ] **Step 2: 删除与验证**

Run: `git rm ip/cpu/plugins/reg_file.cpp && cmake --build build -j$(nproc) 2>&1 | tail -2 && ./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -2`
Expected: build 成功 + `[cpu]` 133 PASS

> 注：`git rm` 会把删除**留在 index**（未提交）。这正是 Task 13 Step 6 必须用 `git commit -- <paths>` 的原因（否则删除会被第 1 批夹带）。

Run: `grep -rn "RegFilePlugin<std::uint64_t>" ip/ || echo "AC-clean: 0"`
Expected: `AC-clean: 0`（AC5 后半）

- [ ] **Step 3: Defer commit**

---

### Task 11: 缺口表 §2 状态列翻转

**Files:**
- Modify: `ip/cpu/docs/rv64_readiness_gap.md` §2 表（0.1–0.6 / 0.9 / 0.10）

- [ ] **Step 1: 状态列 `⬜` → `✅ (rv64-isa-declaration-correction)`（8 行）**

0.1（三态语义）/ 0.2（`isa` 装饰字段 → 契约校验，注解写 `✅ 契约化（dispatch 留 phase-5）`）/ 0.3（schema xlen）/ 0.4（3 JSON isa）/ 0.5（cpu_embedded）/ 0.6（integration-guide — **architect 批已修为 rv32im**，此处仅追认）/ 0.9（cpu_sim 去 pin）/ 0.10（reg_file.cpp 删除）。0.7 / 0.8 保持 ✅ 不动。

- [ ] **Step 1b: 同步 §1 结论摘要中因本 change 失效的行**

§1 表中 `tools/cpu_sim/main.cpp:16,188`「显式 `cfg.isa = "rv32i"` / ✅ 代码自证」一行在 Task 7 后过期 ⇒ 在该行判定列追加 `✅ 已修复（rv64-isa-declaration-correction）`；同理 `ip/cpu/cpu_factory.h:127` 与 3 个 JSON 的行追加「已纠偏」标注（**只加标注，不删历史行**——§1 是当时事实的快照）。

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

- [ ] **Step 1: 新脚本（全量内容；allowlist = 缺口表 §5 + 实测 grep 复核的 **9 个真实文件 / 9 处定义点**）**

```bash
#!/usr/bin/env bash
# tools/check_xlen_hygiene.sh — ADR-087 §D6 软门禁 (warn-only, rv64-isa-declaration-correction)
# 新增 XLEN 硬编码假设必须登记 ip/cpu/docs/rv64_readiness_gap.md (缺口表 §5)。
# allowlist = 缺口表 §5 登记的 9 处 kXlenBits 定义点（分布于 9 个文件）:
#   3.1 ip/cpu/plugins/reg_file_chmem.h
#   3.2 ip/cpu/plugins/decode_chmem.h
#   3.3 ip/cpu/arch/riscv/int_alu_chmem.h
#   3.4 ip/cpu/plugins/branch_chmem.h
#   3.5 ip/cpu/plugins/ibus_chmem.h        (缺口表 3.5 描述未点名 kXlenBits, 但文件内确有定义点)
#   3.6 ip/cpu/plugins/dmem_chmem.h        (同上)
#   3.7 ip/cpu/plugins/mmu_ptw_chmem.h
#   3.8 ip/cpu/cpu_factory_chmem.h
#   3.10 ip/mmu/plugins/MMUPlugin_chmem.h
# 出现新定义点 → WARN (exit 0); STRICT_XLEN_GATE=yes 时升级 BLOCK (exit 1)。
# XLEN_SCAN_ROOT: 扫描根目录（默认 ip），供变异自检把探针目录指进来。
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
SCAN_ROOT="${XLEN_SCAN_ROOT:-ip}"

ALLOW="ip/cpu/plugins/reg_file_chmem.h
ip/cpu/plugins/decode_chmem.h
ip/cpu/arch/riscv/int_alu_chmem.h
ip/cpu/plugins/branch_chmem.h
ip/cpu/plugins/ibus_chmem.h
ip/cpu/plugins/dmem_chmem.h
ip/cpu/plugins/mmu_ptw_chmem.h
ip/cpu/cpu_factory_chmem.h
ip/mmu/plugins/MMUPlugin_chmem.h"

WARN=0
while IFS= read -r hit; do
  [ -z "$hit" ] && continue
  f="${hit%%:*}"
  case "$f" in *.h) ;; *) continue ;; esac
  # allowlist 用「路径后缀」匹配（而非全等）：探针模式下扫描根是绝对路径，
  # 全等比较会把 9 个已登记文件全部误报为 warn。
  registered=0
  while IFS= read -r a; do
    [ -z "$a" ] && continue
    case "$f" in *"$a") registered=1; break ;; esac
  done <<< "$ALLOW"
  if [ "$registered" -eq 0 ]; then
    echo "WARN: 未登记的 kXlenBits 定义点（须入缺口表 §5）: $hit"
    WARN=$((WARN + 1))
  fi
done < <(grep -rnE "kXlenBits *=" --include='*.h' "$SCAN_ROOT" || true)

echo "check_xlen_hygiene: warn=${WARN} (ADR-087 §D6, allowlist 口径见缺口表 §5, scan=${SCAN_ROOT})"
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

- [ ] **Step 4: 变异自检（dry 探针，**只读复制到 `/tmp/opencode`，不写 `ip/`、不 stash**；用 `XLEN_SCAN_ROOT` 把探针目录指给脚本，否则探针永不被扫描 = 零检验力）**

> 探针必须落在**非 allowlist 文件**上（如 `ip/cpu/plugins/branch_predictor.h`）：allowlist 内的文件即使新增定义点也不告警（按文件粒度登记）⇒ 探针放错文件 ⇒ 变异对照无检验力。

Run:
```bash
rm -rf /tmp/opencode/xlen_probe && mkdir -p /tmp/opencode/xlen_probe \
  && cp -r ip /tmp/opencode/xlen_probe/ip \
  && printf '\nstatic constexpr std::size_t kXlenBits = 32;\n' >> /tmp/opencode/xlen_probe/ip/cpu/plugins/branch_predictor.h \
  && XLEN_SCAN_ROOT=/tmp/opencode/xlen_probe/ip bash tools/check_xlen_hygiene.sh
```
Expected: **恰好 1 条** `WARN: 未登记的 kXlenBits 定义点`（指向探针路径）+ `warn=1`（**证明脚本能抓住未登记定义点**；若 `warn=9/10` ⇒ allowlist 匹配口径坏了，见脚本内「路径后缀匹配」注释）

Run: `bash tools/check_xlen_hygiene.sh && echo "warn-only confirmed" && git status --porcelain ip/ && echo "(ip/ clean)"`
Expected: `warn=0` + `warn-only confirmed` + `(ip/ clean)`（**仓库 `ip/` 未被改动**）

- [ ] **Step 4b: `tools/README.md` 登记新脚本**

在该文件既有 gate 脚本清单（`tools/README.md:11-14` 登记 4 个架构门禁脚本）处追加 `check_xlen_hygiene.sh` 一行（说明：ADR-087 §D6 XLEN 假设 hygiene 软门禁，warn 级）。

- [ ] **Step 5: Defer commit**

---

### Task 13: 全门禁收口 + AC 终验

**Files:**
- Test: 全部门禁（终验表落在 change tasks.md）

- [ ] **Step 1: 双 binary 全量测试（CH_MEM 全量实测 >120s，用 timeout 上限）**

Run: `timeout 1200 bash tools/run_chipforge_tests.sh --all 2>&1 | tail -8`
Expected: **3 个 ctest entry PASS + 1 个 entry 因 1 个 pre-existing FAIL 而 Failed**

| ctest entry | 基线态 | 本 change 后期望 |
|---|---|---|
| `verify_plugin_decision` | PASS | PASS |
| `chipforge_tests` | **Failed**（含 1 个 pre-existing FAIL） | **Failed**（**且失败集合恰为 `{test_7stage_riscv.cpp:124}`**，不得新增失败） |
| `chipforge_tests_chmem` | PASS | PASS |
| `cpu_verilator_sim_add` | PASS | PASS |

> ⚠️ 脚本 `--all` 路径 `ctest … ; exit $?` ⇒ 整体退出码非 0（**基线即如此**）。**不要把「退出码非 0」当成本 change 的失败**。机判口径：

Run: `./build/bin/chipforge_tests 2>&1 | grep -A2 "FAILED:" | grep -c "test_7stage_riscv.cpp:124"`
Expected: `1`（失败**仅**这一处）

Run: `ctest --test-dir build -E '^chipforge_tests$' --output-on-failure 2>&1 | tail -5`
Expected: `100% tests passed, 0 tests failed out of 3`（排除已知红的 TLM entry 后，其余 3 条全绿）

- [ ] **Step 2: 6 架构门禁 + hygiene**

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

- [ ] **Step 3: AC 逐条机械复核（grep 落证；**AC2 的 pattern 与 Task 4 Step 6 保持一致**）**

```bash
grep -rn '"rv64' ip/cpu/configs/ soc/ tools/dse/sweep_config.example.json || echo "AC3-a OK: 0"   # 期望 0
grep -n 'isa="rv32i"' tools/cpu_sim/main.cpp                            || echo "AC4 OK: 0"     # 期望 0
grep -rn "RegFilePlugin<std::uint64_t>" ip/                             || echo "AC5 OK: 0"     # 期望 0
grep -n 'std::string isa = "rv32im"' ip/cpu/cpu_factory.h                || echo "AC1-a OK"      # 期望命中
grep -n "xlen = 32" ip/cpu/cpu_factory.h                                || echo "AC1-b OK"      # 期望命中
grep -rn "uint64_t>::build_cpu\|CpuFactory<T64>\|BranchPredictorPlugin<std::uint64_t>" tests/ || echo "AC2 OK: 0"     # 期望 0（T64 已迁走）
grep -rn 'isa = "rv64\|isa = "rv32imac' tests/ --include='*.cpp' | grep -v 'test_cpu_factory_isa_xlen.cpp' || echo "AC-residual OK: 0"  # 期望 0（排除 reserved 语义专用测试）
```
Expected: 7 条全符合

- [ ] **Step 4: 计数终验（与基线快照表对照）**

Run: `./build/bin/chipforge_tests "[cpu]" 2>&1 | tail -2` → 期望 `133 test cases`（129 − 1 + 5）
Run: `./build/bin/chipforge_tests "[cpu-integration]" 2>&1 | tail -3` → 期望 `96 | 95 passed | 1 failed`（1 failed = pre-existing）
Run: `./build/bin/chipforge_tests "[riscv-tests],[mmu],[cache]" 2>&1 | tail -2` → 期望 `132 test cases` 全 PASS（48 + 59 + 25）
Run: `./build/bin/chipforge_tests_chmem --list-tests 2>&1 | tail -1` → 期望 `130 test cases`

- [ ] **Step 5: `openspec change validate`**

Run: `openspec change validate rv64-isa-declaration-correction`
Expected: valid（0 error）

- [ ] **Step 6: 分批 commit（按 4 类改动拆，不做单 mega-commit）**

> ⚠️ **必须用 `git commit -- <paths>` 限定 pathspec**：Task 10 的 `git rm` 会把删除**留在 index**，裸 `git commit` 提交整个 index ⇒ 第 1 批会夹带 `reg_file.cpp` 删除（违反本步纪律）。
> ⚠️ **批 4 不能带 `git add`**：该路径已被 `git rm` 删除 ⇒ `git add <已删除路径>` 返回 `fatal: pathspec ... did not match any files`（exit 128）⇒ `&&` 短路 ⇒ **该批永不产生**。正确写法是直接 `git commit -m "..." -- <path>`（pathspec 形式的 commit 会把已 staged 的删除一并提交）。

```bash
git add tests/ ip/cpu/cpu_factory.h && git commit -m "feat(cpu): isa/xlen 三层 fail-fast 契约 + T64 实例化点迁移 (ADR-087 Change 1)" -- tests/ ip/cpu/cpu_factory.h
git add ip/cpu/configs/ tools/dse/sweep_config.example.json && git commit -m "fix(cpu): 声明面纠偏 — isa enum 收窄 + mmu_mode 归位 sv32" -- ip/cpu/configs/ tools/dse/sweep_config.example.json
git add ip/cpu/arch/riscv/ tools/cpu_sim/main.cpp && git commit -m "feat(cpu): TLM 模板入口 XLEN 编译期护栏 + cpu_sim 去 pin" -- ip/cpu/arch/riscv/ tools/cpu_sim/main.cpp
git commit -m "chore(cpu): 删除 reg_file.cpp 死代码 (uint64_t 显式实例化 + keys_rv32 混用)" -- ip/cpu/plugins/reg_file.cpp
git add tools/check_xlen_hygiene.sh tools/README.md .github/workflows/architecture-gates.yml ip/cpu/docs/rv64_readiness_gap.md && git commit -m "ci: XLEN 假设 hygiene 软门禁 + 缺口表状态列翻转" -- tools/check_xlen_hygiene.sh tools/README.md .github/workflows/architecture-gates.yml ip/cpu/docs/rv64_readiness_gap.md
```
（message 按当次实际改动行数调整；**每批只含本类文件**，不混批；`-- <paths>` 保证 index 里其他内容不会被夹带）
