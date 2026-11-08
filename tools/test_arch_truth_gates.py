# SPDX-License-Identifier: Apache-2.0
"""门禁①④（check_arch_truth.py）与 AC↔V 校验（check_ac_v_coverage.py）的单元测试。

change: architecture-truth-boundary-enforcement（AC-110 / AC-113 / AC-115a / AC-115b）

覆盖两类**易错且承重**的逻辑：
  ① 词表命中：长词优先、不重复计数、`Phase Nd 完成` 的字母后缀
  ② 语法豁免：frontmatter **仅认首行**、表头行看**下一行**、自标注非目标态的**关闭**
  ③ 门禁④：只认 `.md` 文件引用，不认「仅命名目录」
  ④ AC↔V：缺标签 / 孤儿 V / 未授权重复 / 已授权交叉 / self-exempt

★ import 按**文件路径**加载（本机存在同名 `tools` 包会抢先命中，见
  `tools/test_parity_timings.py` 的同款说明）。
"""

import importlib.util
import os
import pathlib
import unittest

_HERE = pathlib.Path(__file__).resolve().parent
_ROOT = _HERE.parent


def _load(name, rel):
    spec = importlib.util.spec_from_file_location(name, _HERE / rel)
    assert spec and spec.loader, rel
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


gate = _load("_arch_truth_under_test", "check_arch_truth.py")
acv = _load("_ac_v_under_test", "check_ac_v_coverage.py")

FIX = _HERE / "fixtures" / "arch_truth"


class TestFindHits(unittest.TestCase):
    def test_longest_word_wins_no_double_count(self):
        """`当前实际状态` 不应同时被 `当前状态` 再计一次。"""
        hits = gate._find_hits("当前实际状态")
        self.assertEqual([w for _, w in hits], ["当前实际状态"])

    def test_multiple_distinct_words(self):
        hits = gate._find_hits("当前实现已落地")
        self.assertEqual(sorted(w for _, w in hits), ["已落地", "当前实现"])

    def test_phase_with_letter_suffix(self):
        """`Phase 6d 完成` —— 实测曾是漏匹配（正则只允许数字）。"""
        self.assertEqual(len(gate._find_hits("Phase 6d 完成")), 1)

    def test_phase_with_decimal(self):
        self.assertEqual(len(gate._find_hits("Phase 6d.5 完成")), 1)

    def test_no_hit(self):
        self.assertEqual(gate._find_hits("该长什么样"), [])


class TestFrontmatterDetection(unittest.TestCase):
    def test_first_line_dash_is_frontmatter(self):
        lines = ["---", "a: 当前状态", "---", "body"]
        self.assertEqual(gate._frontmatter_end(lines), 3)

    def test_rule_later_is_not_frontmatter(self):
        """★ 实测教训：把任意 `---` 当开关会吞掉 104 处真实命中。"""
        lines = ["# t", "", "---", "当前状态"]
        self.assertEqual(gate._frontmatter_end(lines), 0)

    def test_unclosed_is_not_frontmatter(self):
        lines = ["---", "a: 1"] + ["x"] * 25
        self.assertEqual(gate._frontmatter_end(lines), 0)


class TestScanFile(unittest.TestCase):
    def test_fixture_a_frontmatter_exempt(self):
        f, e = gate.scan_file(str(FIX / "a_frontmatter.md"))
        self.assertEqual(len(f), 0)
        self.assertIn("a-frontmatter", {x.rule for x in e})

    def test_fixture_b_status_cell(self):
        f, e = gate.scan_file(str(FIX / "b_status_cell.md"))
        self.assertEqual(len(f), 0)
        self.assertIn("b-status-cell", {x.rule for x in e})

    def test_fixture_h_section_closes(self):
        """自标注「非目标态」的节内豁免，但下一个同级标题后**恢复检查**。"""
        f, e = gate.scan_file(str(FIX / "h_self_declared_nontarget.md"))
        self.assertEqual(len(f), 1, [x.text for x in f])
        self.assertIn("h-self-declared-nontarget", {x.rule for x in e})
        self.assertIn("当前实现", f[0].word)

    def test_fixture_z_true_positive(self):
        f, _ = gate.scan_file(str(FIX / "z_true_positive.md"))
        self.assertEqual(len(f), 5)

    def test_table_header_looks_at_next_line(self):
        f, e = gate.scan_file(str(FIX / "f_table_header.md"))
        self.assertEqual(len(f), 0)
        self.assertIn("f-table-header", {x.rule for x in e})


