# Phase 1：基础 TLM 平台（L1CachePlugin "Hello World"）

> **Status**: Not Started
> **Milestone**: M1 - L1CachePlugin 在 TLM 模式下端到端跑通
> **Depends on**: Phase 0（Plugin 最小脚手架）
> **决策依据**: `.omo/drafts/decision-plugin-framework-2026-06-08.md`
> **目标版本**: ChipForge 0.1.x

**目标**：在 Phase0 提供的 Plugin 脚手架上，实现第一个真实 Plugin（`L1CachePlugin`），验证 Plugin-style 设计在 TLM 模式下的可行性。

> **关键约束（D4 强制）**：所有业务逻辑必须采用 **Plugin-style** 设计（无 `tick()`、无状态机、Bundle 字段用 `uint_t<N>`），不使用原 `cpptlm::CacheTLM` 的 `tick()` 风格。Phase6 升级时，**业务代码不重写**。

---

## 1. 任务清单

### 1.1 Bundle 层定义（1 天）

- [ ] `MemReqBundle` / `MemRespBundle`（内存请求/响应）
  - 基于 `CppHDL/include/bundle/stream_bundle.h` 现有实现
  - 字段：`address` / `data` / `is_write` / `burst_len` / `id`
- [ ] `CacheReqBundle` / `CacheRespBundle`（缓存请求/响应）
- [ ] `L1CachePluginBundle`（L1Cache 内部使用的 Payload 集合）
- [ ] `IntBundle`（中断接口，预留）

**约束**：所有 Bundle 字段类型使用 `cf::plugin::uint_t<N>`（Phase0 提供的编译期类型切换），**不直接用** `uint64_t` 或 `ch_uint<N>`。

### 1.2 L1CachePlugin 实现（3-4 天）

L1CachePlugin 的最小可验证形态：

```cpp
struct L1CachePlugin : cf::plugin::PluginBase {
    // Payload Key 声明（全局静态）
    cf::plugin::Payload<cf::plugin::uint_t<64>>  addr{"addr"};
    cf::plugin::Payload<cf::plugin::uint_t<512>> data{"data"};
    cf::plugin::Payload<cf::plugin::uint_t<20>>  tag {"tag"};
    cf::plugin::Payload<cf::plugin::uint_t<8>>   idx {"idx"};
    cf::plugin::Payload<cf::plugin::bool_t>     hit {"hit"};

    void setup(PipeBuilder& pb) override {
        pb.declare_substage("lookup", "refill", 1);
    }

    void build(PipeBuilder& pb) override {
        auto* n = pb.node_of_logic_stage("lookup");

        pb.at_stage("lookup", Phase::NORMAL, [this, n]() {
            (*n)(idx) = (*n)(addr)(11, 4);    // 提取 index
            (*n)(tag) = (*n)(addr)(31, 12);   // 提取 tag
            (*n)(hit) = (tags_[(*n)(idx)] == (*n)(tag)) & valid_[(*n)(idx)];
            (*n)(data) = data_[(*n)(idx)];
        });

        pb.at_stage("refill", Phase::LATE, [this, n]() {
            // 从内存响应填充 cache line
            tags_[(*n)(idx)] = (*n)(tag);
            data_[(*n)(idx)] = (*n)(data);
            valid_[(*n)(idx)] = true;
        });
    }

private:
    ch_mem<cf::plugin::uint_t<20>,  256> tags_;
    ch_mem<cf::plugin::uint_t<512>, 256> data_;
    ch_mem<cf::plugin::bool_t,      256> valid_;
};
```

**任务**：
- [ ] 实现 L1CachePlugin 类（lookup + refill 两阶段）
- [ ] 单元测试 `test_l1_cache_plugin_unit.cpp`
  - hit 路径正确性
  - miss 路径正确性
  - refill 后二次访问命中

### 1.3 最小 SoC 装配（1 天）

