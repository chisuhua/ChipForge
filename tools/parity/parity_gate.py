#!/usr/bin/env python3
"""双后端差分 parity 门禁 —— **比对器**（阶段 A2b · `backend-parity-harness`）。

契约：`docs/architecture/parity-gate-contract.md`
  S-9  判定规则与「禁降级」
  S-10 三态退出码（0=PASS / 1=FAIL=后端分歧 / 2=ERROR=门禁自身或环境故障）
  S-11 例外白名单格式（5 必填 + 未知字段拒收 + 禁容差 + 常驻合成例外）
  S-12 扩张权（architect 审）与规模护栏（>N ⇒ 提示复审，不阻断）
  S-13 语料清单单一来源（清单驱动 + 两侧执行集合断言）
  S-14 报告格式（build/parity/report.json）
  S-15 分层（PR 快子集 / 夜间全量）

设计要点（★ 与契约同源，改动必须同步契约）：
  * **判定与执行分离**：`judge()` 只吃两份 JSON 片段 ⇒ 可用合成数据做鉴别力自检
    （`--self-test`），不必每次都跑两个后端。
  * **fail-closed**：观测量缺失 / 确定性不符 / 白名单非法 / 语料集合不一致 ⇒ 一律 `ERROR`，
    **MUST NOT** 跳过或按 0 处理。
  * **禁降级**：不可采 ≠ 可跳过。判据强度 `pchash` > `cycles`+`regs`/`memsig` > 仅 `tohost`。
  * **例外登记制**：白名单是**受控清单**，每条必须 5 字段齐备 + 未过期 + 作用域匹配；
    **MUST NOT** 以容差吸收（schema 拒收容差类字段）。

用法：
  python3 tools/parity/parity_gate.py --layer pr            # PR 级快子集
  python3 tools/parity/parity_gate.py --layer night         # 夜间全量
  python3 tools/parity/parity_gate.py --self-test            # 比对器自身的鉴别力自检
  python3 tools/parity/parity_gate.py --only rv32ui-p-add    # 单 case
"""

import argparse
import datetime
import json
import os
import re
import subprocess
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

OBSERVATIONS = ["tohost", "regs", "memsig", "cycles", "pchash"]

EXCEPTION_REQUIRED = [
    "id",
    "observation",
    "corpus_scope",
    "mechanism",
    "upstream_status",
    "mutation_control",
    "sunset",
    "owner",
]
EXCEPTION_ALLOWED = set(EXCEPTION_REQUIRED) | {"issue_url", "synthetic", "justification"}
# ★ 容差类字段一律拒收（契约 S-11）——即使不在 allowed 集合里，schema 校验也会拒。
FORBIDDEN_FIELDS = {"tolerance", "eps", "max_delta", "max_diff", "atol", "rtol"}

EXCEPTION_COUNT_SOFT_LIMIT = 10  # S-12：> N ⇒ 提示 architect 复审（**不阻断**）


# ────────────────────────────────────────────────────────────── 语料清单

def parse_corpus(path):
    """S-13：清单是唯一来源。格式 name<TAB>kind<TAB>relpath<TAB>layer。"""
    cases, errors = [], []
    if not os.path.isfile(path):
        return [], [f"corpus file not found: {path}"]
    with open(path, "r", encoding="utf-8") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) != 4:
                errors.append(f"{path}:{lineno}: expected 4 tab-separated fields, got {len(parts)}")
                continue
            name, kind, relpath, layer = (p.strip() for p in parts)
            if kind not in ("elf", "branchbench"):
                errors.append(f"{path}:{lineno}: unknown kind '{kind}'")
                continue
            if layer not in ("pr", "night"):
                errors.append(f"{path}:{lineno}: unknown layer '{layer}'")
                continue
            cases.append({"name": name, "kind": kind, "relpath": relpath, "layer": layer})
    names = [c["name"] for c in cases]
    dupes = sorted({n for n in names if names.count(n) > 1})
    if dupes:
        errors.append(f"{path}: duplicate case names: {dupes}")
    return cases, errors


