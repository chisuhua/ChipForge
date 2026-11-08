#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""门禁① 顶层纯净 lint —— 实施态时点语汇不得进 `docs/architecture/` 顶层。

治理依据：`docs/architecture/adr/ADR-095-architecture-truth-boundary-and-dir-governance.md` §2.5①
change：`architecture-truth-boundary-enforcement`（AC-110 / AC-113 / AC-119）

★ ★ 判据 = **词表** + **语法上下文豁免**（缺一则门禁不可用）
  - 只有词表 ⇒ Day-1 大面积误伤（实测原始 grep 201 处，其中真正的实施态断言 < 10）
  - 只有豁免 ⇒ 门禁为空

★ ★ **词表冻结在本文件内**（单一真源 · 禁运行时读外部文件 · 禁 CLI 传入）
  变更 MUST 走 PR + 审查（否则「随便加词」会让门禁逐步失效）。

★ ★ **禁按路径豁免**（ADR-095 §2.5①）：豁免一律由**语法位置**判定，不看文件路径。
  唯一的内容锚定豁免是 (h) —— 文档**自标注**「非目标态」的节，即文档自己声明该节是实施态
  （`hierarchical-plugin-architecture.md` §1 依 ADR-094 M-8 保留实施快照区）。

★ 范围：`docs/architecture/*.md`（**顶层，非递归**）。
  ★ `adr/` 不在范围内（2026-11-08 用户裁决）：ADR 是历史决策记录，必然含决策时点的状态
  （「当前实现 = RV32IM」「当前状态：0/9 满足」）⇒ 禁掉等于禁掉决策依据。`adr/` 由门禁③（孤儿检查）
  与 Status frontmatter 治理。`mixed/` `archive/` 同理豁免（非真相源）。