- [ ] 创建 `soc/l1_cache_minimal.json`（最小验证拓扑）
  - `traffic_gen`（流量生成器）→ `l1_cache`（L1CachePlugin）→ `memory`（DRAM 模型）
- [ ] 验证 `cpptlm::ModuleFactory` 能识别 L1CachePlugin 类
- [ ] 集成测试 `test_l1_cache_plugin_e2e.cpp`
  - 跑 1000+ 事务
  - hit rate 应在合理范围

### 1.4 与 `cpptlm::CacheTLM` 对比（1-2 天）

- [ ] 创建 `soc/l1_cache_baseline.json`（用 `cpptlm::CacheTLM` 作为对比基线）
- [ ] 用相同输入 trace（地址序列）驱动两个配置
- [ ] 对比关键指标：
  - 命中/缺失次数
  - 延迟分布
  - 最终 cache 状态
- [ ] Golden reference 测试 `test_l1_cache_plugin_vs_cachetlm.cpp`

### 1.5 验证与文档（1 天）

- [ ] 单元测试覆盖率 ≥ 80%
- [ ] API 文档（Doxygen）
- [ ] 用户指南 `ip/cache/README.md`（说明 L1CachePlugin 使用方法）
- [ ] 更新 `bundles/` 共享 Bundle 定义

---

## 2. 退出标准

满足以下**全部**条件才能进入 Phase 2：

### 2.1 功能标准

- [ ] L1CachePlugin 在 TLM 模式下端到端跑通（`soc/l1_cache_minimal.json` 可执行）
- [ ] 与 `cpptlm::CacheTLM` 同输入比对结果**完全一致**（功能等价基线）
  - 命中/缺失模式
  - 最终 cache 状态（tags/data/valid）
  - 延迟分布（允许 ±5% 误差）

### 2.2 设计风格标准（D4 强制）

- [ ] 业务代码**无 `tick()`**（grep 静态检查）
  ```bash
  # 必须通过
  ! grep -r "void tick()" ip/cache/tlm/
  ```
- [ ] 业务代码**无状态机**（switch/case state 不允许）
- [ ] Bundle 字段使用 `uint_t<N>`（编译期验证 + grep 检查）
  ```bash
  # 必须通过
  ! grep -rE "uint(32|64)_t addr|ch_uint<\d+> addr" ip/cache/tlm/
  ```
- [ ] 所有阶段用 `at_stage()` 注册（无 `if (state_ == ...)` 模式）
- [ ] 阶段间通信通过 `Payload<T>` Key（无显式成员变量做 IPC）

### 2.3 集成标准

- [ ] `ModuleFactory` 可通过 JSON 注册 L1CachePlugin
- [ ] 在最小 SoC 中端到端跑 10000+ 事务无崩溃
- [ ] 单元测试 + 集成测试 + 对比测试全部通过

### 2.4 文档标准

- [ ] L1CachePlugin 用户文档
- [ ] Bundle 定义文档
- [ ] `soc/l1_cache_minimal.json` 配置说明

---

## 3. 设计风格约束（D4 强制细节）

### 3.1 禁止模式

| 禁止 | 替代 |
|------|------|
| `void tick() override { switch(state_) {...} }` | `at_stage("stage_name", Phase::NORMAL, []{...})` |
| `state_ = State::MISS;` | 写到下一阶段可见的 Payload：`(node)(pl::STATE) = MISS;` |
| `if (last_was_write_) {...}` | 显式 Payload: `if ((node)(pl::IS_WRITE)) {...}` |
| `ch_uint<64> addr;` (in Bundle) | `cf::plugin::uint_t<64> addr;` |
| `uint64_t addr;` (in Bundle) | `cf::plugin::uint_t<64> addr;` |
| `if (valid_[idx])` (跨阶段) | 把 `valid_[idx]` 写入 Payload |

### 3.2 强制模式