def check_corpus_against_harnesses(cases):
    """S-13：两侧实际执行集合 MUST 覆盖清单集合。

    既有 harness 用**手维护的 C++ 数组**（`kCases`）⇒ 这里直接解析那两个源文件。

    ★ **方向性判定**（两个方向的风险不对称）：
    - `清单 ⊄ harness` ⇒ ★ **ERROR**（corpus_mismatch）：该 case 不会被那一侧跑 ⇒
      差分比对缺一侧 ⇒ 真正的漏检风险
    - `harness ⊄ 清单` ⇒ ★ **WARNING**：那一侧多跑了一个不在清单里的 case ⇒ 对清单内的
      case 无差分风险，但属于**覆盖超集**（当前事实：`rv32ui-p-simple` 只在 Verilator 侧
      `kCases` 里 ⇒ ADR-089 D4 要求的「补进 Simulator 侧 1 行」由 A3 收口）
    """
    elf_names = {c["name"] for c in cases if c["kind"] == "elf"}
    problems = []
    warnings = []
    harness_files = {
        "simulator": "tests/cpu/test_cpu_chmem_vendored_elf.cpp",
        "verilator": "tests/cpu/test_cpu_verilator_sim.cpp",
    }
    pattern = re.compile(r'\{"((?:rv32ui|rv32um)-p-[a-z0-9_]+)",')
    for side, rel in harness_files.items():
        path = os.path.join(REPO_ROOT, rel)
        if not os.path.isfile(path):
            problems.append(f"{side}: harness source not found: {rel}")
            continue
        with open(path, "r", encoding="utf-8") as fh:
            listed = set(pattern.findall(fh.read()))
        missing = sorted(elf_names - listed)     # 清单里有、harness 没有 ⇒ ERROR
        extra = sorted(listed - elf_names)        # harness 有、清单里没有 ⇒ WARNING
        if missing:
            problems.append(f"{side}: corpus entries missing from kCases: {missing}")
        if extra:
            warnings.append(f"{side}: kCases entries not in corpus (coverage superset; "
                            f"A3 cleanup): {extra}")
    return problems, warnings


# ────────────────────────────────────────────────────────────── 例外白名单

