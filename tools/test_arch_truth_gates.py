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
pgage = _load("_parity_gate_under_test", os.path.join("parity", "parity_gate.py"))

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

    def test_fence_fake_heading_does_not_open_exemption(self):
        """★ oracle 2026-11-08 报的 BLOCKER：围栏里的 `# 非目标态` 曾会静默开豁免。"""
        f, e = gate.scan_file(str(FIX / "k_fence_fake_heading.md"))
        self.assertEqual(len(f), 2, [x.text for x in f])
        self.assertNotIn("h-self-declared-nontarget", {x.rule for x in e})

    def test_nontarget_requires_date(self):
        """豁免 (h) 收紧：标题含「非目标态」但**无日期** ⇒ 不豁免；带日期 ⇒ 豁免。"""
        f, e = gate.scan_file(str(FIX / "m_nontarget_no_date.md"))
        self.assertEqual(len(f), 1, [x.text for x in f])
        self.assertIn("h-self-declared-nontarget", {x.rule for x in e})


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


class TestTopLevelLayout(unittest.TestCase):
    """★ critic 2026-11-08 指出的规避手法：`mkdir docs/architecture/<new>/` 即绕过门禁①。"""

    def _mk(self, names):
        import tempfile
        root = tempfile.mkdtemp(prefix="layout-")
        base = os.path.join(root, "docs", "architecture")
        os.makedirs(base)
        for n in names:
            full = os.path.join(base, n)
            # 带扩展名的当文件，否则当目录
            if "." in n.rsplit("/", 1)[-1]:
                open(full, "w").close()
            else:
                os.makedirs(full)
        return root

    def test_allowed_layout_clean(self):
        root = self._mk(["README.md", "adr.md", "adr", "mixed", "archive", "plugin-research"])
        self.assertEqual(gate.check_top_level_layout(root), [])

    def test_unexpected_dir_flagged(self):
        root = self._mk(["README.md", "notes"])
        problems = gate.check_top_level_layout(root)
        self.assertEqual(len(problems), 1)
        self.assertEqual(problems[0][0], "unexpected-dir")
        self.assertEqual(problems[0][1], "notes")

    def test_non_markdown_file_flagged(self):
        root = self._mk(["README.md", "scratch.txt"])
        problems = gate.check_top_level_layout(root)
        self.assertEqual(len(problems), 1)
        self.assertEqual(problems[0][0], "unexpected-file")