| 强制 | 用途 |
|------|------|
| 所有外部接口用 `ch_stream<Bundle>` | 类型安全的流式 IO |
| 业务逻辑用 `at_stage()` 注册 | 框架决定调度 |
| Bundle 字段用 `uint_t<N>` | 编译期 TLM/RTL 切换 |
| 阶段间通信用 `Payload<T>` | 类型安全 key + 跨节点隔离 |

### 3.3 静态检查脚本

`tools/check_plugin_style.sh`：

```bash
#!/bin/bash
# 静态检查 Plugin 风格约束
set -e
echo "Checking for forbidden patterns..."

# 禁止 void tick() 业务重写
if grep -rn "void tick()" ip/cache/tlm/ 2>/dev/null; then
    echo "ERROR: void tick() forbidden in Plugin-style code"
    exit 1
fi

# 禁止直接用 ch_uint<N> 或 uintN_t 做 Bundle 字段
if grep -rnE "(ch_uint<\d+>|uint(32|64)_t) +addr" ip/cache/tlm/ 2>/dev/null; then
    echo "ERROR: Bundle fields must use uint_t<N> (compile-time switch)"
    exit 1
fi

# 禁止状态机
if grep -rnE "enum class.*State|switch \(state_" ip/cache/tlm/ 2>/dev/null; then
    echo "ERROR: State machines forbidden in Plugin-style code"
    exit 1
fi

echo "All checks passed."
```

---

## 4. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| R1: L1CachePlugin 与 CacheTLM 行为无法对齐 | 中 | 高 | Phase1 退出标准 2.1 强制一致；不通过则返工 |
| R2: Plugin 风格在 TLM 模式下性能不佳 | 中 | 中 | benchmark 对比；如果性能差距 > 30%，重新评估 |
| R3: 设计风格约束（D4）造成代码复杂度增加 | 中 | 中 | 静态检查工具 + 代码审查；权衡可读性 vs 范式一致性 |
| R4: Phase0 脚手架接口不稳定 | 低 | 中 | Phase0 退出标准 6.1 承诺接口稳定；如必须修改需同步决策文档 |
| R5: 与原 `cpptlm::CacheTLM` 共存导致 SoC 装配混乱 | 低 | 中 | 在 SoC JSON 中明确指定类型（`L1CachePlugin` vs `CacheTLM`）|

---

## 5. Phase1 工期估算

| 任务 | 工时 |
|------|------|
| Bundle 定义 | 1 天 |
| L1CachePlugin 实现 | 3-4 天 |
| 最小 SoC 装配 | 1 天 |
| 与 CacheTLM 对比 | 1-2 天 |
| 验证 + 文档 | 1 天 |
| **总计** | **~7-9 工作日（约 1.5 周）** |

---

## 6. 与其他 Phase 的关系

### 6.1 依赖关系

- **依赖 Phase 0**：所有 `cf::plugin::*` 接口来自 Phase0
- **不依赖 Phase 2-5**：Phase1 是独立的 TLM 验证里程碑
- **为 Phase 2-5 提供模式**：后续 Plugin-style IP 应遵循 L1CachePlugin 的设计模式

### 6.2 Phase1 成功后

- Phase 2（RTOS）：可重用 L1CachePlugin 作为 SoC 组件
- Phase 5（RTL 协同验证）：L1CachePlugin 是第一个 RTL 化的目标
- Phase 6（完整框架）：L1CachePlugin 业务代码不重写

---

## 7. 决策可追溯

本 Phase1 的所有设计决策来源于：
- **决策记录**: `.omo/drafts/decision-plugin-framework-2026-06-08.md`（D2, D4, D10）
- **架构文档**: `docs/architecture/declarative-hybrid-framework.md` v2.0.2 §4.1, §6.8
- **Phase0 接口承诺**: `phase-0-plugin-scaffolding.md` §6.1

任何对 Phase1 范围/接口/退出标准的修改，**必须**同步更新决策记录。

---

*Phase1 完成后，建议立即进入 Phase 2（RTOS），但前提是 L1CachePlugin 已在 SoC 上下文中跑通"Hello World"。*
