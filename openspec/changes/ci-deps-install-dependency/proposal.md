---
initiative: wave7-architecture-governance
priority: P1
version_target: v0.11.0
status: proposed
adr: []
depends_on: []
---

# ci-deps-install-dependency —— deps 安装与 ChipForge target 的依赖边（★ C1 的 L1）

> **来源**：`openspec/changes/backend-parity-open-core/tasks.md` §3.5 的 **L1**（★ 该行标注「⇒ 独立 change」）
> ★ **本 change 的根因已铁证锁定**（★ 2026-11-08 实测）：★ 见 §1 的复现日志。

## Why

★ **新建 build 目录的首次构建必然失败** ⇒ ★ 而**已存在的 build 目录永远不暴露它** ⇒ ★ 这是一条
**只在「干净环境」出现的缺陷**（★ 新克隆的仓库 / CI runner / 新同事的第一天）。

### 1. 复现（2026-11-08 实测 · 铁证）

```bash
cmake -S . -B /tmp/opencode/cf-cfg -DCMAKE_BUILD_TYPE=Debug   # 新目录
cmake --build /tmp/opencode/cf-cfg -j$(nproc)                  # ⇒ 失败
```

日志（关键三行）：

```
[ 9%] Built target cpptlm_external-build
/workspace/.../tools/verilator_runner/cpu_verilator_sim.cpp:29:10: fatal error: ch.hpp: No such file or directory
gmake[2]: *** No rule to make target '_deps/install/lib/libcpptlm_core.a', needed by 'bin/cpu_verilator_sim'.  Stop.
gmake: *** [Makefile:101: all] Error 2
```

### 2. 根因（★ 读 CMake 生成的 Makefile 确认，非推测）

```make
all: CMakeFiles/cpptlm_external.dir/all
CMakeFiles/cpptlm_external.dir/all: CMakeFiles/cpptlm_external-build.dir/all
CMakeFiles/cpptlm_external.dir/all: CMakeFiles/cpptlm_external-install.dir/all
```

⇒ ★ `install` **确实在 `all` 链里** ⇒ ★ 所以不是「install 没被调度」，而是 **★ 并行 + 无依赖边 ★**：

| # | 事实 | 后果 |
|---|---|---|
| ① | `CMakeLists.txt` 的 deps 段落**从未给 ChipForge 的 target 建立任何依赖边** | ★ `make` 并行时，**ChipForge 的 target 与 deps-install 无序** |
| ② | ChipForge 的 include / link 路径指向 **`_deps/install`**（★ 已安装态）⇒ ★ 编译期就需要 deps **已装好** | ★ ChipForge 的 target 先跑 ⇒ `ch.hpp` / `libcpptlm_core.a` **尚不存在** ⇒ 编译失败 |
| ③ | make 在第一个失败处**立即停止** | ★ `cpptlm_external-install` **没机会跑** ⇒ ★ deps 停在「已 build 未 install」 |

★ ★ **附带发现（声明与实现不符）**：★ `CMakeLists.txt:201` 的注释写着「已 install，创建空目标，
★ **占位让 add_dependencies 通过**」⇒ ★ 但**全仓 grep 无任何 `add_dependencies(... chipforge_deps_skip ...)`**
⇒ ★ 那句注释描述的是一个**从未实现**的意图 ⇒ ★ **注释把「缺陷」伪装成了「设计」**。

### 2b. ★★ **机制 B：子构建不继承父项目的编译器指定**（★ 2026-11-08 实测复现 · 与机制 A 独立）

★ **实测触发路径**：★ `build/CMakeCache.txt` 里
```
CMAKE_CXX_COMPILER:STRING=/workspace/main/opt/bin/g++-14
```
（★ **STRING 类型** ⇒ 用户**显式指定**过编译器，★ **不在 PATH**）
⇒ ★ ExternalProject 子构建（`cpptlm_external`）**重新 configure** 时 ⇒ ★ 用**自己的 cache**
⇒ ★ **不继承**父项目的 `CMAKE_CXX_COMPILER` ⇒ ★ **重新探测** ⇒ ★ 探测到 PATH 里的
**`/usr/bin/g++-14`** ⇒ ★ ★ **该路径不存在** ⇒ ★ ⇒ 子构建 configure 失败：

