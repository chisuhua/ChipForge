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

# ★ 被校验的 change（★ 每个 change 的 AC 段 = 一个「并集」的边界）
CHANGES = (
    "architecture-truth-boundary",
    "architecture-truth-boundary-enforcement",
    # ★ 2026-11-08 扩到 L4 两个 change（ADR-094 的 18 个 V ⇒ V-27~V-43 含 V-27b）
    "chmem-hierarchical-assembly",
    "chmem-hierarchical-unit-landing",
)

# ★ V 全集（AC-115b① 的判据基准）—— L1 的 V-101~V-110 + L4 的 V-27~V-43（含 V-27b）
def _l4_v_universe():
    out = []
    for n in range(27, 44):
        out.append(f"V-{n}")
        if n == 27:
            out.append("V-27b")     # ★ V-27b 紧随 V-27
    return tuple(out)


def _v_sort_key(v):
    return (int(re.match(r"\d+", v[2:]).group()), v)


V_UNIVERSE = tuple(sorted([f"V-{n}" for n in range(101, 111)] + list(_l4_v_universe()),
                           key=_v_sort_key))

# ★ 允许「交叉覆盖」的 V ⇒ 理由（不在表内的 V 被多条 AC 覆盖即判违规）
CROSS_ALLOWED = {
    "V-101": "顶层结构（边界/导航/非权威标注）由 3 个不同落点的 AC 共同承载",
    "V-103": "实施态语汇的**判定装置**与**内容净化**是两件事，分别由门禁与净化 AC 承载",
    "V-104": "引用完整性的「入检 / 修正 / 豁免 / 段头可提取」是四个独立动作",
    "V-106": "根级文档同步与 IP 索引归属分别是两份文件的交付",
    "V-107": "迁移工具的机械改写与人工复核（内部链接）分别可验",
    "V-109": "迁移分批执行与归档/索引的最终位置是不同时点的判据",
    "V-110": "可回滚性由「每批单 commit」与「实测 revert」两条独立证据支撑",
    # ★ ★ L4（ADR-094）—— ★ 键 = 「V|侧面」（★ 侧面为空 ⇒ 该 V 无侧面标记）
    # ★ ★ 判定口径：**同一 V 的同一侧面**被多条 AC 覆盖才算「未授权重复分配」；
    #   ★ 不同侧面（框架侧 / 落地侧 / 回归侧）⇒ **合法**（★ 同一 V 的两侧验证）
    "V-30|落地侧": "V-30 含「ch_device 独立复用」与「Wrapper 父路径命名」两个场景 ⇒ 成层 / 独立复用 / Wrapper 命名 三条 AC",
    "V-31|框架侧": "V-31 的「负向三变体」外，AC-1（root 与 emitter 同一对象）/ AC-2（create_child 路径）/ AC-7（r.6 负向断言）各是独立判据点",
    "V-32|落地侧": "V-32 含「端口名逐字不同」与「ch_in 均有 driver」两个断言 ⇒ 加「成本登记」第三条",
    "V-34|框架侧": "V-34 的「2 TU 链接探针」与 AC-1 的「root 与 emitter 同一对象」是两个判据点",
    "V-35|框架侧": "基线**捕获**（①-C 出口，AC-15）与**比对 + 退役**（①-D 验收，AC-18）是两个时点",
    "V-36|框架侧": "基线**捕获**（①-C 出口，AC-15）与**比对 + 退役**（①-D 验收，AC-18）是两个时点",
    "V-39|框架侧": "「三态化」（AC-13）与「同名普查」（AC-14）是两个判据点",
    "V-40|框架侧": "①-A′ 前置适配（AC-9）/ ①-D 同批切换（AC-16）/ ①-D 三条同批改动（AC-17）是三个判据点",
    "V-41|框架侧": "「槽位单例」（①-A，AC-6）与「安装期自检」（①-B，AC-11）是两个判据点",
}

HDR_RE = re.compile(r"^##\s+(?:验收标准|Acceptance Criteria)\s*$")
BULLET_RE = re.compile(r"^-\s+(?:\[[ xX]\]\s+)?\*\*(AC-[0-9A-Za-z]+)\*\*(.*)$")
# `覆盖 V:` 之后到「（」/「）」/「。」为止的那一段才是标签本体
TAG_RE = re.compile(r"覆盖\s*V\s*[:：]\s*([^（()）。\n]*)")
V_RE = re.compile(r"V-\d+b?")
SELF_EXEMPT_HINT = "自豁免"
# ★ ★ 「侧面」标记（ADR-094 v0.2）：★ 同一 V 可被两个 change 各覆盖一次（框架侧 / 落地侧 / 回归侧）
#   ⇒ 机检「无重复」MUST 按「**同一 V 的同一侧面**」判定 ⇒ ★ NOT 按 V 计数
ASPECT_RE = re.compile(r"(框架侧|落地侧|回归侧)")
ASPECT_NONE = ""


