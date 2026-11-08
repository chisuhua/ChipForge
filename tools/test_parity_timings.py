# SPDX-License-Identifier: Apache-2.0
"""Regression tests for the A4 timing collection in tools/parity/parity_gate.py.

背景（A4 · backend-parity-ci · 契约 S-15）：
  契约 S-15 要求「A4 MUST 先把实测 p95 写进报告 timings，再据此定阈值」，
  且「本期不设未实测的硬阈值」⇒ 本模块**只采集**，不参与判定。

覆盖：
  1. `_percentile` —— 分位数计算（线性插值口径）
  2. `_summarize_timings` —— 汇总结构（per_case / count / p50 / p95 / total）
  3. 边界：空输入 / 单样本 / 含非数值项

★ ★ **MUST NOT**：★ 这些测试**不触碰判定逻辑**（★ judge / 退出码映射 / S-1~S-17 契约）。
"""

import importlib.util
import unittest
from pathlib import Path

# ★ ★ **按文件路径加载**（★ 不用 `from tools.parity.parity_gate import ...`）：
#   本机存在同名 `tools` 包（另一个 checkout 的 `.../rdd-workflow/tools`，★ 带 `__init__.py`）
#   ⇒ ★ 标准包解析会抢先命中它 ⇒ ★ `tools.parity` 不可达（★ `tools/test_doc_checker.py` 同因不可运行）。
#   ⇒ ★ 按路径加载 ⇒ ★ 与包解析无关 ⇒ ★ 任何环境都能跑。
_PARITY_GATE_PATH = Path(__file__).resolve().parent / "parity" / "parity_gate.py"
_spec = importlib.util.spec_from_file_location("_parity_gate_under_test", _PARITY_GATE_PATH)
assert _spec and _spec.loader, f"cannot load {_PARITY_GATE_PATH}"
_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)

_percentile = _mod._percentile
_summarize_timings = _mod._summarize_timings


class TestPercentile(unittest.TestCase):
    """★ `_percentile` —— 线性插值分位数。"""

    def test_empty_returns_none(self):
        """★ 空输入 ⇒ None（★ 不编造 0 —— 0 会被误读为「实测 0 秒」）。"""
        self.assertIsNone(_percentile([], 50))
        self.assertIsNone(_percentile([], 95))

    def test_single_sample(self):
        """★ 单样本 ⇒ 该样本本身（★ p50 与 p95 相同）。"""
        self.assertEqual(_percentile([7.5], 50), 7.5)
        self.assertEqual(_percentile([7.5], 95), 7.5)

    def test_two_samples_interpolates(self):
        """★ 两样本：p50 落在中点。"""
        vals = [10.0, 20.0]
        self.assertEqual(_percentile(vals, 50), 15.0)

    def test_p50_of_even_count(self):
        """★ 偶数样本的 p50 = 两个中间值的均值。"""
        self.assertEqual(_percentile([1.0, 2.0, 3.0, 4.0], 50), 2.5)

    def test_p95_interpolates_toward_max(self):
        """★ p95 在 n=8 时落在倒数第二个样本与最大值之间（★ 非 nearest-rank ⇒ 略小于 max）。"""
        vals = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]
        p95 = _percentile(vals, 95)
        self.assertLess(p95, 8.0)
        self.assertGreaterEqual(p95, 7.0)

    def test_unsorted_input_is_sorted_internally(self):
        """★ 输入无序 ⇒ 结果与有序输入一致（★ 门禁的 per_case 按清单序但时长独立）。"""
        self.assertEqual(
            _percentile([3.0, 1.0, 2.0], 50),
            _percentile([1.0, 2.0, 3.0], 50),
        )

    def test_monotonic_in_pct(self):
        """★ p50 ≤ p95（★ 单调性 —— 阈值定档的基本前提）。"""
        vals = [float(i) for i in range(1, 21)]
        self.assertLessEqual(_percentile(vals, 50), _percentile(vals, 95))


class TestSummarizeTimings(unittest.TestCase):
    """★ `_summarize_timings` —— 汇总结构。"""

    def test_empty_per_case(self):
        """★ 无 case ⇒ 各统计量为 None（★ 不是 0）。"""
        out = _summarize_timings([])
        self.assertEqual(out["per_case"], {})
        self.assertEqual(out["count"], 0)
        self.assertIsNone(out["p50"])
        self.assertIsNone(out["p95"])
        self.assertIsNone(out["total"])

    def test_structure_keys(self):
        """★ 结构含 5 个键（★ 与 A4 AC-4 的判据一致）。"""
        out = _summarize_timings([{"name": "a", "elapsed_s": 1.0}])
        for k in ("per_case", "count", "p50", "p95", "total"):
            self.assertIn(k, out)

    def test_per_case_mapping(self):
        """★ per_case 是 {name: seconds} 映射。"""
        out = _summarize_timings([
            {"name": "case-a", "elapsed_s": 10.0},
            {"name": "case-b", "elapsed_s": 20.0},
        ])
        self.assertEqual(out["per_case"], {"case-a": 10.0, "case-b": 20.0})
        self.assertEqual(out["count"], 2)
        self.assertEqual(out["total"], 30.0)

    def test_non_numeric_entries_skipped(self):
        """★ 非数值 / 缺失的 elapsed_s ⇒ 跳过（★ 不进统计、不污染 count）。"""
        out = _summarize_timings([
            {"name": "ok", "elapsed_s": 5.0},
            {"name": "missing"},
            {"name": "bad", "elapsed_s": "not-a-number"},
            {"name": "none", "elapsed_s": None},
        ])
        self.assertEqual(out["count"], 1)
        self.assertEqual(out["per_case"], {"ok": 5.0})
        self.assertEqual(out["total"], 5.0)

    def test_total_equals_sum(self):
        """★ total = 各 case 之和（★ 并行执行下 total < 墙钟 ⇒ 两者都记录）。"""
        vals = [3.0, 7.5, 1.25]
        out = _summarize_timings([
            {"name": f"c{i}", "elapsed_s": v} for i, v in enumerate(vals)
        ])
        self.assertAlmostEqual(out["total"], sum(vals), places=6)

    def test_p50_p95_within_range(self):
        """★ p50 / p95 落在 [min, max] 内（★ 阈值定档的合理性前提）。"""
        vals = [5.0, 1.0, 9.0, 3.0]
        out = _summarize_timings([
            {"name": f"c{i}", "elapsed_s": v} for i, v in enumerate(vals)
        ])
        self.assertGreaterEqual(out["p50"], min(vals))
        self.assertLessEqual(out["p50"], max(vals))
        self.assertGreaterEqual(out["p95"], min(vals))
        self.assertLessEqual(out["p95"], max(vals))

    def test_does_not_mutate_input(self):
        """★ 汇总函数**不修改**入参的 per_case 条目（★ 报告的 per_case 原样保留）。"""
        entry = {"name": "x", "elapsed_s": 2.0}
        _summarize_timings([entry])
        self.assertEqual(entry, {"name": "x", "elapsed_s": 2.0})


if __name__ == "__main__":
    unittest.main()