```
-- The CXX compiler identification is unknown
CMake Error at CMakeLists.txt:21 (project):
  The CMAKE_CXX_COMPILER:
    /usr/bin/g++-14
  is not a full path to an existing compiler tool.
```

★ ★ **两个机制的区分（★ 都很重要，缺一不可）**：
| # | 机制 | 症状 | 修法 |
|---|---|---|---|
| ★ **A** | deps install 与 ChipForge target **无依赖边** ⇒ 并行竞态 | `No rule to make target '_deps/install/lib/libcpptlm_core.a'` | ★ 建立 make 依赖边 |
| ★ **B** | 子构建**不继承**父项目的编译器指定 ⇒ 重新探测 ⇒ 可能探到**不存在的**工具 | `is not a full path to an existing compiler tool` | ★ 把 `CMAKE_C_COMPILER` / `CMAKE_CXX_COMPILER` **显式传给子构建** |

★ ★ **机制 B 的精确触发条件**：★ 父项目 cache 里编译器是 **`STRING` 类型**（显式指定）
★ **且** PATH 里的同名工具**不存在** ⇒ ★ **干净目录一般不触发**（父项目也是探测来的 ⇒ 两边一致）
★ ⇒ ★ **但「父项目曾被显式配置过」的目录**（★ 本机 `build/` 就是）**在 re-configure 时必踩**。

### 3. ★ 为什么这条以前没被发现

| 场景 | 是否暴露 |
|---|---|
| 本地已有 `build/`（deps 早装好 ⇒ `_rebuild_deps=FALSE`） | ❌ 不暴露 |
| 本地**新建** build 目录 | ✅ **必然失败** |
| CI（★ 全新 runner ⇒ 全新 build 目录） | ✅ **必然失败** |

★ ★ **旁证**：★ 2026-11-08 的 **CI 首次 run**（`42472260469`）失败于
★ `No rule to make target '_deps/install/lib/libcpptlm_core.a'` ⇒ ★ **同一个根因**。
★ 当时在 **workflow 层绕过**了（★ 先 `cmake --build build --target cpptlm_external cpphdl_external`
★ 再 build runner）⇒ ★ **根因仍在**（★ 本地新建 build 目录依然踩）⇒ ★ 本 change 补上根因修复。

## What Changes

- ★ **机制 A（依赖边）**：★ 根 `CMakeLists.txt` 定义**聚合目标** `chipforge_deps_install`
  （★ **必须在所有 `add_subdirectory()` 之前**定义 ⇒ ★ 否则子目录执行时它还不存在
  ⇒ ★ 子目录的 `if(TARGET ...)` 为 FALSE ⇒ ★ 依赖边**静默不生效**）⇒ ★ 各子目录末尾：
  - `src/cf_plugin/CMakeLists.txt` ⇒ `cpu_sim`
  - `tests/CMakeLists.txt` ⇒ `chipforge_tests` · `chipforge_tests_chmem`
  - `tools/parity_runner/CMakeLists.txt` ⇒ `parity_sim_runner`
  - `tools/verilator_runner/CMakeLists.txt` ⇒ `cpu_verilator_sim`
- ★ **机制 B（编译器传递）**：★ `ExternalProject_Add` 的 `CMAKE_ARGS` 显式传
  `-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}` · `-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}`
  ⇒ ★ 子构建用**同一个**编译器 ⇒ 不依赖 PATH 探测
- ★ **不改** include / link 路径、★ **不改** deps 的构建方式 ⇒ 改动面最小

## Out of Scope

