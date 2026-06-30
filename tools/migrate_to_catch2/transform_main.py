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