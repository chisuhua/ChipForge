#!/usr/bin/env python3
"""ChipForge 文档健康度自动检查工具

功能：
  1. IP 目录结构合规检查
  2. 必需文档存在性检查
  3. Markdown 链接有效性检查
  4. JSON Schema 校验
  5. 旧文件名残留检测
  6. 代码框架映射路径验证

用法：
  python tools/doc_checker.py [--format text|json] [--verbose] [--check all|files|links|ip|json|deprecated|framework]
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import List, Dict, Tuple, Optional

# ============================================================
# 配置常量
# ============================================================

# 项目根目录（脚本位于 tools/ 下，向上一层即项目根）
PROJECT_ROOT = Path(__file__).resolve().parent.parent

# 每个 IP 子目录应包含的文件
REQUIRED_IP_FILES = ["README.md"]

# 成熟 IP 应具备的目录结构
EXPECTED_IP_DIRS = ["tlm", "rtl", "test", "configs"]

# 必需的文档文件列表
REQUIRED_DOCS = [
    "docs/GLOSSARY.md",
    "docs/DEVELOPMENT_SETUP.md",
    "docs/architecture/overview.md",
    "docs/architecture/background-and-goals.md",
    "docs/architecture/interface-design.md",
    "docs/architecture/tech-selection.md",
    "docs/architecture/testing-and-dse.md",
    "docs/architecture/error-handling.md",
    "docs/architecture/performance-guide.md",
    "docs/architecture/code-framework-mapping.md",
    "docs/roadmap/README.md",
    "docs/roadmap/references.md",
    "docs/templates/IP_TEMPLATE.md",
]

# 旧文件名/引用的检测模式
DEPRECATED_PATTERNS = [
    ("backgroud_and_goals", "拼写错误的旧文件名"),
    ("interface_design", "下划线风格的旧文件名"),
    ("testing-dse", "缺少 'and' 的旧文件名"),
    ("implementation/", "旧目录名"),
    ("sc_module", "旧框架引用（应为 ChStreamModuleBase）"),
]


class DocChecker:
    """文档检查器主类"""

    def __init__(self, project_root: Path, verbose: bool = False):
        self.root = project_root
        self.verbose = verbose
        # 收集检查过程中的详细信息
        self._details: Dict[str, List[str]] = {}

    # --------------------------------------------------------
    # 1. 必需文件存在性检查
    # --------------------------------------------------------
    def check_required_files(self) -> Tuple[int, int, List[str]]:
        """检查必需文件存在性

        Returns:
            (通过数, 总数, 错误详情列表)
        """
        errors: List[str] = []
        total = len(REQUIRED_DOCS)
        passed = 0

        for doc_path in REQUIRED_DOCS:
            full_path = self.root / doc_path
            if full_path.exists():
                passed += 1
            else:
                errors.append(f"  ✗ 缺少文件: {doc_path}")

        if self.verbose and not errors:
            print(f"  [文件完整性] 全部 {total} 个必需文件存在")

        return passed, total, errors

    # --------------------------------------------------------
    # 2. IP 目录结构合规检查
    # --------------------------------------------------------
    def check_ip_structure(self) -> Tuple[int, int, List[str]]:
        """检查 IP 目录结构合规性

        对于已有代码的 IP（含有 EXPECTED_IP_DIRS 中任一目录），检查完整性。
        对于规划中的 IP，只检查 README.md。

        Returns:
            (通过数, 总数, 错误详情列表)
        """
        errors: List[str] = []
        ip_dir = self.root / "ip"

        if not ip_dir.exists():
            return 0, 1, ["  ✗ ip/ 目录不存在"]

        # 获取 ip/ 下的子目录（排除文件）
        ip_subdirs = [d for d in ip_dir.iterdir() if d.is_dir()]
        total = len(ip_subdirs)
        passed = 0

        for ip_path in sorted(ip_subdirs):
            ip_name = ip_path.name
            ip_errors: List[str] = []

            # 检查必需文件
            for req_file in REQUIRED_IP_FILES:
                if not (ip_path / req_file).exists():
                    ip_errors.append(f"  ✗ ip/{ip_name}/: 缺少 {req_file}")

            # 判断是否为成熟 IP（已有代码子目录）
            has_code_dirs = any((ip_path / d).exists() for d in EXPECTED_IP_DIRS)
            if has_code_dirs:
                # 成熟 IP 应具备所有期望目录
                for exp_dir in EXPECTED_IP_DIRS:
                    if not (ip_path / exp_dir).exists():
                        ip_errors.append(f"  ✗ ip/{ip_name}/: 缺少目录 {exp_dir}/")

            if ip_errors:
                errors.extend(ip_errors)
            else:
                passed += 1

        return passed, total, errors

    # --------------------------------------------------------
    # 3. Markdown 链接有效性检查
    # --------------------------------------------------------
    def check_markdown_links(self) -> Tuple[int, int, List[str]]:
        """检查 Markdown 链接有效性

        解析所有 .md 文件中的相对链接，检查目标是否存在。
        跳过：http/https 外部链接、锚点链接(#section)、代码块内的内容。

        Returns:
            (有效链接数, 总链接数, 错误详情列表)
        """
        errors: List[str] = []
        total_links = 0
        valid_links = 0

        # Markdown 链接正则：[text](path)
        link_pattern = re.compile(r'\[([^\]]*)\]\(([^)]+)\)')

        # 收集所有 .md 文件（排除 attic/ 和符号链接目录）
        md_files = self._find_md_files()

        for md_file in md_files:
            try:
                content = md_file.read_text(encoding="utf-8")
            except (OSError, UnicodeDecodeError):
                continue

            # 标记哪些行在代码块内
            lines = content.splitlines()
            in_code_block = False

            for line_num, line in enumerate(lines, start=1):
                # 检测代码围栏（``` 或 ~~~）
                stripped = line.strip()
                if stripped.startswith("```") or stripped.startswith("~~~"):
                    in_code_block = not in_code_block
                    continue

                # 跳过代码块内的行
                if in_code_block:
                    continue

                for match in link_pattern.finditer(line):
                    link_target = match.group(2).strip()

                    # 跳过外部链接
                    if link_target.startswith(("http://", "https://", "mailto:")):
                        continue

                    # 跳过纯锚点链接
                    if link_target.startswith("#"):
                        continue

                    total_links += 1

                    # 去除锚点部分（如 file.md#section）
                    link_path = link_target.split("#")[0]
                    if not link_path:
                        # 只有锚点，已被前面跳过逻辑处理
                        valid_links += 1
                        continue

                    # 基于文件所在目录解析相对路径
                    resolved = (md_file.parent / link_path).resolve()

                    if resolved.exists():
                        valid_links += 1
                    else:
                        rel_md = md_file.relative_to(self.root)
                        errors.append(
                            f"  ✗ {rel_md}:{line_num} -> {link_target} (文件不存在)"
                        )

        return valid_links, total_links, errors

    # --------------------------------------------------------
    # 4. JSON Schema 校验
    # --------------------------------------------------------
    def check_json_schemas(self) -> Tuple[int, int, List[str]]:
        """检查 JSON Schema 校验

        - 检查 ip/*/configs/ 下的 JSON 文件格式是否有效
        - 如果存在 *_schema.json 或 *params_schema.json，用它校验同目录下的其他 .json

        Returns:
            (通过数, 总数, 错误详情列表)
        """
        errors: List[str] = []
        total = 0
        passed = 0

        # 尝试导入 jsonschema（可选依赖）
        try:
            import jsonschema
            has_jsonschema = True
        except ImportError:
            has_jsonschema = False

        ip_dir = self.root / "ip"
        if not ip_dir.exists():
            return 0, 0, []

        for ip_path in sorted(ip_dir.iterdir()):
            if not ip_path.is_dir():
                continue
            configs_dir = ip_path / "configs"
            if not configs_dir.exists():
                continue

            # 收集所有 JSON 文件
            json_files = sorted(configs_dir.glob("*.json"))
            if not json_files:
                continue

            # 查找 schema 文件
            schema_file: Optional[Path] = None
            schema_data = None
            for jf in json_files:
                if "schema" in jf.stem.lower():
                    schema_file = jf
                    break

            # 先验证 schema 文件本身
            if schema_file:
                total += 1
                try:
                    schema_data = json.loads(schema_file.read_text(encoding="utf-8"))
                    passed += 1
                except json.JSONDecodeError as e:
                    rel_path = schema_file.relative_to(self.root)
                    errors.append(f"  ✗ {rel_path}: JSON 语法错误 - {e}")
                    schema_data = None

            # 验证其他 JSON 文件
            for jf in json_files:
                if jf == schema_file:
                    continue
                total += 1
                rel_path = jf.relative_to(self.root)

                # 先检查 JSON 语法
                try:
                    data = json.loads(jf.read_text(encoding="utf-8"))
                except json.JSONDecodeError as e:
                    errors.append(f"  ✗ {rel_path}: JSON 语法错误 - {e}")
                    continue

                # 如果有 schema 且 jsonschema 可用，进行校验
                if schema_data and has_jsonschema:
                    try:
                        jsonschema.validate(instance=data, schema=schema_data)
                        passed += 1
                    except jsonschema.ValidationError as e:
                        errors.append(f"  ✗ {rel_path}: Schema 违规 - {e.message}")
                    except jsonschema.SchemaError as e:
                        errors.append(f"  ✗ {rel_path}: Schema 定义错误 - {e.message}")
                else:
                    # 无 schema 或无 jsonschema 库，JSON 语法正确即通过
                    passed += 1

        return passed, total, errors

    # --------------------------------------------------------
    # 5. 旧文件名残留检测
    # --------------------------------------------------------
    def check_deprecated_patterns(self) -> Tuple[int, List[str]]:
        """检查旧文件名残留

        扫描项目中的文件名和 .md 文件内容，检测旧命名模式残留。
        - 文件路径：检查所有模式
        - 文件内容：跳过代码块，对 sc_module 只在非架构文档中检查
          （架构文档中可合理地引用旧框架进行对比说明）

        Returns:
            (残留数, 错误详情列表)
        """
        errors: List[str] = []

        # 收集所有需检查的文件（排除 attic/、.git/、符号链接目录）
        files_to_check = self._find_project_files()

        # 判断文件是否为架构/设计文档（允许引用旧框架作比较）
        docs_arch_dir = self.root / "docs" / "architecture"

        for file_path in files_to_check:
            rel_path = file_path.relative_to(self.root)
            rel_str = str(rel_path)

            # 检查文件名/路径中是否包含旧模式
            for pattern, desc in DEPRECATED_PATTERNS:
                # 对 "testing-dse" 模式需要特殊处理：
                # 排除 "testing-and-dse" 这样的正确命名
                if pattern == "testing-dse":
                    if pattern in rel_str and "testing-and-dse" not in rel_str:
                        errors.append(f"  ✗ 文件路径残留: {rel_path} ({desc})")
                else:
                    if pattern in rel_str:
                        errors.append(f"  ✗ 文件路径残留: {rel_path} ({desc})")

            # 对 .md 文件检查内容中的旧引用（跳过代码块）
            if file_path.suffix == ".md":
                try:
                    content = file_path.read_text(encoding="utf-8")
                except (OSError, UnicodeDecodeError):
                    continue

                # 判断是否为架构文档
                is_arch_doc = self._is_subpath(file_path, docs_arch_dir)

                in_code_block = False
                for line_num, line in enumerate(content.splitlines(), start=1):
                    # 检测代码围栏
                    stripped = line.strip()
                    if stripped.startswith("```") or stripped.startswith("~~~"):
                        in_code_block = not in_code_block
                        continue

                    # 跳过代码块内的行
                    if in_code_block:
                        continue

                    for pattern, desc in DEPRECATED_PATTERNS:
                        # sc_module 在架构文档中可能是合理的对比引用
                        if pattern == "sc_module" and is_arch_doc:
                            continue

                        if pattern == "testing-dse":
                            if pattern in line and "testing-and-dse" not in line:
                                errors.append(
                                    f"  ✗ {rel_path}:{line_num} 含旧引用 '{pattern}' ({desc})"
                                )
                        else:
                            if pattern in line:
                                errors.append(
                                    f"  ✗ {rel_path}:{line_num} 含旧引用 '{pattern}' ({desc})"
                                )

        return len(errors), errors

    # --------------------------------------------------------
    # 6. 代码框架映射路径验证
    # --------------------------------------------------------
    def check_framework_paths(self) -> Tuple[int, int, List[str]]:
        """检查代码框架映射路径

        读取 docs/architecture/code-framework-mapping.md，
        提取其中引用的 CppTLM/CppHDL 头文件路径，
        检查这些路径是否在实际代码中存在（通过符号链接）。

        Returns:
            (有效路径数, 总路径数, 错误详情列表)
        """
        errors: List[str] = []
        mapping_file = self.root / "docs" / "architecture" / "code-framework-mapping.md"

        if not mapping_file.exists():
            return 0, 0, ["  ✗ code-framework-mapping.md 不存在"]

        content = mapping_file.read_text(encoding="utf-8")

        # 提取 CppTLM/... 和 CppHDL/... 路径
        # 匹配反引号中的路径，如 `CppTLM/include/core/sim_object.hh`
        path_pattern = re.compile(r'`(Cpp(?:TLM|HDL)/[^`]+)`')
        paths_found = path_pattern.findall(content)

        # 去重
        unique_paths = sorted(set(paths_found))
        total = len(unique_paths)
        valid = 0

        for framework_path in unique_paths:
            full_path = self.root / framework_path

            # 检查路径是否存在（文件或目录）
            # 注意：对于目录路径（如 CppHDL/include/lnode/），检查目录是否存在
            if full_path.exists():
                valid += 1
            else:
                # 可能是目录引用（不带尾斜杠），也检查一下
                errors.append(f"  ✗ 框架路径不存在: {framework_path}")

        return valid, total, errors

    # --------------------------------------------------------
    # 运行所有检查并返回结果
    # --------------------------------------------------------
    def run_all_checks(self) -> Dict:
        """运行所有检查并返回结果"""
        results: Dict = {"checks": {}, "passed": True}

        # 1. 文件完整性检查
        passed, total, errs = self.check_required_files()
        results["checks"]["files"] = {
            "name": "文件完整性检查",
            "passed": passed,
            "total": total,
            "errors": errs,
            "ok": passed == total,
        }

        # 2. 链接有效性检查
        valid, total, errs = self.check_markdown_links()
        results["checks"]["links"] = {
            "name": "链接有效性检查",
            "passed": valid,
            "total": total,
            "errors": errs,
            "ok": valid == total,
        }

        # 3. IP 结构合规检查
        passed, total, errs = self.check_ip_structure()
        results["checks"]["ip"] = {
            "name": "IP 结构合规检查",
            "passed": passed,
            "total": total,
            "errors": errs,
            "ok": passed == total,
        }

        # 4. JSON Schema 校验
        passed, total, errs = self.check_json_schemas()
        results["checks"]["json"] = {
            "name": "JSON Schema 校验",
            "passed": passed,
            "total": total,
            "errors": errs,
            "ok": passed == total,
        }

        # 5. 旧引用残留检查
        count, errs = self.check_deprecated_patterns()
        results["checks"]["deprecated"] = {
            "name": "旧引用残留检查",
            "count": count,
            "errors": errs,
            "ok": count == 0,
        }

        # 6. 框架路径验证
        valid, total, errs = self.check_framework_paths()
        results["checks"]["framework"] = {
            "name": "框架路径验证",
            "passed": valid,
            "total": total,
            "errors": errs,
            "ok": valid == total,
        }

        # 计算总体是否通过
        results["passed"] = all(c["ok"] for c in results["checks"].values())

        return results

    def run_single_check(self, check_name: str) -> Dict:
        """运行单个指定检查"""
        results: Dict = {"checks": {}, "passed": True}

        check_map = {
            "files": self._run_files_check,
            "links": self._run_links_check,
            "ip": self._run_ip_check,
            "json": self._run_json_check,
            "deprecated": self._run_deprecated_check,
            "framework": self._run_framework_check,
        }

        if check_name in check_map:
            check_map[check_name](results)

        results["passed"] = all(c["ok"] for c in results["checks"].values())
        return results

    def _run_files_check(self, results: Dict):
        passed, total, errs = self.check_required_files()
        results["checks"]["files"] = {
            "name": "文件完整性检查", "passed": passed,
            "total": total, "errors": errs, "ok": passed == total,
        }

    def _run_links_check(self, results: Dict):
        valid, total, errs = self.check_markdown_links()
        results["checks"]["links"] = {
            "name": "链接有效性检查", "passed": valid,
            "total": total, "errors": errs, "ok": valid == total,
        }

    def _run_ip_check(self, results: Dict):
        passed, total, errs = self.check_ip_structure()
        results["checks"]["ip"] = {
            "name": "IP 结构合规检查", "passed": passed,
            "total": total, "errors": errs, "ok": passed == total,
        }

    def _run_json_check(self, results: Dict):
        passed, total, errs = self.check_json_schemas()
        results["checks"]["json"] = {
            "name": "JSON Schema 校验", "passed": passed,
            "total": total, "errors": errs, "ok": passed == total,
        }

    def _run_deprecated_check(self, results: Dict):
        count, errs = self.check_deprecated_patterns()
        results["checks"]["deprecated"] = {
            "name": "旧引用残留检查", "count": count,
            "errors": errs, "ok": count == 0,
        }

    def _run_framework_check(self, results: Dict):
        valid, total, errs = self.check_framework_paths()
        results["checks"]["framework"] = {
            "name": "框架路径验证", "passed": valid,
            "total": total, "errors": errs, "ok": valid == total,
        }

    # --------------------------------------------------------
    # 输出格式化
    # --------------------------------------------------------
    def format_report(self, results: Dict, fmt: str = "text") -> str:
        """格式化输出报告

        Args:
            results: run_all_checks() 或 run_single_check() 返回的结果
            fmt: "text" 或 "json"
        """
        if fmt == "json":
            return json.dumps(results, ensure_ascii=False, indent=2)

        lines: List[str] = []
        lines.append("╔══════════════════════════════════════════════════╗")
        lines.append("║          ChipForge 文档健康度检查报告           ║")
        lines.append("╠══════════════════════════════════════════════════╣")
        lines.append("")

        checks = results["checks"]

        for key, check in checks.items():
            if key == "deprecated":
                # 残留检查格式不同
                count = check["count"]
                if check["ok"]:
                    lines.append(f"✓ {check['name']:<16s}\t{count} 个残留")
                else:
                    lines.append(f"✗ {check['name']:<16s}\t{count} 个残留")
                    for err in check["errors"]:
                        lines.append(err)
            else:
                passed = check["passed"]
                total = check["total"]
                if check["ok"]:
                    lines.append(f"✓ {check['name']:<16s}\t{passed}/{total} 通过")
                else:
                    unit = "有效" if key in ("links", "framework") else "通过"
                    lines.append(f"✗ {check['name']:<16s}\t{passed}/{total} {unit}")
                    for err in check["errors"]:
                        lines.append(err)

        lines.append("")
        lines.append("╚══════════════════════════════════════════════════╝")

        # 计算总体评分
        total_checks = len(checks)
        passed_checks = sum(1 for c in checks.values() if c["ok"])
        if total_checks > 0:
            score = int(passed_checks / total_checks * 100)
        else:
            score = 100

        if results["passed"]:
            lines.append(f"总体评分: {score}% (全部通过)")
        else:
            lines.append(f"总体评分: {score}% ({passed_checks}/{total_checks} 类检查通过)")

        return "\n".join(lines)

    # --------------------------------------------------------
    # 辅助方法
    # --------------------------------------------------------
    def _find_md_files(self) -> List[Path]:
        """查找项目中所有 .md 文件（排除 attic/、.git/、符号链接目录中的文件）"""
        md_files: List[Path] = []
        exclude_dirs = {"attic", ".git", "node_modules", "__pycache__"}

        for root, dirs, files in os.walk(self.root, followlinks=False):
            root_path = Path(root)

            # 排除特定目录
            dirs[:] = [
                d for d in dirs
                if d not in exclude_dirs and not (root_path / d).is_symlink()
            ]

            for f in files:
                if f.endswith(".md"):
                    md_files.append(root_path / f)

        return sorted(md_files)

    def _find_project_files(self) -> List[Path]:
        """查找项目中所有文件（排除 attic/、.git/、符号链接目录）"""
        project_files: List[Path] = []
        exclude_dirs = {"attic", ".git", "node_modules", "__pycache__"}

        for root, dirs, files in os.walk(self.root, followlinks=False):
            root_path = Path(root)

            # 排除特定目录和符号链接
            dirs[:] = [
                d for d in dirs
                if d not in exclude_dirs and not (root_path / d).is_symlink()
            ]

            for f in files:
                project_files.append(root_path / f)

        return sorted(project_files)

    @staticmethod
    def _is_subpath(path: Path, parent: Path) -> bool:
        """判断 path 是否在 parent 目录下"""
        try:
            path.resolve().relative_to(parent.resolve())
            return True
        except ValueError:
            return False


def main():
    parser = argparse.ArgumentParser(
        description="ChipForge 文档健康度检查",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""示例:
  python tools/doc_checker.py                  # 运行所有检查（文本输出）
  python tools/doc_checker.py --format json    # JSON 格式输出
  python tools/doc_checker.py --check links    # 只检查链接
  python tools/doc_checker.py -v               # 详细模式
""",
    )
    parser.add_argument(
        "--format",
        choices=["text", "json"],
        default="text",
        help="输出格式 (默认: text)",
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="详细模式，输出更多过程信息",
    )
    parser.add_argument(
        "--check",
        choices=["all", "files", "links", "ip", "json", "deprecated", "framework"],
        default="all",
        help="指定运行的检查项 (默认: all)",
    )
    args = parser.parse_args()

    checker = DocChecker(PROJECT_ROOT, verbose=args.verbose)

    if args.check == "all":
        results = checker.run_all_checks()
    else:
        results = checker.run_single_check(args.check)

    report = checker.format_report(results, args.format)
    print(report)

    # 如有失败则退出码为 1（供 CI 使用）
    sys.exit(0 if results["passed"] else 1)


if __name__ == "__main__":
    main()
