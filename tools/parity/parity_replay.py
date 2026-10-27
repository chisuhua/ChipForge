#!/usr/bin/env python3
"""A3 首跑结果**重放**：用已落盘的 `{case}.sim.json` / `{case}.vl.json` 重新判定并出报告。

★ **用途**：门禁代码变更（如分层语义修正）后，**不必重跑 50 min** 即可用同一批实测数据
  重新判定 —— 但 ★ **MUST NOT** 把重放结果当作「门禁跑过」的证据（重放不重跑 runner）。
⇒ 重放报告的 `schema_version` 带 `-replay` 后缀 ⇒ **MUST** 与真跑报告区分。

用法：
  python3 tools/parity/parity_replay.py --work-dir build/parity --out build/parity/report-replay.json
"""
import argparse
import datetime
import glob
import json
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO_ROOT, "tools", "parity"))

import parity_gate as pg  # noqa: E402


def main(argv=None):
    ap = argparse.ArgumentParser(description="parity 报告重放（用已落盘的 runner 结果）")
    ap.add_argument("--work-dir", default=os.path.join("build", "parity"))
    ap.add_argument("--out", default=os.path.join("build", "parity", "report-replay.json"))
    ap.add_argument("--corpus", default=os.path.join("tests", "cpu", "parity", "corpus.txt"))
    ap.add_argument("--exceptions", default=os.path.join("tests", "cpu", "parity", "exceptions.json"))
    opts = ap.parse_args(argv)

    work = os.path.join(REPO_ROOT, opts.work_dir)
    cases, corpus_errors = pg.parse_corpus(os.path.join(REPO_ROOT, opts.corpus))
    exceptions, exc_errors, exc_warnings = pg.load_exceptions(
        os.path.join(REPO_ROOT, opts.exceptions))
    if corpus_errors or exc_errors:
        for e in list(corpus_errors) + list(exc_errors):
            print(f"ERROR: {e}", file=sys.stderr)
        return 2

    per_case = []
    missing = []
    verdict, error_kind = "PASS", None
    for case in cases:
        sim_path = os.path.join(work, f"{case['name']}.sim.json")
        vl_path = os.path.join(work, f"{case['name']}.vl.json")
        if not (os.path.isfile(sim_path) and os.path.isfile(vl_path)):
            missing.append(case["name"])
            continue
        with open(sim_path, "r", encoding="utf-8") as fh:
            sim = json.load(fh)
        with open(vl_path, "r", encoding="utf-8") as fh:
            vl = json.load(fh)
        j = pg.judge(case["name"], sim, vl, exceptions)
        entry = {
            "name": case["name"],
            "determinism": sim.get("determinism"),
            "observations": sim.get("observations", {}),
            "completion": j.get("completion"),
            "vacuous_observations": j.get("vacuous_observations", []),
            "first_divergence": j["first_divergence"],
            "verdict": j["verdict"],
            "error_kind": j["error_kind"],
            "differences": j["differences"],
            "exceptions_applied": j["applied"],
            "cap": pg.case_cap(case, argparse.Namespace(max_cycles=3000)),
        }
        per_case.append(entry)
        if entry["verdict"] == "FAIL":
            verdict = "FAIL"
            error_kind = error_kind or entry["error_kind"]
        elif entry["verdict"] == "ERROR" and verdict != "FAIL":
            verdict = "ERROR"
            error_kind = error_kind or entry["error_kind"]

    report = {
        "schema_version": "parity-report-v1-replay",
        "replay_of": os.path.join(opts.work_dir, "*.{sim,vl}.json"),
        "verdict": verdict,
        "error_kind": error_kind,
        "timestamp": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "commit": pg._git_head(),
        "reset_mode": "sim:ctor-init / vl:reset-pulse-1",
        "corpus_source": opts.corpus,
        "layer": "night",
        "jobs": None,
        "per_case": per_case,
        "exceptions_applied": sorted({e for c in per_case for e in c.get("exceptions_applied", [])}),
        "warnings": exc_warnings + ([f"missing runner results for: {missing}"] if missing else []),
        "partial_cases": [c["name"] for c in per_case if c.get("vacuous_observations")],
        "timings": {},
    }
    out = os.path.join(REPO_ROOT, opts.out) if not os.path.isabs(opts.out) else opts.out
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2)
        fh.write("\n")
    print(f"replayed {len(per_case)}/{len(cases)} case  verdict={verdict}  error_kind={error_kind}")
    if missing:
        print(f"WARNING: missing runner results: {missing}")
    print(f"report={out}")
    return {"PASS": 0, "FAIL": 1, "ERROR": 2}[verdict]


if __name__ == "__main__":
    sys.exit(main())