class AC:
    __slots__ = ("change", "ac_id", "v_ids", "self_exempt", "line", "aspect")

    def __init__(self, change, ac_id, v_ids, self_exempt, line, aspect=ASPECT_NONE):
        self.change = change
        self.ac_id = ac_id
        self.v_ids = v_ids
        self.self_exempt = self_exempt
        self.line = line
        self.aspect = aspect


def resolve_proposal_path(root, change):
    """★ 定位 change 的 proposal.md —— ★ 已归档的 change 回退到 `archive/*-<name>/`。

    ★ 归档后 change 会从 `openspec/changes/<name>/` 移到
    `openspec/changes/archive/<YYYY-MM-DD>-<name>/` ⇒ ★ 若不回退，机检会直接崩
    （★ 实测：归档 6 个 change 后 `check_ac_v_coverage.py` 报「找不到 proposal.md」）。
    """
    live = os.path.join(root, "openspec", "changes", change, "proposal.md")
    if os.path.isfile(live):
        return live
    archive_dir = os.path.join(root, "openspec", "changes", "archive")
    if os.path.isdir(archive_dir):
        # ★ 多个候选时取**最新**（目录名带日期前缀 ⇒ 字典序最大 = 最新）
        cands = sorted(d for d in os.listdir(archive_dir) if d.endswith("-" + change))
        if cands:
            return os.path.join(archive_dir, cands[-1], "proposal.md")
    return live


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
        # ★ V-27b 这类带字母后缀的 V ⇒ 排序键取「数字部分」
        v_ids = sorted(set(V_RE.findall(seg)),
                       key=lambda s: (int(re.match(r"\d+", s[2:]).group()), s))
        am = ASPECT_RE.search(seg)
        aspect = am.group(1) if am else ASPECT_NONE
        out.append(AC(change, ac_id, v_ids, self_exempt, n + 1, aspect))
    return out


def evaluate(acs, v_universe=None):
    """纯函数：给定 AC 列表 ⇒ (errors, coverage)。供 --self-test 直接复用。

    ★ `v_universe` 可注入（★ self-test 用小全集，★ 免得被真实全集的孤儿淹没）。
    """
    v_universe = tuple(v_universe) if v_universe is not None else V_UNIVERSE
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

    for v in v_universe:
        if v not in coverage:
            errors.append(("orphan-v", None, f"{v} 未被任何 AC 覆盖（并集不完整）"))

    for v, owners in coverage.items():
        if v not in v_universe:
            errors.append(("unknown-v", owners[0], f"{v} 不在 V 全集内"))
            continue
        # ★ ★ 按「同一 V 的同一侧面」分组 ⇒ 不同侧面（框架侧 / 落地侧 / 回归侧）**合法**
        by_aspect = {}
        for o in owners:
            by_aspect.setdefault(o.aspect, []).append(o)
        for aspect, group in by_aspect.items():
            if len(group) <= 1:
                continue
            key = f"{v}|{aspect}" if aspect else v
            if key in CROSS_ALLOWED:
                continue
            ids = ", ".join(f"{o.change}:{o.ac_id}" for o in group)
            errors.append(("duplicate-assignment", group[0],
                           f"{v} 的侧面「{aspect or '未标注'}」被 {len(group)} 条 AC 覆盖"
                           f"（{ids}）但不在交叉白名单（键 {key}）"))
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
    def mk(ac_id, v_ids, self_exempt=False, change="c", aspect=ASPECT_NONE):
        return AC(change, ac_id, list(v_ids), self_exempt, 0, aspect)

    # ★ self-test 用**小全集**（★ 否则真实全集的孤儿会淹没判别力断言）
    mini = tuple(f"V-{n}" for n in range(101, 111))
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

    # ⑦ ★ 侧面感知（ADR-094 v0.2）：同一 V 的**不同侧面** ⇒ **合法**（0 错误）
    cases.append(("★ 不同侧面合法",
                  full + [mk("AC-S1", ["V-110"], aspect="框架侧"),
                          mk("AC-S2", ["V-110"], aspect="落地侧")], 0))

    # ⑧ ★ 同一 V 的**同一侧面**被多条 AC 覆盖 ⇒ **违规**（且不在白名单 ⇒ 用 V-109）
    cases.append(("★ 同侧面重复违规",
                  full + [mk("AC-D1", ["V-109"], aspect="框架侧"),
                          mk("AC-D2", ["V-109"], aspect="框架侧")], 1))

    failed = 0
    for name, acs, want_errors in cases:
        errors, _ = evaluate(acs, mini)
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
        # ★ 已归档的 change 回退到 `archive/*-<name>/`（★ 归档后机检不得崩）
        path = resolve_proposal_path(root, change)
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
