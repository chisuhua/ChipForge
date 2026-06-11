# 开发环境配置

## 1. 前置条件

### 1.1 代码仓库
ChipForge 依赖以下外部仓库，需要克隆到同一父目录下：

```bash
cd /path/to/your/workspace
git clone <CppTLM-repo-url> CppTLM
git clone <CppHDL-repo-url> CppHDL
git clone <ChipForge-repo-url> ChipForge
```

最终目录结构：
```
workspace/
├── CppTLM/       # TLM 建模框架
├── CppHDL/       # RTL 建模框架
└── ChipForge/    # 本项目
    ├── CppTLM -> ../CppTLM  (符号链接)
    └── CppHDL -> ../CppHDL  (符号链接)
```

### 1.2 工具链要求
| 工具 | 最低版本 | 说明 |
|------|---------|------|
| GCC / Clang | 12+ / 14+ | 需支持 C++20 |
| CMake | 3.20+ | 构建系统 |
| Ninja | 1.10+ | 推荐构建加速 |
| Python | 3.8+ | 脚本和测试工具 |

## 2. 环境配置

### 2.1 创建符号链接
```bash
cd ChipForge
ln -s ../CppTLM CppTLM
ln -s ../CppHDL CppHDL
```

> **注意**：符号链接使用相对路径，确保三个仓库保持同级目录关系。

### 2.2 验证链接
```bash
ls -la CppTLM CppHDL
# 应显示指向 ../CppTLM 和 ../CppHDL 的符号链接
```

### 2.3 CMake 配置（规划）
未来构建时，CMake 将自动通过符号链接找到 CppTLM/CppHDL：

```cmake
# CMakeLists.txt（规划）
set(CPPTLM_DIR ${CMAKE_SOURCE_DIR}/CppTLM)
set(CPPHDL_DIR ${CMAKE_SOURCE_DIR}/CppHDL)

add_subdirectory(${CPPTLM_DIR} cpptlm_build)
add_subdirectory(${CPPHDL_DIR} cpphdl_build)
```

## 3. 日常开发流程

### 3.1 同步更新
CppTLM/CppHDL 的修改会通过符号链接即时反映在 ChipForge 中：

```bash
# 在 CppTLM 中做修改
cd ../CppTLM
# ... 修改代码 ...

# 回到 ChipForge 立即可用
cd ../ChipForge
# 构建时自动使用最新的 CppTLM
```

### 3.2 独立提交
三个仓库独立管理 git 历史：
```bash
# 提交 CppTLM 修改
cd CppTLM  # 通过 symlink 进入
git add -A && git commit -m "feat: ..."

# 提交 ChipForge 修改
cd /path/to/ChipForge
git add -A && git commit -m "feat: ..."
```

## 4. 迁移到 Git Submodule（未来）

当项目需要 CI/CD 或团队协作时，可以将 symlink 方案迁移为 git submodule：

```bash
# 1. 移除符号链接
rm CppTLM CppHDL

# 2. 添加为 submodule
git submodule add <CppTLM-repo-url> CppTLM
git submodule add <CppHDL-repo-url> CppHDL

# 3. 更新 .gitignore（移除 /CppTLM 和 /CppHDL 行）

# 4. 提交
git add .gitmodules CppTLM CppHDL .gitignore
git commit -m "chore: migrate from symlinks to submodules"
```

## 4.5 CppHDL 集成边界（Phase 0–1.4 vs Phase 5）

ChipForge 集成了 CppHDL 作为底层 HDL 框架，但**只消费其库与头文件**，不需要把 CppHDL 自身的回归测试（109 Catch2 + 6 perf）和示例程序（16 个 `spinalhdl-ported/*` 等）作为 ChipForge ctest 列表的一部分。

### 默认行为（Phase 0–1.4）

根 `CMakeLists.txt` 通过两个 cache 变量在 `add_subdirectory(CppHDL)` **之前**强制覆盖默认值：

```cmake
set(CPPHDL_BUILD_TESTS    OFF CACHE BOOL "ChipForge: build CppHDL tests subtree"    FORCE)
set(CPPHDL_BUILD_EXAMPLES OFF CACHE BOOL "ChipForge: build CppHDL examples subtree" FORCE)
```

CppHDL 根 `CMakeLists.txt` 在顶层增加了一对 `option(CPPHDL_BUILD_TESTS ...)` / `option(CPPHDL_BUILD_EXAMPLES ...)` 门控（默认 `ON`，保持上游独立 build 时的行为），当父项目传 `=OFF` 时跳过 `add_subdirectory(samples)` / `add_subdirectory(examples/...)` / `add_subdirectory(tests)`。

验证：

```bash
$ ctest -N | grep "Total Tests"
Total Tests: 15     # 全部为 ChipForge 自身测试;无 test_forwarding/test_hazard/perf_*
```

### Phase 5 RTL 协同启用方式

当 Phase 5（RTL 协同仿真）真正需要跑 HDL 端到端测试时，**不要**直接在默认 build 中打开开关污染 ctest 列表。推荐另起一个独立 build 目录：

```bash
# 默认 ChipForge build (Phase 0-1.4 范围,~15 个测试)
cmake -S . -B build

# CppHDL 全量测试 (Phase 5 / HDL 端到端)
cmake -S . -B build-cpphdl -DCPPHDL_BUILD_TESTS=ON -DCPPHDL_BUILD_EXAMPLES=ON -DBUILD_VERILATOR=OFF
cmake --build build-cpphdl -j$(nproc)
ctest --test-dir build-cpphdl -L base --output-on-failure
```

### 注意事项

- 这两个开关是 **add_subdirectory 范围门控**，不影响 `cpphdl` 库本身的构建。
- 切回 `ON` 时确认系统已装 `flex`（BUILD_VERILATOR 路径需要）或显式 `-DBUILD_VERILATOR=OFF`。
- 修改 `CppHDL/CMakeLists.txt` 是 vendored 副本修改，请避免与上游 CppHDL 同步时产生冲突注释。

## 5. 故障排查

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| `CppTLM: No such file` | 符号链接断裂 | 检查 ../CppTLM 是否存在 |
| CMake 找不到头文件 | 链接未创建 | 执行 `ln -s ../CppTLM CppTLM` |
| git status 显示 CppTLM | .gitignore 未配置 | 确认 .gitignore 包含 `/CppTLM` |
| 跨设备链接失败 | 不同文件系统 | 考虑使用 bind mount 或改用 submodule |

## 6. 相关文档
- [项目架构总览](architecture/overview.md)
- [核心技术选型](architecture/tech-selection.md)
