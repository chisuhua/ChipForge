# ChipForge Catch2 测试框架引入实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) 或 superpowers:executing-plans 实施此计划。步骤使用 checkbox (`- [ ]`) 语法追踪。

**Goal:** 将 ChipForge 的 47 个测试（42 个纯 main()+assert + 5 个 GTest）迁移到 Catch2 v3.7.0 单头文件 vendored 模式，对齐 CppTLM 单二进制+file(GLOB) 集成方式，消除 82 行手动 CMake 测试注册样板，把 "纯 main()+assert, 暂不引入 Catch2" 的项目约定替换为 CppTLM/CppHDL 一致的测试框架约定。

**Architecture:**
- **vendor 模式**: 从 `/workspace/project/CppTLM/test/` 复制 `catch_amalgamated.hpp` (14,064 行) + `catch_amalgamated.cpp` (11,857 行) 到 `tests/catch2/` 子目录
- **单二进制模式**: 一个 `chipforge_tests` 可执行文件链接所有测试 + `catch_amalgamated.cpp`，由 `catch_amalgamated.cpp` 的默认 `main()` 提供入口（`#define CATCH_CONFIG_MAIN` 保留给未来需要自定义 main 的特殊测试）
- **CMake 自动发现**: 用 `file(GLOB)` 替换 82 行 `if(EXISTS) add_executable + add_test` 手动注册
- **架构标签**: 用 Catch2 tag 体系 `[family]` `[phase]` `[risk]` 替代目录结构分组（不打破现有按 family 的目录约定）
- **统一 include**: `"catch_amalgamated.hpp"`（CppTLM 模式），无 `find_package`、无 `FetchContent`、零网络依赖

**Tech Stack:** Catch2 v3.7.0 (vendored amalgamated) + CMake 3.20+ `file(GLOB)` + ctest 集成 + Python 3（用于自动转换器脚本） + SystemC stub（已存在）

---

## 0. 背景与上下文

### 0.1 当前现状（47 个测试文件）

| 家族 | 路径 | 文件数 | 当前模式 | 备注 |
|------|------|--------|----------|------|
| framework | `tests/framework/` | 9 | 纯 main+assert | cf_plugin 框架层 |
| cache | `tests/cache/` | 5 | 纯 main+assert | L1Cache IP 业务 |
| cpu | `tests/cpu/` | 20 | 纯 main+assert | CPU 业务（含 4 个 integration/ + 1 个 configs/） |
| cpu/integration | `tests/cpu/integration/` | 4 | 纯 main+assert | RISC-V 5/7/10-stage 集成 |
| cpu/configs | `tests/cpu/configs/` | 1 | 纯 main+assert | M5.19 schema |
| soc | `tests/soc/` | 2 | 纯 main+assert | SoC JSON 集成 |
| bundles | `tests/bundles/` | 1 | 纯 main+assert | 共享 Bundle |
| mmu | `tests/mmu/` | 5 | **GTest**（配置破碎） | 无 `find_package(GTest)` |
| **总计** | | **47** | | |

**关键问题**:
- `src/cf_plugin/CMakeLists.txt` 第 87-464 行 + `tests/mmu/CMakeLists.txt` 手动注册全部测试
- 每个测试文件都有自己的 `int main()`（共 42 个）
- mmu/ 的 GTest 链接但无 `find_package` → 实际无法构建
- `src/cf_plugin/CMakeLists.txt` 第 82 行显式声明 `# 单元测试 (使用纯 main() + assert, 暂不引入 Catch2)`

### 0.2 参考实现（CppTLM）

- `test/catch_amalgamated.{hpp,cpp}` — vendored Catch2 v3.7.0
- `test/CMakeLists.txt` — 单二进制 `cpptlm_tests` 模式，44 行 CMake
- `test/test_*.cc` — 76 个测试，`#include <catch2/catch_all.hpp>` 解析到本地 header

### 0.3 成功标准

1. ✅ `ctest` 全绿（47 个测试全部迁移并通过）
2. ✅ 全部测试编入单一 `chipforge_tests` 可执行文件
3. ✅ CMake 测试注册代码从 82 行降至 ≤ 15 行
4. ✅ `src/cf_plugin/CMakeLists.txt` 中所有 `if(EXISTS) add_executable` 测试块删除
5. ✅ `tests/mmu/CMakeLists.txt` 中 GTest 引用删除，全部改 Catch2
6. ✅ `int main()` 数量从 42 降至 0（仅 `catch_amalgamated.cpp` 的默认 main）
7. ✅ `assert()` 引用数从 ~250 降至 0，全部改 Catch2 `REQUIRE`/`CHECK`
8. ✅ 新增测试仅需创建 `tests/<family>/test_<name>.cpp` 即可被自动发现

---

## 1. 文件结构映射

### 1.1 创建的文件

```
tests/catch2/
├── catch_amalgamated.hpp         (14,064 行, 522KB, 来自 CppTLM)
├── catch_amalgamated.cpp         (11,857 行, 423KB, 来自 CppTLM)
└── README.md                     (vendor 注释 + MD5 校验 + 升级路径)

tools/migrate_to_catch2/
├── migrate.py                    (转换器主脚本: main+assert → TEST_CASE+REQUIRE)
├── transform_assert.py           (assert(x) → REQUIRE(x) 单行转换)
├── transform_main.py             (int main() {...} → catch_amalgamated 入口)
├── transform_check_macro.py      (手写 CHECK 宏 → Catch2 CHECK)
└── README.md                     (转换器使用说明)
```

### 1.2 修改的文件

```
CMakeLists.txt                                   (添加 CHIPFORGE_BUILD_TESTS 选项 + include(tests/CMakeLists.txt))
src/cf_plugin/CMakeLists.txt                     (删除第 87-464 行测试注册，保留 cf_plugin lib)
tests/CMakeLists.txt                             (新建: 单二进制 + file(GLOB) 模式)
tests/mmu/CMakeLists.txt                         (改写: GTest 替换为 Catch2)
tests/README.md                                  (更新文档反映新约定)
docs/architecture/testing-and-dse.md             (更新测试架构说明)
```

### 1.3 测试文件全部保持原位

所有 47 个 `tests/<family>/test_*.cpp` **路径不变**，只修改其内容（替换 main + assert 为 Catch2 宏）。

---

## 2. 实施阶段

### Phase A: 基础设施（vendor + 转换器）

#### Task A1: Vendor Catch2 amalgamated 文件

**Files:**
- Create: `tests/catch2/catch_amalgamated.hpp`
- Create: `tests/catch2/catch_amalgamated.cpp`
- Create: `tests/catch2/README.md`

- [ ] **Step A1.1: 验证 source 文件完整性**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
SRC="/workspace/project/CppTLM/test"
md5sum "$SRC/catch_amalgamated.hpp" "$SRC/catch_amalgamated.cpp"
# 预期输出:
# 33a2094513de12552236286bbac80a0e  catch_amalgamated.hpp
# 579ca0ed8a834dd271b978074c85accc  catch_amalgamated.cpp
```

- [ ] **Step A1.2: 复制 vendor 文件**

```bash
mkdir -p tests/catch2
cp /workspace/project/CppTLM/test/catch_amalgamated.hpp tests/catch2/
cp /workspace/project/CppTLM/test/catch_amalgamated.cpp tests/catch2/
ls -la tests/catch2/
```

- [ ] **Step A1.3: 验证文件完整（行数 + MD5）**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2/tests/catch2
md5sum catch_amalgamated.hpp catch_amalgamated.cpp
wc -l catch_amalgamated.hpp catch_amalgamated.cpp
# 预期:
# 14064 catch_amalgamated.hpp
# 11857 catch_amalgamated.cpp
```

- [ ] **Step A1.4: 写 vendor README**

创建 `tests/catch2/README.md` 内容:

```markdown
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
```

- [ ] **Step A1.5: 提交 Phase A1**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
git add tests/catch2/
git commit -m "test: vendor Catch2 v3.7.0 amalgamated from CppTLM"
```

#### Task A2: 编写 Python 转换器

**Files:**
- Create: `tools/migrate_to_catch2/migrate.py`
- Create: `tools/migrate_to_catch2/transform_assert.py`
- Create: `tools/migrate_to_catch2/transform_main.py`
- Create: `tools/migrate_to_catch2/transform_check_macro.py`
- Create: `tools/migrate_to_catch2/README.md`

**设计原则:**
- 转换器是**幂等**的：可重复运行，结果稳定
- 转换器是**保守**的：仅做高置信度替换，遇到不确定模式抛出错误让人工处理
- 转换器输出**diff 友好**：保留缩进、注释、空行

- [ ] **Step A2.1: 写 transform_assert.py（assert → REQUIRE 转换）**

```python
#!/usr/bin/env python3
"""
transform_assert.py: 把 <cassert> + assert(x) 模式转换为 Catch2 REQUIRE(x) 模式。

模式识别:
- 1. 移除 #include <cassert>
- 2. 在文件顶部添加 #include "catch_amalgamated.hpp"
- 3. assert(x) → REQUIRE(x)（在函数体内）
- 4. assert(x && "msg") → REQUIRE(x) [INFO(x) << "msg"] （可选，保守不做）
- 5. assert 失败后的 printf PASS 行保留

保守策略:
- 不转换 assert 在宏定义内部
- 不转换 static_assert
- 报告但不动 NDEBUG 条件块
"""
import re
import sys
from pathlib import Path