- ★ **不改** `parity-gate.yml` 的 workflow（★ 已在 A4 里绕过；★ 本 change 是**根因**修复，workflow 绕过**保留**作为双保险）
- ★ 不改 deps 本身的构建（CppTLM / CppHDL 是外部仓库）
- ★ **不删** `chipforge_deps_skip`（★ 它是「已装好」路径的占位目标）

## Acceptance Criteria

- [ ] **AC-1** ★ **干净目录首次构建成功**：★ `cmake -S . -B <新目录>` + `cmake --build <新目录>`
      ★ ⚠️ **部分达成（2026-11-08）**：★ 干净目录 `/tmp/opencode/l1-fix` 构建 ⇒ deps 装好后**进入 ChipForge 自身 target 编译** ⇒ ★ `chipforge_tests` **建成**（83%）⇒ ★ **机制 A + B 均生效**；★ 但 `chipforge_tests_chmem` **编译失败**（★ **上游 CppHDL API 漂移**：`ch_state_machine::is_built()` 已移除 ⇒ ★ **与 L1 无关**，★ 已登记为 **C1.10**）⇒ ★ 故本 AC **不标全绿**，★ 待 C1.10 处置后补验
      ⇒ **exit 0** ⇒ ★ 依赖头文件与库在 ChipForge 编译期**已就位**
- [x] **AC-2** ★ **依赖边可机检**：★ 生成的 `CMakeFiles/Makefile2` 中
      ★ ✅ **2026-11-08**：★ 生成的 `CMakeFiles/Makefile2` 中每个 ChipForge target 的 `.dir/all` MUST 含 `chipforge_deps_install` ⇒ ★ **5/5 命中**（`cpu_sim` · `chipforge_tests` · `chipforge_tests_chmem` · `parity_sim_runner` · `cpu_verilator_sim`）
      ★ 每个 ChipForge target 的 `.dir/all` MUST 含 `chipforge_deps_install` 目标（★ `grep` 可判）
- [x] **AC-2b** ★ **机制 B 可机检**：★ 子构建的 `CMakeCache.txt` 中
      ★ ✅ **2026-11-08**：★ 子构建 `CMakeCache.txt` 的 `CMAKE_CXX_COMPILER` **等于**父项目 ⇒ ★ cpptlm / cpphdl 均为 `/workspace/main/opt/bin/g++-14`（★ 修复前会探到**不存在**的 `/usr/bin/g++-14`）
      ★ `CMAKE_CXX_COMPILER` MUST **等于**父项目的值（★ 而非 PATH 探测出的另一个路径）
- [x] **AC-3** ★ **零回归**：★ 已存在的 build 目录增量构建**行为不变**（★ `_rebuild_deps=FALSE` 路径）
      ★ ✅ **2026-11-08**：★ 已有 build 目录（`_rebuild_deps=FALSE`）增量构建行为**不变**（★ 依赖边指向 `chipforge_deps_skip` ⇒ 无需重装 deps）
- [x] **AC-4** ★ **注释与实现相符**：★ `CMakeLists.txt` 的 deps 段落注释 MUST 与实际依赖边一致
      ★ ✅ **2026-11-08**：★ deps 段落的注释已改为与实现相符（★ 原「占位让 add_dependencies 通过」描述的是**从未实现**的意图 ⇒ ★ 现由文件中的「deps 聚合目标」段真正接上）
      （★ 消除「占位让 add_dependencies 通过」这类**描述未实现意图**的注释）
- [x] **AC-5** ★ **CI 侧不变**：★ `parity-gate.yml` 的既有步骤**不改**（★ 双保险保留）
      ★ ✅ **2026-11-08**：★ `parity-gate.yml` **一行未改**（★ workflow 层的绕过**保留**作双保险）

## Impact

- **修改**：`CMakeLists.txt`（★ 末尾新增一段依赖边 + 修正 deps 段落注释）
- **不动**：deps 的构建方式 · include/link 路径 · 任何 `*.h` / `*.cpp` · CI workflow
- **风险**：★ 低（★ 只增加 make 依赖边；★ 已有 build 目录的增量构建路径不变）
