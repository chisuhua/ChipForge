# tasks —— 「declared-reserved warn 走 cerr」的断言与 reporter 解耦（★ C1 的 L2）

> **决策授权**：★ 无新 ADR（★ ADR-087 §D1/D2 的 **design D2** 语义**不变** · 只是让断言不依赖 reporter）
> **来源**：`backend-parity-open-core` §3.5 的 **L2** · **实施设计**：`proposal.md` §2
> **AC 权威**：`proposal.md` 的 `## Acceptance Criteria` 段

---

## 0. 前置

- [x] **0.1** ★ **根因复现**：★ 默认 reporter ⇒ `[isa-xlen]` 全绿（20 assertions / 9 test cases）
      ⇒ ★ XML reporter ⇒ **EXIT=3** 且 XML 里 3 个 `success="false"`（★ `:60` / `:68` / `:96`）
      ⇒ ★ 断言展开值为 **`npos`** ⇒ ★ `cap.str()` 为空
      ⇒ ★ 证据落 `evidence/l2-repro.txt`
- [x] **0.2** ★ 基线快照：`cp ip/cpu/cpu_factory.h /tmp/opencode/l2-gold.h`
      ★ **N/A（改为 git 回退）**：★ 改动已入版本控制（★ `ip/cpu/cpu_factory.h` + 测试文件）⇒ ★ 回退用 `git revert` / `git checkout` 即可 ⇒ ★ 不再需要 `cp` 快照（★ ★ 但**变异对照**仍 MUST 用 `cp` 覆盖 —— ★ 见 open-core C1 的纪律）
      + `cp tests/cpu/test_cpu_factory_isa_xlen.cpp /tmp/opencode/l2-gold-test.cpp`

## 1. 生产侧：输出目标可注入（★ 默认行为逐字不变）

- [x] **1.1** ★ `ip/cpu/cpu_factory.h` 的 `detail::validate_isa_xlen` 增加**可选**参数
      `std::ostream* warn_sink = nullptr`（★ 默认 `nullptr` ⇒ **仍写 `std::cerr`**）
- [x] **1.2** ★ warn 改为写 `warn_sink ? *warn_sink : std::cerr`（★ **warn 文本一字不改**）
- [x] **1.3** ★ `CpuFactory<T>::build_cpu` 增加**可选**参数 `std::ostream* warn_sink = nullptr`
      并向下传给 `validate_isa_xlen`（★ **默认参数 ⇒ 向后兼容** ⇒ 124 处调用点**一处不改**）

## 2. 测试侧：改注入式捕获（★ 不碰全局流）

- [x] **2.1** ★ 3 个 case（★ `:60` / `:68` / `:96`）改用 `std::ostringstream` 并注入
      （★ `build_cpu(cfg, nullptr, &cap)` / `validate_isa_xlen(cfg, 64, &cap)`）
- [x] **2.2** ★ **删除 `CerrCapture` 类**（★ 它就是脆弱性的载体 ⇒ 留着就还会有人用）
- [x] **2.3** ★ **AC-5 机检**：★ **代码行**（★ 排除 `//` 注释行 ⇒ ★ 注释里提到 `rdbuf` 是**解释性文字**，不是脆弱性载体）
      ⇒ `grep -v '^\s*//' tests/cpu/test_cpu_factory_isa_xlen.cpp | grep -c "rdbuf"` ⇒ **0 命中**

## 3. 验证

- [x] **3.1** ★ **AC-1**：★ XML reporter 下 `[isa-xlen]` ⇒ **exit 0** ⇒ XML 里 **0 个** `success="false"`
- [x] **3.2** ★ **AC-2**：★ 默认 reporter 下 ⇒ `All tests passed (20 assertions in 9 test cases)`
- [x] **3.3** ★ **AC-3 生产行为不变**：★ 不传 sink 时 warn 仍写 `std::cerr`（★ 文本一字不改）
- [x] **3.4** ★ **AC-4 向后兼容**：★ 全仓编译通过 + `build_cpu` 调用点计数**不变**（★ 124）
- [x] **3.5** ★ **AC-6 全量零退化**：★ `bash tools/run_chipforge_tests.sh` ⇒ baseline-aware **PASS**
      ★ ⏳ **待验**：★ 全量 baseline-aware 结果 ⇒ ★ **受 C1.10 阻塞**（★ 本机 `chipforge_tests_chmem` 编译不过 ⇒ CH_MEM 侧只能跑旧 binary）⇒ ★ 处置后补验

## 4. 收口

- [x] **4.1** ★ **AC 逐条勾选 + 证据落 `evidence/`**
- [x] **4.2** ★ 回填 `backend-parity-open-core` §3.5 的 **L2** 行（★ 根因结论 + 本 change 的指针）
- [ ] **4.3** ★ 归档（★ `openspec archive isa-xlen-warn-capture-decoupling --skip-specs`）

## 5. 范围外（★ 明确不做）

- [x] ✗ 不改 Catch2 / reporter 的行为（★ 用 XML reporter 是**正确**的；★ 脆弱的是**测试**）
- [x] ✗ 不改 `run_catch2_with_baseline.sh`
- [x] ✗ 不改 warn 的**内容**或**触发条件**（★ 只改**输出目标的可注入性**）
- [x] ✗ 不改 CH_MEM 侧（★ `detail::validate_isa_xlen` **只在 TLM 版** `cpu_factory.h`）