退出码：0 = 0 命中 · 1 = 有命中（报红）· 2 = usage / IO 错误
"""

import argparse
import os
import re
import sys

# ★★★ 词表（冻结 · 单一真源）★★★
# 2026-11-08 收窄（用户裁决 · 实测依据）：
#   - 移除 `PASS`：它是 S-契约定义的 verdict 取值（`0 = PASS` · `verdict`(PASS|FAIL|ERROR)）
#     + 里程碑表的测试结果 ⇒ 实测贡献 89/201 命中但**零**实施态语义。
#   - 移除 `是 POD`：POD 是类型表示属性（设计不变量），且子串会命中否定句
#     「`ch_uint` 在 TLM 编译下**不是 POD**」（ADR-092:51）⇒ 语义相反。
WORDS = (
    "当前实际状态",
    "当前状态",
    "当前实现",
    "已落地",
    "已完成",
    "已实现",
)
# 「Phase N 完成」是模板化表述（N 可为 6c / 6d.5 等）⇒ 用正则而非字面词
PHASE_COMPLETE_RE = re.compile(r"Phase\s*\d+[a-z]?(?:\.\d+)?\s*完成")

# 状态 emoji（表格状态列的判据）
STATUS_EMOJI = ("\u2705", "\U0001F6A7", "\u26A0\uFE0F", "\u23F3")  # ✅ 🚧 ⚠️ ⏳

# 自标注「非目标态」的节（豁免 h）
NON_TARGET_MARKER = "非目标态"

# frontmatter 只认「文件第 1 行是 ---」，且在这么多行内闭合
FRONTMATTER_MAX_LINES = 20

DATE_RE = re.compile(r"\d{4}-\d{2}-\d{2}")

# ---- 门禁④（AC-113）：顶层引用 mixed/ archive/ 时 MUST 标明「非权威 · 仅作背景」----
NON_AUTHORITATIVE_MARK = "非权威"
SUB_DIRS = ("mixed", "archive")
ARCH_BASE = os.path.join("docs", "architecture")

# ★ ★ 顶层布局白名单（critic 2026-11-08 指出的可复现规避手法）：
#   `mkdir docs/architecture/notes && echo "当前状态：已完成" > .../foo.md` 即可绕过门禁①
#   （门禁只扫顶层 `*.md` 非递归，且门禁②只查链接）
#   ⇒ 顶层目录集合 MUST 冻结；新增子目录属**治理变更**，须先改 ADR-095 §2.1 再改本白名单。
ALLOWED_TOP_DIRS = ("adr", "archive", "mixed", "plugin-research")
MD_LINK_RE = re.compile(r"\]\(([^)\s]+)\)")
TICK_RE = re.compile(r"`([^`\n]+)`")
HEADING_RE = re.compile(r"^(#{1,6})\s")
TABLE_SEP_RE = re.compile(r"^\|[\s:|-]+\|?$")
BARE_DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")


class Finding:
    __slots__ = ("line", "col", "word", "text")

    def __init__(self, line, col, word, text):
        self.line = line
        self.col = col
        self.word = word
        self.text = text


class Exemption:
    __slots__ = ("line", "rule", "word")

    def __init__(self, line, rule, word):
        self.line = line
        self.rule = rule
        self.word = word


def _find_hits(text):
    """返回 [(col, word)] —— 长词优先，避免同一处重复计（当前实际状态 含 当前状态）。"""
    out = []
    consumed = []
    for w in WORDS:  # WORDS 已按长度降序排列
        start = 0
        while True:
            i = text.find(w, start)
            if i < 0:
                break
            if not any(lo <= i < hi for lo, hi in consumed):
                out.append((i, w))
                consumed.append((i, i + len(w)))
            start = i + len(w)
    for m in PHASE_COMPLETE_RE.finditer(text):
        if not any(lo <= m.start() < hi for lo, hi in consumed):
            out.append((m.start(), m.group(0)))
            consumed.append((m.start(), m.end()))
    out.sort()
    return out


def _split_cells(line):
    s = line.strip()
    if s.startswith("|"):
        s = s[1:]
    if s.endswith("|"):
        s = s[:-1]
    return [c.strip() for c in s.split("|")]


def _frontmatter_end(lines):
    """只有第 1 行是 `---` 才算 frontmatter；否则 `---` 是水平分隔线。

    ★ 实测教训（2026-11-08）：把任意 `---` 当 frontmatter 开关，会把 104 处真实命中
      误判为「frontmatter 豁免」⇒ 门禁假绿。
    """
    if not lines or lines[0].strip() != "---":
        return 0
    for j in range(1, min(len(lines), FRONTMATTER_MAX_LINES)):
        if lines[j].strip() == "---":
            return j + 1
    return 0


def scan_file(path):
    """扫单个文件 ⇒ (findings, exemptions)。"""
    with open(path, encoding="utf-8", errors="replace") as fh:
        lines = fh.read().splitlines()

    front_end = _frontmatter_end(lines)
    in_fence = False
    in_comment = False
    nontarget_level = 0  # >0 ⇒ 当前处于自标注「非目标态」的节内，值为该节标题层级
    findings = []
    exemptions = []

    for idx, line in enumerate(lines):
        lineno = idx + 1
        s = line.strip()

        if front_end and lineno <= front_end:
            # frontmatter 内的字段是元数据，不是正文断言 ⇒ 记为豁免（供 evidence）
            for _col, word in _find_hits(s):
                exemptions.append(Exemption(lineno, "a-frontmatter", word))
            continue

        # ---- 代码围栏（★ 围栏内的一切都不是 markdown 结构，必须先于标题解析）----
        if s.startswith("```") or s.startswith("~~~"):
            in_fence = not in_fence
            continue
        if in_fence:
            for _col, word in _find_hits(s):
                exemptions.append(Exemption(lineno, "e-fence", word))
            continue

        # ---- HTML 注释块（同上：注释里的 `# xxx` 不是标题）----
        if in_comment:
            for _col, word in _find_hits(s):
                exemptions.append(Exemption(lineno, "c-html-comment", word))
            if "-->" in line:
                in_comment = False
            continue
        if "<!--" in line and "-->" not in line:
            in_comment = True
            continue

        # ---- 节的开合（★ 仅在围栏/注释之外解析标题）----
        #   ★ ★ oracle 2026-11-08 指出的 BLOCKER：原实现把标题检测放在围栏守卫**之前**
        #     ⇒ 代码示例/注释里出现 `# 非目标态` 就会**静默开豁免**（其下正文免检到下一个同级标题）
        #     且节内围栏里的 `# xxx` 会**提前关闭**豁免（假红）。两条都是假绿/假红通道。
        he = HEADING_RE.match(s)
        if he:
            level = len(he.group(1))
            if nontarget_level and level <= nontarget_level:
                nontarget_level = 0
            # ★ 豁免 (h) 收紧：标题 MUST 同时含日期（与 ADR-094 M-8 的「dated 快照」对齐）
            #   ⇒ 把滥用门槛从「一个魔法词」抬到「魔法词 + 可见 aging（过期节一眼可辨）」
            if NON_TARGET_MARKER in s and DATE_RE.search(s):
                nontarget_level = level

        hits = _find_hits(s)
        if not hits:
            continue

        # ---- 豁免判定（顺序即优先级）----
        if nontarget_level:
            # ★ 文档自标注该节为「非目标态」⇒ 它自己声明了这是实施态区
            exemptions.append(Exemption(lineno, "h-self-declared-nontarget", hits[0][1]))
            continue
        if s.startswith("|"):
            nxt = lines[idx + 1].strip() if idx + 1 < len(lines) else ""
            # ★ 表头判据：**该行的下一行**是 `|---|` 分隔行（标准 markdown 表头）
            #   ★ 不扩到「两行表头」：那会误豁免「连续两表」的第 2 张表的首个数据行；
            #     且多行表头属**不规范写法**，此时报红是 fail-loud（可见、可改），不是假绿。
            if TABLE_SEP_RE.match(nxt):
                exemptions.append(Exemption(lineno, "f-table-header", hits[0][1]))
                continue
            cells = _split_cells(s)
            hit_words = [w for _, w in hits]
            hit_idx = [n for n, c in enumerate(cells) if any(w in c for w in hit_words)]
            if hit_idx and all(cells[n].startswith(STATUS_EMOJI) for n in hit_idx):
                exemptions.append(Exemption(lineno, "b-status-cell", hits[0][1]))
                continue
            if cells and BARE_DATE_RE.match(cells[0]):
                exemptions.append(Exemption(lineno, "g-dated-log-row", hits[0][1]))
                continue
        elif s.startswith("> **") and DATE_RE.search(line):
            exemptions.append(Exemption(lineno, "d-dated-banner", hits[0][1]))
            continue

        for col, word in hits:
            findings.append(Finding(lineno, col, word, s))

    return findings, exemptions


def default_targets(root):
    """顶层 `docs/architecture/*.md` —— 非递归（子目录不属真相源）。"""
    base = os.path.join(root, "docs", "architecture")
    if not os.path.isdir(base):
        return None
    return [os.path.join("docs", "architecture", n)
            for n in sorted(os.listdir(base))
            if n.endswith(".md") and os.path.isfile(os.path.join(base, n))]


def resolve_ref(raw, root):
    """把文档里的引用解析成绝对路径；不可解析（外链/锚点/空）⇒ None。"""
    p = raw.split("#")[0].strip()
    if not p or p.startswith(("http://", "https://", "mailto:")):
        return None
    if p.startswith("docs/architecture/"):
        # 仓库相对写法（反引号里常见）
        return os.path.normpath(os.path.join(root, p))
    return os.path.normpath(os.path.join(root, ARCH_BASE, p))


def check_top_level_layout(root):
    """门禁① 的布局守卫：`docs/architecture/` 顶层只许既有 4 个子目录 + `.md` 文件。

    ★ 为什么是**判据**而不是提示：只扫 `*.md`（非递归）时，新增一个子目录就是**零成本规避**。
      目录位置本身就是 ADR-095 §2.1 的治理对象 ⇒ 出现白名单外的条目 MUST 报红。
    """
    base = os.path.join(root, ARCH_BASE)
    problems = []
    if not os.path.isdir(base):
        return problems
    for name in sorted(os.listdir(base)):
        full = os.path.join(base, name)
        if os.path.isdir(full):
            if name not in ALLOWED_TOP_DIRS:
                problems.append(("unexpected-dir", name,
                                 "顶层子目录不在白名单 " + str(list(ALLOWED_TOP_DIRS))
                                 + "（新增子目录须先改 ADR-095 §2.1 与本白名单）"))
        elif not name.endswith(".md"):
            problems.append(("unexpected-file", name,
                             "顶层只放 markdown 真相源（其它类型文件不属真相源）"))
    return problems


def scan_subdir_refs(path, root):
    """门禁④：顶层文档引用 `mixed/` `archive/` 下的**文件**时，同行 MUST 出现「非权威」。

    ★ 只认**文件**（`.md` 结尾），不认「仅命名目录」（如分区表里的 `mixed/`）——
      命名目录不是引用某份文档，要求标「非权威」纯属噪声。
    """
    find = []
    sub_roots = [os.path.join(root, ARCH_BASE, d) for d in SUB_DIRS]
    with open(path, encoding="utf-8", errors="replace") as fh:
        for lineno, line in enumerate(fh.read().splitlines(), 1):
            if NON_AUTHORITATIVE_MARK in line:
                continue
            refs = []
            for raw in MD_LINK_RE.findall(line) + TICK_RE.findall(line):
                target = resolve_ref(raw, root)
                if not target or not target.endswith(".md"):
                    continue
                if any(target == sr or target.startswith(sr + os.sep) for sr in sub_roots):
                    refs.append(raw)
            if refs:
                find.append((lineno, refs, line.strip()))
    return find


# --------------------------------------------------------------------------
# self-test：4 类必备豁免 + 附加语法豁免 + 1 个真阳性
# --------------------------------------------------------------------------
FIXTURE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "fixtures", "arch_truth")

# (fixture 文件名, 期望 findings 数, 期望命中的豁免规则或 None)
# 门禁④ fixture（顶层引用子目录须标「非权威」）
FIXTURES4 = (
    ("i_subdir_ref_unmarked.md", 2),
    ("j_subdir_ref_marked.md", 0),
)

FIXTURES = (
    ("a_frontmatter.md", 0, "a-frontmatter"),
    ("b_status_cell.md", 0, "b-status-cell"),
    ("c_html_comment.md", 0, "c-html-comment"),
    ("d_dated_banner.md", 0, "d-dated-banner"),
    ("e_fence.md", 0, "e-fence"),
    ("f_table_header.md", 0, "f-table-header"),
    ("g_dated_log_row.md", 0, "g-dated-log-row"),
    ("h_self_declared_nontarget.md", 1, "h-self-declared-nontarget"),
    # ★ v1.1 收紧后的回归 fixture（oracle 2026-11-08 报的 BLOCKER + 两项建议）：
    ("k_fence_fake_heading.md", 2, None),          # 围栏里的「# 非目标态」不得开豁免
    ("m_nontarget_no_date.md", 1, "h-self-declared-nontarget"),  # 无日期不豁免 · 有日期才豁免
    ("z_true_positive.md", 5, None),
)


def self_test():
    failures = []
    for name, want_findings, want_rule in FIXTURES:
        path = os.path.join(FIXTURE_DIR, name)
        if not os.path.isfile(path):
            failures.append(f"{name}: fixture 缺失（{path}）")
            continue
        findings, exemptions = scan_file(path)
        rules = {e.rule for e in exemptions}
        ok = len(findings) == want_findings
        if ok and want_rule:
            ok = want_rule in rules
        status = "PASS" if ok else "FAIL"
        detail = (f"findings={len(findings)}/{want_findings} "
                  f"rules={sorted(rules) if rules else '-'}")
        if want_rule and want_rule not in rules:
            detail += f"  ← 期望命中豁免 '{want_rule}'"
        print(f"[{status}] {name:<34} ({detail})")
        if not ok:
            failures.append(f"{name}: {detail}")

    # ---- 门禁④ fixture ----
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for name, want in FIXTURES4:
        path = os.path.join(FIXTURE_DIR, name)
        if not os.path.isfile(path):
            failures.append(f"{name}: fixture 缺失（{path}）")
            continue
        got = len(scan_subdir_refs(path, root))
        ok = got == want
        print(f"[{'PASS' if ok else 'FAIL'}] {name:<34} (subdir_refs={got}/{want})")
        if not ok:
            failures.append(f"{name}: subdir_refs={got}/{want}")

    print()
    if failures:
        print(f"self-test: {len(failures)} FAILED")
        for f in failures:
            print(f"  - {f}")
        return 1
    print(f"self-test: {len(FIXTURES)}/{len(FIXTURES)} passed")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="门禁① 顶层纯净 lint（docs/architecture/*.md · 非递归）")
    ap.add_argument("--root", default=".", help="仓库根（默认当前目录）")
    ap.add_argument("--self-test", action="store_true", help="跑 fixture 自检")
    ap.add_argument("--show-exempt", action="store_true", help="列出命中的豁免（证据用）")
    ap.add_argument("--quiet", action="store_true", help="只输出汇总")
    opts = ap.parse_args(argv)

    if opts.self_test:
        return self_test()

    root = os.path.abspath(opts.root)
    targets = default_targets(root)
    if targets is None:
        print(f"ERROR: 找不到 {os.path.join(root, 'docs', 'architecture')}", file=sys.stderr)
        return 2
    if not targets:
        print("ERROR: 顶层无 markdown 文件（范围异常 ⇒ 拒绝假绿）", file=sys.stderr)
        return 2

    total = 0
    all_ex = []
    if not opts.quiet:
        print(f"=== 门禁① 扫描 {len(targets)} 份顶层文档（词表 {len(WORDS)} 词 + Phase-N 正则）===")
    for rel in targets:
        findings, exemptions = scan_file(os.path.join(root, rel))
        all_ex.extend((rel, e) for e in exemptions)
        total += len(findings)
        for f in findings:
            print(f"RED  {rel}:{f.line}  [{f.word}]  {f.text[:110]}")

    if opts.show_exempt:
        print()
        print(f"=== 豁免明细（{len(all_ex)} 处）===")
        for rel, e in all_ex:
            print(f"EXEMPT  {rel}:{e.line}  ({e.rule})  [{e.word}]")

    # ---- 门禁① 布局守卫：顶层目录/文件白名单（防「新建子目录绕过」）----
    layout = check_top_level_layout(root)
    for kind, name, why in layout:
        print(f"RED-LAYOUT  {os.path.join(ARCH_BASE, name)}  [{kind}]  {why}")

    # ---- 门禁④：顶层引用 mixed/ archive/ 须标「非权威 · 仅作背景」----
    subdir_total = 0
    if not opts.quiet:
        print()
    for rel in targets:
        for lineno, refs, text in scan_subdir_refs(os.path.join(root, rel), root):
            subdir_total += 1
            print(f"RED-SUBDIR  {rel}:{lineno}  {refs}  {text[:100]}")

    print()
    if total or subdir_total or layout:
        if total:
            print(f"门禁① 结果: {total} 处实施态时点语汇（顶层须为 0）")
            print("  ⇒ 顶层只放真相源（ADR-095 §2.3①）：把「现在长什么样」移出顶层，"
                  "或改为形态定性（「该长什么样」）")
        if layout:
            print(f"门禁①（布局）结果: {len(layout)} 处顶层条目不在白名单（须为 0）")
            print("  ⇒ 顶层位置本身是治理对象（ADR-095 §2.1）：新增子目录 = 零成本规避门禁①")
        if subdir_total:
            print(f"门禁④ 结果: {subdir_total} 处引用 mixed//archive/ 未标「非权威」（须为 0）")
            print("  ⇒ 顶层引用子目录文档时 MUST 标明「★ 非权威 · 仅作背景」（ADR-095 §2.5④）")
        return 1
    print("门禁① 结果: 0 命中 ✓（顶层无实施态时点语汇）")
    print("门禁④ 结果: 0 命中 ✓（子目录引用均已标「非权威」）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
