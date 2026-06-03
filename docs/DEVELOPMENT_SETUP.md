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
