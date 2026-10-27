#!/usr/bin/env python3
"""A3 首跑结果分析 —— 从 parity 报告产出**既存后端分歧清单**（ADR-089 核心交付物）。

用法：
  python3 tools/parity/parity_report.py --report build/parity/report-night.json
  python3 tools/parity/parity_report.py --report ... --markdown   # 输出 Markdown 表

输出：
  * 汇总：verdict / case 总数 / PASS / FAIL / ERROR / partial 数
  * 分歧清单：每条含 case / 观测量 / 两侧值 / 首个分歧点
  * ★ 三分类建议（依据判据，MUST 由 architect 逐条签认）：
      - 上游待对齐：两侧语义不同但都自洽（如哈希口径 / 采样点）
      - 有意例外  ：语义等价但表示不同 ⇒ 需白名单登记（S-11 的 5 必填字段）
      - 真 bug    ：一侧违反 RISC-V 语义
    ⇒ 脚本**只给建议**，分类结论 MUST 人工签认（不自动写白名单）。
"""
import argparse
import json
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def classify(entry):
    """★ 只给**建议**，不自动裁决（⇒ MUST 人工签认）。"""
    if entry.get("verdict") == "ERROR":
        return ("门禁故障", "非后端分歧（runner 崩溃 / 环境问题）⇒ 不计入分歧清单")
    if entry.get("verdict") == "PASS":
        return ("无分歧", "")
    fd = entry.get("first_divergence") or {}
    metric = fd.get("metric")
    if metric == "completion":
        return ("真 bug（高优先）",
                "一侧跑完、另一侧没跑完 ⇒ 语义分歧（不是门禁故障）")
    if metric == "pchash":
        return ("上游待对齐 / 真 bug（需人工判读）",
                "逐拍 PC 流分歧 ⇒ 执行流已不同 ⇒ 需看首个分歧拍定位")
    if metric in ("regs", "memsig"):
        return ("上游待对齐 / 真 bug（需人工判读）",
                "末态寄存器或内存写签名分歧")
    if metric == "tohost":
        return ("真 bug（高优先）", "tohost 值分歧 ⇒ 程序可见行为不同")
    if metric == "cycles":
        return ("上游待对齐（高优先）", "总拍数分歧 ⇒ 执行流长度不同")
    return ("待判读", "")


def main(argv=None):
    ap = argparse.ArgumentParser(description="parity 报告分析（A3 分歧清单）")
    ap.add_argument("--report", default=os.path.join("build", "parity", "report-night.json"))
    ap.add_argument("--markdown", action="store_true")
    opts = ap.parse_args(argv)

    path = os.path.join(REPO_ROOT, opts.report) if not os.path.isabs(opts.report) else opts.report
    if not os.path.isfile(path):
        print(f"ERROR: report not found: {path}", file=sys.stderr)
        return 2
    with open(path, "r", encoding="utf-8") as fh:
        rep = json.load(fh)

    cases = rep.get("per_case", [])
    n_pass = sum(1 for c in cases if c.get("verdict") == "PASS")
    n_fail = sum(1 for c in cases if c.get("verdict") == "FAIL")
    n_err = sum(1 for c in cases if c.get("verdict") == "ERROR")
    partial = [c["name"] for c in cases if c.get("vacuous_observations")]

    if opts.markdown:
        print(f"**verdict = `{rep.get('verdict')}`** · `error_kind = {rep.get('error_kind')}` "
              f"· case {len(cases)}（PASS {n_pass} / FAIL {n_fail} / ERROR {n_err}）"
              f" · partial {len(partial)}")
        print()
        print("| case | verdict | 首分歧观测量 | Simulator 值 | Verilator 值 | 分类建议 |")
        print("|---|---|---|---|---|---|")
        for c in cases:
            fd = c.get("first_divergence") or {}
            cls, _ = classify(c)
            print(f"| `{c['name']}` | {c.get('verdict')} | {fd.get('metric', '—')} | "
                  f"`{fd.get('sim_value', '—')}` | `{fd.get('vl_value', '—')}` | {cls} |")
        return 0

    print(f"verdict      : {rep.get('verdict')}  (error_kind={rep.get('error_kind')})")
    print(f"commit       : {rep.get('commit')}")
    print(f"layer/jobs   : {rep.get('layer')} / {rep.get('jobs')}")
    print(f"cases        : {len(cases)}  (PASS {n_pass} / FAIL {n_fail} / ERROR {n_err})")
    print(f"partial      : {len(partial)} {partial if partial else ''}")
    print(f"exceptions   : {rep.get('exceptions_applied')}")
    if rep.get("warnings"):
        print(f"warnings     :")
        for w in rep["warnings"]:
            print(f"  - {w}")
    print()

    if n_fail == 0 and n_err == 0:
        print(f"★★ 无既存后端分歧（{len(cases)} case 全部 PASS）★")
        print("   ⇒ 「既存后端分歧清单」为空 ⇒ 门禁可直接进 A4。")
        if partial:
            print(f"   ★ 但 {len(partial)} 个 case 为 partial（承重判据 = regs/memsig/pchash）：")
            for nm in partial:
                print(f"     - {nm}")
        return 0

    print("★★ 既存后端分歧清单 ★★")
    for c in cases:
        if c.get("verdict") == "PASS":
            continue
        cls, note = classify(c)
        print(f"\n--- {c['name']}  [{c.get('verdict')}"
              f"{' / ' + c['error_kind'] if c.get('error_kind') else ''}] ---")
        print(f"  分类建议 : {cls}" + (f" —— {note}" if note else ""))
        if c.get("runner_error"):
            print(f"  runner   : {c['runner_error']}")
        if c.get("completion"):
            print(f"  completion: {c['completion']}")
        fd = c.get("first_divergence")
        if fd:
            print(f"  首分歧   : {fd.get('metric')}  sim={fd.get('sim_value')!r}  "
                  f"vl={fd.get('vl_value')!r}")
        for d in c.get("differences", []):
            print(f"    · {d['metric']}: sim={d['sim_value']!r} vl={d['vl_value']!r} "
                  f"excused_by={d.get('excused_by')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
