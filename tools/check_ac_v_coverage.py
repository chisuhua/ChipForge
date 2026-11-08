#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""AC ↔ V（验证项）覆盖校验 —— AC-115a 静态规则 + AC-115b 并集校验。

change: `architecture-truth-boundary-enforcement`（AC-115a / AC-115b）

★ 为什么需要这个脚本：
  治理目标被拆成 2 个 change（L1-A + L1-B）后，**单个 change 的 AC 集合必然只是
  V 全集的真子集** ⇒ 「V 全覆盖」只能按**并集**判定。手工宣称「都覆盖了」不可核，
  故把判据脚本化（AC-115b 明确要求 `tools/check_ac_v_coverage.py`）。

★ 三条判据（AC-115a/b）：
  ① **每条 AC 必须带 `覆盖 V:` 标签**（无标签 ⇒ 无法核销 ⇒ `no-v-tag`）
  ② **每个 V 至少被 1 条 AC 覆盖**，且**并集 == V-101~V-110 全集**（`orphan-v`）
  ③ **同一 V 被多条 AC 覆盖 ⇒ 必须是显式允许的「交叉覆盖」**
     （白名单 `CROSS_ALLOWED` 冻结在脚本内，每条带理由；不在白名单 ⇒ `duplicate-assignment`）

★ `覆盖 V: 全部（本条自豁免于并集计算）` 的 AC（如 AC-115a/115b）记作 self-exempt：
  它们声明的是规则本身，不参与 V 计数。

