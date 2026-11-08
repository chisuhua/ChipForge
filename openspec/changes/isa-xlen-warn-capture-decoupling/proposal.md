---
initiative: wave7-architecture-governance
priority: P1
version_target: v0.11.0
status: proposed
adr: []
depends_on: []
---

# isa-xlen-warn-capture-decoupling —— 「declared-reserved warn 走 cerr」的断言与 reporter 解耦（★ C1 的 L2）

> **来源**：`openspec/changes/backend-parity-open-core/tasks.md` §3.5 的 **L2**（★ 该行标注「⇒ 独立 change」）
> ★ **本 change 的根因已铁证锁定**（★ 2026-11-08 实测）：★ 见 §1。

## Why

★ **3 个测试用例在 XML reporter 下失败、在默认 reporter 下通过** ⇒ ★ 它们**依赖 reporter 的实现细节**
⇒ ★ 这正是 `run_catch2_with_baseline.sh`（★ 用 XML reporter 产报告）会踩的坑。

### 1. 复现（2026-11-08 实测）

```bash
./build/bin/chipforge_tests "[isa-xlen]"                       # ⇒ All tests passed (20 assertions in 9 test cases)  EXIT=0
./build/bin/chipforge_tests "[isa-xlen]" -r xml -o out.xml     # ⇒ EXIT=3（失败）
```

★ XML 里的 3 个失败断言（★ **与 L2 描述的「3 个 case」完全吻合**）：

```
<Expression success="false" type="REQUIRE" filename="tests/cpu/test_cpu_factory_isa_xlen.cpp" line="60">
<Expression success="false" type="REQUIRE" filename="tests/cpu/test_cpu_factory_isa_xlen.cpp" line="68">
<Expression success="false" type="REQUIRE" filename="tests/cpu/test_cpu_factory_isa_xlen.cpp" line="96">
```

★ 三者的断言内容完全相同：

```cpp
REQUIRE(cap.str().find("[declared-reserved]") != std::string::npos);
```

★ 而 XML 的展开值是 **`18446744073709551615 (0xffffffffffffffff)`** ⇒ ★ **就是 `std::string::npos`**
⇒ ★ **`cap.str()` 是空的** ⇒ ★ **没捕获到任何 warn**。

### 2. 根因（★ 读代码 + 读 XML 确认）

| # | 事实 | 后果 |
|---|---|---|
| ① | 测试用 `CerrCapture` 捕获 `std::cerr`：★ `std::cerr.rdbuf(buf_.rdbuf())`（★ `tests/cpu/test_cpu_factory_isa_xlen.cpp:19-22`） | ★ 捕获的是**全局流** ⇒ 与 reporter 耦合 |
| ② | 生产侧的 warn 直接写 `std::cerr`（★ `ip/cpu/cpu_factory.h:216`） | ★ 同样耦合全局流 |
| ③ | ★ **Catch2 的 XML reporter 在测试用例开始前就接管了 `std::cerr`** ⇒ ★ 它把自己的 buffer 装进 `std::cerr` | ★ 测试的 `CerrCapture` 接管的是**已被换过的 rdbuf** |
| ④ | ⇒ ★ 生产的 warn 写进了 **XML reporter 的 buffer**（★ 所以 warn 出现在 XML 里）⇒ ★ `cap.str()` 为空 | ★ 断言失败 |

★ ★ **本质**：★ 这三个用例要测的是「**declared-reserved 的 warn 走 `std::cerr`**」（★ ADR-087 §D1/D2
★ 的 design D2）⇒ ★ 但它**用「接管全局流」的手段去测** ⇒ ★ 于是**同时测到了 reporter 的实现**
⇒ ★ ★ **脆弱测试的根因 = 观测手段与被测行为耦合**。

### 3. ★ 为什么不能靠「改测试的捕获方式」绕过

★ 不能简单改成捕获 `std::cout` ⇒ ★ 那就**不再测 `cerr`** ⇒ ★ 丢掉了 ADR-087 D2 的
★ 「declared-reserved warn 走 **cerr**」这条语义 ⇒ ★ **测不到要测的东西**。

