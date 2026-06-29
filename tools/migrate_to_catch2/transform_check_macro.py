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
    r"""从 start (宏体第一个字符) 找到宏体的结束位置.

    策略: 跨过整个 do { ... } while(0) 或 单个 { ... } 块,
          或找到行尾 (\) 续行符链的末尾."""
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
            if src[i:i+5] == 'while':
                i += 5
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