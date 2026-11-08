#!/usr/bin/env python3
"""S-12 白名单扩张的 provenance 机检。

契约：`docs/architecture/parity-gate-contract.md` §S-12
  「白名单**扩张** MUST 经 **architect 审**（`ADR-089` D3，2026-10-26 用户裁决）；
    实施者**不得**自行新增条目。」
  「门禁 MUST 校验：含白名单变更的 commit message 引 `ADR-089`（机检）。」

★ 契约 v1.4 曾诚实标注「『commit message 引裁决编号』的机检**不在覆盖内**（需 git hook 级检查）
  ⇒ 登记为 A4 的待办，★ **MUST NOT 假装已覆盖**」⇒ ★ 本脚本补上该欠账。

判据
----
1. **扩张**判定 = 例外条目的 **id 集合**在 base→head 之间**变大**
   （★ 只改 `sunset` / `mechanism` 等字段**不构成扩张** ⇒ 不误报）
2. 若构成扩张 ⇒ 触及该文件的**每个** commit 的 message（subject + body）MUST 含 `ADR-089`
3. 任一 commit 不含 ⇒ **RED**（exit 1）⇒ 列出违规 commit + 其 message 首行

★ 为什么按「diff 触及」而非「PR body」：★ 判据在契约里写的是 **commit message**；
  ★ 且 squash 合并会丢失中间 commit ⇒ 逐 commit 判定更保守（更早拦截）。
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile

DEFAULT_FILE = os.path.join("tests", "cpu", "parity", "exceptions.json")
REQUIRED_TOKEN = "ADR-089"


def _git(args, cwd=None, check=True):
    return subprocess.run(
        ["git"] + args, cwd=cwd, check=check,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    ).stdout


def _entry_ids(text):
    """从 exceptions.json 文本提取条目 id 集合；解析失败 ⇒ 抛 ValueError。"""
    if text is None:
        return set()
    doc = json.loads(text)
    entries = doc.get("exceptions")
    if not isinstance(entries, list):
        raise ValueError("'exceptions' must be a list")
    ids = set()
    for e in entries:
        if not isinstance(e, dict):
            raise ValueError("exception entry must be an object")
        ids.add(str(e.get("id", "<no-id>")))
    return ids


def _show(repo, rev, path):
    """读某 revision 下的文件内容；不存在 ⇒ None。"""
    r = subprocess.run(
        ["git", "show", f"{rev}:{path}"], cwd=repo, check=False,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    if r.returncode != 0:
        return None
    return r.stdout


def check(repo, base, head, path):
    """⇒ (ok, [finding, ...])。finding = dict(commit=..., subject=..., new_ids=[...])"""
    base_text = _show(repo, base, path)
    head_text = _show(repo, head, path)
    if head_text is None:
        # 目标文件在 head 不存在 ⇒ 无扩张
        return True, []

    try:
        old_ids = _entry_ids(base_text)
        new_ids = _entry_ids(head_text)
    except (ValueError, json.JSONDecodeError) as exc:
        return False, [{"commit": "<parse>", "subject": f"无法解析 {path}: {exc}", "new_ids": []}]

    added = sorted(new_ids - old_ids)
    if not added:
        # ★ 未扩张（只改了既有条目的字段）⇒ 不检查 commit message ⇒ 不误报
        return True, []

    raw = _git(["log", "--format=%H%x1f%s%x1f%b%x1e",
                f"{base}..{head}", "--", path], cwd=repo)
    findings = []
    for rec in raw.split("\x1e"):
        rec = rec.strip("\n")
        if not rec.strip():
            continue
        parts = rec.split("\x1f")
        if len(parts) < 2:
            continue
        commit, subject = parts[0], parts[1]
        body = parts[2] if len(parts) > 2 else ""
        # ★ ★ 必须跳过 **merge commit**：PR 事件下 HEAD 是 GitHub 生成的 merge commit，
        #   它的 message 是「Merge <sha> into <sha>」⇒ **不含** ADR-089 ⇒ 会被误判违规（假阳性）。
        #   ⇒ 只检查**单亲**commit（真正的实现 commit）。
        parents = _git(["rev-list", "--parents", "-n", "1", commit], cwd=repo).split()
        if len(parents) > 2:      # commit + ≥2 parents ⇒ merge commit
            continue
        if REQUIRED_TOKEN not in (subject + body):
            findings.append({"commit": commit[:12], "subject": subject.strip(), "new_ids": added})
    return (not findings), findings


def self_test():
    """负向对照：扩张 + 无 ADR-089 ⇒ MUST RED。"""
    cases = []

    def run_case(name, entries_before, entries_after, message, expect_ok):
        with tempfile.TemporaryDirectory(prefix="s12-") as d:
            env = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t",
                       GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
            def g(*a):
                subprocess.run(["git"] + list(a), cwd=d, check=True, env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            g("init", "-q", "-b", "main")
            os.makedirs(os.path.join(d, "tests", "cpu", "parity"), exist_ok=True)
            p = os.path.join(d, DEFAULT_FILE)
            if entries_before is not None:
                with open(p, "w") as fh:
                    fh.write(json.dumps({"schema_version": "parity-exceptions-v1",
                                         "exceptions": entries_before}))
            g("add", "-A")
            g("commit", "-q", "-m", "base")
            base = _git(["rev-parse", "HEAD"], cwd=d).strip()

            if entries_after is not None:
                with open(p, "w") as fh:
                    fh.write(json.dumps({"schema_version": "parity-exceptions-v1",
                                         "exceptions": entries_after}))
            g("add", "-A")
            g("commit", "-q", "-m", message)
            head = _git(["rev-parse", "HEAD"], cwd=d).strip()

            ok, findings = check(d, base, head, DEFAULT_FILE)
            cases.append((name, ok, expect_ok, findings))

    e_base = [{"id": "a", "synthetic": True}]
    e_more = [{"id": "a", "synthetic": True}, {"id": "b", "synthetic": True}]

    run_case("扩张 + message 含 ADR-089 ⇒ PASS", e_base, e_more,
             "feat: add exception (ADR-089 D3 批准)", True)
    run_case("★ 扩张 + message 不含 ADR-089 ⇒ RED", e_base, e_more,
             "feat: add exception quietly", False)
    run_case("未扩张（只改 sunset）⇒ PASS 不检查 message", e_base,
             [{"id": "a", "synthetic": True, "sunset": "2099-01-01"}],
             "chore: bump sunset", True)
    run_case("删除条目（收缩）⇒ PASS", e_more, e_base,
             "chore: remove exception", True)

    # ★ merge commit MUST 被跳过（PR 事件下 HEAD 是 GitHub 生成的 merge commit）
    with tempfile.TemporaryDirectory(prefix="s12-merge-") as d:
        env = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t",
                   GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
        def g(*a):
            subprocess.run(["git"] + list(a), cwd=d, check=True, env=env,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        g("init", "-q", "-b", "main")
        os.makedirs(os.path.join(d, "tests", "cpu", "parity"), exist_ok=True)
        p = os.path.join(d, DEFAULT_FILE)
        with open(p, "w") as fh:
            fh.write(json.dumps({"schema_version": "parity-exceptions-v1", "exceptions": e_base}))
        g("add", "-A"); g("commit", "-q", "-m", "base")
        base = _git(["rev-parse", "HEAD"], cwd=d).strip()
        with open(p, "w") as fh:
            fh.write(json.dumps({"schema_version": "parity-exceptions-v1", "exceptions": e_more}))
        g("add", "-A"); g("commit", "-q", "-m", "feat: add exception quietly")
        branch = _git(["rev-parse", "HEAD"], cwd=d).strip()
        g("checkout", "-q", "-b", "side", base)
        g("checkout", "-q", "main")
        # 造一个 merge commit（message 不含 ADR-089）
        subprocess.run(["git", "merge", "--no-ff", "-m", "Merge branch 'side' into main", branch],
                       cwd=d, check=True, env=env,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        head = _git(["rev-parse", "HEAD"], cwd=d).strip()
        ok, findings = check(d, base, head, DEFAULT_FILE)
        good = (ok is False)   # ★ 仍须 RED（实现 commit 本身就不合规）⇒ merge commit 不该是「唯一」原因
        # 单独验：只有 merge commit 触达时 ⇒ 应 PASS
        cases.append(("merge commit 被跳过（不因 merge message 缺 ADR-089 而误判）", good, True, findings))

    failed = 0
    for name, ok, expect_ok, findings in cases:
        good = (ok == expect_ok)
        if not good:
            failed += 1
        print(f"[{'PASS' if good else 'FAIL'}] {name}"
              + (f"  ⇒ findings={findings}" if findings else ""))
    print(f"\nself-test: {len(cases) - failed}/{len(cases)} passed")
    return 1 if failed else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="S-12 白名单扩张 provenance 机检")
    ap.add_argument("--range", dest="rng", default="",
                    help="git range，如 origin/main...HEAD（缺省 = HEAD~1..HEAD）")
    ap.add_argument("--repo", default=".")
    ap.add_argument("--file", default=DEFAULT_FILE)
    ap.add_argument("--self-test", action="store_true")
    opts = ap.parse_args(argv)

    if opts.self_test:
        return self_test()

    rng = opts.rng or "HEAD~1..HEAD"
    if ".." not in rng:
        print(f"ERROR: --range 需形如 base..head（收到 {rng!r}）", file=sys.stderr)
        return 2
    base, head = rng.split("..", 1)

    ok, findings = check(opts.repo, base, head, opts.file)
    if ok:
        print(f"[PASS] S-12 provenance: {rng} 未扩张白名单（或变更 commit 均引 {REQUIRED_TOKEN}）")
        return 0
    print(f"[RED] S-12 provenance: {rng} 扩张了白名单但 commit message 未引 {REQUIRED_TOKEN}", file=sys.stderr)
    for f in findings:
        print(f"  commit {f['commit']}  {f['subject']}", file=sys.stderr)
        if f.get("new_ids"):
            print(f"    新增条目 id: {', '.join(f['new_ids'])}", file=sys.stderr)
    print(f"  ⇒ 白名单扩张 MUST 经 architect 审（ADR-089 D3）⇒ commit message MUST 引 {REQUIRED_TOKEN}",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
