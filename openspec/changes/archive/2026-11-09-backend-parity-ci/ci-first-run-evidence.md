# A4 首次 CI 实跑证据（判据 4 / 判据 7 的完成形式）

> 本文件记录 `backend-parity-ci` 的 **CI 侧真实执行**证据。
> 判据来源：`docs/architecture/adr/ADR-088-tlm-backend-retirement.md` §5.2 判据 4 / 判据 7 ·
> `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md:142`（G-A4）

## 结论

| 判据 | 完成形式 | 证据 |
|---|---|---|
| **判据 7**（CI 具备 build 能力） | CI 内 `cmake` configure + 构建 `parity_sim_runner` / `cpu_verilator_sim` 成功 | run **42473424285** 的 step 7~10 全 ✓ |
| **判据 4**（门禁在 CI 中被执行且结果被回读） | 门禁**实际执行**（非 SKIP）+ `report.json` 的 verdict 被回读 + artifact 上传 | 同 run 的 step 11~13 全 ✓；artifact `parity-report-pr` |

★ 该 run 的 13 个 step **全部 success**（含 `Run parity gate` / `Read back verdict` / `Upload parity report`）。

## 关键数据（artifact 回读）

```json
{
  "verdict": "PASS",
  "error_kind": null,
  "layer": "pr",
  "commit": "c10041ac13bc",
  "timings": { "count": 8, "p95": 30.851 },
  "jobs": 4
}
```

- run id：**42473424285**（`workflow_dispatch`, layer=pr, ref=main）
- artifact：`parity-report-pr`（含 `report.json` + `stdout.log`）
- 8 个 PR 层 case 全部 PASS；**p95 = 30.851 s**（本地实测 37.0 s，同量级 ⇒ S-15 的 6.5 min 预算成立）

## ★ 三次 run 的过程（如实记录，含两次失败）

| # | run | 结果 | 失败原因（均为实现缺陷，非环境） | 修复 |
|---|---|---|---|---|
| 1 | 42472260469 | **failure** | ① deps 未构建：`No rule to make target '_deps/install/lib/libcpptlm_core.a'` + `ch.hpp: No such file`（ExternalProject 只挂 `all` 链，不是 runner 的依赖）② Verilator 静默未安装（按发行版**精确匹配**分流，runner 上两分支都 skipped） | 加 `Build CppTLM/CppHDL deps` step；Verilator 改「apt 优先 + 源码兜底」 |
| 2 | 42472482213 | **failure** | ① `build/parity/` 目录不存在 ⇒ `tee` 先失败 ② `corpus_missing_files`：`--layer pr` 仍校验 **night 专属**的 4 个 branchbench ELF（其构建需 `riscv32-unknown-elf-g++`，CI 不可得） | 加 `mkdir -p build/parity`；语料存在性改为**按选中集合**判定（选中缺失⇒ERROR，未选中缺失⇒WARNING） |
| 3 | **42473424285** | **success** | — | — |

★ 第 2 次的语料问题属**判据层面的设计缺陷**（S-15「PR 层是快子集」被 night 层前置拖红），
已按「选中集合」修正并留实证：移走 4 个 branchbench ELF 后
`--layer pr --only rv32ui-p-add` = PASS + 4 WARNING，`--layer night --only branchbench_m0` = ERROR/exit 2。

## 对判据 4/6/7 的影响

- ★ **判据 4 满足**：门禁不再「仅本机成立」
- ★ **判据 7 满足**：CI 具备 build 能力（且是**最小构建**：只 build 两个 runner + deps）
- ★ **判据 6 可判定**：门禁结论由 CI artifact 承载，可被 archive / 审稿引用

★ 仍**不属于**本证据范围的：`schedule`（夜间层 52 case）从未跑过 ——
夜间层需 `branchbench_elfs`（依赖 riscv32 工具链，CI 未提供）⇒ 见下方开放项。

## 开放项（诚实登记）

| # | 项 | 影响 | 归属 |
|---|---|---|---|
| O-A4-1 | **夜间层在 CI 不可跑**：`branchbench_elfs` 需要 `riscv32-unknown-elf-g++`，`ubuntu-latest` 的 `gcc-riscv64-unknown-elf` 只提供 `riscv64-unknown-elf-*` | `schedule` 触发的 night run 会在门禁阶段因选中项缺失而 **ERROR**（不会假绿） | 需决定：① CI 装 riscv32 工具链 ② 或把 branchbench 的 ELF 改为 vendored ③ 或夜间层改用本地 runner |
| O-A4-2 | **deps 无缓存**：每次 run 都从源码构建 CppTLM/CppHDL（当前 run 全程约 10 min） | 成本，不是正确性问题 | 加 `actions/cache` on `build/_deps` |
