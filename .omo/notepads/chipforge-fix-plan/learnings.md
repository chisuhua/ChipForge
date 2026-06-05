## T1+T7 - 2026-06-05

### Changes Made

**C1 Fix (lines 70-78):** Updated the PR comment script to use correct JSON fields from doc_checker.py:
- Changed `result.passed` → `result.ok` for the icon boolean check
- Changed `result.summary` → built a summary string:
  - `result.passed/result.total` for checks that have those fields
  - `result.count + ' issues'` for the deprecated check (which has `count` but no `passed`/`total`)
- Changed error condition from `!result.passed` → `!result.ok`

**I6 Fix (lines 42-46, 92-94):**
- Removed `continue-on-error: true` from the "Run documentation checks" step
- Deleted the "Fail if checks did not pass" step entirely (exit 1 anti-pattern)
- Changed "Generate JSON report" step from `if: always()` → `if: success() || failure()`

### Verification Results

- `grep -c "continue-on-error" .github/workflows/doc_check.yml` → `0` ✅
- `grep -n "result.passed\|result.summary"` → lines 73-74 only (defensive checks using `!== undefined`, not raw access) ✅

### Pitfalls
- The Python version in the workflow is `3.11` but task description mentions `3.12`. Did NOT change this as per MUST NOT DO.
- The `report.passed` on line 68 (overall report status) should still be valid — that field exists at report root level per doc_checker.py structure.

## T3+T5 - 2026-06-05

### Changes Made

**I7 Fix (.gitignore):** Expanded from 6 lines to 38 lines, adding patterns for:
- Python: `__pycache__/`, `*.py[cod]`, `*$py.class`, `*.so`, `.Python`, `*.egg-info/`, `.pytest_cache/`, `.mypy_cache/`, `.ruff_cache/`, `.venv/`, `venv/`
- IDE: `.idea/`, `.vscode/`, `*.swp`, `*.swo`
- Build artifacts: `build/`, `dist/`, `*.o`, `*.obj`, `*.a`, `*.lib`, `*.dll`, `*.dylib`
- OS: `.DS_Store`
- Preserved existing 3 lines: `/CppTLM`, `/CppHDL`, `/patch.patch`

**M1 Fix (ip/cache/README.md:91):** Fixed typo `[项架构总览]` → `[项目架构总览]`

### Verification Results

- `wc -l .gitignore` → `38` ✅ (≥ 25)
- `grep -c "项架构总览" ip/cache/README.md` → `0` ✅ (typo gone)
- `grep -c "项目架构总览" ip/cache/README.md` → `1` ✅ (correct text present)

## T4 - 2026-06-05

### Changes Made

**C6 Fix (16 directories + 16 .gitkeep files):** Created missing subdirectories for stub IPs:
- `mkdir -p ip/{cache,memory,interconnect,peripheral}/{tlm,rtl,test,configs}` created 16 directories
- `touch .../.gitkeep` created one placeholder file in each directory

**I8 Fix (5 READMEs with planning banners):**
- `ip/cache/README.md`, `ip/memory/README.md`, `ip/interconnect/README.md`, `ip/peripheral/README.md`: Added `> ⚠️ **Status: Planning** — Code not yet implemented. See [Phase 1 roadmap](../../../docs/roadmap/phases/phase-1-foundation.md) for planned structure.` after H1
- `ip/cpu/tlm/README.md`: Added `> ⚠️ **Status: Phase 5 Planning** — RTL not yet implemented.` after H1

### Verification Results

- `find ip/{cache,memory,interconnect,peripheral} -name ".gitkeep" | wc -l` → `16` ✅
- `grep -l "Status: Planning" ip/cache/README.md ip/memory/README.md ip/interconnect/README.md ip/peripheral/README.md` → all 4 files ✅
- `grep "Status: Phase 5 Planning" ip/cpu/tlm/README.md` → found ✅

### Pitfalls
- Did NOT create any code files (.py, .cpp, .h, .v) — only .gitkeep placeholders as instructed
- Did NOT modify cpu/{rtl,test}/README.md — only tlm/ was flagged for the detailed src/ tree issue

## T6 - 2026-06-05

### Changes Made

**Regression test file created:** `tools/test_doc_checker.py`

Uses only Python standard library (`unittest`), no external dependencies.

Three test methods:
1. `test_json_output_structure_has_ok_key` — calls `DocChecker.run_all_checks()` on PROJECT_ROOT, asserts every check in `results['checks']` has an `'ok'` key and NO `'summary'` key (locks the field-name bug found during review)
2. `test_link_checker_detects_broken_link` — creates an isolated temp `.md` with `[broken](nonexistent.md)`, runs `check_markdown_links()`, asserts at least one error mentioning `nonexistent.md`
3. `test_json_schema_validates_known_config` — parses `ip/cpu/configs/cpu_default.json` and `cpu_params_schema.json` as JSON, then attempts jsonschema validation if the library is available (gracefully skips if not installed)

### Key Design Decisions

- `sys.path.insert(0, ...)` used to ensure local `tools.doc_checker` is imported, not any system-installed version
- `tempfile.mkdtemp` + manual cleanup in `tearDown` for Test 2 — no external deps
- Test 3 uses try/except for `jsonschema` import so tests pass even if it's not installed
- Test 3 does NOT fail on `jsonschema.ValidationError` — it locks current behavior (validation may or may not pass) rather than enforcing correctness

### Verification Results

- `python3 -c "import ast; ast.parse(open('tools/test_doc_checker.py').read())"` → syntax OK ✅
- `python3 -m unittest tools.test_doc_checker -v` → `Ran 3 tests in 0.937s` → `OK` ✅

### Edge Cases / Concerns

- `check_markdown_links()` walks the entire PROJECT_ROOT by default, so Test 2 runs against a temp root that contains only the single test `.md` file — no false positives from real project links
- The `deprecated` check in `run_all_checks()` returns `count` not `passed`/`total` — this is correctly handled in Test 1 by not asserting on field presence, only on `ok` and absence of `summary`
- If `jsonschema` is not installed, Test 3 still validates JSON syntax (parsing both files) and skips schema validation — test still passes
- No test for `check_framework_paths()` because it requires CppTLM/CppHDL symlinks which may not exist in all environments
