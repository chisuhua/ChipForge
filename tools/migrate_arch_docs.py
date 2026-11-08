#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""docs/architecture 迁移的引用改写器 —— change: architecture-truth-boundary。

★ 用途（AC-102 / AC-104 / AC-116 / AC-117）：
  1. `git mv` 一批文档到新位置
  2. 改写**全仓 md** 中指向它们的链接（含**被移动文档自身**的内部相对链接）
  3. 逐条报告改写结果，供 evidence 存档

★ ★ 为什么需要「先记录、后改写」两阶段：
  文件移动后，其内部 `./xxx` 的相对基准**变了**（旧目录 → 新目录）
  ⇒ 直接对移动后的文件做「绝对路径解析 + 改写成新相对路径」会把同批同目录的链接改坏
  ⇒ 因此 MUST 在移动**前**记录每条链接的「旧绝对目标」，移动后再按新基准重算。

★ 边界：
  - 只改 markdown 链接 `[text](target)`；★ 不改反引号路径（除根级文档外不在 doc_link_check 范围）
  - 跳过 `.git/` · `build/` · `openspec/changes/archive/`（历史快照，★ 不篡改）
  - 保留 `#fragment` 与 `<...>` 包裹形式

★ 用法：
  python3 tools/migrate_arch_docs.py --batch 1 --dry-run     # 只看计划
  python3 tools/migrate_arch_docs.py --batch 1               # 执行（含 git mv）
