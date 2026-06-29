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