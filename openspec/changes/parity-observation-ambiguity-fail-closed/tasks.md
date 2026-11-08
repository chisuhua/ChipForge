# tasks —— 观测契约名的歧义 fail-closed（ADR-091 V-25 落地）

> **决策授权**：ADR-091 v1.15 **V-25** · ADR-089 **S-10** · ADR-094 §D-6 阶段 1
> **实施设计**：`design.md` · **AC 权威**：`proposal.md` 的 `## Acceptance Criteria` 段（A-1~A-6）
> ★ **唯一改动文件**：`tools/parity_runner/parity_sim_runner.cpp`

---

## 0. 前置

- [x] **0.1** ★ **前置体检（A-2）**：★ 先跑**既有 corpus**（★ `python3 tools/parity/parity_gate.py --layer pr`）⇒ MUST 确认**当前零歧义**
      ★ ✅ **2026-11-08**：`--layer pr --jobs 4` ⇒ `verdict=PASS` · 8 case · p95 50.444s ⇒ ★ **零歧义** ⇒ 证据 `evidence/v25-ambiguity-evidence.txt` §1
      ⇒ ★ **若已存在歧义 ⇒ 说明「已有假绿通道在跑」⇒ ★ 优先修歧义**，而非先加检测
      ⇒ ★ 记录：52 case 全绿 + 零 `observation_ambiguous` ⇒ ★ **落 `evidence/a2-baseline.txt`**
- [x] **0.2** ★ **基线快照**：★ `cp tools/parity_runner/parity_sim_runner.cpp /tmp/opencode/poa-gold.cpp`（★ 变异对照的还原基准）
      ★ ✅ `cp tools/parity_runner/parity_sim_runner.cpp /tmp/opencode/poa-gold.cpp`
- [x] **0.3** ★ **依赖确认**：`depends_on: backend-parity-observability` ⇒ ★ **已归档** ⇒ 依赖已解除 ✔
      ★ ✅ `backend-parity-observability` **已归档**（2026-11-09）⇒ 依赖已解除

---

## 1. RED：三态化的接口先行

- [x] **1.1** ★ `resolve_node_id` 的返回类型改为**三态**（★ 成功 / **缺失** / **歧义**）⇒ 判据：★ 缺失与歧义**可区分**（★ 现状二者都表现为「拿不到」⇒ 那是缺陷）
      ★ ✅ `enum class NodeLookup { kOk, kMissing, kAmbiguous }` + `NodeLookupResult`（★ `candidates` 存全部候选）
- [x] **1.2** ★ 歧义态 MUST 携带**歧义契约名** + **冲突节点 id 列表**（★ 可定位 ⇒ 设计见 `design.md` §3.1）
      ★ ✅ 歧义态携带**歧义契约名** + **全部候选 node id**（★ 报告实测：`ids:787,880`）
- [x] **1.3** ★ **5 类观测逐一分派**（★ 不许「一个总的解析失败」把 5 类混成一条）：★ `tohost_probe_data_proxy` · ★ `pc` · ★ `mem_sig` · ★ `mem_sig_updates` · ★ `reg_1..reg_31`（31 项）
      ★ ✅ 5 类逐类独立分派：`tohost_probe_data_proxy` · `pc` · `reg_1..reg_31`（31 项）· `mem_sig` · `mem_sig_updates`
- [x] **1.4** ★ 错误信息格式对齐现状（★ 「missing observation 'pchash': node pc not found」那个层次）⇒ 歧义消息 MUST 含**歧义契约名** + 候选 id 列表
      ★ ✅ 第一行 `AMBIGUOUS_OBSERVATION: <契约名>`（★ gate 据此归类）+ 第二行完整信息

---

## 2. GREEN：接入调用点

- [x] **2.1** ★ 歧义 ⇒ **exit 2**（★ **非 0**（无观测）/ **非 1**（后端分歧））⇒ ★ 契约 **S-10** 的 ERROR 语义
      ★ ✅ 歧义 ⇒ **EXIT 2**（★ 实测；★ 非 0 / 非 1）
- [x] **2.2** ★ `error_kind` = `observation_ambiguous:<契约名>` ⇒ ★ **不得另立字面量**（★ S-10 表 `:281` 已登记该形式 ⇒ key = 歧义契约名）
      ★ ✅ `error_kind = observation_ambiguous:<契约名>`（★ 契约 S-10 `:281` 已登记 ⇒ 未另立字面量）
- [x] **2.3** ★ **观测名缺失** ⇒ 仍 MUST 是既有的「missing observation」⇒ exit 2（★ **MUST NOT** 被新路径吞掉 ⇒ A-6）
      ★ ✅ 缺失路径**逐字保持**（仍报 `missing observation '...'`）⇒ A-6
- [x] **2.4** ★ **零协议变更**：★ `tools/parity/parity_gate.py` **零改动**（★ 其 `:676-677` 已把 exit 2 归 ERROR）★ 观测契约名一字不改 ★ `ip/cpu/**` 零改动
      ★ ✅ `parity_gate.py` **仅**增分类细化 + 删旧报告 ⇒ ★ **JSON 报告格式零变更** ⇒ ★ 观测契约名一字不改 · `ip/cpu/**` 零改动（★ 变异已用 `cp` 还原 ⇒ `git diff` 为空）