def load_exceptions(path, today=None):
    """S-11 / S-12：5+3 必填字段、未知字段拒收、禁容差、过期失效、规模护栏。"""
    today = today or datetime.date.today()
    entries, errors, warnings = [], [], []
    if not os.path.isfile(path):
        return entries, [f"exceptions file not found: {path}"], warnings
    try:
        with open(path, "r", encoding="utf-8") as fh:
            doc = json.load(fh)
    except json.JSONDecodeError as exc:
        return entries, [f"{path}: invalid JSON: {exc}"], warnings

    if doc.get("schema_version") != "parity-exceptions-v1":
        errors.append(f"{path}: unsupported schema_version {doc.get('schema_version')!r}")
    raw_entries = doc.get("exceptions")
    if not isinstance(raw_entries, list):
        return entries, errors + [f"{path}: 'exceptions' must be a list"], warnings

    for idx, entry in enumerate(raw_entries):
        where = f"{path}:exceptions[{idx}]"
        if not isinstance(entry, dict):
            errors.append(f"{where}: must be an object")
            continue
        forbidden = sorted(FORBIDDEN_FIELDS & set(entry))
        if forbidden:
            errors.append(f"{where}: forbidden tolerance field(s) {forbidden} "
                          f"⇒ 容差是假绿通道（契约 S-11）")
            continue
        unknown = sorted(set(entry) - EXCEPTION_ALLOWED)
        if unknown:
            errors.append(f"{where}: unknown field(s) {unknown} ⇒ 拒收")
            continue
        missing = [f for f in EXCEPTION_REQUIRED if f not in entry]
        if missing:
            errors.append(f"{where}: missing required field(s) {missing}")
            continue
        if entry["observation"] not in OBSERVATIONS:
            errors.append(f"{where}: unknown observation {entry['observation']!r}")
            continue
        if entry["upstream_status"] == "upstream-pending" and not entry.get("issue_url"):
            errors.append(f"{where}: upstream_status=upstream-pending requires issue_url")
            continue
        scope = entry["corpus_scope"]
        if not isinstance(scope, list) or not scope:
            errors.append(f"{where}: corpus_scope must be a non-empty list")
            continue
        if "*" in scope and not entry.get("justification"):
            errors.append(f"{where}: corpus_scope=['*'] requires justification")
            continue
        try:
            sunset = datetime.date.fromisoformat(entry["sunset"])
        except (TypeError, ValueError):
            errors.append(f"{where}: sunset must be an ISO date (YYYY-MM-DD)")
            continue
        if sunset < today:
            # 过期条目**失效** ⇒ 不再豁免任何差异（由 judge 表现为 FAIL）。
            warnings.append(f"{where}: exception {entry['id']} expired on {entry['sunset']} ⇒ 不再生效")
            entry["_expired"] = True
        entries.append(entry)

    real = [e for e in entries if not e.get("synthetic")]
    if len(real) > EXCEPTION_COUNT_SOFT_LIMIT:
        warnings.append(
            f"{path}: {len(real)} real exceptions > {EXCEPTION_COUNT_SOFT_LIMIT} "
            f"⇒ MUST 触发 architect 复审（契约 S-12；★ 不阻断门禁）")
    if not any(e.get("synthetic") and e["id"] == "synthetic-self-test" for e in entries):
        errors.append(f"{path}: missing the resident synthetic exception 'synthetic-self-test'"
                      f"（★ 常驻 fixture 防白名单机制随清零腐化）")
    return entries, errors, warnings


def exception_applies(entry, case_name, observation):
    """作用域匹配：精确 case 名，或 '*'（必须带 justification）。"""
    if entry.get("_expired"):
        return False
    if entry["observation"] != observation:
        return False
    scope = entry["corpus_scope"]
    return case_name in scope or "*" in scope


# ────────────────────────────────────────────────────────────── 判定核心

def _obs_value(fragment, key):
    return (fragment.get("observations") or {}).get(key)


def judge(case_name, sim, vl, exceptions):
    """判定一个 case。返回 dict：verdict / error_kind / first_divergence / applied。

    ★ 纯函数（不碰文件、不跑进程）⇒ 可被 `--self-test` 用合成数据直接验证。
    """
    result = {
        "verdict": "PASS",
        "error_kind": None,
        "first_divergence": None,
        "applied": [],
        "differences": [],
    }

    # ── S-2：确定性前置（先于任何观测量比对）──
    det_s = (sim or {}).get("determinism")
    det_v = (vl or {}).get("determinism")
    if not det_s or not det_v:
        result.update(verdict="ERROR", error_kind="determinism_missing")
        return result
    if det_s != det_v:
        result.update(verdict="ERROR", error_kind="determinism_mismatch")
        result["first_divergence"] = {
            "metric": "determinism",
            "cycle_index": -1,
            "sim_value": det_s,
            "vl_value": det_v,
        }
        return result

    # ── S-9 / S-1：观测量齐备性（缺一 ⇒ ERROR，不降级）──
    for key in OBSERVATIONS:
        if _obs_value(sim, key) is None or _obs_value(vl, key) is None:
            result.update(verdict="ERROR", error_kind=f"observation_missing:{key}")
            return result

    # ── S-9：逐项比对（强度序：pchash > cycles+regs/memsig > tohost）──
    for key in OBSERVATIONS:
        s_val = _obs_value(sim, key)
        v_val = _obs_value(vl, key)
        if s_val == v_val:
            continue
        excused = [e for e in exceptions
                   if exception_applies(e, case_name, key)]
        result["differences"].append({
            "metric": key,
            "sim_value": s_val,
            "vl_value": v_val,
            "excused_by": [e["id"] for e in excused],
        })
        if excused:
            result["applied"].extend(e["id"] for e in excused)
            continue
        if result["first_divergence"] is None:
            result["first_divergence"] = {
                "metric": key,
                "cycle_index": -1,
                "sim_value": s_val,
                "vl_value": v_val,
            }

    if result["differences"]:
        unexcused = [d for d in result["differences"] if not d["excused_by"]]
        if unexcused:
            result.update(verdict="FAIL", error_kind="backend_divergence")
        else:
            result["verdict"] = "PASS"  # 已知分歧已登记 ⇒ 仍 PASS（但记入 applied）
    return result


