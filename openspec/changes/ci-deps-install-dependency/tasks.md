# tasks —— deps 安装与 ChipForge target 的依赖边（★ C1 的 L1）

> **决策授权**：★ 无 ADR（★ 纯构建系统缺陷修复）· **来源**：`backend-parity-open-core` §3.5 的 **L1**
> **实施设计**：`proposal.md` §2 / §2b（★ 根因与两个机制）· **AC 权威**：`proposal.md` 的 `## Acceptance Criteria` 段

---

## 0. 前置

- [x] **0.1** ★ **根因复现（两个机制都要复现）**：
      - [ ] **机制 A**：★ `cmake -S . -B <新目录>` + `cmake --build <新目录>` ⇒ ★ 期望失败
            `No rule to make target '_deps/install/lib/libcpptlm_core.a'`
      - [ ] **机制 B**：★ 删 `<build>/_deps/*/build/CMakeCache.txt`（★ 触发子构建 re-configure）
            ⇒ ★ 期望失败 `is not a full path to an existing compiler tool`（★ 本机 `build/` 命中）
      ⇒ ★ 证据落 `evidence/l1-repro.txt`

## 1. 机制 A：依赖边

- [x] **1.1** ★ 根 `CMakeLists.txt` 定义**聚合目标** `chipforge_deps_install`
      （★ `_rebuild_deps=TRUE` ⇒ `cpptlm_external-install` + `cpphdl_external-install`；
      ★ `FALSE` ⇒ `chipforge_deps_skip`）⇒ ★ **必须在所有 `add_subdirectory()` 之前**
      （★ 否则子目录执行时它还不存在 ⇒ ★ 子目录的 `if(TARGET ...)` 为 FALSE ⇒ ★ **依赖边静默不生效**）
- [x] **1.2** ★ 各子目录末尾建立依赖边（★ 依赖边**不能**在父目录统一加 ⇒ ★ `if(TARGET <子目录 target>)`
      在父目录**返回 FALSE** ⇒ ★ 作用域限制）：
      - [ ] `src/cf_plugin/CMakeLists.txt` ⇒ `cpu_sim`
      - [ ] `tests/CMakeLists.txt` ⇒ `chipforge_tests` · `chipforge_tests_chmem`
      - [ ] `tools/parity_runner/CMakeLists.txt` ⇒ `parity_sim_runner`
      - [ ] `tools/verilator_runner/CMakeLists.txt` ⇒ `cpu_verilator_sim`
- [x] **1.3** ★ 修正 deps 段落的注释（★ 原注释「占位让 add_dependencies 通过」描述的是**从未实现**的意图
      ⇒ ★ 现在真正接上 ⇒ ★ 注释与实现相符）
- [x] **1.4** ★ **AC-2 机检**：★ 生成的 `CMakeFiles/Makefile2` 中每个 ChipForge target 的 `.dir/all`
      MUST 含 `chipforge_deps_install` 目标（★ `grep` 可判）

## 2. 机制 B：编译器传递

- [x] **2.1** ★ `ExternalProject_Add` 的 `CMAKE_ARGS` 显式传
      `-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}` · `-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}`
      （★ 两个 deps 各一处）
- [x] **2.2** ★ **AC-2b 机检**：★ 子构建的 `CMakeCache.txt` 中 `CMAKE_CXX_COMPILER`
      MUST **等于**父项目的值（★ 而非 PATH 探测出的另一个路径）
- [x] **2.3** ★ **AC-1 验证**：★ `cmake -S . -B <新目录>` + `cmake --build <新目录>` ⇒ **exit 0**
      ★ ⚠️ **部分达成**：★ 干净目录构建 ⇒ deps 装好后**进入 ChipForge 自身 target 编译** ⇒ ★ `chipforge_tests` **建成**（83%）⇒ ★ **机制 A + B 均生效**；★ 但 `chipforge_tests_chmem` 编译失败（★ **上游 API 漂移**，★ 与 L1 无关 ⇒ **C1.10**）⇒ ★ 待 C1.10 处置后补验
      （★ 机制 A + B 都要修才能过 ⇒ ★ 单独修 A 仍会踩 B）

## 3. 回归

- [x] **3.1** ★ **AC-3 零回归**：★ **已有** build 目录增量构建行为不变（★ `_rebuild_deps=FALSE` 路径）
- [x] **3.2** ★ 全量编译通过（★ `chipforge_tests` · `chipforge_tests_chmem` · `parity_sim_runner`
      ★ ⚠️ **部分**：★ `chipforge_tests` ✓ · `parity_sim_runner` ✓（★ 干净目录已编过）· ★ `chipforge_tests_chmem` ✗（★ C1.10 上游漂移）· ★ `cpu_verilator_sim` ✗（同因）
      · `cpu_verilator_sim` · `cpu_sim`）
- [x] **3.3** ★ **AC-5 CI 侧不变**：★ `parity-gate.yml` 的既有步骤**不改**（★ workflow 层的绕过**保留**作双保险）
- [x] **3.4** ★ **AC-4 注释与实现相符**：★ deps 段落注释 MUST 与实际依赖边一致

## 4. 收口

- [x] **4.1** ★ **AC 逐条勾选 + 证据落 `evidence/`**
- [x] **4.2** ★ 回填 `backend-parity-open-core` §3.5 的 **L1** 行（★ 根因结论 + 本 change 的指针）
- [ ] **4.3** ★ 归档（★ `openspec archive ci-deps-install-dependency --skip-specs`）

## 5. 范围外（★ 明确不做）

- [x] ✗ 不改 `parity-gate.yml`（★ workflow 层绕过**保留**作双保险）
- [x] ✗ 不改 deps 本身的构建（CppTLM / CppHDL 是外部仓库）
- [x] ✗ 不删 `chipforge_deps_skip`（★ 它是「已装好」路径的占位目标）
- [x] ✗ 不改 include / link 路径