class TestAcVCoverage(unittest.TestCase):
    @staticmethod
    def _mk(ac_id, v_ids, self_exempt=False, aspect=acv.ASPECT_NONE):
        return acv.AC("c", ac_id, list(v_ids), self_exempt, 0, aspect)

    @staticmethod
    def _mini():
        """★ self-test 用**小全集**（★ 真实全集已扩到 28 个 ⇒ 会被孤儿淹没）。"""
        return tuple(f"V-{n}" for n in range(101, 111))

    def _full(self):
        return [self._mk(f"AC-{i}", [f"V-{100 + i}"]) for i in range(1, 11)]

    def test_full_union_passes(self):
        errors, cov = acv.evaluate(self._full(), self._mini())
        self.assertEqual(errors, [])
        self.assertEqual(len(cov), 10)

    def test_missing_tag_detected(self):
        acs = self._full()[:-1] + [self._mk("AC-X", [])]
        kinds = {k for k, _, _ in acv.evaluate(acs, self._mini())[0]}
        self.assertIn("no-v-tag", kinds)

    def test_orphan_v_detected(self):
        kinds = {k for k, _, _ in acv.evaluate(self._full()[:-1], self._mini())[0]}
        self.assertIn("orphan-v", kinds)

    def test_duplicate_not_allowed(self):
        acs = self._full() + [self._mk("AC-Y", ["V-102"])]
        kinds = {k for k, _, _ in acv.evaluate(acs, self._mini())[0]}
        self.assertIn("duplicate-assignment", kinds)

    def test_cross_allowed_passes(self):
        """V-104 在白名单里 ⇒ 重复覆盖不判违规。"""
        self.assertIn("V-104", acv.CROSS_ALLOWED)
        errors, _ = acv.evaluate(self._full() + [self._mk("AC-Z", ["V-104"])], self._mini())
        self.assertEqual(errors, [])

    def test_self_exempt_ignored(self):
        errors, cov = acv.evaluate(self._full() + [self._mk("AC-115b", [], True)], self._mini())
        self.assertEqual(errors, [])
        self.assertEqual(len(cov), 10)

    # ★ ★ ADR-094 v0.2：同一 V 可被两个 change 各覆盖一次（框架侧 / 落地侧 / 回归侧）
    #   ⇒ 「无重复」MUST 按「同一 V 的同一侧面」判定 ⇒ NOT 按 V 计数
    def test_same_v_different_aspect_is_legal(self):
        """同一 V 的**不同侧面** ⇒ 合法（0 错误）。"""
        errors, _ = acv.evaluate(
            self._full() + [self._mk("AC-S1", ["V-110"], aspect="框架侧"),
                            self._mk("AC-S2", ["V-110"], aspect="落地侧")],
            self._mini())
        self.assertEqual(errors, [])

    def test_same_v_same_aspect_is_violation(self):
        """同一 V 的**同一侧面**被多条 AC 覆盖 ⇒ 违规。"""
        errors, _ = acv.evaluate(
            self._full() + [self._mk("AC-D1", ["V-109"], aspect="框架侧"),
                            self._mk("AC-D2", ["V-109"], aspect="框架侧")],
            self._mini())
        kinds = {k for k, _, _ in errors}
        self.assertIn("duplicate-assignment", kinds)

    def test_aspect_parsed_from_tag(self):
        """`· 侧面: **落地侧**` ⇒ AC.aspect 被解析出来。"""
        tmp = pathlib.Path("/tmp/opencode/l1b/ac_aspect_case.md")
        tmp.parent.mkdir(parents=True, exist_ok=True)
        tmp.write_text(
            "## Acceptance Criteria\n\n"
            "- [ ] **AC-901** 规则 ⇒ （覆盖 V: V-42, V-30 · 侧面: **落地侧**）\n",
            encoding="utf-8",
        )
        acs = acv.parse_change(str(tmp), "c")
        self.assertEqual(len(acs), 1)
        self.assertEqual(acs[0].v_ids, ["V-30", "V-42"])
        self.assertEqual(acs[0].aspect, "落地侧")

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


class TestCorpusScoping(unittest.TestCase):
    """★ v1.6：存在性校验按「本次实际要跑的集合」判定。

    实测背景：CI 首次 run 时 `--layer pr` 因 **night 专属**的 branchbench ELF 缺失而 ERROR，
    而 branchbench 的构建需要 CI 上不可得的 riscv32 工具链 ⇒ PR 层被 night 前置拖红。
    """

    @staticmethod
    def _case(name, rel, layer):
        return {"name": name, "kind": "elf", "relpath": rel, "layer": layer}

    def test_missing_out_of_layer_is_warning_only(self):
        pr = self._case("ok", "CMakeLists.txt", "pr")
        night_missing = self._case("bb", "build/__nope__.elf", "night")
        missing, warnings = pgage.check_corpus_files_exist([pr], [pr, night_missing])
        self.assertEqual(missing, [])
        self.assertEqual(len(warnings), 1)
        self.assertIn("__nope__", warnings[0])

    def test_missing_selected_is_error(self):
        pr_missing = self._case("bad", "build/__nope__.elf", "pr")
        missing, warnings = pgage.check_corpus_files_exist([pr_missing], [pr_missing])
        self.assertEqual(len(missing), 1)
        self.assertEqual(warnings, [])


if __name__ == "__main__":
    unittest.main()