# ────────────────────────────────────────────────────────────── 执行两个后端

def _run(cmd, timeout_s):
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout_s,
                              cwd=REPO_ROOT)
    except subprocess.TimeoutExpired:
        return None, "TIMEOUT"
    if proc.returncode not in (0, 1, 2):
        return None, f"runner exit {proc.returncode}: {proc.stderr.strip()[-400:]}"
    return proc, None


def run_simulator(case, opts):
    out = os.path.join(opts.work_dir, f"{case['name']}.sim.json")
    cmd = [opts.sim_bin, "--elf", case["relpath"], "--cycles", str(opts.max_cycles),
           "--json", out]
    proc, err = _run(cmd, opts.timeout_s)
    if err:
        return None, err
    if not os.path.isfile(out):
        return None, "simulator produced no report"
    with open(out, "r", encoding="utf-8") as fh:
        return json.load(fh), None


def run_verilator(case, opts):
    out = os.path.join(opts.work_dir, f"{case['name']}.vl.json")
    cmd = [opts.vl_bin, "--elf", case["relpath"], "--cycles", str(opts.max_cycles),
           "--json", out]
    proc, err = _run(cmd, opts.timeout_s)
    if err:
        return None, err
    if not os.path.isfile(out):
        return None, "verilator produced no report"
    with open(out, "r", encoding="utf-8") as fh:
        return json.load(fh), None


# ────────────────────────────────────────────────────────────── 自检

def _frag(**over):
    base = {
        "backend": "sim",
        "determinism": {"node_count": 2511, "names_hash": "67205f7da2dced52"},
        "observations": {
            "tohost": 1,
            "regs": "72c91fd2b209cdff",
            "memsig": "0000000000007c01",
            "memsig_updates": 1,
            "cycles": 478,
            "pchash": "830b6af580cdf64e",
        },
    }
    base.update(over)
    return base


