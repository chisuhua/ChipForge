# SPDX-License-Identifier: Apache-2.0
"""Regression tests for tools/parity/read_back_verdict.py — A4 · 契约 GN-1.

背景（A4 · backend-parity-ci · ADR-088 §5.2 判据 4）：
  判据 4 的**完成形式** = 「差分门禁在 CI 中被执行且**结果被回读**」
  ⇒ 本脚本是「回读」那一半。

★ GN-1：★ 判定机制自身失效 MUST 响亮失败，MUST NOT 表现为「无问题」
  ⇒ 报告缺失 / JSON 损坏 / verdict 字段缺失 ⇒ 一律 exit 2（ERROR）
  ⇒ ★ 绝不能因为「读不到」而返回 0（那会把「没结论」伪装成「通过」）。

退出码契约：0=PASS · 1=FAIL · 2=ERROR
"""

import importlib.util
import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path

# ★ 按文件路径加载（不用 from tools.parity... —— 本机存在同名 tools 包会抢先命中）
_SCRIPT = Path(__file__).resolve().parent / "parity" / "read_back_verdict.py"
_spec = importlib.util.spec_from_file_location("_read_back_under_test", _SCRIPT)
assert _spec and _spec.loader, f"cannot load {_SCRIPT}"
_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)

_TMPDIRS = []


def _write_report(payload=None, raw=None):
    d = tempfile.mkdtemp(prefix="parity-rb-")
    _TMPDIRS.append(d)
    path = os.path.join(d, "report.json")
    with open(path, "w", encoding="utf-8") as fh:
        if raw is not None:
            fh.write(raw)
        else:
            json.dump(payload, fh)
    return path


def tearDownModule():
    for d in _TMPDIRS:
        shutil.rmtree(d, ignore_errors=True)


def _report(verdict="PASS", **extra):
    base = {
        "verdict": verdict,
        "error_kind": None,
        "layer": "pr",
        "commit": "deadbeef",
        "jobs": 4,
        "timings": {"count": 8, "p50": 27.1, "p95": 36.7, "total": 216.1, "per_case": {}},
    }
    base.update(extra)
    return base


class TestMissingOrBrokenReport(unittest.TestCase):
    """★ GN-1 核心：判定机制失效 => 响亮失败（exit 2），不是 PASS。"""

    def test_missing_report_returns_2(self):
        missing = os.path.join(tempfile.mkdtemp(prefix="parity-rb-miss-"), "nope.json")
        self.assertEqual(_mod.main(["--report", missing]), 2)

    def test_corrupt_json_returns_2(self):
        self.assertEqual(_mod.main(["--report", _write_report(raw="{not valid json")]), 2)

    def test_non_object_json_returns_2(self):
        self.assertEqual(_mod.main(["--report", _write_report(raw="[1,2,3]")]), 2)

    def test_missing_verdict_field_returns_2(self):
        payload = _report()
        payload.pop("verdict")
        self.assertEqual(_mod.main(["--report", _write_report(payload=payload)]), 2)


class TestVerdictMapping(unittest.TestCase):
    """★ 退出码与 parity_gate.py 对齐：0=PASS · 1=FAIL · 2=ERROR。"""

    def test_pass_returns_0(self):
        self.assertEqual(_mod.main(["--report", _write_report(payload=_report("PASS"))]), 0)

    def test_fail_returns_1(self):
        p = _write_report(payload=_report("FAIL", error_kind="pchash_mismatch"))
        self.assertEqual(_mod.main(["--report", p]), 1)

    def test_error_returns_2(self):
        p = _write_report(payload=_report("ERROR", error_kind="runner_error"))
        self.assertEqual(_mod.main(["--report", p]), 2)

    def test_unknown_verdict_returns_2(self):
        """★ 未知 verdict => 不当成 PASS（只认 PASS 为通过）。"""
        self.assertEqual(_mod.main(["--report", _write_report(payload=_report("MAYBE"))]), 2)


class TestSummarize(unittest.TestCase):
    """★ summarize —— 回读字段提取（含 timings 缺失的降级）。"""

    def test_extracts_timings(self):
        s = _mod.summarize(_report("PASS"))
        self.assertEqual(s["verdict"], "PASS")
        self.assertEqual(s["layer"], "pr")
        self.assertEqual(s["cases"], 8)
        self.assertEqual(s["p50"], 27.1)
        self.assertEqual(s["p95"], 36.7)
        self.assertEqual(s["jobs"], 4)

    def test_timings_missing_degrades_to_none(self):
        """★ timings 缺失 => 各统计量 None（不编造 0）。"""
        payload = _report("PASS")
        payload.pop("timings")
        s = _mod.summarize(payload)
        self.assertIsNone(s["cases"])
        self.assertIsNone(s["p50"])
        self.assertIsNone(s["p95"])

    def test_timings_not_a_dict_degrades(self):
        self.assertIsNone(_mod.summarize(_report("PASS", timings=["wrong"]))["cases"])


class TestStepSummary(unittest.TestCase):
    """★ step summary 写入（CI 可读性）。"""

    def test_writes_summary_when_path_given(self):
        fd, summary = tempfile.mkstemp(prefix="parity-rb-sum-", suffix=".md")
        os.close(fd)
        try:
            rc = _mod.main(["--report", _write_report(payload=_report("PASS")),
                            "--summary-file", summary])
            self.assertEqual(rc, 0)
            text = Path(summary).read_text(encoding="utf-8")
            self.assertIn("双后端差分 parity 门禁", text)
            self.assertIn("verdict: `PASS`", text)
            self.assertIn("p95", text)
        finally:
            os.remove(summary)

    def test_no_summary_flag_is_fine(self):
        old = os.environ.pop("GITHUB_STEP_SUMMARY", None)
        try:
            self.assertEqual(_mod.main(["--report", _write_report(payload=_report("PASS"))]), 0)
        finally:
            if old is not None:
                os.environ["GITHUB_STEP_SUMMARY"] = old


if __name__ == "__main__":
    unittest.main()