"""

import argparse
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ★ 批次定义（★ 判据 = 弱连通分量：★ 任何 A→B（两者都移动）MUST 同批，否则中间态断链）
BATCHES = {
    1: {  # 6 份 · mixed/ 的 5 份 + archive/ 的 performance-guide（跨目录环）
        "docs/architecture/plugin-framework.md": "docs/architecture/mixed/plugin-framework.md",
        "docs/architecture/declarative-hybrid-framework.md": "docs/architecture/mixed/declarative-hybrid-framework.md",
        "docs/architecture/code-framework-mapping.md": "docs/architecture/mixed/code-framework-mapping.md",
        "docs/architecture/tech-selection.md": "docs/architecture/mixed/tech-selection.md",
        "docs/architecture/testing-and-dse.md": "docs/architecture/mixed/testing-and-dse.md",
        "docs/architecture/performance-guide.md": "docs/architecture/archive/performance-guide.md",
    },
    2: {"docs/architecture/tlm-coverage-parity-ledger.md": "docs/architecture/archive/tlm-coverage-parity-ledger.md"},
    3: {"docs/architecture/tlm-retirement-b1-capacity-plan.md": "docs/architecture/archive/tlm-retirement-b1-capacity-plan.md"},
    4: {"docs/architecture/m4-dse-cpufactory-real-pr-description.md": "docs/architecture/archive/m4-dse-cpufactory-real-pr-description.md"},
    5: {"docs/architecture/ip-catalog.md": "ip/ip-catalog.md"},
}

SKIP_DIR_PREFIXES = (
    ".git/",
    "build/",
    ".rddf/wt/",
    "openspec/changes/archive/",
    # ★ 第三方 vendored / 依赖树 —— ★ 不属本仓文档，MUST NOT 改写
    "sail-riscv-Linux-x86_64/",
    "CppTLM/",
    "CppHDL/",
    "3rdparty/",
    "node_modules/",
)

LINK_RE = re.compile(r"(\[[^\]]*\]\()([^)\s]+)(\))")


def iter_md_files():
    """★ 全仓 md（★ 跳过 SKIP_DIR_PREFIXES）。"""
    for dirpath, dirnames, filenames in os.walk(REPO):
        rel_dir = os.path.relpath(dirpath, REPO)
        if rel_dir == ".":
            rel_dir = ""
        if any((rel_dir + "/").startswith(p) or rel_dir.startswith(p.rstrip("/")) for p in SKIP_DIR_PREFIXES):
            dirnames[:] = []
            continue
        dirnames[:] = [d for d in dirnames if d not in (".git", "build", "node_modules")]
        for fn in filenames:
            if fn.endswith(".md"):
                yield os.path.join(rel_dir, fn) if rel_dir else fn


def split_target(target):
    """★ 拆出 (path, suffix)：suffix 含 `#fragment`。"""
    if "#" in target:
        path, frag = target.split("#", 1)
        return path, "#" + frag
    return target, ""


def scan(moves):
    """★ 阶段 A：记录每条链接的「旧绝对目标」。

    返回 {file_rel: [(line_idx, raw_target, abs_target_old), ...]}
    """
    rec = {}
    for f in iter_md_files():
        try:
            text = open(os.path.join(REPO, f), encoding="utf-8").read()
        except (OSError, UnicodeDecodeError):
            continue
        entries = []
        for m in LINK_RE.finditer(text):
            raw = m.group(2)
            if raw.startswith(("http://", "https://", "mailto:", "#")):
                continue
            path, _ = split_target(raw)
            if not path:
                continue
            abs_old = os.path.normpath(os.path.join(REPO, os.path.dirname(f), path))
            entries.append((m.start(2), raw, abs_old))
        if entries:
            rec[f] = entries
    return rec


def rewrite(rec, moves):
    """★ 阶段 C：按新基准重算相对路径并改写。返回 (changed_files, 日志行)。"""
    # 反向映射：新绝对路径 → 旧绝对路径（用于识别「已改写」意图）
    changed = []
    log = []
    for f, entries in rec.items():
        # ★ 文件移动后的实际位置
        #   ★ 注意：rec 的 key 是**仓库相对路径**，而 moves 的 key 是**绝对路径**
        #   ⇒ ★ 必须先归一（2026-11-08 实测 bug：直接 moves.get(f) 恒 None ⇒ 文件内部链接从未被改）
        f_abs = os.path.normpath(os.path.join(REPO, f))
        file_moved = f_abs in moves
        new_f = os.path.relpath(moves[f_abs], REPO) if file_moved else f
        base = os.path.join(REPO, os.path.dirname(new_f))
        src = os.path.join(REPO, new_f)
        if not os.path.isfile(src):
            continue
        text = open(src, encoding="utf-8").read()
        # ★ 从后往前替换，避免偏移失效
        pieces = []
        for start, raw, abs_old in entries:
            path, suffix = split_target(raw)
            target_moved = abs_old in moves
            # ★ ★ 只在「目标移动」或「文件自身移动」时才重算相对路径。
            #   ★ 否则 os.path.relpath 会把 `dir/` 规范化成 `dir`、`./x` 规范成 `x`
            #   ⇒ ★ **误改无关链接**（★ 2026-11-08 实测：52 个文件被误改，★ 已回滚）。
            if not (file_moved or target_moved):
                continue
            # ★ 目录链接：★ 保留尾部 `/`（relpath 会去掉）
            trailing_slash = path.endswith("/")
            abs_new = moves.get(abs_old, abs_old)
            new_rel = os.path.relpath(abs_new, base)
            if trailing_slash and not new_rel.endswith("/"):
                new_rel += "/"
            new_target = new_rel + suffix
            if new_target == raw:
                continue
            pieces.append((start, start + len(raw), new_target))
            log.append(f"  {f}: {raw}  ->  {new_target}")
        if not pieces:
            continue
        for start, end, newt in sorted(pieces, reverse=True):
            text = text[:start] + newt + text[end:]
        with open(src, "w", encoding="utf-8") as fh:
            fh.write(text)
        changed.append(f)
    return changed, log


def main(argv=None):
    ap = argparse.ArgumentParser(description="docs/architecture 迁移的引用改写器")
    ap.add_argument("--batch", type=int, required=True, choices=sorted(BATCHES))
    ap.add_argument("--dry-run", action="store_true")
    opts = ap.parse_args(argv)

    moves = {os.path.normpath(os.path.join(REPO, k)): os.path.normpath(os.path.join(REPO, v))
             for k, v in BATCHES[opts.batch].items()}

    print(f"=== batch {opts.batch}: {len(moves)} 份 ===")
    for k, v in BATCHES[opts.batch].items():
        print(f"  {k}\n    -> {v}")

    rec = scan(moves)
    print(f"\n扫描到 {len(rec)} 个含链接的 md 文件")

    if opts.dry_run:
        # ★ dry-run：★ 只计算「哪些链接会指向被移动的文件」，**绝不写文件**
        n = 0
        for f, entries in rec.items():
            f_abs = os.path.normpath(os.path.join(REPO, f))
            file_moved = f_abs in moves
            for _start, raw, abs_old in entries:
                if file_moved or abs_old in moves:
                    print(f"  {f}: {raw}  →  (会被改写)")
                    n += 1
        print(f"\n[dry-run] 预计改写 {n} 处；未执行 git mv、未写任何文件")
        return 0

    # ---- 阶段 B：git mv ----
    for old, new in BATCHES[opts.batch].items():
        os.makedirs(os.path.join(REPO, os.path.dirname(new)), exist_ok=True)
        r = subprocess.run(["git", "mv", old, new], cwd=REPO, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"ERROR: git mv {old} -> {new} failed: {r.stderr}", file=sys.stderr)
            return 1
        print(f"  git mv {old} -> {new}")

    # ---- 阶段 C：改写（★ rec 的 key 仍是旧路径 ⇒ 用 moves 映射到新位置）----
    # ★ rec 里的「旧绝对目标」需要重算：scan 记录的 abs_old 是旧绝对路径，
    #   而 moves 的 key 也是旧绝对路径 ⇒ 直接可用。
    changed, log = rewrite(rec, moves)
    print(f"\n改写了 {len(changed)} 个文件 · {len(log)} 处链接：")
    for line in log:
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