INCLUDE_CASSERT = re.compile(r'^\s*#\s*include\s+<cassert>\s*$', re.MULTILINE)
ASSERT_CALL = re.compile(r'\bassert\(([^;]+?)\)\s*;')
ASSERT_WITH_MSG = re.compile(r'\bassert\(([^;]+?)\s*&&\s*"([^"]+)"\)\s*;')

def transform_file(path: Path) -> tuple[str, int]:
    src = path.read_text()
    original = src
    
    # 1. 替换 #include <cassert> 为 catch_amalgamated.hpp
    if INCLUDE_CASSERT.search(src):
        src = INCLUDE_CASSERT.sub('', src)
        # 在文件最前面插入 catch_amalgamated 头
        if '#include "catch_amalgamated.hpp"' not in src:
            # 找到第一个 #include 行
            m = re.search(r'^(#include [^\n]+)$', src, re.MULTILINE)
            if m:
                src = src[:m.start()] + '#include "catch_amalgamated.hpp"\n' + src[m.start():]
    
    # 2. 转换 assert(x && "msg") → 保留
    # 3. 转换 assert(x) → REQUIRE(x)
    count = 0
    def repl(m):
        nonlocal count
        count += 1
        cond = m.group(1).strip()
        return f'REQUIRE({cond});'
    
    src = ASSERT_CALL.sub(repl, src)
    return src, count

if __name__ == '__main__':
    for arg in sys.argv[1:]:
        p = Path(arg)
        new_src, n = transform_file(p)
        if n > 0:
            p.write_text(new_src)
            print(f"  transformed {n} assert(s) in {p}")
```

- [ ] **Step A2.2: 写 transform_main.py（main → TEST_CASE 包装）**

```python
#!/usr/bin/env python3
"""
transform_main.py: 把「static void test_xxx() + int main() { test_xxx(); test_yyy(); }」模式
转换为 Catch2 「TEST_CASE(\"xxx\", \"[family]\")」 模式。

转换策略:
- 1. 把每个 static void test_<name>() 转换为 TEST_CASE("<name>", "[<family>]")
- 2. 删除函数定义结尾（保留函数体）
- 3. 删除 int main() 整个块

⚠️ 这是转换器中最复杂的一步，需要对每个文件做：
- 解析 static void test_<name>() 函数定义
- 解析 int main() 块
- 删除 main，重新组织函数为 TEST_CASE
- 函数内的 printf PASS 行可以保留（不是错误）或删除（更干净）
"""
import re
import sys
from pathlib import Path

STATIC_TEST_FN = re.compile(
    r'^static\s+void\s+(test_\w+)\s*\(\s*\)\s*\{',
    re.MULTILINE
)
MAIN_BLOCK = re.compile(
    r'^(int\s+main\s*\(\s*\)\s*\{[^{}]*(?:\{[^{}]*\}[^{}]*)*\})\s*$',
    re.MULTILINE
)
# 简化: 匹配到 main { 开始, 然后贪婪找匹配的 }
MAIN_SIMPLE = re.compile(r'^int\s+main\s*\(\s*\)\s*\{', re.MULTILINE)

def find_matching_brace(src: str, start: int) -> int:
    """从 start 位置（'{'）找匹配的 '}'"""
    depth = 0
    i = start
    while i < len(src):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1

def transform_file(path: Path, family: str) -> tuple[str, int]:
    src = path.read_text()
    
    # 1. 找到所有 static void test_<name>() 函数
    test_fns = []
    for m in STATIC_TEST_FN.finditer(src):
        fn_start = m.start()
        # 函数体开始位置
        brace_start = m.end() - 1  # '{' 的位置
        brace_end = find_matching_brace(src, brace_start)
        if brace_end == -1:
            print(f"WARN: 未匹配的 brace in {path}:{fn_start}")
            continue
        # 函数体内容（不含大括号）
        body_start = brace_start + 1
        body = src[body_start:brace_end]
        test_fns.append({
            'name': m.group(1),
            'start': fn_start,
            'end': brace_end + 1,
            'body': body
        })
    
    if not test_fns:
        return src, 0
    
    # 2. 转换每个 static void test_x() 为 TEST_CASE
    # 从后向前处理避免偏移错乱
    for fn in reversed(test_fns):
        new_block = f'TEST_CASE("{fn["name"][5:]}", "[{family}]") {{{fn["body"]}}}'
        src = src[:fn['start']] + new_block + src[fn['end']:]
    
    # 3. 删除 int main() 块
    m = MAIN_SIMPLE.search(src)
    if m:
        brace_start = m.end() - 1
        brace_end = find_matching_brace(src, brace_start)
        if brace_end != -1:
            # 删除 main 块（从函数定义开始到闭合 }）
            # 也删除 main 前的注释（"// main"）
            # 找到 main 前的换行符
            start = m.start()
            # 向后找 line start
            line_start = src.rfind('\n', 0, start) + 1
            end = brace_end + 1
            src = src[:line_start] + src[end:]
    
    return src, len(test_fns)

if __name__ == '__main__':
    # 用法: transform_main.py <family> <file>...
    if len(sys.argv) < 3:
        print("Usage: transform_main.py <family> <file>...")
        sys.exit(1)
    family = sys.argv[1]
    for arg in sys.argv[2:]:
        p = Path(arg)
        new_src, n = transform_file(p, family)
        if n > 0:
            p.write_text(new_src)
            print(f"  transformed {n} test fn(s) in {p}")
```

- [ ] **Step A2.3: 写 transform_check_macro.py（手写 CHECK 宏 → Catch2 CHECK）**

```python
#!/usr/bin/env python3
"""
transform_check_macro.py: 把项目内手写的 #define CHECK(cond) 宏完整移除。
catch_amalgamated.hpp 已提供同名 CHECK 宏。

实现说明:
- 不能用纯正则, 因为宏体可能包含嵌套的 { } (如 do { if { } else { } } while(0))
- 用 brace-matching 算法: 从 #define CHECK(...) 找到匹配的 结束 \n 或 ;
- 匹配 #define 行后跳过整个宏体（含 do { ... } while(0) 或简单 { ... }）
- 保守: 若未找到 #define 开头则不动文件

实际例子 (test_int_alu_full.cpp):
    #define CHECK(cond) do { \\
      ++total_cases; \\
      if (cond) { ++passed_cases; } \\
      else { printf("  [FAIL] line %d: %s\\n", __LINE__, #cond); } \\
    } while(0)

转换后: 整个 #define ... while(0) 块被删除, 调用点 CHECK(x) 不变 (Catch2 提供)。
"""
import re
import sys
from pathlib import Path

CHECK_DEFINE_START = re.compile(r'^\s*#\s*define\s+CHECK\s*\([^)]*\)\s+', re.MULTILINE)

def find_macro_end(src: str, start: int) -> int:
    """从 start (宏体第一个字符) 找到宏体的结束位置.
    
    策略: 跨过整个 do { ... } while(0) 或 单个 { ... } 块,
          或找到行尾 (\) 续行符链的末尾.
    """
    # 跳过前导空白
    i = start
    while i < len(src) and src[i] in ' \t':
        i += 1
    
    # 处理行尾 \ 续行
    if i < len(src) and src[i] == '\\':
        i += 1
        if i < len(src) and src[i] == '\n':
            i += 1
        # 继续找下一个有效字符
    
    # 检查 do { ... } while(0)
    rest = src[i:].lstrip()
    if rest.startswith('do'):
        # 跳过 'do'
        i = src.index('do', i) + 2
        # 跳过空白
        while i < len(src) and src[i] in ' \t':
            i += 1
        if i < len(src) and src[i] == '{':
            # 找匹配的 '}'
            brace_end = find_matching_brace(src, i)
            if brace_end == -1:
                return -1
            i = brace_end + 1
            # 跳过空白
            while i < len(src) and src[i] in ' \t':
                i += 1
            # 期望 'while(0)'
            if src[i:i+6] == 'while':
                i += 6
                # 找 ')'
                while i < len(src) and src[i] != ')':
                    i += 1
                if i < len(src):
                    i += 1
                # 跳过到行尾
                while i < len(src) and src[i] != '\n':
                    i += 1
                if i < len(src):
                    i += 1  # 包含换行
        return i
    
    # 检查简单 { ... } 块
    if i < len(src) and src[i] == '{':
        brace_end = find_matching_brace(src, i)
        if brace_end == -1:
            return -1
        i = brace_end + 1
        # 跳过到行尾
        while i < len(src) and src[i] != '\n':
            i += 1
        if i < len(src):
            i += 1
        return i
    
    # 其他情况: 单行宏 (如 #define CHECK(x) printf(...))
    while i < len(src) and src[i] != '\n':
        i += 1
    if i < len(src):
        i += 1
    return i

