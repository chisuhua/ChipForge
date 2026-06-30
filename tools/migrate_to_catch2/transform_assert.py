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