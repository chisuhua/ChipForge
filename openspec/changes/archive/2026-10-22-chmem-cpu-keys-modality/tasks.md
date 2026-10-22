# Tasks — chmem-cpu-keys-modality

> 施工计划：`.rddf/plans/chmem-cpu-keys-modality.md`（TDD 5 步）。本文件是 rdd-builder 的勾选台账。
> 立项草案：`.rddf/improvements/chmem-cpu-keys-modality.md`（AC-CK-1..14）。
> **判据 = failing-set 逐条相等**，不是"全绿"（baseline 含 1 known TLM fail + 4 pre-existing spec fail）。

## 0. 前置 baseline（Step 0.0）

- [x] **0.0** 记录 HEAD `962ecb2` baseline：TLM **481 case / 67135 assertions / 480 passed / 1 failed**（唯一失败 `tests/cpu/integration/test_7stage_riscv.cpp:124 REQUIRE(pb->node_count() == 17)` —— 已知 infra drift，**本 change 不修**）；CH_MEM **125 case / 19661 assertions / All passed**；5 架构 gate 全绿
  - ⚠ **计划里的 baseline 已过期**：`.rddf/plans/chmem-cpu-keys-modality.md` 写的是 CH_MEM `118 case`（Phase B 之前）⇒ 实测 **125**，本 change 完成后预期 **129**
- [x] **0.0b** 确认 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` 是**手工 list**（GLOB 只覆盖 TLM target）⇒ 新测试必须手动加入
- [x] **0.0c** 施工前清点 9 个 key 的**全部**生产/消费点（`grep` 实测）⇒ 见 §0.1 消费点地图

### 0.1 消费点地图（施工前清点，`grep` 实测）

| key | 生产者 | 消费者 |
|---|---|---|
| `cpu_keys::SAT` | **无 CH_MEM 生产者**（`ip/cpu/plugins/mmu.cpp:96` 只读；CH_MEM 侧写入方 = `chmem-ptw-walk` 的 `csr_write_satp` 适配器） | `ip/cpu/plugins/mmu.cpp:96`（TLM） |
| `cpu_keys::SFENCE_VADDR` / `SFENCE_ASID` | 同上 | `ip/cpu/plugins/mmu.cpp:108-113`（TLM） |
| `cpu_keys::CPU_EXCEPTION_CODE` | `ip/cpu/plugins/mmu.cpp:130`（TLM） | `ip/cpu/plugins/mmu.cpp:120`（TLM，读 `mmu_keys::EXCEPTION_CODE`）/ `ip/cpu/plugins/mmu_exception_handler.h:52`（TLM）/ `ip/cpu/plugins/mmu_exception_handler_chmem.h:92`（**注释态**，属 C.1） |
| `csr_keys::CSR_READ_VALID` | `ip/cpu/plugins/csr_chmem.h:194`（**折叠**） | `ip/cpu/arch/riscv/int_alu_chmem.h:369-374` |
| `csr_keys::CSR_WRITE_EN` | `ip/cpu/plugins/csr_chmem.h:277`（**折叠**）/ `ip/cpu/cpu_factory_chmem.h:188`（预填 `false`） | `ip/cpu/plugins/csr_chmem.h:313-318`（`ch_bool(cell)` ⇒ **常量**）/ `ip/cpu/cpu_factory_chmem.h:400,431`（跨 stage 复制） |
| `csr_keys::Zicsr_VALID` / `Zicsr_OP` | **无生产者**（`csr_chmem.h` 只在注释里提） | **无消费者** |
| `csr_keys::TRAP_PENDING` | `ip/cpu/plugins/csr_chmem.h:449`（**折叠**） | **无消费者**（`csr_bundles.h:17` 的声称与代码不符，已登记 ADR-040 v3.0 §10.8） |

⇒ **CPU/CSR 侧 9 个 key 全部无 CH_MEM 消费点**（除 CSR_READ_VALID / CSR_WRITE_EN 两个）⇒ 本 change 的价值在于**解除 C.1 的类型阻塞 + 拆 3 处折叠**。

## 1. RED（Step 0.1）

- [x] **1.1** 新增 `tests/cpu/test_cpu_chmem_keys_modality.cpp`（5 case / 25 assertions）
- [x] **1.2** 手动加入 `tests/CMakeLists.txt` `CHMEM_TEST_SOURCES`
- [x] **1.3** **[RED] 编译失败**（RED = compile error，不是运行时红）—— **15 个 error**：9 条 `static_assert` 全红（AC-CK-1/2 判据）+ `put(Payload<bool>&, ch_uint<1>)` 无匹配重载 + `cannot convert ch_uint<1> to bool in assignment`（裸类型 cell 装不下节点的编译器实证）
  - **证据存档**：`/tmp/opencode/red-evidence/chmem-cpu-keys-modality-RED.txt`

## 2. GREEN（Step 0.2 - 0.4）

- [x] **2.1** `ip/cpu/tlm/cpu_keys.h`：`static_assert` 加 `#ifndef CF_PLUGIN_USE_CH_MEM` 守护 + `kBits` 常量 + 4 个 key 改 mode-aware（模板参数 T 保持 raw；key 字符串名全保留）
- [x] **2.2** `bundles/csr_bundles.h`：5 个裸类型 key 改 mode-aware + 注释更正（含 `TRAP_PENDING` 消费者未接线的如实记录）
- [x] **2.3** `ip/cpu/plugins/csr_chmem.h:188-194` —— 拆 `CSR_READ_VALID` 折叠，直写 `ch_uint<1>` 节点
- [x] **2.4** `ip/cpu/plugins/csr_chmem.h:258,277` —— 拆 `CSR_WRITE_EN` 折叠，直写 `ch_uint<1>` 节点
- [x] **2.5** `ip/cpu/plugins/csr_chmem.h:449` —— 拆 `TRAP_PENDING` 折叠，直写 `ch_uint<1>` 节点
- [x] **2.6** `ip/cpu/plugins/csr_chmem.h:313-318` —— 消费点改「读节点」（`ch_bool(cell.impl())` 显式走 impl 指针 ⇒ 保持同一节点）
- [x] **2.7** `ip/cpu/arch/riscv/int_alu_chmem.h:369-374` —— 消费点改「读节点」（`ch_uint<1>` 直取作 `select` 条件；**代码本身不改** —— cell 类型一变，条件就从 POD 快照变成 DAG 节点）
- [x] **2.8** `ip/cpu/cpu_factory_chmem.h:188` —— `put(CSR_WRITE_EN, false)` → `ch_uint<1>` 零值节点
- [x] **2.9** `ip/cpu/plugins/mmu_exception_handler_chmem.h` / `ip/cpu/plugins/hazard_chmem.h` —— 更新**已过期**的"key 类型仍是裸 uint8_t"注释（类型阻塞已解除；剩余阻塞 = 无 CH_MEM 生产者 ⇒ 接线归 `chmem-ptw-walk` / C.1）