退出码：0 = 全绿 · 1 = 有违规 · 2 = 输入/解析错误
"""

import argparse
import os
import re
import sys

# ★ 被校验的 change（拆批的两份 —— 这是「并集」的边界）
CHANGES = (
    "architecture-truth-boundary",
    "architecture-truth-boundary-enforcement",
)

# ★ V 全集（AC-115b① 的判据基准）
V_UNIVERSE = tuple(f"V-{n}" for n in range(101, 111))

# ★ 允许「交叉覆盖」的 V ⇒ 理由（不在表内的 V 被多条 AC 覆盖即判违规）
CROSS_ALLOWED = {
    "V-101": "顶层结构（边界/导航/非权威标注）由 3 个不同落点的 AC 共同承载",
    "V-103": "实施态语汇的**判定装置**与**内容净化**是两件事，分别由门禁与净化 AC 承载",
    "V-104": "引用完整性的「入检 / 修正 / 豁免 / 段头可提取」是四个独立动作",
    "V-106": "根级文档同步与 IP 索引归属分别是两份文件的交付",
    "V-107": "迁移工具的机械改写与人工复核（内部链接）分别可验",
    "V-109": "迁移分批执行与归档/索引的最终位置是不同时点的判据",
    "V-110": "可回滚性由「每批单 commit」与「实测 revert」两条独立证据支撑",
}

HDR_RE = re.compile(r"^##\s+(?:验收标准|Acceptance Criteria)\s*$")
BULLET_RE = re.compile(r"^-\s+(?:\[[ xX]\]\s+)?\*\*(AC-[0-9A-Za-z]+)\*\*(.*)$")
# `覆盖 V:` 之后到「（」/「）」/「。」为止的那一段才是标签本体
TAG_RE = re.compile(r"覆盖\s*V\s*[:：]\s*([^（()）。\n]*)")
V_RE = re.compile(r"V-\d+")
SELF_EXEMPT_HINT = "自豁免"


class AC:
    __slots__ = ("change", "ac_id", "v_ids", "self_exempt", "line")

    def __init__(self, change, ac_id, v_ids, self_exempt, line):
        self.change = change
        self.ac_id = ac_id
        self.v_ids = v_ids
        self.self_exempt = self_exempt
        self.line = line


def parse_change(proposal_path, change):
    """从 proposal.md 的 AC 段提取 [(AC, ...)]。"""
    with open(proposal_path, encoding="utf-8", errors="replace") as fh:
        lines = fh.read().splitlines()
    idx = next((i for i, l in enumerate(lines) if HDR_RE.match(l)), None)
    if idx is None:
        raise ValueError(f"{proposal_path}: 无 AC 段头（^## Acceptance Criteria$）")

    out = []
    for n in range(idx + 1, len(lines)):
        line = lines[n]
        if line.startswith("## "):
            break
        m = BULLET_RE.match(line.strip())
        if not m:
            continue
        ac_id, rest = m.group(1), m.group(2)
        tm = TAG_RE.search(rest)
        if not tm:
            out.append(AC(change, ac_id, [], False, n + 1))
            continue
        seg = tm.group(1).strip()
        self_exempt = SELF_EXEMPT_HINT in rest
        v_ids = sorted(set(V_RE.findall(seg)), key=lambda s: int(s[2:]))
        out.append(AC(change, ac_id, v_ids, self_exempt, n + 1))
    return out


def evaluate(acs):
    """纯函数：给定 AC 列表 ⇒ (errors, coverage)。供 --self-test 直接复用。"""
    errors = []
    coverage = {}

    for a in acs:
        if a.self_exempt:
            continue
        if not a.v_ids:
            errors.append(("no-v-tag", a, "AC 未带 `覆盖 V:` 标签"))
            continue
        for v in a.v_ids:
            coverage.setdefault(v, []).append(a)

    for v in V_UNIVERSE:
        if v not in coverage:
            errors.append(("orphan-v", None, f"{v} 未被任何 AC 覆盖（并集不完整）"))

    for v, owners in coverage.items():
        if v not in V_UNIVERSE:
            errors.append(("unknown-v", owners[0], f"{v} 不在 V-101~V-110 全集内"))
            continue
        if len(owners) > 1 and v not in CROSS_ALLOWED:
            ids = ", ".join(f"{o.change}:{o.ac_id}" for o in owners)
            errors.append(("duplicate-assignment", owners[0],
                           f"{v} 被 {len(owners)} 条 AC 覆盖（{ids}）但不在交叉白名单"))
    return errors, coverage


def print_report(acs, errors, coverage):
    print("=== AC ↔ V 覆盖校验（AC-115a 静态标签 + AC-115b 并集）===")
    print(f"change: {', '.join(CHANGES)}")
    print(f"V 全集: {V_UNIVERSE[0]} ~ {V_UNIVERSE[-1]}（{len(V_UNIVERSE)} 个）")
    print()
    for change in CHANGES:
        mine = [a for a in acs if a.change == change]
        bad = [a for a in mine if not a.self_exempt and not a.v_ids]
        print(f"[{change}] AC={len(mine)} · 无标签={len(bad)}" +
              (f"  ← {[a.ac_id for a in bad]}" if bad else ""))
    print()
    print("V 覆盖明细：")
    for v in V_UNIVERSE:
        owners = coverage.get(v, [])
        mark = "OK " if owners else "MISS"
        way = "交叉" if len(owners) > 1 else "    "
        ids = ", ".join(o.ac_id for o in owners) or "-"
        print(f"  [{mark}] {v}  {len(owners)} 条 {way}  {ids}")
    print()
    if errors:
        print(f"违规 {len(errors)} 条：")
        for kind, ac, msg in errors:
            where = f"{ac.change}:{ac.ac_id}" if ac else "（全局）"
            print(f"  [{kind}] {where}  {msg}")
        return 1
    covered = len([v for v in V_UNIVERSE if coverage.get(v)])
    print(f"结果: PASS ✓  并集覆盖 {covered}/{len(V_UNIVERSE)} · "
          f"无孤儿 V · 无未授权重复分配")
    return 0


def self_test():
    """判别力自检：三类违规各构造一个用例 + 一个全绿用例。"""
    def mk(ac_id, v_ids, self_exempt=False, change="c"):
        return AC(change, ac_id, list(v_ids), self_exempt, 0)

    cases = []

    # ① 全绿：10 个 V 各一条
    full = [mk(f"AC-{i}", [f"V-{100+i}"]) for i in range(1, 11)]
    cases.append(("全绿", full, 0))

    # ② 缺标签
    cases.append(("缺标签", full[:-1] + [mk("AC-X", [])], 1))

    # ③ 孤儿 V（少覆盖 V-110）
    cases.append(("孤儿 V", full[:-1], 1))

    # ④ 未授权重复（V-102 被两条覆盖，且不在白名单）
    cases.append(("未授权重复", full + [mk("AC-Y", ["V-102"])], 1))

    # ⑤ 已授权交叉（V-104 在 CROSS_ALLOWED 内，重复不算违规）
    cases.append(("已授权交叉", full + [mk("AC-Z", ["V-104"])], 0))

    # ⑥ self-exempt 的 AC 不参与计数
    cases.append(("self-exempt", full + [mk("AC-115b", [], self_exempt=True)], 0))

    failed = 0
    for name, acs, want_errors in cases:
        errors, _ = evaluate(acs)
        ok = (len(errors) > 0) == (want_errors > 0) if want_errors == 0 else len(errors) > 0
        # 精确期望：want_errors=0 ⇒ 必须零错误；>0 ⇒ 必须有错误
        ok = (len(errors) == 0) if want_errors == 0 else (len(errors) > 0)
        print(f"[{'PASS' if ok else 'FAIL'}] {name:<14} errors={len(errors)} (期望{'无' if want_errors == 0 else '有'})")
        if not ok:
            failed += 1
    print()
    print(f"self-test: {len(cases) - failed}/{len(cases)} passed")
    return 0 if failed == 0 else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="AC ↔ V 覆盖校验（AC-115a/115b）")
    ap.add_argument("--root", default=".", help="仓库根（默认当前目录）")
    ap.add_argument("--self-test", action="store_true", help="判别力自检")
    opts = ap.parse_args(argv)

    if opts.self_test:
        return self_test()

    root = os.path.abspath(opts.root)
    acs = []
    for change in CHANGES:
        path = os.path.join(root, "openspec", "changes", change, "proposal.md")
        if not os.path.isfile(path):
            print(f"ERROR: 找不到 {path}", file=sys.stderr)
            return 2
        try:
            acs.extend(parse_change(path, change))
        except ValueError as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 2

    errors, coverage = evaluate(acs)
    return print_report(acs, errors, coverage)


if __name__ == "__main__":
    sys.exit(main())
