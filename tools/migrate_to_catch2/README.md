# Catch2 迁移工具集

> 用于把 ChipForge 的「纯 main()+assert」或 GTest 测试自动转换为 Catch2 TEST_CASE 模式。
> 一次性脚本，迁移完成后可保留作为参考或删除。

## 使用方法

### 单文件
```bash
python3 tools/migrate_to_catch2/migrate.py tests/cache/test_replacement_policy.cpp
```

### 全部文件
```bash
python3 tools/migrate_to_catch2/migrate.py --all
```

## 转换步骤（按序）

1. **transform_assert.py** — `assert(x)` → `REQUIRE(x)` + 替换 `#include <cassert>` 为 `#include "catch_amalgamated.hpp"`
2. **transform_check_macro.py** — 移除手写 `#define CHECK(cond) ...` 宏（catch_amalgamated 已提供）
3. **transform_main.py** — `static void test_x() { ... }` + `int main() { test_x(); test_y(); }` → `TEST_CASE("x", "[family]") { ... }`（删除 main 整体）

## 转换后处理

- 移除文件中的 `printf("  [PASS] xxx\n")` 行（catch_amalgamated 会自动报告）
- 移除文件顶部的 `// 设计说明: 纯 main() + assert (项目约定, 不引入 Catch2)` 注释
- 移除 `#include <cstdio>`（如果仅用于 printf PASS）

## 不处理

- 测试函数体内 `assert(x && "msg")` 中的消息文本（仅做 `REQUIRE(x)`，不构造 INFO）
- `printf("=== Test name ===\n")` 类的大标题（保留或删除均可）
- `int argc, char** argv` 风格的 main（不期望出现）

> ⚠️ 此工具是 2026-06-30 Catch2 迁移的一次性脚本。
> 保留至 2026-12-30 供未来 catch2 升级或新测试迁移参考。
> 到期后请评估删除 (chore: remove migrate_to_catch2 toolchain)。