## 3. 变异对照（Step 0.5，真实改码 → 确认变红 → 回退）

- [x] **3.1** 变异 **M1a**（生产代码 `CSR_READ_VALID` 折回 `static_cast<bool>`）⇒ **编译失败**：`no matching function for call to 'PayloadStore::put(Payload<ch_uint<1>>&, bool)'` —— 编译器当场拦住折叠回归
- [x] **3.2** 变异 **M1b**（生产代码 `CSR_READ_VALID` 写成字面量 = 折叠的语义等价物）⇒ **1 case RED**（`cpu_chmem_keys_csr_producer_cells_are_live_nodes`）
- [x] **3.3** 变异 **M2a / M2b**（`CSR_WRITE_EN` 同上两形态）⇒ 编译失败 / 1 case RED（同上 case）
- [x] **3.4** 变异 **M3**（探针读错 key：`TRAP_PENDING` 代替 `CSR_READ_VALID`）⇒ **2 case RED**
- [x] **3.5** 变异 **M4**（探针把 cell 折成 literal）⇒ **2 case RED**
- [x] **3.6** 全部回退后复跑 ⇒ **5 case / 25 assertions / All passed**
  - **证据存档**：`/tmp/opencode/red-evidence/chmem-cpu-keys-modality-mutation.txt`（驱动脚本 `/tmp/opencode/mutate_ck.py`，无人值守）

## 4. 回归（Step 0.6）

- [x] **4.1** TLM 全量：**481 case / 67135 assertions / 480 passed / 1 failed**（同一 known infra drift `test_7stage_riscv.cpp:124`，**0 新增**）
- [x] **4.2** CH_MEM 全量：**125 + 5 = 130 case / 19686 assertions / All passed**（9m12s；baseline `125 / 19661` ⇒ **+5 case / +25 assertions 全部来自本 change**，0 回归）
- [x] **4.3** 5 架构 gate 0 失败；`openspec validate --specs` = **72 passed + 4 failed**（与 baseline 相等）
  - ⚠ **计划里的命令串有误**：`.rddf/plans/chmem-cpu-keys-modality.md` 写的是 `openspec validate --specs --strict`；`--strict` 会把 **warning** 升级为 fail（实测 6 passed / 70 failed，**全部是 pre-existing 的 "Requirement text is very long" / "Purpose is placeholder" 警告**，与本 change 无关）。正确的 baseline 口径是**不带** `--strict`。已在本文件更正。

