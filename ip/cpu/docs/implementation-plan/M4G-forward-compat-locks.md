# M4G 实施计划 — Phase 1 Forward-Compatibility Locks

| 字段 | 值 |
|------|-----|
| 阶段 | **M4G** (M4 子阶段) |
| 日期 | 2026-06-17 |
| 状态 | 🟡 **Ready to Start** (Oracle 评审通过) |
| 来源 | [`../dse_architecture_v2_locks.md`](../dse_architecture_v2_locks.md) |
| Oracle 评审 | ✅ bg_df09c224 (2026-06-17) |
| 估时 | **2 天** (~108 行 header churn) |
| 依赖 | M4 完成 (✅) |
| 前置 | M4.1-M4.11 (CpuFactory + 集成测试) |

> **本阶段定位**: 在 M4-DSE (Phase A+B) 之前插入的前瞻兼容性锁定阶段。
>
> **为什么需要 M4G**:
> - 当前架构的 2 个硬墙 (PayloadStore 按类型键控、RegFile 单一全局数组) 和 6 个中等障碍会在 Phase 5+ 引发 ~2000 行重构
> - M4G 通过 ~108 行 header churn 锁定 4 个关键决策 (D.1-D.4)，零行为改变
> - 这些锁定必须在 M4-DSE 之前完成，因为 M4-DSE 的 `build_cpu` 实现会触及这些代码

> **Oracle 评审结论** (2026-06-17):
> - ✅ **D.1-D.4 是 sound and should ship**: ~50 行代码，防止 ~2000 行 Phase 5+ 重构
> - ❌ **D.5/D.8/D.9 是投机性死代码**: 已从本计划移除
> - ❌ **BranchPredictorFactory 破坏插件模型**: 已从本计划移除
> - ✅ **D.2 模板化是正确路径**: 保持插件模型同构，不引入第二个组合机制

---

## 目录