def find_matching_brace(src: str, start: int) -> int:
    """从 start 位置（'{'）找匹配的 '}'"""
    assert src[start] == '{', f"Expected '{{' at position {start}, got '{src[start]}'"
    depth = 0
    i = start
    in_string = False
    in_char = False
    while i < len(src):
        c = src[i]
        if c == '/' and i + 1 < len(src) and src[i+1] == '/':
            # 行注释, 跳过到行尾
            while i < len(src) and src[i] != '\n':
                i += 1
            continue
        elif c == '/' and i + 1 < len(src) and src[i+1] == '*':
            # 块注释
            i += 2
            while i + 1 < len(src) and not (src[i] == '*' and src[i+1] == '/'):
                i += 1
            i += 2
            continue
        elif c == '"' and not in_char:
            in_string = not in_string
        elif c == "'" and not in_string:
            in_char = not in_char
        elif not in_string and not in_char:
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    return i
        i += 1
    return -1

def transform_file(path: Path) -> tuple[str, int]:
    src = path.read_text()
    new_src = src
    count = 0
    
    # 反复扫描直到没有 #define CHECK
    while True:
        m = CHECK_DEFINE_START.search(new_src)
        if not m:
            break
        start = m.start()
        body_start = m.end()  # 宏体第一个字符位置
        end = find_macro_end(new_src, body_start)
        if end == -1:
            print(f"WARN: {path}: 未找到 #define CHECK 宏体结束 (line ~{new_src[:m.start()].count(chr(10))+1})")
            break
        # 删除从 start 到 end 的整个 #define 块
        new_src = new_src[:start] + new_src[end:]
        count += 1
    
    if count > 0:
        # 清理连续空行（最多保留 1 个空行）
        new_src = re.sub(r'\n\n\n+', '\n\n', new_src)
        path.write_text(new_src)
    
    return new_src, count

if __name__ == '__main__':
    for arg in sys.argv[1:]:
        p = Path(arg)
        new_src, n = transform_file(p)
        if n > 0:
            print(f"  removed {n} CHECK macro(s) in {p}")
```

- [ ] **Step A2.4: 写 migrate.py 统一调度脚本**

```python
#!/usr/bin/env python3
"""
migrate.py: 统一调度四个转换器，把单个测试文件从「纯 main+assert」或「GTest」模式迁移到 Catch2。

用法:
    python3 migrate.py cache/test_replacement_policy.cpp --family cache
    python3 migrate.py --all    # 遍历 tests/ 下所有 test_*.cpp

执行顺序（重要）:
    1. transform_assert.py    (先转 assert)
    2. transform_check_macro.py (移除手写 CHECK 宏)
    3. transform_gtest.py     (GTest → Catch2 宏, 仅 mmu/ 用)
    4. transform_main.py      (最后转 main+static void 为 TEST_CASE)

输出:
    - 原地修改文件
    - 打印每个文件的转换计数
    - 失败时退出码 1
"""
import argparse
import subprocess
import sys
from pathlib import Path

FAMILY_BY_DIR = {
    'tests/framework': 'framework',
    'tests/cache': 'cache',
    'tests/cpu': 'cpu',
    'tests/cpu/integration': 'cpu-integration',
    'tests/cpu/configs': 'cpu-configs',
    'tests/soc': 'soc',
    'tests/bundles': 'bundles',
    'tests/mmu': 'mmu',
}

def detect_family(path: Path) -> str:
    p = str(path)
    for prefix, family in FAMILY_BY_DIR.items():
        if p.startswith(prefix):
            return family
    return 'unknown'

def migrate_file(path: Path) -> bool:
    family = detect_family(path)
    print(f"\n=== Migrating {path} (family={family}) ===")
    
    # Step 1: transform_assert (assert 阵营)
    r = subprocess.run(['python3', 'tools/migrate_to_catch2/transform_assert.py', str(path)],
                        capture_output=True)
    if r.returncode != 0:
        print(f"  FAIL: assert transform: {r.stderr.decode()}")
        return False
    if r.stdout:
        print(f"  {r.stdout.decode().strip()}")
    
    # Step 2: transform_check_macro (手写 CHECK 宏)
    r = subprocess.run(['python3', 'tools/migrate_to_catch2/transform_check_macro.py', str(path)],
                        capture_output=True)
    if r.returncode != 0:
        print(f"  FAIL: check_macro transform: {r.stderr.decode()}")
        return False
    if r.stdout:
        print(f"  {r.stdout.decode().strip()}")
    
    # Step 3: transform_gtest (GTest → Catch2, 仅 mmu/ 用)
    r = subprocess.run(['python3', 'tools/migrate_to_catch2/transform_gtest.py', family, str(path)],
                        capture_output=True)
    if r.returncode != 0:
        print(f"  FAIL: gtest transform: {r.stderr.decode()}")
        return False
    if r.stdout:
        print(f"  {r.stdout.decode().strip()}")
    
    # Step 4: transform_main (最后转 main+static void)
    r = subprocess.run(['python3', 'tools/migrate_to_catch2/transform_main.py', family, str(path)],
                        capture_output=True)
    if r.returncode != 0:
        print(f"  FAIL: main transform: {r.stderr.decode()}")
        return False
    if r.stdout:
        print(f"  {r.stdout.decode().strip()}")
    
    return True

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='*', help='Specific test files to migrate')
    ap.add_argument('--all', action='store_true', help='Migrate all test_*.cpp under tests/')
    args = ap.parse_args()
    
    if args.all:
        files = sorted(Path('tests').rglob('test_*.cpp'))
    else:
        files = [Path(f) for f in args.files]
    
    if not files:
        print("No files to migrate")
        sys.exit(1)
    
    failed = []
    for f in files:
        if not migrate_file(f):
            failed.append(f)
    
    print(f"\n=== Migration complete ===")
    print(f"  Total: {len(files)}")
    print(f"  Failed: {len(failed)}")
    if failed:
        for f in failed:
            print(f"    - {f}")
        sys.exit(1)

if __name__ == '__main__':
    main()
```

- [ ] **Step A2.4b: 写 transform_gtest.py（GTest → Catch2 转换器）**

```python
#!/usr/bin/env python3
"""
transform_gtest.py: 把 GTest 风格的测试代码转换为 Catch2 等价模式。

转换规则:
  #include <gtest/gtest.h>                          → #include "catch_amalgamated.hpp"
  TEST(SuiteName, TestName) { body }                → TEST_CASE("TestName", "[mmu][SuiteName]") { body }
  TEST_F(FixtureClass, TestName) { body }           → TEST_CASE_METHOD(FixtureClass, "TestName", "[mmu]") { body }
  EXPECT_TRUE(x)                                    → CHECK(x)
  EXPECT_FALSE(x)                                   → CHECK_FALSE(x)
  EXPECT_EQ(a, b)                                   → CHECK(a == b)
  EXPECT_NE(a, b)                                   → CHECK(a != b)
  EXPECT_LT(a, b) / EXPECT_LE / GT / GE             → CHECK(a < b) / etc.
  ASSERT_TRUE(x)                                    → REQUIRE(x)
  ASSERT_FALSE(x)                                   → REQUIRE_FALSE(x)
  ASSERT_EQ(a, b)                                   → REQUIRE(a == b)

边界情况:
  - EXPECT_EQ 中的参数可能是 macro 或 template, 用括号强制
  - TEST 宏可能跨多行 (有 trailing {), 用 brace-matching 找函数体
  - TEST_F 转换时若原文件没有 fixture class 定义, 报 WARN 但不删除
"""
import re
import sys
from pathlib import Path

INCLUDE_GTEST = re.compile(r'#\s*include\s+<gtest/gtest\.h>')
TEST_MACRO = re.compile(r'\bTEST\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*\{')
TEST_F_MACRO = re.compile(r'\bTEST_F\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*\{')

