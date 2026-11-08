#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""回读 parity 门禁结论 —— A4（change: backend-parity-ci）· 契约 GN-1。

★ 用途（ADR-088 v1.4 §5.2 判据 4 的「完成形式」）：
  「差分门禁在 CI 中被执行且**结果被回读**」⇒ 本脚本就是「回读」那一半。

★ ★ 通用总则 GN-1：★ 判定机制自身失效 MUST 响亮失败，MUST NOT 表现为「无问题」
  ⇒ ★ report.json 缺失 ⇒ **exit 2**（ERROR，不是 PASS）
  ⇒ ★ verdict 字段缺失 ⇒ **exit 2**
  ⇒ ★ 绝不把「读不到」当成「没问题」。

★ 退出码（与 parity_gate.py 一致）：
  0 = PASS · 1 = FAIL · 2 = ERROR（报告缺失 / 字段缺失 / 无法解析）

★ 用法：
  python3 tools/parity/read_back_verdict.py [--report build/parity/report.json]
                                             [--summary-file "$GITHUB_STEP_SUMMARY"]
"""

import argparse
import json
import os
import sys


def load_report(path):
    """★ 读报告 ⇒ (dict, None) 或 (None, error_str)。"""
    if not os.path.isfile(path):
        return None, f"report not found: {path}"
    try:
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)
    except (json.JSONDecodeError, OSError) as exc:
        return None, f"cannot parse {path}: {exc}"
    if not isinstance(data, dict):
        return None, f"{path}: top-level JSON is not an object"
    return data, None


def summarize(data):
    """★ 从报告提取回读字段（★ 缺失字段返回 None，由调用方决定是否致命）。"""
    timings = data.get("timings") or {}
    if not isinstance(timings, dict):
        timings = {}
    return {
        "layer": data.get("layer"),
        "verdict": data.get("verdict"),
        "error_kind": data.get("error_kind"),
        "commit": data.get("commit"),
        "cases": timings.get("count"),
        "p50": timings.get("p50"),
        "p95": timings.get("p95"),
        "total": timings.get("total"),
        "jobs": data.get("jobs"),
    }


def render(s):
    lines = [
        "=== Parity verdict (read back) ===",
        f"layer        : {s['layer']}",
        f"verdict      : {s['verdict']}",
        f"error_kind   : {s['error_kind']}",
        f"commit       : {s['commit']}",
        f"cases        : {s['cases']}",
        f"p50/p95/tot  : {s['p50']} / {s['p95']} / {s['total']} (s)",
        f"jobs         : {s['jobs']}",
    ]
    return "\n".join(lines)


def render_markdown(s):
    return "\n".join([
        "## 双后端差分 parity 门禁",
        "",
        f"- layer: `{s['layer']}`",
        f"- **verdict: `{s['verdict']}`**",
        f"- error_kind: `{s['error_kind']}`",
        f"- cases: {s['cases']} · p50 {s['p50']}s · p95 {s['p95']}s · total {s['total']}s",
        f"- jobs: {s['jobs']}",
        f"- commit: `{s['commit']}`",
        "",
    ])


def main(argv=None):
    ap = argparse.ArgumentParser(description="回读 parity 门禁结论（A4 · GN-1 fail-loud）")
    ap.add_argument("--report", default=os.path.join("build", "parity", "report.json"))
    ap.add_argument("--summary-file", default=os.environ.get("GITHUB_STEP_SUMMARY", ""),
                    help="GitHub Actions step summary 文件（缺省读 $GITHUB_STEP_SUMMARY）")
    opts = ap.parse_args(argv)

    data, err = load_report(opts.report)
    if data is None:
        # ★ GN-1：★ 读不到结论 = 判定机制失效 ⇒ 响亮失败，MUST NOT 当作「无问题」
        print(f"ERROR: ★ 门禁结论不可读 ⇒ 判据 4 未满足（GN-1：不得表现为『无问题』）", file=sys.stderr)
        print(f"  {err}", file=sys.stderr)
        return 2

    s = summarize(data)
    print(render(s))

    if s["verdict"] is None:
        print("ERROR: ★ 报告中缺 verdict 字段 ⇒ 判据 4 未满足", file=sys.stderr)
        return 2

    if opts.summary_file:
        try:
            with open(opts.summary_file, "a", encoding="utf-8") as fh:
                fh.write(render_markdown(s) + "\n")
        except OSError as exc:
            print(f"WARNING: cannot write step summary: {exc}", file=sys.stderr)

    # ★ 判定：★ 只认 PASS 为通过（★ 其余一律失败 ⇒ 与 parity_gate.py 的退出码语义对齐）
    if s["verdict"] == "PASS":
        return 0
    return 1 if s["verdict"] == "FAIL" else 2


if __name__ == "__main__":
    sys.exit(main())