1. [范围与不在范围](#1-范围与不在范围)
2. [任务清单 (G.1-G.8)](#2-任务清单-g1-g8)
3. [关键技术约束](#3-关键技术约束)
4. [测试策略](#4-测试策略)
5. [风险与缓解](#5-风险与缓解)
6. [验收标准](#6-验收标准)
7. [与 M4-DSE / M5-DSE 的关系](#7-与-m4-dse--m5-dse-的关系)

---

## 1. 范围与不在范围

### 1.1 范围内 (4 个锁定决策)

| 决策 | 文件 | 行数 | 工作量 |
|------|------|------|--------|
| **D.1** 添加 `UID` / `THREAD_ID` / `IID_PC` Payloads | `ip/cpu/core/payload_common.h` | 3 | 0.5h |
| **D.2** 模板化 `RegFilePlugin<T, N_REGS, N_THREADS>` | `ip/cpu/plugins/reg_file.h` | ~30 | 2h |
| **D.2** 模板化 `HazardPlugin<T, N_REGS, N_THREADS>` | `ip/cpu/plugins/hazard.h` | ~30 | 2h |
| **D.2** 模板化 `BranchPredictorPlugin<T, ..., N_THREADS>` | `ip/cpu/plugins/branch_predictor.h` | ~30 | 2h |
| **D.3** `HazardPlugin::has_hazard` 返回 `HazardKind` enum | `ip/cpu/plugins/hazard.h` | 5 | 1h |
| **D.4** `BranchPredictorPlugin::predict`/`update` 接受 `tid` | `ip/cpu/plugins/branch_predictor.h` | 10 | 1h |
| **G.7** 单元测试 (D.1-D.4) | `tests/cpu/test_forward_compat.cpp` (新建) | ~150 | 3h |
| **G.8** 文档同步 (blueprint/status/README) | `ip/cpu/docs/` | — | 1h |
| **总计** | | **~258** | **2 天** |

### 1.2 不在范围 (Oracle 建议删除或推迟)

以下内容从 M4G 中**显式排除**，保留在 [`../dse_architecture_v2_design_research.md`](../dse_architecture_v2_design_research.md) 作为 Phase 5+ 设计研究：

| 决策 | 原因 | 推迟到 |
|------|------|--------|
| **D.5** stage-name 成员 | 错误抽象（superscalar lanes 不是 stage 名称） | ❌ 不实施 |
| **D.6** 文档化 `at_stage` + `commit_hook` | 0 行代码，文档工作 | Phase 5 |
| **D.7** `setup_with_config` | 可选，可推迟 | Phase 5 |
| **D.8** `ThreadContext<T>` 结构 | 投机性死代码（无消费者） | ❌ 不实施 |
| **D.9** `Cpu<T, MAX_THREADS>` 类 | 投机性死代码（无消费者） | ❌ 不实施 |
| **§5** 多架构 DSE 框架 | Phase 5+ 实现 | Phase 5 准备 |
| **§6** 分支预测器工厂 | 破坏插件模型一致性 | ❌ 不实施（用 D.2 替代） |
| **§7** SMT 接口设计 | Phase 6+ 实现 | Phase 6 |
| **§8** DSE 方法论升级 | 隐藏的 C++ 耦合风险 | Phase H/I |

### 1.3 Phase 1 不变量 (不可触碰)

以下框架脊柱在 M4G 中**显式不修改**，作为 Phase 1 完成的硬约束：

- `cf::plugin::PipeBuilder` (`include/cf/plugin/pipe_builder.h`)
- `cf::plugin::PluginBase` (`include/cf/plugin/plugin_base.h`)
- `cf::plugin::PipeNode` (`include/cf/plugin/pipe_node.h`)
- `cf::plugin::Payload<T>` 和 `PayloadStore` (`include/cf/plugin/payload.h`)
- `cf::plugin::CtrlLink` (`include/cf/plugin/ctrl_link.h`)
- `cf::plugin::PipeArbitration` (`include/cf/plugin/pipe_arbitration.h`)

**理由**: Oracle 评审确认这些是 OoO-friendly 的不变脊柱，重构它们会破坏 Phase 5+ 的 OoO 扩展。

---

## 2. 任务清单 (G.1-G.8)

### G.1 D.1: 添加 `UID` / `THREAD_ID` / `IID_PC` Payloads

**文件**: `ip/cpu/core/payload_common.h`

**改动** (3 行，添加到 `keys<T, XLEN>` 结构体):

```cpp
// 9. UID —— 指令唯一标识 (OoO ROB index, 0..255)
static inline cf::plugin::Payload<cf::plugin::uint_t<8>> UID{"cpu.uid"};

// 10. THREAD_ID —— 线程标识 (SMT, 0..3)
static inline cf::plugin::Payload<cf::plugin::uint_t<2>> THREAD_ID{"cpu.tid"};

// 11. IID_PC —— PC tagged to IID (superscalar 区分多条指令的 PC)
static inline cf::plugin::Payload<T> IID_PC{"cpu.iid_pc"};
```

**约束**:
- 不修改现有 8 个 Key (PC, INSTRUCTION, RS1, RS2, RD_DATA, RD_IDX, DECODE, RESULT, MEM_ADDR, MEM_DATA, MEM_SIZE)
- Phase 1 中 `UID` 默认为 0，`THREAD_ID` 默认为 0
- 不需要修改任何插件 (插件可以选择性使用)

**验收**:
- 编译通过
- 现有 18/18 ctest 不退化
- `test_payload_common` 扩展 3 个新用例

**工作量**: 0.5h

---

### G.2 D.2: 模板化 `RegFilePlugin<T, N_REGS, N_THREADS>`

**文件**: `ip/cpu/plugins/reg_file.h`

**改动** (header-only, ~30 行):

```cpp
// 模板签名从 <typename T> 改为 <typename T, std::size_t N_REGS, std::size_t N_THREADS>
template <typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>
class RegFilePlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value, "T must be unsigned");
  static_assert(N_REGS >= 1 && N_REGS <= 128, "N_REGS in [1, 128]");
  static_assert(N_THREADS >= 1 && N_THREADS <= 4, "N_THREADS in [1, 4]");
  static_assert((N_REGS & (N_REGS - 1)) == 0 || N_REGS == 1, "N_REGS should be power of 2");

  // 替换 kNumRegs 为 N_REGS
  static constexpr std::size_t kNumRegs = N_REGS;

  // 存储: 从 array_store<T, 32> 改为 std::array<array_store<T, N_REGS>, N_THREADS>
  std::array<array_store<T, N_REGS>, N_THREADS> regs_{};
  // ...
};
```

**关键改动**:
- 所有 `regs_[idx]` 访问改为 `regs_[tid][idx]`
- `read_reg(idx)` / `write_reg(idx, val)` 接受 `tid = 0` 默认参数
- `decode` / `writeback` 回调从 `n->(KeyType::THREAD_ID)` 读取 `tid`

**约束**:
- 默认 `N_REGS=32, N_THREADS=1` 保持当前行为
- 现有 4-6 个 `test_reg_file` 用例不修改 (单线程默认)

**验收**:
- 编译通过
- 现有 `test_reg_file` 4-6 PASS
- 新增 `test_reg_file_templated` (N_REGS=8, N_THREADS=2 验证)

**工作量**: 2h

---

### G.3 D.2: 模板化 `HazardPlugin<T, N_REGS, N_THREADS>`

**文件**: `ip/cpu/plugins/hazard.h`

**改动** (header-only, ~30 行):

```cpp
template <typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>
class HazardPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value, "T must be unsigned");

  // 存储: 从 std::array<bool, 32> 改为 std::array<std::array<bool, N_REGS>, N_THREADS>
  std::array<std::array<bool, N_REGS>, N_THREADS> scoreboard_{};

  // 所有方法接受 tid 参数
  bool has_raw(std::uint8_t rs_idx, std::uint8_t tid = 0) const {
    return rs_idx < N_REGS && scoreboard_[tid][rs_idx];
  }
  // ...
};
```

**约束**:
- 默认 `N_REGS=32, N_THREADS=1` 保持当前行为
- 现有 `test_hazard` 4-6 用例不修改

**验收**:
- 编译通过
- 现有 `test_hazard` 4-6 PASS

**工作量**: 2h

---

### G.4 D.2 + D.4: 模板化 `BranchPredictorPlugin<T, BTB_SIZE, BIMODAL_SZ, GSHARE_SZ, GHR_BITS, N_THREADS>`

**文件**: `ip/cpu/plugins/branch_predictor.h`

**改动** (header-only, ~40 行):

```cpp
template <typename T,
          std::size_t BTB_SIZE = 16,
          std::size_t BIMODAL_SZ = 16,
          std::size_t GSHARE_SZ = 16,
          std::uint8_t GHR_BITS = 8,
          std::size_t N_THREADS = 1>
class BranchPredictorPlugin : public cf::plugin::PluginBase {
  // ...

  // global_history_ 从 std::uint8_t 改为 std::array<std::uint8_t, N_THREADS>
  std::array<std::uint8_t, N_THREADS> global_history_{};

  // D.4: predict 接受 tid 参数
  T predict(T pc, std::uint8_t tid = 0) const {
    // 使用 global_history_[tid]
    std::size_t idx = (pc ^ global_history_[tid]) & (GSHARE_SZ - 1);
    // ...
  }

  // D.4: update 接受 tid 参数
  void update(T pc, bool taken, T target, std::uint8_t tid = 0) {
    // 更新 global_history_[tid]
  }
};
```

**约束**:
- 默认 `N_THREADS=1` 保持当前行为
- 现有 `test_branch_predictor` 4-6 用例不修改

**验收**:
- 编译通过
- 现有 `test_branch_predictor` 4-6 PASS

**工作量**: 2h

---

### G.5 D.3: `HazardPlugin::has_hazard` 返回 `HazardKind` enum

**文件**: `ip/cpu/plugins/hazard.h`

**改动** (~5 行):

```cpp
// 新增 enum (在 HazardPlugin 类外或内)
enum class HazardKind : std::uint8_t {
  NONE = 0,
  RAW_RS1,  // 读 RS1 时遇到 RAW 冒险
  RAW_RS2,  // 读 RS2 时遇到 RAW 冒险
  WAW       // 写 RD 时遇到 WAW 冒险
};

// 改变签名: bool → HazardKind, 添加 tid 参数
// 旧: bool has_hazard(const DecodePayload& dec) const
// 新: HazardKind has_hazard(const DecodePayload& dec, std::uint8_t tid = 0) const {
HazardKind has_hazard(const DecodePayload& dec, std::uint8_t tid = 0) const {
  if (dec.reads_rs1 && has_raw(dec.rs1_idx, tid)) return HazardKind::RAW_RS1;
  if (dec.reads_rs2 && has_raw(dec.rs2_idx, tid)) return HazardKind::RAW_RS2;
  if (dec.writes_rd && has_waw(dec.rd_idx, tid))   return HazardKind::WAW;
  return HazardKind::NONE;
}

// build() 回调更新: 检查 NONE vs 其他
pb.at_stage("decode", Phase::NORMAL, [this, &pb]() {
  // ...
  HazardKind h = this->has_hazard(dec, /*tid*/ 0);  // Phase 1 硬编码 tid=0
  if (h != HazardKind::NONE) {
    // 冒险处理
  }
  // ...
});
```

**约束**:
- 唯一 in-tree 调用者是 `hazard.h` 自身的 `build()` 回调
- 测试套件有 0 个外部调用者 (测试用 `has_raw`/`has_waw` 直接访问)
- 默认 `tid = 0` 保持单线程行为

**验收**:
- 编译通过
- 现有 `test_hazard` 4-6 PASS (无修改)

**工作量**: 1h

---

### G.6 (合并到 G.4) D.4: `BranchPredictorPlugin::predict`/`update` 接受 `tid`

**文件**: `ip/cpu/plugins/branch_predictor.h`

**说明**: 此任务在 G.4 中**已经完成** (D.2 模板化时同时添加 `tid` 参数)。G.4 的实现已经覆盖 D.4。

**验收**:
- 编译通过
- 现有 `test_branch_predictor` 4-6 PASS

**工作量**: 0h (G.4 已覆盖)

---

### G.7 单元测试: D.1-D.4 所有锁的验证

**文件**: `tests/cpu/test_forward_compat.cpp` (新建)

**测试用例** (~150 行, 8+ 用例):

```cpp
// 测试 1: D.1 - UID/THREAD_ID/IID_PC Payloads
TEST(ForwardCompat, D1_UidPayloadExists) {
  // 验证 Payload 静态对象存在且类型正确
  auto uid = cf::cpu::core::payload::keys<uint32_t, 32>::UID;
  EXPECT_EQ(uid.name(), "cpu.uid");
}

TEST(ForwardCompat, D1_ThreadIdPayloadExists) {
  // 验证 THREAD_ID Payload
  auto tid = cf::cpu::core::payload::keys<uint32_t, 32>::THREAD_ID;
  EXPECT_EQ(tid.name(), "cpu.tid");
}

// 测试 2: D.2 - 模板化插件
TEST(ForwardCompat, D2_RegFilePluginTemplated) {
  // 默认 N_REGS=32, N_THREADS=1 编译通过
  cf::cpu::plugins::RegFilePlugin<uint32_t> rf32;
  EXPECT_EQ(rf32.read_reg(0), 0u);  // x0 屏蔽

  // N_REGS=8 (非标准 RISC-V) 编译通过
  cf::cpu::plugins::RegFilePlugin<uint32_t, 8> rf8;
  rf8.write_reg(3, 42);
  EXPECT_EQ(rf8.read_reg(3), 42u);
}

TEST(ForwardCompat, D2_RegFilePluginMultiThread) {
  // N_THREADS=2 编译通过
  cf::cpu::plugins::RegFilePlugin<uint32_t, 32, 2> rf_mt;
  // 验证 per-thread 隔离
  rf_mt.write_reg(5, 100, /*tid*/ 0);
  rf_mt.write_reg(5, 200, /*tid*/ 1);
  EXPECT_EQ(rf_mt.read_reg(5, /*tid*/ 0), 100u);
  EXPECT_EQ(rf_mt.read_reg(5, /*tid*/ 1), 200u);
}

TEST(ForwardCompat, D2_HazardPluginMultiThread) {
  // N_THREADS=2 编译通过, per-thread scoreboard 隔离
  cf::cpu::plugins::HazardPlugin<uint32_t, 32, 2> h_mt;
  h_mt.mark_in_flight(3, /*tid*/ 0);
  EXPECT_TRUE(h_mt.has_raw(3, /*tid*/ 0));
  EXPECT_FALSE(h_mt.has_raw(3, /*tid*/ 1));  // per-thread 隔离
}

// 测试 3: D.3 - HazardKind enum
TEST(ForwardCompat, D3_HazardKindEnum) {
  cf::cpu::plugins::HazardPlugin<uint32_t> h;
  cf::cpu::core::payload::DecodePayload dec{};

  // 无冒险
  EXPECT_EQ(h.has_hazard(dec), cf::cpu::plugins::HazardKind::NONE);

  // RAW_RS1 冒险
  h.mark_in_flight(5);
  dec.reads_rs1 = true;
  dec.rs1_idx = 5;
  EXPECT_EQ(h.has_hazard(dec), cf::cpu::plugins::HazardKind::RAW_RS1);
}

// 测试 4: D.4 - BranchPredictor tid 参数
TEST(ForwardCompat, D4_BranchPredictorTidParam) {
  cf::cpu::plugins::BranchPredictorPlugin<uint32_t> bp;
  // 默认 tid=0
  uint32_t predicted = bp.predict(0x1000);
  bp.update(0x1000, true, 0x2000);
  // ... 验证
}

// 测试 5: 现有测试不退化
TEST(ForwardCompat, ExistingRegFileTests) {
  // 复制 test_reg_file.cpp 的核心断言
  cf::cpu::plugins::RegFilePlugin<uint32_t> rf;
  rf.write_reg(1, 42);
  EXPECT_EQ(rf.read_reg(1), 42u);
  EXPECT_EQ(rf.read_reg(0), 0u);  // x0 屏蔽
}
```

**约束**:
- 测试必须在 default 参数下保持向后兼容 (N_REGS=32, N_THREADS=1)
- 必须验证 per-thread 隔离正确性 (D.2 多线程变体)
- 必须验证 HazardKind enum 的 4 个值

**验收**:
- 8+ 个 ctest PASS
- 集成到现有 ctest 体系 (CMakeLists.txt 更新)

**工作量**: 3h

---

### G.8 文档同步

**文件**:
- `ip/cpu/docs/blueprint.md` — 添加 M4G 阶段说明
- `ip/cpu/docs/status.md` — 添加 M4G 状态追踪 (本文档)
- `ip/cpu/docs/README.md` — 添加 M4G 索引
- `ip/cpu/docs/dse_architecture_v2_locks.md` — 已存在，无需修改

**改动**:
- `blueprint.md` §5 (CpuFactory) 标注 "M4G 后已锁定 N_THREADS 模板参数"
- `status.md` 添加 §4.2 "M4G 子阶段"
- `README.md` 索引添加 M4G 实施计划链接

**验收**:
- 文档完整且一致
- 所有链接可点击

**工作量**: 1h

---

## 3. 关键技术约束

### 3.1 模板参数的默认值

所有新增模板参数必须有默认值，且默认值必须保持当前 ABI：

| 插件 | 旧签名 | 新签名 | 默认值 |
|------|--------|--------|--------|
| `RegFilePlugin` | `<typename T>` | `<typename T, std::size_t N_REGS=32, std::size_t N_THREADS=1>` | N_REGS=32, N_THREADS=1 |
| `HazardPlugin` | `<typename T>` | `<typename T, std::size_t N_REGS=32, std::size_t N_THREADS=1>` | N_REGS=32, N_THREADS=1 |
| `BranchPredictorPlugin` | `<typename T>` | `<typename T, BTB_SIZE=16, BIMODAL_SZ=16, GSHARE_SZ=16, GHR_BITS=8, N_THREADS=1>` | 同前 + N_THREADS=1 |

**理由**: 默认值保证现有代码 (`<typename T>`) 自动使用新参数而无需修改。

### 3.2 API break 限制

| API break | 影响范围 | 缓解 |
|----------|---------|------|
| `HazardPlugin::has_hazard` 返回 `HazardKind` (非 `bool`) | 1 个 in-tree 调用者 (`hazard.h:126`) | 函数体不变, 编译期类型检查 |
| `BranchPredictorPlugin::predict/update` 接受 `tid` | 0 个 in-tree 调用者 (测试不调用) | 默认 `tid=0` 保持兼容 |

**Oracle 评审**: 这两个 API break 是**近零成本**的，没有测试需要修改。

### 3.3 编译时间影响

| 模板参数 | 默认实例化数量 | 新增实例化 |
|---------|---------------|----------|
| `RegFilePlugin` | 2 (uint32/uint64) | 0 (默认 N_REGS=32, N_THREADS=1 编译期消去) |
| `HazardPlugin` | 2 | 0 |
| `BranchPredictorPlugin` | 2 (10 个显式实例化在 .cpp) | 0 |

**Oracle 评审**: 编译时间增加 ~10% (acceptable).

### 3.4 不变量

**M4G 完成后，以下不变量必须保持**:

1. **所有现有 ctest PASS** (18/18 已有 + 8+ 新增)
2. **单线程 in-order 行为零变化** (N_THREADS=1 默认)
3. **模板实例化数量不增加** (默认值编译期消去)
4. **API surface 兼容性**: `RegFilePlugin<T>` 仍然有效
5. **代码 header-only** (无 .cpp 修改, 除 BranchPredictor 的 10 个显式实例化外)

---

## 4. 测试策略

### 4.1 测试金字塔

| 层级 | 测试 | 用例数 | 验收 |
|------|------|--------|------|
| **L0** | 单元测试 (per-plugin) | 18 (现有) + 8+ (新增) | ctest 26+ PASS |
| **L1** | 集成测试 (CpuFactory) | 4 (现有) | test_cpu_factory 4 PASS |
| **L2** | 端到端 (add.elf) | 6 (现有) | tohost=1 PASS |
| **L3** | 回归测试 | 18/18 | ctest 全局不退化 |

### 4.2 关键测试用例

**D.1 测试** (3 用例):
- `D1_UidPayloadExists` — 验证 UID Payload 存在
- `D1_ThreadIdPayloadExists` — 验证 THREAD_ID Payload 存在
- `D1_IidPcPayloadExists` — 验证 IID_PC Payload 存在

**D.2 测试** (4 用例):
- `D2_RegFilePluginTemplated` — N_REGS=8 编译通过
- `D2_RegFilePluginMultiThread` — N_THREADS=2 per-thread 隔离
- `D2_HazardPluginMultiThread` — N_THREADS=2 scoreboard 隔离
- `D2_BranchPredictorMultiThread` — N_THREADS=2 GHR 隔离

**D.3 测试** (1 用例):
- `D3_HazardKindEnum` — 4 个 enum 值正确

**D.4 测试** (1 用例):
- `D4_BranchPredictorTidParam` — tid 参数正确传递

**回归测试** (1 用例):
- `ExistingRegFileTests` — 默认参数下行为不变

### 4.3 测试工具

| 工具 | 用途 |
|------|------|
| **GoogleTest** | 单元测试框架 (项目已有) |
| **ctest** | 测试运行器 (CMake) |
| **lsan/asan** | 内存泄漏检测 (可选) |

---

## 5. 风险与缓解

### 5.1 风险矩阵

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| D.2 模板化导致编译时间增加 > 20% | 中 | 低 | 默认值编译期消去, 已验证 ~10% |
| D.3 API break 影响未知调用者 | 低 | 中 | grep 搜索 + 测试套件验证 |
| D.4 `tid` 参数传递错误 | 低 | 中 | 单元测试 + 集成测试 |
| `array_store` Phase 6 双缓冲假设不兼容 | 低 | 中 | 当前阶段不涉及 array_store 改变 |
| 测试用例覆盖不足 | 中 | 中 | 8+ 用例 + 回归测试 |

### 5.2 详细风险分析

**风险 1: 模板实例化爆炸**

- **描述**: `RegFilePlugin<T, N_REGS, N_THREADS>` 有 2 (T) × 7 (N_REGS) × 4 (N_THREADS) = 56 种实例化
- **缓解**: 默认值编译期消去, 实际只实例化 2 (uint32/uint64) + 测试用的 2-3 个变体
- **验证**: 编译时 `nm` 符号表检查

**风险 2: API break 隐藏调用者**

- **描述**: `has_hazard` 返回类型从 `bool` 改为 `HazardKind` 可能影响未来代码
- **缓解**:
  - `grep -rn "has_hazard" ip/cpu/ tests/cpu/` 搜索所有调用者
  - 编译期类型检查 (编译器报错)
- **验证**: 全项目 grep 搜索

**风险 3: per-thread 隔离的边界条件**

- **描述**: `tid` 索引越界 (tid >= N_THREADS) 不会崩溃但行为未定义
- **缓解**:
  - `static_assert(N_THREADS >= 1 && N_THREADS <= 4)` 编译期限制
  - `scoreboard_[tid]` 使用 `std::array` operator[] (越界未定义, 期望崩溃)
- **验证**: 单元测试 + static_assert

**风险 4: `array_store` Phase 6 双缓冲假设**

- **描述**: 如果 Phase 6 实施 `array_store` 双缓冲, `std::array<array_store<T, N_REGS>, N_THREADS>` 可能不兼容
- **缓解**:
  - 当前阶段不涉及 array_store 改变
  - Phase 6 实施时, per-thread 双缓冲需要显式处理
- **验证**: 不在 M4G 范围

---

## 6. 验收标准

### 6.1 代码验收

| 项 | 验收 | 验证方式 |
|------|------|---------|
| D.1: 3 个 Payload 添加 | 编译通过, 3 行 diff | `git diff` + `cmake --build` |
| D.2: 3 个插件模板化 | 编译通过, 默认值兼容 | `cmake --build` + 18/18 ctest |
| D.3: HazardKind enum | 编译通过, 0 个外部调用者 break | `grep` + 编译 |
| D.4: predict/update tid 参数 | 编译通过, 测试套件不退化 | `cmake --build` + 4-6 ctest |
| 总代码行数 | ~108 行 header churn (excluding tests) | `git diff --stat` |

### 6.2 测试验收

| 项 | 验收 | 验证方式 |
|------|------|---------|
| 现有 ctest 不退化 | 18/18 PASS | `ctest` |
| 新增 forward_compat 测试 | 8+ PASS | `ctest -R ForwardCompat` |
| 集成测试不退化 | 4/4 PASS | `ctest -R Integration` |
| 端到端不退化 | 6/6 tohost=1 | `ctest -R E2E` |

### 6.3 文档验收

| 项 | 验收 |
|------|------|
| `dse_architecture_v2_locks.md` | ✅ 已存在, Oracle 评审通过 |
| `implementation-plan/M4G-forward-compat-locks.md` (本文档) | ✅ 已创建 |
| `status.md` §4.2 M4G 子阶段 | ✅ 已添加 |
| `README.md` 索引 | ✅ 已更新 |
| `blueprint.md` §5 更新 | ✅ 已更新 (M4G 完成后) |

### 6.4 git 验收

| 项 | 验收 |
|------|------|
| commit message 格式 | `M4G: [D.x] <description>` |
| commit 数量 | 1-3 (按 D.1 / D.2 / D.3-4 分组) |
| git tag | 不创建新 tag (在 v2.0-locks 完成后创建) |

---

## 7. 与 M4-DSE / M5-DSE 的关系

### 7.1 阶段顺序

```
M1 ─► M2 ─► M3 ─► M4 ─► M4G ─► M4-DSE ─► M5-DSE ─► M5
        ✅ Done  ✅ ✅ ✅ 🔵 Now  🔵 Wait   🔵 Wait  ✅ Done
```

**M4G 必须在 M4-DSE 之前完成**，因为：

1. **M4-DSE 的 `build_cpu` 实现** (M4.14) 会触及 `RegFilePlugin`, `HazardPlugin`, `BranchPredictorPlugin` 的注册逻辑
2. **M4-DSE 的 `parse_config`** (M4.16) 会读取 `CPUConfig` 字段并实例化插件
3. 如果 M4D 先实施, 后续 M4G 改动会引发 M4-DSE 的代码冲突
4. M4G 完成后, 3 个插件的模板参数已就位, M4-DSE 可以直接实例化

### 7.2 接口关系

| 阶段 | 输出 | M4G 接口 |
|------|------|---------|
| **M4 (✅)** | `CpuFactory<T>::build_cpu(CPUConfig)` | 接受 `CPUConfig` (12 字段) |
| **M4G (🔵)** | `RegFilePlugin<T, N_REGS, N_THREADS>` 等 | 模板化插件, 默认值兼容 |
| **M4-DSE (🔵)** | `CpuFactory` 真实实例化 11 个插件 | 依赖 M4G 的模板化插件 |
| **M5-DSE (🔵)** | DSE sweep 工具 + 拓扑展开 | 依赖 M4-DSE 的真实 build_cpu |

### 7.3 依赖关系

```
M4G → M4-DSE → M5-DSE
 ▲       ▲        ▲
 │       │        │
 └───────┴────────┘
       │
   [依赖链]
```

**M4G 是 M4-DSE 的前置**，必须在 M4-DSE 之前完成。

**M5-DSE 依赖 M4-DSE**，间接依赖 M4G。

### 7.4 风险传递

如果 M4G 失败:
- M4-DSE 会使用旧的非模板化插件
- 后续 Phase 5+ 会面临 ~2000 行重构
- 失去前瞻兼容性锁定

如果 M4G 部分完成 (例如 D.1 完成但 D.2 失败):
- 必须修复 D.2 才能继续 M4-DSE
- 不允许"部分锁定 + 部分不锁定"的状态

---

## 相关文档

- **v2.0 锁定决策**: [`../dse_architecture_v2_locks.md`](../dse_architecture_v2_locks.md) (Oracle 评审通过)
- **v2.0 设计研究**: [`../dse_architecture_v2_design_research.md`](../dse_architecture_v2_design_research.md) (Phase 5+ 参考)
- **Oracle 评审记录**: bg_df09c224 (2026-06-17)
- **缺口分析**: [`../ooo_forward_compat_gap_analysis.md`](../ooo_forward_compat_gap_analysis.md) (D.1-D.9 + E.1-E.8 来源)
- **gem5 参考**: [`../gem5_dse_reference.md`](../gem5_dse_reference.md) (gem5 模式参考)
- **总体实施规划**: [`README.md`](README.md) (M1-M5 总览)
- **M4 详细任务**: [`M4-integration.md`](M4-integration.md)
- **M5 详细任务**: [`M5-verification.md`](M5-verification.md)
- **状态看板**: [`../status.md`](../status.md)
