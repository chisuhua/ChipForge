#!/usr/bin/env python3
"""tasks.md 的 GFM 表格结构检查器（★ 本 change 的「表格结构」纪律配套工具）

检查两件事：
  ① **行 cell 数 == 表头 cell 数** —— 否则 GFM 会把 excess cell **静默丢弃**
     （★ 这正是 critic 十三/十四轮连续抓到的 BLOCKER-1 / BLOCKER-2）
  ② **表头必须在标题之下、数据行之间无空行** —— 否则整张表**不渲染成表格**

用法：python3 tools/check_md_tables.py <file.md>...
退出码：0 = 全部通过；1 = 有违例（打印 file:line + 原因）
"""
import re
import sys


def split_cells(line: str):
    """按 GFM 规则切分一行表格的 cell：分隔符 = 未被转义且不在 code span 内的 '|'。"""
    cells = []
    cur = []
    in_code = False
    i = 0
    while i < len(line):
        ch = line[i]
        if ch == '\\' and i + 1 < len(line) and line[i + 1] == '|':
            cur.append('|')          # 转义管道 ⇒ 字面量
            i += 2
            continue
        if ch == '`':
            in_code = not in_code
            cur.append(ch)
            i += 1
            continue
        if ch == '|' and not in_code:
            cells.append(''.join(cur))
            cur = []
            i += 1
            continue
        cur.append(ch)
        i += 1
    cells.append(''.join(cur))
    return cells


def unescaped_pipes_in_code(line: str):
    """返回「code span 内未转义的 '|'」个数。

    ★ GFM 规范：表格 cell 内的 pipe **即使在 code span 内也必须转义**，
      否则它就是 cell 分隔符（⇒ 该行的 cell 数被撑大 ⇒ excess cell 被**静默丢弃**）。
      —— 这正是 critic 十四轮 BLOCKER-1（S3-a 行）的成因。
    """
    n = 0
    in_code = False
    i = 0
    while i < len(line):
        ch = line[i]
        if ch == chr(92) and i + 1 < len(line):
            i += 2
            continue
        if ch == '`':
            in_code = not in_code
            i += 1
            continue
        if ch == '|' and in_code:
            n += 1
        i += 1
    return n


def is_row(line: str) -> bool:
    s = line.strip()
    return s.startswith('|') and s.endswith('|') and len(s) > 1


def check(path: str):
    problems = []
    lines = open(path, encoding='utf-8').read().split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        if not is_row(line):
            i += 1
            continue
        # 找到表头行 + 分隔行
        if i + 1 >= len(lines) or not is_row(lines[i + 1]) or \
           not re.fullmatch(r'\|[\s:|-]+\|', lines[i + 1].strip()):
            i += 1
            continue
        header = lines[i]
        sep = lines[i + 1]
        n_head = len(split_cells(header)) - 2      # 去掉首尾两个空 cell
        n_sep = len(split_cells(sep)) - 2
        if n_sep != n_head:
            problems.append((i + 1, f'分隔行 cell 数 {n_sep} != 表头 {n_head}'))
        j = i + 2
        while j < len(lines) and is_row(lines[j]):
            n = len(split_cells(lines[j])) - 2
            if n != n_head:
                problems.append((j + 1, f'数据行 cell 数 {n} != 表头 {n_head}'))
            bad = unescaped_pipes_in_code(lines[j])
            if bad:
                problems.append(
                    (j + 1, f'code span 内有 {bad} 个未转义的管道符 ⇒ GFM 会当 cell 分隔符 '
                            f'(★ MUST 转义成 \\|)'))
            j += 1
        # 数据行之后紧跟非表格行 ⇒ 表格在渲染时被断开（空行也算断开）
        if j < len(lines) and lines[j].strip() == '' and j + 1 < len(lines) \
           and is_row(lines[j + 1]):
            problems.append((j + 1, '表格中间出现空行 ⇒ GFM 渲染时断表'))
        i = j
    return problems


def main(argv):
    rc = 0
    for path in argv[1:]:
        probs = check(path)
        if probs:
            rc = 1
            for ln, why in probs:
                print(f'{path}:{ln}: FAIL: {why}')
        else:
            print(f'{path}: OK (所有表格行 cell 数 == 表头 cell 数)')
    return rc


if __name__ == '__main__':
    sys.exit(main(sys.argv))