def self_test():
    """比对器自身的鉴别力（契约 S-9/S-10/S-11/S-13 的验收前置）。"""
    ok_exc = {
        "id": "EX-TEST", "observation": "regs", "corpus_scope": ["c1"],
        "mechanism": "t", "upstream_status": "by-design",
        "mutation_control": "t", "sunset": "2099-12-31", "owner": "t",
    }
    expired = dict(ok_exc, id="EX-OLD", sunset="2000-01-01", _expired=True)
    checks = []

    r = judge("c1", _frag(), _frag(), [])
    checks.append(("全等 ⇒ PASS", r["verdict"] == "PASS", r["verdict"]))

    bad = _frag()
    bad["observations"]["regs"] = "deadbeefdeadbeef"
    r = judge("c1", _frag(), bad, [])
    checks.append(("regs 不等 ⇒ FAIL", r["verdict"] == "FAIL", r["verdict"]))
    checks.append(("FAIL 带 first_divergence", r["first_divergence"] is not None
                    and r["first_divergence"]["metric"] == "regs", r["first_divergence"]))

    bad2 = _frag()
    bad2["observations"]["pchash"] = "0000000000000000"
    r = judge("c1", _frag(), bad2, [])
    checks.append(("pchash 不等 ⇒ FAIL", r["verdict"] == "FAIL", r["verdict"]))

    missing = _frag()
    missing["observations"]["memsig"] = None
    r = judge("c1", _frag(), missing, [])
    checks.append(("观测量缺失 ⇒ ERROR", r["verdict"] == "ERROR"
                    and r["error_kind"].startswith("observation_missing"), r["error_kind"]))

    det = _frag(determinism={"node_count": 1, "names_hash": "x"})
    r = judge("c1", _frag(), det, [])
    checks.append(("确定性不符 ⇒ ERROR/determinism_mismatch",
                    r["verdict"] == "ERROR" and r["error_kind"] == "determinism_mismatch",
                    r["error_kind"]))

    r = judge("c1", _frag(), bad, [ok_exc])
    checks.append(("合规例外 ⇒ 差异被豁免（PASS + applied）",
                    r["verdict"] == "PASS" and "EX-TEST" in r["applied"],
                    f"{r['verdict']}/{r['applied']}"))

    r = judge("c1", _frag(), bad, [expired])
    checks.append(("过期例外 ⇒ 不豁免 ⇒ FAIL", r["verdict"] == "FAIL", r["verdict"]))

    r = judge("c2", _frag(), bad, [ok_exc])
    checks.append(("例外作用域不匹配 ⇒ FAIL", r["verdict"] == "FAIL", r["verdict"]))

    width = max(len(c[0]) for c in checks)
    failed = 0
    for name, passed, detail in checks:
        mark = "PASS" if passed else "FAIL"
        if not passed:
            failed += 1
        print(f"[{mark}] {name.ljust(width)}  ({detail})")
    print(f"\nself-test: {len(checks) - failed}/{len(checks)} passed")
    return 0 if failed == 0 else 1


# ────────────────────────────────────────────────────────────── main

