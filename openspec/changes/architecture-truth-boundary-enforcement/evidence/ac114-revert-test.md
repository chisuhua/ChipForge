# AC-114 回滚验证（两批实测 `git revert`）

> 方法：每个批次的 commit 在**其自身提交点**用独立 `git worktree` 检出（避免污染工作区），
> `git revert --no-commit HEAD` 后跑三个检查。worktree 内补 `CppTLM` / `CppHDL` symlink
>（二者未被 git 跟踪，是本地 dev 依赖）。

## 批 1（含 3 节点环 · 也是最大批 · 6 份）— `00e239a`

```
$ git worktree add --detach /tmp/opencode/wt-rev1 00e239a
$ git revert --no-commit HEAD          # 无冲突，干净完成
```

| 检查 | 结果 |
|---|---|
| `doc_link_check.sh` | ✅ 0 broken（markdown + 反引号两类） |
| `verify_adr.sh` | ✅ FAILED 0 · STALE 0 · pass verification |
| `arch_paths` liveness | ✅ 无死条目（原 16 条**全部复原**，含 `testing-and-dse.md` / `plugin-framework.md` / `code-framework-mapping.md` / `tech-selection.md` / `declarative-hybrid-framework.md` / `tlm-coverage-parity-ledger.md`） |

位置复原：顶层 `docs/architecture/*.md` **12 → 18** 份；
`docs/architecture/plugin-framework.md` 与 `docs/architecture/performance-guide.md` 均回到顶层。

⇒ **AC-114①② 满足**（引用与 `arch_paths` 全部复原 · 三检查全绿）。

## 批 5（`ip-catalog.md` → `ip/`）— `f4361ab`

```
$ git worktree add --detach /tmp/opencode/wt-rev5 f4361ab
$ git revert --no-commit HEAD          # 无冲突，干净完成
```

| 检查 | 结果 |
|---|---|
| `doc_link_check.sh` | ✅ 0 broken |
| `verify_adr.sh` | ✅ FAILED 0 · pass verification |
| `arch_paths` liveness | ⚠️ **6 条死条目**（不是 revert 造成） |

位置复原：`ip/ip-catalog.md` → `docs/architecture/ip-catalog.md`（`ip/` 下已无该文件）。

★ ★ **那 6 条死条目的归因（实测，非推断）**：`f4361ab` 这个提交点上的
`arch-docs.json` **仍是旧的 16 条逐份表达**，而批 1~4 已把其中 6 份移入 `mixed/` /
`archive/`。即：**该点本就有 6 条死条目**，与批 5 的 revert 无关
（配置改版 AC-108 在更晚的 `28b01db` 落地）。

## ★ AC-114③ 的边界（实测推翻其字面表述）

AC-114③ 写「其余批由『单 commit 可 revert』结构性保证」。实测**只在「该 commit 自身时点」成立**：

```
$ git worktree add --detach /tmp/opencode/wt-rev5b 28b01db   # 含 AC-108 配置改版
$ git revert --no-commit f4361ab                             # 自动合并成功
$ bash tools/doc_link_check.sh
BROKEN	docs/architecture/README.md:41	'`ip/ip-catalog.md`' -> ../../ip/ip-catalog.md
BROKEN	ip/README.md:10	'`ip/ip-catalog.md`' -> ip-catalog.md
BROKEN	ip/README.md:13	'`ip/ip-catalog.md`' -> ip-catalog.md
Total broken: 4
```

★ 这 4 处断链**全部来自更晚的 commit**：`docs/architecture/README.md`（`c12679f` 新建）与
`ip/README.md`（`fc7f017` 改指针）都**假定 `ip/ip-catalog.md` 已存在**。

⇒ ★ **结论**：单 commit 的可 revert 性是**时点性质**，不是全局性质。
  在历史中间点单独 revert 一个早期移动批，后续依赖它的 commit 必然断链。
  这是迁移顺序的固有性质，不是缺陷 —— 但 AC-114③ 的措辞应据此更正
  （记录于本 change 的 tasks.md「流程偏差 / 判据修正」）。

## 汇总

| 批 | revert 干净 | doc_link | verify_adr | arch_paths 复原 |
|---|---|---|---|---|
| 1（`00e239a`，最大批） | ✅ | ✅ 0 broken | ✅ 0 FAILED | ✅ 16/16 |
| 5（`f4361ab`） | ✅ | ✅ 0 broken | ✅ 0 FAILED | ✅ 位置复原（死条目为该点既有） |

★ HEAD 上的 `arch_paths` liveness（独立复核）：3 条目录级（`docs/architecture` /
`ip/mmu/docs/adr` / `ip/cache/docs/adr`）+ 4 条 exclude ⇒ **0 死条目**
（仅 `build/` 因本地未构建而不存在，属环境项，不在 `arch_paths`）。