# EXPECT_* / ASSERT_* 转换表
MACRO_REPLACEMENTS = [
    # (regex, replacement, require_args_count)
    (re.compile(r'\bEXPECT_TRUE\s*\(\s*(.+?)\s*\)\s*;'), lambda m: f'CHECK({m.group(1)});', 1),
    (re.compile(r'\bEXPECT_FALSE\s*\(\s*(.+?)\s*\)\s*;'), lambda m: f'CHECK_FALSE({m.group(1)});', 1),
    (re.compile(r'\bEXPECT_EQ\s*\(\s*(.+?)\s*,\s*(.+?)\s*\)\s*;'), lambda m: f'CHECK({m.group(1)} == {m.group(2)});', 2),
    (re.compile(r'\bEXPECT_NE\s*\(\s*(.+?)\s*,\s*(.+?)\s*\)\s*;'), lambda m: f'CHECK({m.group(1)} != {m.group(2)});', 2),
    (re.compile(r'\bEXPECT_LT\s*\(\s*(.+?)\s*,\s*(.+?)\s*\)\s*;'), lambda m: f'CHECK({m.group(1)} < {m.group(2)});', 2),
    (re.compile(r'\bEXPECT_LE\s*\(\s*(.+?)\s*,\s*(.+?)\s*\)\s*;'), lambda m: f'CHECK({m.group(1)} <= {m.group(2)});', 2),
    (re.compile(r'\bEXPECT_GT\s*\(\s*(.+?)\s*,\s*(.+?)\s*\)\s*;'), lambda m: f'CHECK({m.group(1)} > {m.group(2)});', 2),
    (re.compile(r'\bEXPECT_GE\s*\(\s*(.+?)\s*,\s*(.+?)\s*\)\s*;'), lambda m: f'CHECK({m.group(1)} >= {m.group(2)});', 2),
    (re.compile(r'\bASSERT_TRUE\s*\(\s*(.+?)\s*\)\s*;'), lambda m: f'REQUIRE({m.group(1)});', 1),
    (re.compile(r'\bASSERT_FALSE\s*\(\s*(.+?)\s*\)\s*;'), lambda m: f'REQUIRE_FALSE({m.group(1)});', 1),
    (re.compile(r'\bASSERT_EQ\s*\(\s*(.+?)\s*,\s*(.+?)\s*\)\s*;'), lambda m: f'REQUIRE({m.group(1)} == {m.group(2)});', 2),
    (re.compile(r'\bASSERT_NE\s*\(\s*(.+?)\s*,\s*(.+?)\s*\)\s*;'), lambda m: f'REQUIRE({m.group(1)} != {m.group(2)});', 2),
]

def find_matching_brace(src: str, start: int) -> int:
    """从 start 位置（'{'）找匹配的 '}', 跳过字符串/注释"""
    assert src[start] == '{'
    depth = 0
    i = start
    in_string = False
    in_char = False
    while i < len(src):
        c = src[i]
        if c == '/' and i + 1 < len(src) and src[i+1] == '/':
            while i < len(src) and src[i] != '\n':
                i += 1
            continue
        elif c == '/' and i + 1 < len(src) and src[i+1] == '*':
            i += 2
            while i + 1 < len(src) and not (src[i] == '*' and src[i+1] == '/'):
                i += 1
            i += 2
            continue
        elif c == '"' and not in_char:
            in_string = not in_string
        elif c == "'" and not in_string:
            in_char = not in_char
        elif not in_string and not in_char:
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    return i
        i += 1
    return -1

def transform_test_macro(src: str, family: str) -> tuple[str, int]:
    """转换 TEST(Suite, Name) { body } → TEST_CASE("Name", "[family][Suite]") { body }
    
    必须从后向前处理 (避免偏移错乱)
    """
    count = 0
    matches = list(TEST_MACRO.finditer(src))
    for m in reversed(matches):
        suite, name = m.group(1), m.group(2)
        brace_start = m.end() - 1
        brace_end = find_matching_brace(src, brace_start)
        if brace_end == -1:
            print(f"WARN: TEST({suite}, {name}) 未匹配的 brace, 跳过")
            continue
        new_decl = f'TEST_CASE("{name}", "[{family}][{suite}]") {{'
        src = src[:m.start()] + new_decl + src[brace_start+1:brace_end] + '}' + src[brace_end+1:]
        count += 1
    return src, count

def transform_test_f_macro(src: str, family: str) -> tuple[str, int]:
    """转换 TEST_F(Fixture, Name) { body } → TEST_CASE_METHOD(Fixture, "Name", "[family]") { body }"""
    count = 0
    matches = list(TEST_F_MACRO.finditer(src))
    for m in reversed(matches):
        fixture, name = m.group(1), m.group(2)
        brace_start = m.end() - 1
        brace_end = find_matching_brace(src, brace_start)
        if brace_end == -1:
            print(f"WARN: TEST_F({fixture}, {name}) 未匹配的 brace, 跳过")
            continue
        new_decl = f'TEST_CASE_METHOD({fixture}, "{name}", "[{family}]") {{'
        src = src[:m.start()] + new_decl + src[brace_start+1:brace_end] + '}' + src[brace_end+1:]
        count += 1
    return src, count

def transform_expect_assert(src: str) -> tuple[str, int]:
    """转换所有 EXPECT_* 和 ASSERT_* 宏"""
    count = 0
    for regex, repl, nargs in MACRO_REPLACEMENTS:
        new_src, n = regex.subn(repl, src)
        if n > 0:
            count += n
            src = new_src
    return src, count

def transform_file(path: Path, family: str) -> tuple[str, int]:
    src = path.read_text()
    total = 0
    
    # 0. 替换 #include <gtest/gtest.h>
    if INCLUDE_GTEST.search(src):
        src = INCLUDE_GTEST.sub('#include "catch_amalgamated.hpp"', src)
    
    # 1. 转换 EXPECT_*/ASSERT_*
    src, n = transform_expect_assert(src)
    total += n
    
    # 2. 转换 TEST()
    src, n = transform_test_macro(src, family)
    total += n
    
    # 3. 转换 TEST_F()
    src, n = transform_test_f_macro(src, family)
    total += n
    
    if total > 0:
        path.write_text(src)
    
    return src, total

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: transform_gtest.py <family> <file>...")
        sys.exit(1)
    family = sys.argv[1]
    for arg in sys.argv[2:]:
        p = Path(arg)
        new_src, n = transform_file(p, family)
        if n > 0:
            print(f"  transformed {n} GTest macro(s) in {p}")
```

- [ ] **Step A2.5: 写转换器 README**

创建 `tools/migrate_to_catch2/README.md`:

```markdown
# Catch2 迁移工具集

> 用于把 ChipForge 的「纯 main()+assert」测试自动转换为 Catch2 TEST_CASE 模式。
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
```

- [ ] **Step A2.6: 测试转换器（用 1 个样本验证）**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
# 备份样本
cp tests/cache/test_replacement_policy.cpp /tmp/opencode/sample_original.cpp
# 跑转换器
python3 tools/migrate_to_catch2/migrate.py tests/cache/test_replacement_policy.cpp
# 检查输出
head -50 tests/cache/test_replacement_policy.cpp
echo "---"
# 关键检查点:
# - 没有 int main() 出现
# - 有 TEST_CASE 出现
# - 有 REQUIRE 出现
# - 有 #include "catch_amalgamated.hpp"
grep -c "int main" tests/cache/test_replacement_policy.cpp
grep -c "TEST_CASE" tests/cache/test_replacement_policy.cpp
grep -c "REQUIRE" tests/cache/test_replacement_policy.cpp
grep -c "catch_amalgamated.hpp" tests/cache/test_replacement_policy.cpp
# 预期: main=0, TEST_CASE>=1, REQUIRE>=5, header=1
```

- [ ] **Step A2.7: 恢复样本文件（不让样本污染后续阶段）**

```bash
cp /tmp/opencode/sample_original.cpp tests/cache/test_replacement_policy.cpp
# 不提交，保持原状用于 Phase D 的并行迁移
```

- [ ] **Step A2.8: 提交 Phase A2**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
git add tools/migrate_to_catch2/
git commit -m "test: add Catch2 migration toolchain (transform_assert/main/check_macro)"
```

### Phase B: CMake 重构（单二进制 + file(GLOB)）

#### Task B1: 新建 tests/CMakeLists.txt

**Files:**
- Create: `tests/CMakeLists.txt`

- [ ] **Step B1.1: 写单二进制 CMakeLists.txt**

```cmake
# tests/CMakeLists.txt
# 单二进制 Catch2 测试套件（与 CppTLM 模式一致）
# 设计原则: 测试自动发现, 新增 test_*.cpp 无需修改本文件

# 收集所有测试源文件（含子目录, RECURSE 处理 cpu/integration/ 和 cpu/configs/）
file(GLOB_RECURSE TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*/test_*.cpp"
)

# 把 catch_amalgamated.cpp 加入编译（提供默认 main）
list(APPEND TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/catch2/catch_amalgamated.cpp"
)

