# Tasks — chmem-mmu-keys-modality

> 施工计划：`.rddf/plans/chmem-mmu-keys-modality.md`（TDD 5 步）。本文件是 rdd-builder 的勾选台账。
> **判据 = failing-set 逐条相等**，不是"全绿"（baseline 含 1 known TLM fail + 4 pre-existing spec fail）。

## 0. 前置 baseline（Step 0.0）

- [x] **0.0** 记录 HEAD `fc8452f` baseline：TLM **481 case / 67135 assertions / 480 passed / 1 failed**（唯一失败 `tests/cpu/integration/test_7stage_riscv.cpp:124 REQUIRE(pb->node_count() == 17)` —— 已知 infra drift，**本 change 不修**）；CH_MEM **114 case / 19622 assertions / All passed**；5 架构 gate 全绿
- [x] **0.0b** 确认 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` 是**手工 list**（GLOB 只覆盖 TLM target）⇒ 新测试必须手动加入

## 1. RED（Step 0.1）

- [x] **1.1** 新增 `tests/mmu/test_mmu_keys_modality.cpp`（4 case）
- [x] **1.2** 手动加入 `tests/CMakeLists.txt` `CHMEM_TEST_SOURCES`
- [x] **1.3** **[RED] 编译失败**（RED = compile error，不是运行时红）—— 5 个 `static_assert` 全红 + `put(Payload<bool>&, ch_uint<1>)` → `error: no matching function for call to 'PayloadStore::put(Payload<bool>&, ch_uint<1>)'`
  - **证据存档**：`/tmp/opencode/red-evidence/chmem-mmu-keys-modality-RED.txt`（412 行）

## 2. GREEN（Step 0.2 - 0.4）

- [x] **2.1** `ip/mmu/tlm/mmu_keys.h`：5 个目标 key 改 mode-aware（`kBits = sizeof(T)*8`；模板参数 T 保持 raw；key 字符串名全保留；其余 8 key 不动）
- [x] **2.2** `ip/cpu/plugins/ibus_chmem.h:141-149` —— 消费点改「读节点」（`ch_uint<1>` 直取 + `ch_uint<32>(ch_uint<64>)` 截断）
- [x] **2.3** `ip/cpu/plugins/dmem_chmem.h:142-148` —— 同上
- [x] **2.4** 顺序契约探针（测试内最小探针插件，**自建 PipeBuilder** —— `cpu_factory_chmem.h:119` 当前不注册任何 MMU plugin，**不改 factory**）

## 3. 变异对照（Step 0.5，真实改码 → 确认变红 → 回退）

- [x] **3.1** 变异① 消费端把 cell 折成常量 ⇒ **2 个断言 RED**（`seen_valid_impl_ == src()->impl()` / `cell_flips >= 5`）
- [x] **3.2** 变异② 消费端读错 key（`PADDR_VALID` → `MMU_VADDR`）⇒ **2 个断言 RED**
- [x] **3.3** 变异③ 顺序倒挂（生产者 LATE / 消费者 EARLY）⇒ **1 个断言 RED**
- [x] **3.4** 全部回退后复跑 ⇒ **All tests passed (15 assertions in 4 test cases)**

## 4. 回归（Step 0.6）

- [x] **4.1** TLM 全量：**481 | 480 passed | 1 failed**（同一 known infra drift，**0 新增**）
- [x] **4.2** CH_MEM 全量：**114 case + 4 新增 = 118 case / All passed**
- [x] **4.3** 5 架构 gate 0 失败；`openspec validate --specs --strict` 与 baseline 相等

## 5. 可见性自证（Step 0.7）

- [x] **5.1** `ip/mmu/plugins/` 放一个不含 `ch_*` 的 `_probe_chmem.h` ⇒ Check 2 **FAIL**（证明 Phase B 的落点 CI 看得见）
- [x] **5.2** 删除后 ⇒ **PASS**

## 6. 规则与文档（Step 0.8）

- [x] **6.1** `tools/check_plugin_portability.sh` 新增 **Check 13**（WARN-only + 已知违规 allowlist：csr_bundles / cache_keys / cpu_keys / mmu_keys）
- [x] **6.2** ADR-040 v3.0 **§10.6「payload key 模态约束」**（含顺序契约 + 已知违规登记 + `ch_uint(const ch_bool&)` N==1 接缝 + 跨宽静默截断告警）
- [x] **6.3** `openspec/specs/payload-key-modality/spec.md`（新 capability）
- [x] **6.4** `CHANGELOG.md` v0.11.0 段 + `AGENTS.md` 纪律段（含 `check_plugin_portability.sh` 12/12 → **13/13**）
- [x] **6.5** `ip/cpu/plugins/mmu_exception_handler_chmem.h:86` 注释代码处置（指向姊妹 change）

## 7. 下游文档同步（AC-MK-17）

- [x] **7.1** `.rddf/plans/mmu-chmem-pipeline-integration.md:33,467-472` —— key 裁决补「类型 mode-aware 化 + 生产者 EARLY 挂载」两段
- [x] **7.2** `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md:30` —— B.0 改为「依赖 `chmem-mmu-keys-modality` archive」
- [x] **7.3** `.rddf/protocols/fetch_stall_requirements_v0.11.0.md` §1.5 —— BLOCKER-B5 注明 PTW_ACTIVE 经 **CtrlLink 聚合路径**兑现
- [x] **7.4** `.rddf/improvements/chmem-mmu-keys-modality.md` status → `done` + handoff 刷新

## 8. Archive（Step 0.9）

- [x] **8.1** `openspec archive chmem-mmu-keys-modality -y`
- [x] **8.2** `openspec validate --specs --strict` ⇒ 与 baseline 相等（71 passed / 4 pre-existing failed）

---

## AC 清单（AC-MK-1..17）

- [x] **AC-MK-1** CH_MEM 下 cell 类型 ≡ `ch_uint<1>&` / `ch_uint<64>&` / `ch_uint<8>&`（编译期断言）
- [x] **AC-MK-2** 鉴别力 —— consumer 读到**同一节点**（`impl()` 相等）且**随拍更新**（≥5 翻转 / 6 拍）；含 **K5 边界约束**（API 生成的节点须断言值等价）
- [x] **AC-MK-3** `ch_uint<32>(ch_uint<64>)` 截断语义与旧 `ch_uint<32>(uint64_t POD)` 等价（跨宽构造 M>N ⇒ `bits<N-1,0>`，上游 `core/uint.h:104-121`）
- [x] **AC-MK-4** TLM 侧**行为等价**（flag key bool→uint8_t；地址 key `uint_t<64>` ≡ `uint64_t`）；6 个 TLM 测试文件零改动通过
- [x] **AC-MK-5** failing-set 逐条相等（TLM 481/1 known fail；CH_MEM 114；openspec 71+4）
- [x] **AC-MK-6** 变异对照 ≥2 全红（实测 3 个全红）
- [x] **AC-MK-7** Check 2 可见性自证通过
- [x] **AC-MK-8** 5 架构 gate 0 失败 + 新增 Check 13 的 WARN 数**不增加** + doc_link_check 0 失败
- [x] **AC-MK-9** spec 落地，含顺序契约
- [x] **AC-MK-10** 无惰性代码；Bare 路径 byte-identical
- [x] **AC-MK-11** 反回归 grep：`grep -rn "mmu_keys<ch_uint" ip/ tests/` 零命中（**模板参数必须保持 raw `std::uint64_t`** —— 写 `mmu_keys<ch_uint<64>>` 会撞 `mmu_keys.h` 的 `static_assert` 且与消费端实例不同 ⇒ 指针不等 ⇒ 静默 Bare）。实测零命中
- [x] **AC-MK-12** 顺序契约探针通过且**不依赖** plugin 注册顺序
- [x] **AC-MK-13** 反例探针 —— producer 改挂 LATE ⇒ consumer 拿不到
- [x] **AC-MK-14** 2 个 CH_MEM 消费点改后编译通过
- [x] **AC-MK-15** 新测试已手动加入 `CHMEM_TEST_SOURCES`，`--list-tests` 可列出
- [x] **AC-MK-16** RED 证据（编译错误输出）已捕获并贴进本文件
- [x] **AC-MK-17** 下游文档同批修订（见 §7）