class TestSubdirRefs(unittest.TestCase):
    def test_unmarked_flagged(self):
        got = gate.scan_subdir_refs(str(FIX / "i_subdir_ref_unmarked.md"), str(_ROOT))
        self.assertEqual(len(got), 2)

    def test_marked_not_flagged(self):
        got = gate.scan_subdir_refs(str(FIX / "j_subdir_ref_marked.md"), str(_ROOT))
        self.assertEqual(len(got), 0)

    def test_resolve_ref_repo_relative(self):
        p = gate.resolve_ref("docs/architecture/archive/x.md", str(_ROOT))
        self.assertTrue(p.endswith(os.path.join("docs", "architecture", "archive", "x.md")))

    def test_resolve_ref_ignores_external(self):
        self.assertIsNone(gate.resolve_ref("https://example.com/a.md", str(_ROOT)))
        self.assertIsNone(gate.resolve_ref("#anchor", str(_ROOT)))


class TestAcVCoverage(unittest.TestCase):
    @staticmethod
    def _mk(ac_id, v_ids, self_exempt=False):
        return acv.AC("c", ac_id, list(v_ids), self_exempt, 0)

    def _full(self):
        return [self._mk(f"AC-{i}", [f"V-{100 + i}"]) for i in range(1, 11)]

    def test_full_union_passes(self):
        errors, cov = acv.evaluate(self._full())
        self.assertEqual(errors, [])
        self.assertEqual(len(cov), 10)

    def test_missing_tag_detected(self):
        acs = self._full()[:-1] + [self._mk("AC-X", [])]
        kinds = {k for k, _, _ in acv.evaluate(acs)[0]}
        self.assertIn("no-v-tag", kinds)

    def test_orphan_v_detected(self):
        kinds = {k for k, _, _ in acv.evaluate(self._full()[:-1])[0]}
        self.assertIn("orphan-v", kinds)

    def test_duplicate_not_allowed(self):
        acs = self._full() + [self._mk("AC-Y", ["V-102"])]
        kinds = {k for k, _, _ in acv.evaluate(acs)[0]}
        self.assertIn("duplicate-assignment", kinds)

    def test_cross_allowed_passes(self):
        """V-104 在白名单里 ⇒ 重复覆盖不判违规。"""
        self.assertIn("V-104", acv.CROSS_ALLOWED)
        errors, _ = acv.evaluate(self._full() + [self._mk("AC-Z", ["V-104"])])
        self.assertEqual(errors, [])

    def test_self_exempt_ignored(self):
        errors, cov = acv.evaluate(self._full() + [self._mk("AC-115b", [], True)])
        self.assertEqual(errors, [])
        self.assertEqual(len(cov), 10)

    def test_tag_parse_stops_at_paren(self):
        """`覆盖 V: 全部（… 自豁免 …）` 不应把正文里的 V-101/V-104 当成标签。"""
        tmp = pathlib.Path("/tmp/opencode/l1b/ac_parse_case.md")
        tmp.parent.mkdir(parents=True, exist_ok=True)
        tmp.write_text(
            "## Acceptance Criteria\n\n"
            "- [ ] **AC-900** 规则本身 ⇒ 覆盖 V: 全部（★ 本条自豁免于并集计算；见 V-101 ~ V-110 与 V-104）\n",
            encoding="utf-8",
        )
        acs = acv.parse_change(str(tmp), "c")
        self.assertEqual(acs[0].v_ids, [])
        self.assertTrue(acs[0].self_exempt)


if __name__ == "__main__":
    unittest.main()