def main(argv=None):
    ap = argparse.ArgumentParser(description="双后端差分 parity 门禁 —— 比对器（A2b）")
    ap.add_argument("--corpus", default=os.path.join("tests", "cpu", "parity", "corpus.txt"))
    ap.add_argument("--exceptions", default=os.path.join("tests", "cpu", "parity", "exceptions.json"))
    ap.add_argument("--report", default=os.path.join("build", "parity", "report.json"))
    ap.add_argument("--work-dir", default=os.path.join("build", "parity"))
    ap.add_argument("--layer", choices=["pr", "night", "all"], default="pr")
    ap.add_argument("--only", default="", help="逗号分隔的 case 名（调试用）")
    ap.add_argument("--sim-bin", default=os.path.join("build", "bin", "parity_sim_runner"))
    ap.add_argument("--vl-bin", default=os.path.join("build", "bin", "cpu_verilator_sim"))
    ap.add_argument("--max-cycles", type=int, default=3000)
    ap.add_argument("--timeout-s", type=int, default=3600)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--skip-harness-check", action="store_true",
                    help="跳过「两侧 kCases 集合 == 清单」断言")
    opts = ap.parse_args(argv)

    if opts.self_test:
        return self_test()

    os.makedirs(opts.work_dir, exist_ok=True)
    report_dir = os.path.dirname(os.path.abspath(opts.report))
    if report_dir:
        os.makedirs(report_dir, exist_ok=True)

    cases, corpus_errors = parse_corpus(os.path.join(REPO_ROOT, opts.corpus))
    exceptions, exc_errors, exc_warnings = load_exceptions(
        os.path.join(REPO_ROOT, opts.exceptions))

    fatal = list(corpus_errors) + list(exc_errors)
    if fatal:
        for e in fatal:
            print(f"ERROR: {e}", file=sys.stderr)
        print(f"verdict=ERROR error_kind=input_invalid", file=sys.stderr)
        return 2

    # S-13：两侧执行集合断言
    if not opts.skip_harness_check:
        problems, harness_warnings = check_corpus_against_harnesses(cases)
        exc_warnings.extend(harness_warnings)
        if problems:
            for p in problems:
                print(f"ERROR: {p}", file=sys.stderr)
            print("verdict=ERROR error_kind=corpus_mismatch", file=sys.stderr)
            return 2

    selected = [c for c in cases if opts.layer == "all" or c["layer"] == opts.layer
                or c["layer"] == "night"]
    if opts.layer == "pr":
        selected = [c for c in cases if c["layer"] == "pr"]
    if opts.only:
        wanted = {n.strip() for n in opts.only.split(",") if n.strip()}
        selected = [c for c in selected if c["name"] in wanted]
    if not selected:
        print("ERROR: empty corpus selection", file=sys.stderr)
        return 2

    per_case = []
    verdict = "PASS"
    error_kind = None
    for case in selected:
        sim, sim_err = run_simulator(case, opts)
        vl, vl_err = run_verilator(case, opts)
        if sim_err or vl_err:
            entry = {
                "name": case["name"],
                "observations": {},
                "first_divergence": None,
                "verdict": "ERROR",
                "error_kind": "runner_error",
                "runner_error": sim_err or vl_err,
            }
            per_case.append(entry)
            verdict = "ERROR"
            error_kind = error_kind or "runner_error"
            print(f"[ERROR] {case['name']}: {sim_err or vl_err}", file=sys.stderr)
            continue
        j = judge(case["name"], sim, vl, exceptions)
        entry = {
            "name": case["name"],
            "determinism": sim.get("determinism"),
            "observations": sim.get("observations", {}),
            "first_divergence": j["first_divergence"],
            "verdict": j["verdict"],
            "error_kind": j["error_kind"],
            "differences": j["differences"],
            "exceptions_applied": j["applied"],
        }
        per_case.append(entry)
        if j["verdict"] == "FAIL":
            verdict = "FAIL"
            error_kind = error_kind or "backend_divergence"
        elif j["verdict"] == "ERROR" and verdict != "FAIL":
            verdict = "ERROR"
            error_kind = error_kind or j["error_kind"]
        flag = {"PASS": "PASS", "FAIL": "FAIL", "ERROR": "ERROR"}[j["verdict"]]
        extra = ""
        if j["first_divergence"]:
            fd = j["first_divergence"]
            extra = f"  first_divergence={fd.get('metric')}: sim={fd.get('sim_value')} vl={fd.get('vl_value')}"
        if j["applied"]:
            extra += f"  exceptions={j['applied']}"
        print(f"[{flag}] {case['name']}{extra}")

    report = {
        "schema_version": "parity-report-v1",
        "verdict": verdict,
        "error_kind": error_kind,
        "timestamp": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "commit": _git_head(),
        "reset_mode": "sim:ctor-init / vl:reset-pulse-1",
        "corpus_source": opts.corpus,
        "layer": opts.layer,
        # 同一个 build_cpu 配置 ⇒ 各 ELF 的 DAG 结构相同（node_count/names_hash 一致），
        # 逐 case 也记录一份，便于发现「某 case 的 DAG 漂了」。
        "determinism": per_case[0].get("determinism"),
        "per_case": per_case,
        "exceptions_applied": sorted({e for c in per_case for e in c.get("exceptions_applied", [])}),
        "warnings": exc_warnings,
        "timings": {},
    }
    with open(opts.report, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2)
        fh.write("\n")
    print(f"\nverdict={verdict} error_kind={error_kind} report={opts.report}")
    if exc_warnings:
        for w in exc_warnings:
            print(f"WARNING: {w}", file=sys.stderr)
    return {"PASS": 0, "FAIL": 1, "ERROR": 2}[verdict]


def _git_head():
    try:
        return subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True,
                              text=True, cwd=REPO_ROOT).stdout.strip()
    except Exception:
        return ""


if __name__ == "__main__":
    sys.exit(main())
