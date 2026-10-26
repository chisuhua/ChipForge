# ⑤-前置2 chmem-bp-btb-gshare-poc — 执行文件

> **change**: `chmem-bp-btb-gshare-poc`（B1 ② 的**机制 PoC**）
> **规模**: 小-中 · **前置**: `chmem-branch-microbench-fix`（★ 修 0c 的工作量污染）
> **权威规格**: `ip/cpu/docs/bp-btb-gshare-design-brief.md` **v2.2** §2.1.2 / §3.4.2 / §4.2
> **设计文档**: `openspec/changes/chmem-bp-btb-gshare/design.md`（五级 mux 契约 + 三步时序 + flush 门控）
> ★ **AC 清单**: `openspec/changes/chmem-bp-btb-gshare-poc/tasks.md`（AC-F2-1 … AC-F2-17）
> ★ **owner 表**: 见本 change 的 tasks.md（★ 本 change = 前置2 owner：`branch_predictor_chmem.h` 最小版 / `ibus_chmem.h` 首次改 / `cpu_factory_chmem.h` 首次注册 / FLUSH 计数）

---

## Goal

验证「**fetch 期提前一拍预测 ⇒ 消除 taken 转移气泡**」这个**机制**在 CH_MEM 上**是否成立**。

★ **若机制不成立，② 主体 MUST NOT 开工**（否则会造出「测试能绿、性能零变化」的惰性 BP —— 正是 ADR-040 §10.5 反复防的那类假绿）。

---

## Tasks

- [ ] **Task 1** 最小 BTB（`branch_predictor_chmem.h` 最小版：BTB 256×64b 62b 编码 + Bimodal 2-bit PHT 256×2b）+ fetch NORMAL **0 拍**（`aread`）查表 + payload 产出（`pred_target` / `pred_taken` / `pred_valid`）+ `ch_reg` 存预测记录
- [ ] **Task 2** `ibus_chmem.h` 的 **5 臂显式 select 链**（情形 A/B/C + actual + seq，伪码见 design.md §2.2）+ ★ **FLUSH 门控**（`flush = hold_z | (taken_buf && !bp_spec_taken_buf) | mispredict_A_buf` ⇒ BP 关闭时与现状逐位相同 ⇒ **零退化**）
- [ ] **Task 3** 误预测纠错（★ **按情形 A/B 区分**：A ⇒ `pc_lag+4` + flush；B ⇒ `actual_target` + **flush（MUST 抑制）**；C ⇒ 抑制自身 redirect + **MUST NOT 抑制**；E（BTB miss 且实际跳转）⇒ **MUST 抑制**）
- [ ] **Task 4** 测量设施（★ **FLUSH 事件数计数器归本 change** —— DUT 内部 payload ⇒ host 不可见 ⇒ 须改 `ibus_chmem.h` ⇒ 本 change 是首个可改 `ip/` 的 change；★ **MUST 复用**前置1 的 `--stats` 框架；★ 捕获 stderr ⇒ 「flag 未生效却测出无差异」的假绿通道是开着的）+ BP 开关面（工厂参数 + CLI `--bp=on|off`）
- [ ] **Task 5** 判定（★ **AC-F2-3 的量化阈值** + ★ **AC-F2-16 的功能等价**（无此项则 PoC 可在「功能已坏」的机器上判「机制成立」⇒ 假阳性）+ ★ **AC-F2-17 的 flush 时序对**）⇒ 成立 ⇒ 进 ② 主体；不成立 ⇒ 落 ADR-088 §5.3.2 结论 + 按 P-3 临时口径结案 + **关闭本 change**（不留悬空项）

## ★ 兜底句纪律

任何「上报 architect / 待裁决」句 MUST 同句给出 **① 触发阈值 ② 落点文件 ③ 可判定时限**（「≤ 2 个工作日」或「本 change 内闭环」）；MUST NOT 以「等待回复」结束本 change。

## ★ 不可触碰（owner 表）

`tests/cpu/manual_elf/branchbench/` · `tools/measure_branchbench.sh` 的**模式名数组与表头解析逻辑**（前置1 的范围）· `docs/architecture/tlm-coverage-parity-ledger.md`（主体 owner）· ADR-088 §5.3.2 的**绝对值门禁结论段**（主体 owner；★ 本 change 只写**机制结论段**）。

## 实测留证

（实施后填写）