# 关键: cache 测试需要的实现 .cpp 源文件
# 这些 .cpp 之前在 src/cf_plugin/CMakeLists.txt 中被显式编译进独立 test target
# 单二进制模式下, 必须显式列出以确保链接找到符号
# (cf_plugin 是 INTERFACE 库, 不包含 .cpp 实现)
set(CACHE_IMPL_SOURCES
    ${CMAKE_SOURCE_DIR}/ip/cache/tlm/L1CachePlugin.cpp
    ${CMAKE_SOURCE_DIR}/ip/cache/policies/replacement_policy.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp
)
list(APPEND TEST_SOURCES ${CACHE_IMPL_SOURCES})

# 单二进制 chipforge_tests
add_executable(chipforge_tests ${TEST_SOURCES})

# tests/ 子目录加入 include 路径（让 #include "catch_amalgamated.hpp" 解析）
target_include_directories(chipforge_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/bundles
    ${CMAKE_SOURCE_DIR}/ip
    ${CMAKE_SOURCE_DIR}/src
)

# 链接 cf_plugin 框架 + CppTLM/CppHDL（按需）
target_link_libraries(chipforge_tests PRIVATE
    cf_plugin
    cf_plugin_bridge
    cpptlm_core
    cpphdl
    nlohmann_json::nlohmann_json
)

# 启用 ASan (可选, 跟随父选项)
if(ENABLE_ASAN)
    target_compile_options(chipforge_tests PRIVATE --coverage)
    target_link_options(chipforge_tests PRIVATE --coverage)
endif()

