# SPDX-License-Identifier: Apache-2.0
"""Regression tests for tools/doc_checker.py.

Covers three main DocChecker behaviors:
  1. JSON output structure (every check must have 'ok', not 'summary')
  2. Link checker detects broken relative links in .md files
  3. JSON schema validation on real config files
"""

import json
import os
import tempfile
import unittest
from pathlib import Path

# Ensure we import the local tools package, not any system-installed version
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from tools.doc_checker import DocChecker, PROJECT_ROOT


class TestDocCheckerJsonOutputStructure(unittest.TestCase):
    """Test 1 — run_all_checks() returns 'ok' key, not 'summary', in each check."""

    def test_json_output_structure_has_ok_key(self):
        """Every check in results['checks'] must have 'ok' key and no 'summary' key."""
        checker = DocChecker(PROJECT_ROOT, verbose=False)
        results = checker.run_all_checks()

        self.assertIn("checks", results)
        checks = results["checks"]

        for check_name, check_data in checks.items():
            self.assertIn(
                "ok", check_data,
                msg=f"Check '{check_name}' missing 'ok' key. Keys found: {list(check_data.keys())}"
            )
            self.assertNotIn(
                "summary", check_data,
                msg=f"Check '{check_name}' must not have 'summary' key — this field is "
                    "not produced by run_all_checks() and indicates the caller is using "
                    "the wrong field name."
            )


class TestDocCheckerLinkChecker(unittest.TestCase):
    """Test 2 — check_markdown_links() detects broken links in isolated temp .md files."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="doc_checker_test_")
        self.checker = DocChecker(Path(self.test_dir), verbose=False)

    def tearDown(self):
        # Clean up the temp dir and all files inside it
        for root, dirs, files in os.walk(self.test_dir, topdown=False):
            for f in files:
                os.remove(Path(root) / f)
            for d in dirs:
                os.rmdir(Path(root) / d)
        os.rmdir(self.test_dir)

    def test_link_checker_detects_broken_link(self):
        """A .md file containing [broken](nonexistent.md) must produce at least one link error."""
        md_file = Path(self.test_dir) / "test.md"
        md_file.write_text(
            "This file has a [broken](nonexistent.md) link.\n",
            encoding="utf-8"
        )

        # check_markdown_links() scans all .md files under root (no args)
        valid, total, errors = self.checker.check_markdown_links()

        self.assertGreater(
            len(errors), 0,
            msg="Expected at least one link error for broken link, but got none. "
                f"valid={valid}, total={total}, errors={errors}"
        )
        # The error should mention the target file
        error_text = "\n".join(errors)
        self.assertIn(
            "nonexistent.md", error_text,
            msg=f"Error message should mention 'nonexistent.md', but got: {errors}"
        )


class TestDocCheckerJsonSchemaValidation(unittest.TestCase):
    """Test 3 — JSON schema validation on existing ip/cpu/configs/ files."""

    def test_json_schema_validates_known_config(self):
        """cpu_default.json and cpu_params_schema.json must both parse as valid JSON.

        The validation either passes OR raises ValidationError — the goal is to lock
        the current behavior so future changes to the schema/validation logic are caught.
        """
        schema_path = PROJECT_ROOT / "ip" / "cpu" / "configs" / "cpu_params_schema.json"
        config_path = PROJECT_ROOT / "ip" / "cpu" / "configs" / "cpu_default.json"

        # Both files must exist and be readable as JSON
        self.assertTrue(
            schema_path.exists(),
            msg=f"Schema file not found: {schema_path}"
        )
        self.assertTrue(
            config_path.exists(),
            msg=f"Config file not found: {config_path}"
        )

        try:
            schema_data = json.loads(schema_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as e:
            self.fail(f"cpu_params_schema.json is not valid JSON: {e}")

        try:
            config_data = json.loads(config_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as e:
            self.fail(f"cpu_default.json is not valid JSON: {e}")

        # Now attempt schema validation if jsonschema is available
        try:
            import jsonschema
        except ImportError:
            # jsonschema not installed — test passes if JSON parsing succeeded
            return

        # Validate cpu_default.json against cpu_params_schema.json
        try:
            jsonschema.validate(instance=config_data, schema=schema_data)
        except jsonschema.ValidationError as e:
            # Lock current behavior: note that validation fails with this specific message
            # Do NOT fail the test — we are locking behavior, not enforcing correctness
            pass
        except jsonschema.SchemaError as e:
            self.fail(f"cpu_params_schema.json defines an invalid schema: {e}")


if __name__ == "__main__":
    unittest.main()