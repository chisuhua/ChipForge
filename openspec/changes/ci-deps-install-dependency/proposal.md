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

- `CMakeLists.txt` **末尾**（★ 所有 ChipForge target 定义之后）建立依赖边：
  - `_rebuild_deps=TRUE` ⇒ 依赖 `cpptlm_external-install` + `cpphdl_external-install`
  - `_rebuild_deps=FALSE` ⇒ 依赖 `chipforge_deps_skip`（★ 让那句注释**终于名副其实**）
- 覆盖**全部**依赖 deps 的 target：`cpu_sim` · `chipforge_tests` · `chipforge_tests_chmem`
  · `parity_sim_runner` · `cpu_verilator_sim`
- ★ **不改** include / link 路径、★ **不改** deps 的构建方式 ⇒ 改动面最小

## Out of Scope

- ★ **不改** `parity-gate.yml` 的 workflow（★ 已在 A4 里绕过；★ 本 change 是**根因**修复，workflow 绕过**保留**作为双保险）
- ★ 不改 deps 本身的构建（CppTLM / CppHDL 是外部仓库）
- ★ **不删** `chipforge_deps_skip`（★ 它是「已装好」路径的占位目标）

## Acceptance Criteria

- [ ] **AC-1** ★ **干净目录首次构建成功**：★ `cmake -S . -B <新目录>` + `cmake --build <新目录>`
      ⇒ **exit 0** ⇒ ★ 依赖头文件与库在 ChipForge 编译期**已就位**
- [ ] **AC-2** ★ **依赖边可机检**：★ 生成的 `CMakeFiles/Makefile2` 中
      ★ 每个 ChipForge target 的 `.dir/all` MUST 含 deps-install 目标（★ `grep` 可判）
- [ ] **AC-3** ★ **零回归**：★ 已存在的 build 目录增量构建**行为不变**（★ `_rebuild_deps=FALSE` 路径）
- [ ] **AC-4** ★ **注释与实现相符**：★ `CMakeLists.txt` 的 deps 段落注释 MUST 与实际依赖边一致
      （★ 消除「占位让 add_dependencies 通过」这类**描述未实现意图**的注释）
- [ ] **AC-5** ★ **CI 侧不变**：★ `parity-gate.yml` 的既有步骤**不改**（★ 双保险保留）

## Impact

- **修改**：`CMakeLists.txt`（★ 末尾新增一段依赖边 + 修正 deps 段落注释）
- **不动**：deps 的构建方式 · include/link 路径 · 任何 `*.h` / `*.cpp` · CI workflow
- **风险**：★ 低（★ 只增加 make 依赖边；★ 已有 build 目录的增量构建路径不变）