★ ⇒ ★ **正解 = 依赖注入**：★ 让 warn 的**输出目标可注入** ⇒ ★ 测试注入 `std::ostringstream`
⇒ ★ **完全绕开全局流** ⇒ ★ 与 reporter **彻底无关** ⇒ ★ 脆弱性**根除**（不是绕开）。

## What Changes

- `ip/cpu/cpu_factory.h`：
  - `detail::validate_isa_xlen(config, kernel_xlen)` ⇒ 增加**可选**参数
    `std::ostream* warn_sink = nullptr`（★ 默认 `nullptr` ⇒ 仍写 `std::cerr` ⇒ **生产行为逐字不变**）
  - `CpuFactory<T>::build_cpu(config, mem = nullptr)` ⇒ 增加**可选**参数
    `std::ostream* warn_sink = nullptr`，并向下传给 `validate_isa_xlen`
    （★ **默认参数 ⇒ 向后兼容** ⇒ ★ 现有 **124** 处 `build_cpu` 调用点**一处不改**）
- `tests/cpu/test_cpu_factory_isa_xlen.cpp`：
  - 3 个 case（★ `:60` / `:68` / `:96`）改用 `std::ostringstream` + 注入 ⇒ ★ 断言 `oss.str()`
  - `CerrCapture` 类**不再被使用** ⇒ 删除（★ 它就是脆弱性的载体）

## Out of Scope

- ★ **不改** Catch2 / reporter 的行为
- ★ **不改** `run_catch2_with_baseline.sh`（★ 用 XML reporter 是**正确**的；★ 脆弱的是**测试**）
- ★ **不改** warn 的**内容**或**触发条件**（★ 只改**输出目标的可注入性**）
- ★ **不改** CH_MEM 侧（★ `detail::validate_isa_xlen` **只在 TLM 版** `cpu_factory.h`；★ CH_MEM 版不调它）

## Acceptance Criteria

- [ ] **AC-1** ★ **XML reporter 下全绿**：★ `./build/bin/chipforge_tests "[isa-xlen]" -r xml -o out.xml`
      ⇒ **exit 0** ⇒ ★ XML 里 **0 个** `success="false"`
- [ ] **AC-2** ★ **默认 reporter 下仍全绿**：★ `[isa-xlen]` ⇒ `All tests passed`（★ 20 assertions / 9 test cases）
- [ ] **AC-3** ★ **生产行为逐字不变**：★ 默认路径（`warn_sink == nullptr`）的 warn 仍写 `std::cerr`
      ⇒ ★ 判据 = **不传 sink** 时 stdout/stderr 内容与改动前一致（★ 现有 warn 文本一字不改）
- [ ] **AC-4** ★ **向后兼容**：★ **124** 处 `build_cpu` 调用点**一处不改**仍编译通过
      （★ 判据 = 全仓编译通过 + `grep` 计数不变）
- [ ] **AC-5** ★ **脆弱性根除**：★ 测试**不再**触碰 `std::cerr` / `std::cout` 的 rdbuf
      ⇒ ★ `grep "rdbuf" tests/cpu/test_cpu_factory_isa_xlen.cpp` ⇒ **0 命中**
- [ ] **AC-6** ★ **全量零退化**：★ `bash tools/run_chipforge_tests.sh` ⇒ baseline-aware **PASS**
      （★ 与既有基线逐位一致）

## Impact

- **修改**：`ip/cpu/cpu_factory.h`（★ 2 个签名加**可选**参数）· `tests/cpu/test_cpu_factory_isa_xlen.cpp`（★ 3 个 case + 删 1 个辅助类）
- **不动**：warn 的内容与触发条件 · Catch2 · `run_catch2_with_baseline.sh` · CH_MEM 侧
- **风险**：★ 低（★ 默认参数 ⇒ 既有调用零影响；★ 但 `cpu_factory.h` 是**公共头** ⇒ 需全量编译验证）