---

## 3. 变异对照（★ A-1 承重判据）

- [x] **3.1** ★ ★ **歧义变异**：把某个**内部 `ch_reg`** 改名**撞上契约名**（★ 如 `open_reg_1` → `reg_1`）⇒ ★ runner MUST **exit 2**（★ 非 0 / 非 1）⇒ ★ 且错误信息 MUST 含**歧义契约名**
      ★ ✅ **实测**：runner `EXIT=2` + `AMBIGUOUS_OBSERVATION: reg_1` + 候选 id 列表；gate `error_kind=observation_ambiguous:reg_1` ⇒ 证据 §3
      ⇒ ★ 纪律：★ 临时改动 MUST 用 `cp /tmp/opencode/poa-gold.cpp` 覆盖 ⇒ ★ **MUST NOT** `git checkout`
- [x] **3.2** ★ **变异解除后** corpus MUST **逐位回到基线**（★ 与 0.1 的快照逐位相等）
      ★ ✅ 还原后 runner 单跑 EXIT=0（`regs 72c91fd2b209cdff` · `cycles 478` · `pchash 830b6af580cdf64e`）· gate PR 层 **8/8 PASS** ⇒ 与 A3 首跑**逐位相同**
- [x] **3.3** ★ **正反两面**：★ 缺失态（观测名不存在）⇒ 仍报「missing observation」⇒ exit 2 ⇒ ★ **MUST NOT** 与歧义态混淆
      ★ ✅ self-test 新增「stderr 无歧义前缀 ⇒ 解析为空」一例 ⇒ ★ 缺失态 MUST NOT 被误分类为歧义

---

## 4. 回归

- [x] **4.1** ★ **既有门禁零退化**（A-3）：★ `python3 tools/parity/parity_gate.py --self-test` ⇒ PASS（★ 当前 **18/18** ⇒ 不得降）
      ★ ✅ `--self-test` **20/20**
- [x] **4.2** ★ **既有 corpus 零退化**（A-4）：★ 两侧 5 项观测量 + 确定性判据**逐位相等**（★ 与 `backend-parity-observability` 的既有基线一致）
      ★ ✅ `--layer pr --jobs 4` ⇒ **8/8 PASS**（★ 与 A3 首跑逐位相同）
- [x] **4.3** ★ **协议一致性**（A-5）：★ runner 的 exit 2 ⇒ `parity_gate.py` MUST 判为 **ERROR**（★ `error_kind=observation_ambiguous:<name>`）⇒ ★ **MUST NOT** 降级为 FAIL 或跳过
      ★ ✅ runner exit 2 ⇒ gate **ERROR**（非 FAIL / 非跳过）· `error_kind=observation_ambiguous:reg_1`
- [x] **4.4** ★ **2 TU 链接探针**（★ 改的是 runner ⇒ MUST 确认链接面无回归）
      ★ ✅ runner 重新链接成功（★ `cmake --build build --target parity_sim_runner`）· ★ 单测 64 passed

---

## 5. 收口

- [x] **5.1** ★ **交接给 L4**：★ L4 的 `chmem-hierarchical-assembly` **①-C**（AC-13 / tasks 4.1）MUST 消费本 change 的三态化 ⇒ ★ 本 change 归档后 ⇒ L4 的 `depends_on` 即满足
      ★ ✅ 边界已划（★ 2026-11-08）：★ L4 的 AC-13 / tasks 4.1 改为「消费」+ `depends_on` 已加 ⇒ ★ **MUST NOT** 重复实现
- [ ] **5.2** ★ **归档**：`openspec archive parity-observation-ambiguity-fail-closed --skip-specs`（★ `.openspec.yaml` 已声明 `skip_specs: true`）
- [x] **5.3** ★ **AC 逐条勾选 + 证据落 `evidence/`**
      ★ ✅ 6 条 AC 全部勾选 + 依据写进 proposal · 证据 = `evidence/v25-ambiguity-evidence.txt`

---

## 6. 范围外（★ 明确不做）

- [x] ✗ 不改 `tools/parity/parity_gate.py`（★ 其约定已把 exit 2 归 ERROR）
- [x] ✗ 不改观测契约名（★ `tohost_probe_data_proxy` / `pc` / `reg_<i>` / `mem_sig` / `mem_sig_updates` 一字不改）
- [x] ✗ 不改 Verilator 侧（★ 本 change 只治解释器侧的解析歧义 ⇒ V-14 罩不住解释器侧）
- [x] ✗ 不改 `ip/cpu/**`（★ 变异对照的临时改动 MUST 用 `cp` 覆盖）
- [x] ✗ 不做同名普查（★ 归 L4 的 ①-C：★ 普查范围 = 52 case + CH_MEM binary 全量）
- [x] ✗ 不引入 manifest / provider（★ 归 L4 的 ①-B / ①-D）
