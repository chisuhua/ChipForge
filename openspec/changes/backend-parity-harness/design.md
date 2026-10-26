# backend-parity-harness — 设计

> 契约正文（`docs/architecture/parity-gate-contract.md`）是唯一引用来源。

---

## 1. 判定与执行**分离**（本 design 的核心结构）

```
run_simulator(case) ──► sim.json ┐
                                 ├─► judge(case, sim, vl, exceptions) ──► verdict
run_verilator(case) ──► vl.json  ┘
```

`judge()` 是**纯函数**（只吃两份 JSON + 白名单，不碰文件、不跑进程）⇒
★ 可以用**合成数据**直接验证判定逻辑（`--self-test` 9 例），不必每次都跑两个后端。

★ 这一点直接对应契约 S-9/S-10 的验收形式：门禁的鉴别力 MUST 可被**离线**证明。

## 2. 三态退出码（S-10）

| 码 | 含义 | 触发 |
|---|---|---|
| 0 | PASS | 全部 case 的 5 项观测量相等（或差异已被合规例外登记） |
| 1 | FAIL | **后端语义分歧**（某项观测量不等且无合规例外） |
| 2 | ERROR | **门禁自身/环境故障**：观测量缺失 · 确定性不符 · 白名单非法 · 语料集合不一致 · runner 崩溃/超时 |

★ ERROR **优先于** FAIL 之外的语义 ⇒ 但两者并存时以 FAIL 呈现分歧（`error_kind` 记录首个）。

## 3. ★ 语料集合断言的**方向性**（S-13）

| 方向 | 判定 | 理由 |
|---|---|---|
| 清单 ⊄ harness | **ERROR** | 该 case 不会被那一侧跑 ⇒ 差分比对缺一侧 ⇒ **真漏检风险** |
| harness ⊄ 清单 | **WARNING** | 那一侧多跑了一个不在清单里的 case ⇒ 对清单内 case 无差分风险，只是**覆盖超集** |

★ 当前事实：Verilator 侧 `kCases` 多一个 `rv32ui-p-simple` ⇒ WARNING ⇒
由 A3 按 `ADR-089` D4 收口（补进 Simulator 侧 1 行 ⇒ 清单扩到 48）。

## 4. ★★ 缓存策略被实测推翻（契约 S-15 更正）

S-15 原写「Verilog 构建产物按 **config** 缓存 ⇒ 同 config 的 47 ELF 只 build 一次」。
★ **实测不成立**：生成的 Verilog **把 ELF 的指令内存烤进了网表**
（`imem[32] = 32'h200193;`）⇒ **每个 ELF 的网表都不同** ⇒ 跨 ELF 无可复用。
⇒ 更正为：**每个 corpus 一次编译**（无跨 ELF 缓存），并据此重算时长预算：

| 层 | corpus | Simulator（9.1 拍/s） | Verilator（生成+编译） | 合计 |
|---|---|---|---|---|
| PR | 8 | ≈ 7 min | ≈ 3 min | **≈ 10 min** |
| 夜间 | 51 | ≈ 45 min | ≈ 17 min | **≈ 1 h** |

★ 要真正省掉 Verilator 侧的编译，必须把 ELF 从 **elaboration 期**移出（imem 初始化改为运行期加载）
⇒ 属 DUT/上游级改动，登记为开放项（不在本期）。

## 5. 白名单（S-11/S-12）实现要点

- **未知字段一律拒收**（含容差类字段 `tolerance`/`eps`/`max_delta` …）⇒ 防止偷偷加容差
- **5+3 必填字段**校验（`id`/`observation`/`corpus_scope`/`mechanism`/`upstream_status`/
  `mutation_control`/`sunset`/`owner` + `issue_url`/`synthetic`/`justification` 可选）
- `upstream_status=upstream-pending` ⇒ MUST 有 `issue_url`
- `corpus_scope=['*']` ⇒ MUST 有 `justification`
- **过期条目自动失效** ⇒ 不再豁免任何差异（表现为 FAIL）⇒ 逼出收敛
- **常驻合成例外** `synthetic-self-test` 缺失 ⇒ ERROR（防机制随清零腐化）
- 真实例外数 > 10 ⇒ **WARNING**（提示 architect 复审，**不阻断** —— 阻断会逼出假登记）

## 6. 报告（S-14）

`build/parity/report.json`：`verdict` / `error_kind` / `determinism` / `per_case[]`（含 5 项观测量、
`first_divergence`、`differences`、`exceptions_applied`）/ `exceptions_applied` / `warnings`。
★ 每个 case 的 5 项观测量**全部出现**；缺失即 ERROR（**MUST NOT** 省略或填 0）。
