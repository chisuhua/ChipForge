# Vendored Catch2 v3.7.0

> 单一可信来源: `/workspace/project/CppTLM/test/catch_amalgamated.{hpp,cpp}`
> 与 CppHDL 的 `tests/catch_amalgamated.{hpp,cpp}` MD5 完全一致（兄弟项目同步升级）。

| 文件 | 行数 | MD5 | 大小 |
|------|------|-----|------|
| `catch_amalgamated.hpp` | 14,064 | 33a2094513de12552236286bbac80a0e | 522 KB |
| `catch_amalgamated.cpp` | 11,857 | 579ca0ed8a834dd271b978074c85accc | 423 KB |

## 升级路径

1. 在 Catch2 upstream 仓库中运行:
   ```bash
   python3 tools/scripts/generateAmalgamatedFiles.py
   ```
2. 复制新文件到 CppTLM 仓库 `test/`
3. 验证 CppTLM CI 全绿
4. 用 `cp /workspace/project/CppTLM/test/catch_amalgamated.{hpp,cpp} tests/catch2/` 同步
5. 更新本 README 的 MD5 与行数
6. 提交 PR 标题: `chore(tests): upgrade Catch2 to v3.x.y`

## 集成模式（与 CppTLM 一致）

- **不** 定义 `CATCH_CONFIG_MAIN`（使用 `catch_amalgamated.cpp` 的默认 main）
- **统一** include 风格: `#include "catch_amalgamated.hpp"`
- **单二进制** + `file(GLOB)` 自动发现