# 让测试可执行文件找到项目根目录（soc JSON, manual_elf 等）
target_compile_definitions(chipforge_tests PRIVATE
    CHIPFORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# 设置运行时输出目录
set_target_properties(chipforge_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 注册到 ctest（单测试）
add_test(NAME chipforge_tests COMMAND chipforge_tests)
set_tests_properties(chipforge_tests PROPERTIES LABELS "unit;integration;catch2")

# 让 ctest 可执行文件找到项目根
set_tests_properties(chipforge_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

**关于 CACHE_IMPL_SOURCES 的关键说明**:
- `cf_plugin` 是 INTERFACE 库（仅头文件），不包含任何 .cpp 实现
- cache 测试 (test_l1_cache_*) 在原 CMakeLists.txt 中显式将 `L1CachePlugin.cpp`/`replacement_policy.cpp`/`l1_cache_bridge.cpp`/`l1_cache_bridge_adapter.cpp` 编译进各自 test target
- 单二进制模式下，这些 .cpp **必须**显式列入 `chipforge_tests` 的 sources，否则 `L1CachePlugin` 等符号无法解析
- 替代方案：把这些 .cpp 编译为 STATIC 库再链接（更复杂，本计划采用显式列出方案）

- [ ] **Step B1.2: 验证 tests/CMakeLists.txt 语法**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
# 用 cmake -P 验证语法
cmake -P tests/CMakeLists.txt 2>&1 | head -20 || true
# 实际只能在完整 configure 时验证
```

#### Task B2: 修改根 CMakeLists.txt 与 src/cf_plugin/CMakeLists.txt

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/cf_plugin/CMakeLists.txt`

- [ ] **Step B2.1: 在根 CMakeLists.txt 添加 CHIPFORGE_BUILD_TESTS 选项**

定位：在 `CMakeLists.txt` 中 `enable_testing()` 附近添加门控。

```cmake
# 现有:
enable_testing()

# 新增:
option(CHIPFORGE_BUILD_TESTS "Build Catch2 test suite" ON)
if(CHIPFORGE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

- [ ] **Step B2.2: 在 src/cf_plugin/CMakeLists.txt 中移除测试注册段**

定位：`src/cf_plugin/CMakeLists.txt` 第 33-34 行注释 + 第 82-464 行所有 `if(EXISTS) add_executable + add_test` 块。

**操作**:
- 保留第 33-34 行注释（作为历史记录）
- 删除第 35-465 行所有 if(EXISTS) add_executable + add_test 块
- **保留** `add_library(cf_plugin ...)` 主目标定义
- 保留第 87 行 `add_executable(test_plugin_lifecycle` 起的后续所有 if(EXISTS) 块全部删除
- 删除后 `src/cf_plugin/CMakeLists.txt` 应该只剩库定义（无测试注册）

**验证**:
```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
# 删除后, 应没有 add_executable + add_test
grep -c "add_executable" src/cf_plugin/CMakeLists.txt
grep -c "add_test" src/cf_plugin/CMakeLists.txt
# 预期: 全部 = 0
```

- [ ] **Step B2.3: 提交 Phase B**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
git add tests/CMakeLists.txt CMakeLists.txt src/cf_plugin/CMakeLists.txt
git commit -m "build(tests): switch to single-binary Catch2 with file(GLOB) discovery

- Add tests/CMakeLists.txt using CppTLM pattern (single binary + GLOB)
- Add CHIPFORGE_BUILD_TESTS option to root CMakeLists.txt
- Remove 82 lines of manual test registration from src/cf_plugin/CMakeLists.txt
- Migration target: 47 test files, vendor catch_amalgamated, single ctest entry"
```

### Phase C: 第一次构建验证（迁移前 baseline）

#### Task C1: 验证当前 build 干净（迁移前 baseline）

- [ ] **Step C1.1: 配置 CMake（不实际编译）**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
mkdir -p build-baseline
cmake -S . -B build-baseline -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON 2>&1 | tail -30
# 预期: 配置成功, 找到 47 个测试 target
```

- [ ] **Step C1.2: 列出当前注册的所有 ctest 测试**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
cd build-baseline
ctest -N 2>&1 | head -60
# 预期: 47 个 test (test_plugin_lifecycle, test_payload, ... test_cpu_sim_real_tohost)
```

- [ ] **Step C1.3: 不提交，仅记录 baseline（为 Phase E 对比用）**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
ctest -N 2>&1 | wc -l > /tmp/opencode/baseline_ctest_count.txt
echo "Baseline ctest count: $(cat /tmp/opencode/baseline_ctest_count.txt)"
```

### Phase D: 批量迁移（按 family 并行 dispatch agents）

**重要**: 此阶段使用 dispatching-parallel-agents，按 family 并行启动 6 个 agent 同时迁移。**不要**串行执行。

#### Task D0: 批量迁移前准备

- [ ] **Step D0.1: 把所有 test_*.cpp 列出，按 family 分组**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
mkdir -p /tmp/opencode/migration_families
for family in framework cache cpu soc bundles; do
    find tests/$family -name "test_*.cpp" -type f | sort > /tmp/opencode/migration_families/$family.txt
    echo "$family: $(wc -l < /tmp/opencode/migration_families/$family.txt) files"
done
# mmu/ 单独处理（已经有 GTest, 转换方式不同）
find tests/mmu -name "test_*.cpp" -type f | sort > /tmp/opencode/migration_families/mmu.txt
echo "mmu: $(wc -l < /tmp/opencode/migration_families/mmu.txt) files"
```

**预期输出**:
- framework: 9
- cache: 5
- cpu: 25 (含 cpu/integration/* 和 cpu/configs/*)
- soc: 2
- bundles: 1
- mmu: 5

#### Task D1: 并行迁移 framework/ (9 files)

- [ ] **Step D1.1: 启动 framework 迁移 agent（后台）**

**agent prompt 完整内容**（在主 session 实际执行时调用 `task(subagent_type="general", run_in_background=true, load_skills=["cmake"], prompt=<以下内容>)`）:

```
You are migrating 9 test files in tests/framework/ from "纯 main()+assert" to Catch2.

SCOPE:
- Working directory: /workspace/project/ChipForge/.worktrees/testing-catch2
- 9 files (full list - read from /tmp/opencode/migration_families/framework.txt):
  - tests/framework/test_coexistence.cpp
  - tests/framework/test_ctrl_link.cpp
  - tests/framework/test_hello_plugin.cpp
  - tests/framework/test_payload.cpp
  - tests/framework/test_pipe_arbitration.cpp
  - tests/framework/test_pipe_builder.cpp
  - tests/framework/test_pipe_node.cpp
  - tests/framework/test_plugin_lifecycle.cpp
  - tests/framework/test_storage.cpp
- Family tag for Catch2: [framework]

TOOL:
  python3 tools/migrate_to_catch2/migrate.py <file>
  Already handles: assert→REQUIRE, 手写 CHECK 宏, static void test_xxx() + int main() → TEST_CASE

POST-PROCESS (manual, you must do after running the tool):
1. For each file, remove any remaining printf("  [PASS] ...\n") lines (Catch2 reports automatically)
2. Remove any "设计说明: 纯 main() + assert, 不引入 Catch2" comments
3. Remove #include <cstdio> if only used for printf PASS
4. Verify file still compiles (just check syntax: no remaining int main, no remaining assert, no remaining #define CHECK)

VERIFICATION:
After migrating all 9 files, run:
    grep -c "int main" tests/framework/test_*.cpp | grep -v ":0" && echo "FAIL: leftover main" || echo "OK: no main"
    grep -c "^assert" tests/framework/test_*.cpp | grep -v ":0" && echo "FAIL: leftover assert" || echo "OK: no assert"
    grep -c "TEST_CASE" tests/framework/test_*.cpp   # 至少 1 per file

DO NOT MODIFY:
- src/cf_plugin/CMakeLists.txt
- tests/CMakeLists.txt
- Catch2 vendor files in tests/catch2/

COMMIT:
After verification, commit:
    git add tests/framework/
    git commit -m "test(framework): migrate 9 tests from main+assert to Catch2"

REPORT:
Return summary:
- Files migrated: N/9
- Files needing manual fix: list
- Commit hash
```

- [ ] **Step D1.2: 等待 D1.1 完成, 静态检查（不编译）**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
# ⚠️ 重要: 不在此步骤编译 chipforge_tests
# 原因: tests/CMakeLists.txt 用 file(GLOB_RECURSE) 一次性发现所有 47 个测试源文件.
# 如果此时其他 family (cache/cpu/soc/bundles/mmu) 还未迁移, 它们仍含 int main(),
# 与 catch_amalgamated.cpp 的 main() 冲突 → multiple definition 错误.
# 编译验证统一在 Phase E 做 (E1.2).
#
# 此步骤只做 grep 静态检查:
echo "framework/ files with leftover int main:"
grep -l "int main" tests/framework/test_*.cpp
# 预期: 无输出 (D1.1 agent 已删除)
echo "framework/ files with leftover assert:"
grep -l "assert(" tests/framework/test_*.cpp
# 预期: 无输出
echo "framework/ files with TEST_CASE:"
grep -l "TEST_CASE" tests/framework/test_*.cpp | wc -l
# 预期: 9 (全部)
```

#### Task D2: 并行迁移 cache/ (5 files)

- [ ] **Step D2.1: 启动 cache 迁移 agent（后台）**

**agent prompt 完整内容**（在主 session 实际执行时调用 `task(subagent_type="general", run_in_background=true, load_skills=["cmake"], prompt=<以下内容>)`）:

```
You are migrating 5 test files in tests/cache/ from "纯 main()+assert" to Catch2.

SCOPE:
- Working directory: /workspace/project/ChipForge/.worktrees/testing-catch2
- 5 files:
  - tests/cache/test_l1_cache_bridge.cpp
  - tests/cache/test_l1_cache_json_instantiate.cpp
  - tests/cache/test_l1_cache_plugin_e2e.cpp
  - tests/cache/test_l1_cache_plugin_unit.cpp
  - tests/cache/test_replacement_policy.cpp
- Family tag for Catch2: [cache]

TOOL:
  python3 tools/migrate_to_catch2/migrate.py <file>
  Already handles: assert→REQUIRE, 手写 CHECK 宏, static void test_xxx() + int main() → TEST_CASE

POST-PROCESS (manual, you must do after running the tool):
1. For each file, remove any remaining printf("  [PASS] ...\n") lines
2. Remove any "纯 main() + assert" comments
3. Remove #include <cstdio> if only used for printf PASS
4. Verify file still compiles

VERIFICATION:
    grep -c "int main" tests/cache/test_*.cpp | grep -v ":0" && echo "FAIL: leftover main" || echo "OK: no main"
    grep -c "^assert" tests/cache/test_*.cpp | grep -v ":0" && echo "FAIL: leftover assert" || echo "OK: no assert"
    grep -c "TEST_CASE" tests/cache/test_*.cpp   # 至少 1 per file

DO NOT MODIFY:
- src/cf_plugin/CMakeLists.txt, tests/CMakeLists.txt, tests/catch2/

COMMIT:
    git add tests/cache/
    git commit -m "test(cache): migrate 5 tests from main+assert to Catch2"

REPORT:
- Files migrated: N/5
- Files needing manual fix: list
- Commit hash
```

#### Task D3: 并行迁移 cpu/ (25 files，含 cpu/integration/ 和 cpu/configs/ 子目录)

- [ ] **Step D3.1: 启动 cpu 迁移 agent（后台）**

**agent prompt 完整内容**:

```
You are migrating 25 test files in tests/cpu/ (含 tests/cpu/integration/ 和 tests/cpu/configs/ 子目录) from "纯 main()+assert" to Catch2.

SCOPE:
- Working directory: /workspace/project/ChipForge/.worktrees/testing-catch2
- 25 files (full list - read from /tmp/opencode/migration_families/cpu.txt):
  - tests/cpu/test_branch.cpp
  - tests/cpu/test_branch_predictor.cpp
  - tests/cpu/test_branch_predictor_runtime_btb.cpp
  - tests/cpu/test_cpu_factory.cpp
  - tests/cpu/test_cpu_sim_real_tohost.cpp
  - tests/cpu/test_dbus.cpp
  - tests/cpu/test_decode.cpp
  - tests/cpu/test_decode_full.cpp   ← 注意: 这个文件有手写 CHECK 宏
  - tests/cpu/test_forward_compat.cpp
  - tests/cpu/test_hazard.cpp
  - tests/cpu/test_ibus.cpp
  - tests/cpu/test_int_alu.cpp
  - tests/cpu/test_int_alu_full.cpp   ← 注意: 这个文件有手写 CHECK 宏
  - tests/cpu/test_lsu.cpp
  - tests/cpu/test_mul.cpp
  - tests/cpu/test_mul_latency.cpp
  - tests/cpu/test_payload_common.cpp
  - tests/cpu/test_reg_file.cpp
  - tests/cpu/test_topology_builder.cpp
  - tests/cpu/integration/test_10stage_riscv.cpp
  - tests/cpu/integration/test_3stage_riscv.cpp
  - tests/cpu/integration/test_5stage_riscv.cpp
  - tests/cpu/integration/test_7stage_riscv.cpp
  - tests/cpu/integration/test_demo_soc.cpp
  - tests/cpu/configs/test_schema_m5_19.cpp
- Family tags:
  - tests/cpu/*.cpp → [cpu]
  - tests/cpu/integration/*.cpp → [cpu-integration]
  - tests/cpu/configs/*.cpp → [cpu-configs]

TOOL:
  python3 tools/migrate_to_catch2/migrate.py <file>
  Already handles: assert→REQUIRE, 手写 CHECK 宏, static void test_xxx() + int main() → TEST_CASE

⚠️ SPECIAL FILES (need extra attention):
- test_int_alu_full.cpp: has #define CHECK(cond) { total++; if (cond) passed++; else printf("[FAIL] %s:%d\n", __FILE__, __LINE__); }
- test_decode_full.cpp: has similar hand-written CHECK macro
  The transform_check_macro.py step will remove these #define blocks automatically.

⚠️ integration/ tests may have WORKING_DIRECTORY set in CMake for manual_elf path.
Make sure file content (e.g. #include "manual_elf/add.S" paths) is preserved.

POST-PROCESS (manual, you must do after running the tool):
1. For each file, remove any remaining printf("  [PASS] ...\n") lines
2. Remove any "纯 main() + assert" comments
3. Remove #include <cstdio> if only used for printf PASS
4. Verify file still compiles
5. For integration/* tests: preserve any include paths to ../../manual_elf/ if present

VERIFICATION:
    grep -c "int main" tests/cpu/test_*.cpp tests/cpu/integration/test_*.cpp tests/cpu/configs/test_*.cpp | grep -v ":0" && echo "FAIL" || echo "OK: no main"
    grep -c "^assert" tests/cpu/test_*.cpp tests/cpu/integration/test_*.cpp tests/cpu/configs/test_*.cpp | grep -v ":0" && echo "FAIL" || echo "OK: no assert"
    grep -c "TEST_CASE" tests/cpu/test_*.cpp tests/cpu/integration/test_*.cpp tests/cpu/configs/test_*.cpp   # 至少 1 per file

DO NOT MODIFY:
- src/cf_plugin/CMakeLists.txt, tests/CMakeLists.txt, tests/catch2/

COMMIT (single commit for entire family):
    git add tests/cpu/
    git commit -m "test(cpu): migrate 25 tests from main+assert to Catch2 (incl. integration/ and configs/)"

REPORT:
- Files migrated: N/25
- Files needing manual fix: list
- Commit hash
```

#### Task D4: 并行迁移 soc/ (2 files)

- [ ] **Step D4.1: 启动 soc 迁移 agent（后台）**

**agent prompt 完整内容**:

```
You are migrating 2 test files in tests/soc/ from "纯 main()+assert" to Catch2.

SCOPE:
- Working directory: /workspace/project/ChipForge/.worktrees/testing-catch2
- 2 files:
  - tests/soc/test_cache_params_schema_json.cpp
  - tests/soc/test_soc_l1_cache_minimal_json.cpp
- Family tag for Catch2: [soc]

TOOL:
  python3 tools/migrate_to_catch2/migrate.py <file>

POST-PROCESS (manual):
1. Remove any remaining printf("  [PASS] ...\n") lines
2. Remove any "纯 main() + assert" comments
3. Remove #include <cstdio> if only used for printf PASS
4. Verify file still compiles

VERIFICATION:
    grep -c "int main" tests/soc/test_*.cpp | grep -v ":0" && echo "FAIL" || echo "OK: no main"
    grep -c "^assert" tests/soc/test_*.cpp | grep -v ":0" && echo "FAIL" || echo "OK: no assert"
    grep -c "TEST_CASE" tests/soc/test_*.cpp   # 至少 1 per file

DO NOT MODIFY:
- src/cf_plugin/CMakeLists.txt, tests/CMakeLists.txt, tests/catch2/

COMMIT:
    git add tests/soc/
    git commit -m "test(soc): migrate 2 tests from main+assert to Catch2"

REPORT:
- Files migrated: N/2
- Commit hash
```

#### Task D5: 并行迁移 bundles/ (1 file)

- [ ] **Step D5.1: 启动 bundles 迁移 agent（后台）**

**agent prompt 完整内容**:

```
You are migrating 1 test file in tests/bundles/ from "纯 main()+assert" to Catch2.

SCOPE:
- Working directory: /workspace/project/ChipForge/.worktrees/testing-catch2
- 1 file: tests/bundles/test_mem_bundles.cpp
- Family tag for Catch2: [bundles]

TOOL:
  python3 tools/migrate_to_catch2/migrate.py tests/bundles/test_mem_bundles.cpp

POST-PROCESS (manual):
1. Remove any remaining printf("  [PASS] ...\n") lines
2. Remove any "纯 main() + assert" comments
3. Remove #include <cstdio> if only used for printf PASS
4. Verify file still compiles

VERIFICATION:
    grep -c "int main" tests/bundles/test_mem_bundles.cpp   # 预期: 0
    grep -c "^assert" tests/bundles/test_mem_bundles.cpp    # 预期: 0
    grep -c "TEST_CASE" tests/bundles/test_mem_bundles.cpp  # 预期: >= 1

DO NOT MODIFY:
- src/cf_plugin/CMakeLists.txt, tests/CMakeLists.txt, tests/catch2/

COMMIT:
    git add tests/bundles/
    git commit -m "test(bundles): migrate test_mem_bundles from main+assert to Catch2"

REPORT:
- Files migrated: 1/1
- Commit hash
```

#### Task D6: 并行迁移 mmu/ (5 files, GTest → Catch2 特殊处理)

- [ ] **Step D6.1: 启动 mmu 迁移 agent（后台）**

**agent prompt 完整内容**:

```
You are migrating 5 test files in tests/mmu/ from GTest to Catch2.

⚠️ SPECIAL: mmu/ already uses GTest (TEST/EXPECT_*/ASSERT_*). Use a different tool.

SCOPE:
- Working directory: /workspace/project/ChipForge/.worktrees/testing-catch2
- 5 files:
  - tests/mmu/test_mmu_config_schema.cpp
  - tests/mmu/test_mmu_plugin.cpp
  - tests/mmu/test_multi_level_tlb.cpp
  - tests/mmu/test_tlb_factory.cpp
  - tests/mmu/test_tlb_unit.cpp
- Family tag for Catch2: [mmu]

TOOL (use this, NOT transform_main.py):
  python3 tools/migrate_to_catch2/transform_gtest.py mmu <file>
  This tool handles: #include <gtest/gtest.h> → catch_amalgamated.hpp,
                      TEST/EXPECT_*/ASSERT_* → TEST_CASE/CHECK/REQUIRE,
                      TEST_F → TEST_CASE_METHOD

⚠️ TEST_F handling (if any file uses it):
- transform_gtest.py converts TEST_F(FixtureClass, TestName) to TEST_CASE_METHOD(FixtureClass, "TestName", "[mmu]")
- If the original fixture class is already a plain class (not inheriting from ::testing::Test), no changes needed
- If it inherits from ::testing::Test, manually change to inherit from Catch::TestEventListenerBase or remove inheritance

POST-PROCESS (manual):
1. Run transform_gtest.py first
2. Remove any "Google Test" / "GTest" comments
3. For files that originally had TEST_F: verify the fixture class compatibility
4. Verify file still compiles (this will be checked in Phase E1.2)

VERIFICATION:
    grep -c "gtest\|GTest" tests/mmu/test_*.cpp | grep -v ":0" && echo "FAIL: leftover GTest" || echo "OK: no GTest"
    grep -c "TEST_CASE\|TEST_CASE_METHOD" tests/mmu/test_*.cpp   # 至少 1 per file
    grep -c "EXPECT_\|ASSERT_" tests/mmu/test_*.cpp | grep -v ":0" && echo "FAIL" || echo "OK: no GTest macros"

DO NOT MODIFY:
- src/cf_plugin/CMakeLists.txt, tests/CMakeLists.txt, tests/catch2/
- ⚠️ tests/mmu/CMakeLists.txt will be DELETED in Step D7.1, so don't bother modifying it.

COMMIT (single commit for entire family):
    git add tests/mmu/
    git commit -m "test(mmu): migrate 5 tests from GTest to Catch2 (fix broken GTest config)"

REPORT:
- Files migrated: N/5
- Files needing manual fix (especially TEST_F fixtures): list
- Commit hash
```

#### Task D7: mmu/ 后续清理（删除破碎的 CMakeLists.txt）

- [ ] **Step D7.1: 删除 tests/mmu/CMakeLists.txt**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
git rm tests/mmu/CMakeLists.txt
# mmu/ 目录的 5 个 .cpp 仍保留, 由 file(GLOB_RECURSE "tests/*/test_*.cpp") 自动发现
```

- [ ] **Step D7.2: 验证 mmu/ 5 个文件无 GTest 残留**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
grep -l "gtest\|GTest\|EXPECT_\|ASSERT_\|TEST_F" tests/mmu/*.cpp
# 预期: 无输出（已全部转换）
```

- [ ] **Step D7.3: 提交清理**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
git add tests/mmu/
git commit -m "chore(tests): remove mmu/CMakeLists.txt (merged into tests/CMakeLists.txt)"
```

#### Task D8: 等待所有并行 agent 完成

- [ ] **Step D8.1: 在主 session 用 background_output 收集所有 agent 结果**

> 等待 `<system-reminder>` 通知（系统会在所有 background task 完成后通知），然后用：
> ```typescript
> background_output(task_id="<bg_xxx>")
> ```
> 收集每个 agent 的 summary 报告。

- [ ] **Step D8.2: 验证所有迁移 commit 都成功**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
git log --oneline -10
# 预期: 看到 D1, D2, D3, D4, D5, D6, D7 各自的 commit
```

### Phase E: 集成验证

#### Task E1: 完整构建 + 跑 ctest

- [ ] **Step E1.1: 全量 CMake 重新配置（启用新 tests/CMakeLists.txt）**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
rm -rf build-new
cmake -S . -B build-new -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DCHIPFORGE_BUILD_TESTS=ON 2>&1 | tail -30
# 预期: 配置成功
```

- [ ] **Step E1.2: 编译 chipforge_tests 单二进制**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
cmake --build build-new --target chipforge_tests -j$(nproc) 2>&1 | tail -50
# 预期: 编译成功
# 失败常见原因:
#   - mmu/ 残留 GTest 引用 → 回到 D7.5
#   - cpu/integration 的 RISCV 工具链缺失 → 配置 BUILD_RTL=OFF
#   - bundles/ 的 c++17/20 兼容性 → 调整 CMAKE_CXX_STANDARD
```

- [ ] **Step E1.3: 跑 ctest 验证（对比 baseline 数量）**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
cd build-new
ctest -N 2>&1 | head -20
# 预期: 1 个测试 entry (chipforge_tests)
echo "---"
ctest --output-on-failure 2>&1 | tail -50
# 预期: 1 test passed
```

- [ ] **Step E1.4: 直接运行 chipforge_tests 查看所有 TEST_CASE**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
./build-new/bin/chipforge_tests --list-tests 2>&1 | head -50
# 预期: 列出所有 TEST_CASE, 数量应 >= 47 (一些原文件有 1 main 含多个 static void)
./build-new/bin/chipforge_tests 2>&1 | tail -10
# 预期: "All tests passed" 或类似成功总结
```

- [ ] **Step E1.5: 按 family 标签验证**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
./build-new/bin/chipforge_tests "[framework]" 2>&1 | tail -5
./build-new/bin/chipforge_tests "[cache]" 2>&1 | tail -5
./build-new/bin/chipforge_tests "[cpu]" 2>&1 | tail -5
./build-new/bin/chipforge_tests "[mmu]" 2>&1 | tail -5
./build-new/bin/chipforge_tests "[soc]" 2>&1 | tail -5
./build-new/bin/chipforge_tests "[bundles]" 2>&1 | tail -5
# 预期: 每个 family 都有成功总结
```

#### Task E2: 清理与验证

- [ ] **Step E2.1: 验证「int main 数量 = 0」**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
echo "Remaining 'int main' in tests/ (excluding catch2/):"
grep -r "int main" tests/ --include="*.cpp" --exclude-dir=catch2 | wc -l
# 预期: 0
```

- [ ] **Step E2.2: 验证「assert 数量 = 0」**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
echo "Remaining 'assert' calls in tests/ (excluding catch2/):"
grep -rE "\bassert\s*\(" tests/ --include="*.cpp" --exclude-dir=catch2 | wc -l
# 预期: 0 (static_assert 不算)
```

- [ ] **Step E2.3: 验证「手写 #define CHECK = 0」**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
grep -rE "^\s*#\s*define\s+CHECK\s*\(" tests/ --include="*.cpp" --exclude-dir=catch2 | wc -l
# 预期: 0
```

- [ ] **Step E2.4: 验证 src/cf_plugin/CMakeLists.txt 无测试注册**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
grep -c "add_test" src/cf_plugin/CMakeLists.txt
grep -c "if(EXISTS.*tests/" src/cf_plugin/CMakeLists.txt
# 预期: 都是 0
```

- [ ] **Step E2.5: 验证 ctest 数量对比**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
NEW_COUNT=$(ctest -N 2>&1 | grep "Test #" | tail -1 | awk '{print $3}')
OLD_COUNT=$(cat /tmp/opencode/baseline_ctest_count.txt)
echo "Old ctest entries: $OLD_COUNT"
echo "New ctest entries: $NEW_COUNT"
# 预期: 新 = 1 (单二进制 chipforge_tests), 旧 = 47
# 注意: TEST_CASE 数量(从 --list-tests 看)应 >= 47
```

### Phase F: 清理与文档

#### Task F1: 文档更新

**Files:**
- Modify: `tests/README.md`
- Modify: `docs/architecture/testing-and-dse.md`
- Create: `docs/superpowers/plans/2026-06-30-catch2-migration-results.md`（实施完成报告）

- [ ] **Step F1.1: 更新 tests/README.md**

修改点:
- "状态" 行更新为 "✅ Catch2 迁移完成 (2026-06-30)"
- 删除"## 3. CMake 集成" 整节（已被 tests/CMakeLists.txt 替代）
- 新增 "## 3. 测试框架" 节:
  ```markdown
  ## 3. 测试框架

  - **Catch2 v3.7.0** (vendored amalgamated, 与 CppTLM/CppHDL 完全一致)
  - 单一可执行文件 `chipforge_tests` (位于 build/bin/)
  - 47 个原测试文件按 family tag 分组: `[framework]` `[cache]` `[cpu]` `[cpu-integration]` `[cpu-configs]` `[soc]` `[bundles]` `[mmu]`

  ### 运行方式
  ```bash
  # 全部
  ctest --output-on-failure

  # 单 family
  ./build/bin/chipforge_tests "[framework]"

  # 排除某 family
  ./build/bin/chipforge_tests "~[mmu]"

  # 列表
  ./build/bin/chipforge_tests --list-tests
  ```

  ### 新增测试
  1. 在 `tests/<family>/` 下创建 `test_<name>.cpp`
  2. 使用 TEST_CASE 模式（参考 tests/cache/test_replacement_policy.cpp）
  3. file(GLOB) 自动发现, 无需修改 CMake
  ```

- [ ] **Step F1.2: 更新 docs/architecture/testing-and-dse.md**

修改点:
- 替换"测试框架"章节, 引用 Catch2 vendored 模式
- 添加迁移记录

- [ ] **Step F1.3: 写实施完成报告**

创建 `docs/superpowers/plans/2026-06-30-catch2-migration-results.md`:
- 47/47 测试迁移成功
- CMake 测试注册从 82 行降至 12 行
- int main 数量从 42 降至 0
- assert 数量从 ~250 降至 0
- mmu/ GTest 配置破碎已修复
- 已知问题（如有）

- [ ] **Step F1.4: 提交 Phase F**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
git add tests/README.md docs/architecture/testing-and-dse.md docs/superpowers/plans/2026-06-30-catch2-migration-results.md
git commit -m "docs(tests): update README and architecture docs for Catch2 migration

- tests/README.md: new '测试框架' section with Catch2 usage
- docs/architecture/testing-and-dse.md: vendor Catch2 reference
- docs/superpowers/plans/2026-06-30-catch2-migration-results.md: migration summary"
```

#### Task F2: 清理迁移工具（可选）

- [ ] **Step F2.1: 决定保留还是删除 tools/migrate_to_catch2/**

**保留理由**: 未来新增测试或 catch2 升级时可参考转换模式
**删除理由**: 一次性脚本, 保持仓库干净

**建议**: 保留 6 个月（到 2026-12-30），到时根据情况删除。

- [ ] **Step F2.2: 在 tools/migrate_to_catch2/README.md 添加 "保留期限" 注释**

```markdown
> ⚠️ 此工具是 2026-06-30 Catch2 迁移的一次性脚本。
> 保留至 2026-12-30 供未来 catch2 升级或新测试迁移参考。
> 到期后请评估删除 (chore: remove migrate_to_catch2 toolchain)。
```

- [ ] **Step F2.3: 提交清理决策**

```bash
cd /workspace/project/ChipForge/.worktrees/testing-catch2
git add tools/migrate_to_catch2/README.md
git commit -m "chore(tests): add retention notice to Catch2 migration toolchain"
```

### Phase G: 集成回 main

#### Task G1: 用 finishing-a-development-branch skill 集成

- [ ] **Step G1.1: 加载 finishing-a-development-branch skill**

调用 skill: `superpowers/finishing-a-development-branch`

- [ ] **Step G1.2: 按 skill 指引选择集成方式**（merge / PR / cleanup）

- [ ] **Step G1.3: 执行集成**

- [ ] **Step G1.4: 清理 worktree**

```bash
cd /workspace/project/ChipForge
git worktree remove .worktrees/testing-catch2
git branch -d testing-catch2
```

---

## 3. 风险与缓解

| 风险 | 等级 | 缓解策略 |
|------|------|----------|
| 转换器对某些模式失效 | 🟡 中 | 1) 抽样测试后再批量; 2) 失败文件单独处理; 3) 保留原文件 git 历史可 revert |
| mmu/ GTest fixture 复杂 | 🟡 中 | Task D7 单独处理; 必要时人工改写 |
| cpu/integration 依赖 RISC-V 工具链 | 🟡 中 | 配置 BUILD_RTL=OFF 绕过 |
| vendor 文件意外修改 | 🟢 低 | tests/catch2/ 路径清晰, GLOB 排除 |
| 并行 agent 互相干扰 | 🟢 低 | 各 agent 修改不同目录, 无共享状态 |
| 总编译时间显著增加 | 🟢 低 | 单二进制反而减少链接开销 |
| catch2 v3.7.0 升级到 v4 破坏 ABI | 🟢 低 | 同步升级 CppTLM/CppHDL 时一起升 |

## 4. 回滚策略

```bash
# 如果整体回滚, 整个 testing-catch2 分支可以放弃
cd /workspace/project/ChipForge
git branch -D testing-catch2
git worktree remove .worktrees/testing-catch2 --force
# main 不受影响
```

## 5. 关键事实参考

- **Catch2 vendor 来源**: `/workspace/project/CppTLM/test/catch_amalgamated.{hpp,cpp}`
  - MD5: 33a2094513de12552236286bbac80a0e / 579ca0ed8a834dd271b978074c85accc
- **CppTLM 模式参考**: `/workspace/project/CppTLM/test/CMakeLists.txt` (44 行)
- **ChipForge 主 CMake**: `src/cf_plugin/CMakeLists.txt` 行 87-464 含 47 个测试注册
- **mmu/ GTest 配置**: `tests/mmu/CMakeLists.txt` (11 行, 含 GTest::gtest_main 但无 find_package)
- **worktree 路径**: `/workspace/project/ChipForge/.worktrees/testing-catch2/`
- **主分支**: `main`, 新分支: `testing-catch2`