## 5. 规则与文档（Step 0.8）

- [x] **5.1** `tools/check_plugin_portability.sh` Check 13 allowlist 从 4 文件收到 2 文件（删 `bundles/csr_bundles.h` + `ip/cpu/tlm/cpu_keys.h`）+ 状态注释更新；**另补 `[INFO]` 段**让 allowlist 内文件的既有违规人眼可见（否则"0 新增 WARN"判据无法核对）
- [x] **5.2** ADR-040 v3.0 §10.8 违规表：删 `cpu_keys.h` / `csr_bundles.h` 两行；`csr_chmem.h` 3 处折叠标注已修；`TRAP_PENDING` 未接线 / `cache_keys` / `mmu_keys` 剩余 8 key 仍登记
- [x] **5.3** `openspec/specs/payload-key-modality/spec.md` 合并（archive 实测：**+2 added, ~1 modified**）—— 覆盖范围扩展到 CPU/CSR + 新增「生产者 MUST 直写 ch 节点」+「裸 key 收敛 MUST 与消费点落地绑定」2 个 Requirement
- [x] **5.4** `AGENTS.md` 纪律段（#10 修订 + **新增 #11**「mode-aware 还不够 —— 生产者必须直写 ch 节点」，原 #11 顺延为 #12）+ `[chmem]` 计数 84 → **89**
- [x] **5.5** `CHANGELOG.md` v0.11.0 段（新增条目，含 6 个变异的分级结论）

## 6. Archive（Step 0.9）

- [x] **6.1** `openspec change validate chmem-cpu-keys-modality` ⇒ valid（0 error 0 warning —— 首次验证有 1 条 "Requirement text is very long" warning，已把反例表移进 scenario 后消除）
- [x] **6.2** `openspec archive chmem-cpu-keys-modality -y` ⇒ 归档为 `openspec/changes/archive/2026-10-22-chmem-cpu-keys-modality/`
- [x] **6.3** 归档后回归复跑（见 §4.3 + §7）

## AC 清单（AC-CK-1..14）

- [x] **AC-CK-1** CH_MEM 下 `cpu_keys<std::uint64_t>::SAT` / `SFENCE_VADDR` / `SFENCE_ASID` ≡ `ch_uint<64>&`；`CPU_EXCEPTION_CODE` ≡ `ch_uint<8>&`（编译期断言）
- [x] **AC-CK-2** CH_MEM 下 `csr_keys<ch_uint<32>>::CSR_READ_VALID` / `CSR_WRITE_EN` / `Zicsr_VALID` / `TRAP_PENDING` ≡ `ch_uint<1>&`；`Zicsr_OP` ≡ `ch_uint<8>&`
- [x] **AC-CK-3** **鉴别力** —— `csr_chmem` 写入的 `CSR_READ_VALID` 被消费端读到时**节点指针相同**且**随拍翻转**
- [x] **AC-CK-4** **反回归 grep** —— `grep -rn "static_cast<bool>(\s*select" ip/cpu/plugins/*_chmem.h` **零命中**；`grep -rn "cpu_keys<ch_uint\|csr_keys<ch_uint<1>>" ip/` 零命中
- [x] **AC-CK-5** TLM 侧行为等价；`[cpu]` / `[cpu-integration]` / `[cache]` 的 TLM 用例**零改动**通过
- [x] **AC-CK-6** **failing-set 逐条相等** —— baseline：TLM `481 case / 1 known fail` / CH_MEM `125 case`
- [x] **AC-CK-7** **变异对照 ≥2 全红**
- [x] **AC-CK-8** Check 13 allowlist 收缩后 **0 新增 WARN**
- [x] **AC-CK-9** 5 架构 gate 全 PASS + `doc_link_check.sh` 0 失败
- [x] **AC-CK-10** `csr_bundles.h` 的注释与实际声明一致（无自相矛盾残留）
- [x] **AC-CK-11** **无惰性代码** —— 不新增 `TRAP_PENDING` 消费者；不触碰 `mmu_keys` 其余 8 key
- [x] **AC-CK-12** 新测试已加入 `CHMEM_TEST_SOURCES`，`--list-tests` 可列出
- [x] **AC-CK-13** **RED 证据** —— HEAD 状态下新测试的所有断言在旧代码上失败（编译期断言 = 编译失败）
- [x] **AC-CK-14** ADR-040 §10.8 违规表删 2 行 + `csr_chmem.h` 3 处折叠标注已修
