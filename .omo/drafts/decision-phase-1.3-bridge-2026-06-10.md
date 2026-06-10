# 决策记录：Phase 1.3 L1CachePlugin ↔ cpptlm::ModuleFactory 桥接方案

| 字段 | 值 |
|------|-----|
| 决策ID | DECISION-2026-06-10-02 |
| 决策日期 | 2026-06-10 |
| 决策状态 | **Proposed（待用户最终确认）** |
| 提出方 | Sisyphus（基于 Phase 1.2 归档后的就绪度分析） |
| 决策影响 | Phase 1.3 启动前提、Phase 1.1/1.2 代码回溯、cf_plugin 独立性边界 |
| 关联文档 | `docs/roadmap/phases/phase-1-tlm-foundation.md` §1.3, `.omo/drafts/decision-plugin-framework-2026-06-08.md` D4, `include/cf/plugin/storage.h` (Phase 1.3+ 注释), `docs/lessons/phase-1.2-l1cacheplugin.md` |

---

## 1. 决策背景

Phase 1.2 落地后 (`e8deacc`)，对 Phase 1.3 就绪度做体检，发现 **2 个关键架构障碍** 使 spec 估算的 1 天实施不可行：

### 障碍 #1：Bundle 类型不兼容

| Bundle | CppTLM SoC JSON 期望 | cf_plugin 现有 |
|--------|----------------------|----------------|
| 请求 | `bundles::CacheReqBundle` (派生自 `bundle_base`, 字段 `ch_uint<N>`) | `cf::bundles::CacheReq` (POD, 字段 `cf::plugin::uint_t<N>`) |
| 字段集 | 完整 AXI: `transaction_id/parent_id/fragment_id/fragment_total/address/size/is_write/data` | 简化: `address/data/is_write/op/id` |
| 命名空间 | `bundles::` (cpptlm 全局) | `cf::bundles::` |

**影响**: 类型系统不重叠，无法直接互操作。需要类型转换层。

### 障碍 #2：类层次不匹配

```
CppTLM 模块继承链:                 cf::plugin 模块继承链:
  SimObject                           PluginBase (cf::plugin)
   └─ ChStreamModuleBase                  └─ L1CachePlugin
       └─ CacheTLM/MemoryTLM/...
```

`cpptlm::ModuleFactory::instantiateAll()` 用 `dynamic_cast<ChStreamModuleBase*>` 识别需要 StreamAdapter 的模块。`L1CachePlugin` 不在 ChStreamModuleBase 体系内 → 无法注册端口适配器 → req/resp 数据流不通。

### 决策 D1：Bundle 统一方案

**选项 A — Adapter Wrapper（推荐）**

- 新增 `ip/cache/tlm/l1_cache_tlm_bridge.h/.cpp`
- 定义 `L1CacheTLMBridge : public ChStreamModuleBase`
- 内部持有 `std::unique_ptr<L1CachePlugin>`，在 `tick()` 内驱动 `pb.run()` 并做 Bundle 转换
- `cf::bundles::CacheReq` ↔ `bundles::CacheReqBundle` 在 Bridge 内做字段映射
- 优点：cf_plugin 完全独立；D4 Plugin-style 不被污染；`L1CachePlugin` 单元测试不变
- 缺点：Bundle 字段映射需手工维护；性能多一跳（cycle 精度可能略降）

**选项 B — Bundle 全面改造**

- 把 `cf::bundles::*` 全部改为 `ch_uint<N>` 派生自 `bundle_base`
- 为 `L1CachePlugin` 添加 `ChStreamModuleBase` 兼容方法
- 优点：Bundle 统一，少一层转换；性能略好
- 缺点：**破坏 Phase 1.1/1.2 已交付代码**（单元测试断言、storage.h 接口承诺）；cf_plugin 失去独立性（D4 决策 §3.1 "不直接用 ch_uint<N>" 被违背）

**推荐 A**。理由：
1. 最小破坏性——已交付代码 0 行修改
2. 保留 cf_plugin 独立性——Phase 6 完整框架可平滑升级
3. `coexistence.cpp` 已证明可行性——`cf::plugin::*` 与 `cpptlm::*` 可共存编译
4. `cf::plugin::storage.h:34` 注释"Phase 1.3+" 已暗示会引入类似抽象层

### 决策 D2：Bridge 的 D4 静态检查定位

**选项 A — Bridge 在 `ip/cache/tlm/bridge.h`，作为 Plugin 业务代码**

- Bridge 受 `verify_plugin_decision.sh` 检查约束
- 不能写 `void tick() override`、不能写状态机
- 缺点：Bridge **必须**有 `tick()` 才能跟 cpptlm EventQueue 交互（这是 cpptlm 协议层要求，不是业务逻辑）—— **与 D4 强制约束正面冲突**

**选项 B — Bridge 在 `src/cf_plugin/bridge/`，作为框架层（推荐）**

- Bridge 不在 `ip/cache/tlm/`，**不在 D4 静态检查范围**（脚本只扫 `ip/`）
- Bridge 是 "cf_plugin 框架层" 的一部分
- 命名：`src/cf_plugin/bridge/l1_cache_bridge.h/.cpp`
- 优点：Bridge 可用 `tick()` 满足 cpptlm 协议；D4 检查范围不变（继续只针对业务 IP）
- 缺点：cf_plugin 框架层需要新增子目录

**推荐 B**。理由：
1. Bridge 逻辑本质是"框架适配器"——与 `coexistence.cpp` 同属框架层
2. D4 静态检查脚本语义保持稳定——只针对业务 IP
3. Phase 6 完整框架自然会包含此类 Bridge——提前沉淀是顺势而为

### 决策 D3：Phase 1.3 范围

**选项 A — 仅最小 e2e（推荐）**

- 严格按 spec 阶段切分：1.3 落地最小 SoC JSON，1.4 落地 cpptlm::CacheTLM baseline 对比
- 1.3 交付物：`soc/l1_cache_minimal.json` + Bridge + e2e 测试
- 1.4 留到下次会话

**选项 B — 1.3 + 1.4 同步落地**

- 1.3 落地 L1CachePlugin e2e
- 1.4 同步做 cpptlm::CacheTLM baseline 对比
- 优点：一次会话完成 Phase 1 M1 milestone
- 缺点：会话过长（1.3 已需 4.5 天；1.4 spec 估 1-2 天；总计 6+ 天）；认知负担大

**推荐 A**。理由：spec 阶段切分是有意设计（小步快跑，每次提交可独立验收）；1.3 Bridge 设计会被 1.4 baseline 对比复用（验证 Bridge 不引入额外 cycle 偏差），但应该分两次提交以便 reviewer 专注。

---

## 2. 工作量重估

| 任务 | 原 spec 估算 | 重估 | 差异来源 |
|------|-------------|------|---------|
| 1.3a Bridge 设计+实现 | (未识别) | 1.5 天 | 新增工作（spec 未预料） |
| 1.3b `soc/l1_cache_minimal.json` | 1 天 | 0.5 天 | 模板参考 `cpu_tlm_test.json` |
| 1.3c `ip/cache/configs/params_schema.json` | (未列出) | 0.5 天 | 仿 `cpu_params_schema.json` |
| 1.3d `test_l1_cache_plugin_e2e.cpp` | (未列出) | 1 天 | 1000+ tx 集成测试 |
| 1.3e D4 静态检查豁免 (bridge 例外) | (未列出) | 0.5 天 | `verify_plugin_decision.sh` 路径过滤 |
| 1.3f `ip/cache/README.md` 用户指南 | (含在 1.5) | 0.5 天 | §2.4 文档标准 |
| **Phase 1.3 合计** | **1 天** | **4.5 天** | **+3.5 天** |
| Phase 1.4 baseline 对比 | 1-2 天 | (不变) | 取决于 1.3 Bridge 形态 |
| Phase 1.5 验证+文档 | 1 天 | (不变) | |

**`roadmap-status.md` 需同步更新**：Phase 1.3 进度估算从 1 天改为 4.5 天。

---

## 3. 推荐启动顺序

```
Step 1: 用户确认 D1/D2/D3（本决策）
        ↓
Step 2: 更新 roadmap-status.md (Phase 1.3 估算 1→4.5 天)
        ↓
Step 3: 实施 1.3a Bridge (1.5 天)
        ↓
Step 4: 实施 1.3b + 1.3d soc JSON + e2e 测试 (1.5 天)
        ↓
Step 5: 实施 1.3c + 1.3e + 1.3f 文档+豁免 (1.5 天)
        ↓
Step 6: Phase 1.3 归档 + 沉淀 lessons
        ↓
Step 7: Phase 1.4 启动 (cpptlm::CacheTLM baseline 对比)
```

---

## 4. 风险与缓解

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|------|------|------|
| R1 | Bridge 在 cpptlm tick() 中调用 `pb.run()`，phase 调度语义不匹配 | 中 | 中 | Bridge 显式记录 phase mapping (NORMAL=lookup, LATE=refill)，单元测试验证 cycle 推进 |
| R2 | Bundle 字段映射遗漏（如 fragment_id）导致 e2e 失败 | 中 | 低 | 字段映射单测覆盖；miss 路径回退到 transaction_id 简化模式 |
| R3 | Bridge 引入额外 cycle 延迟，破坏 1.4 baseline 对比的 ±5% 误差 | 低 | 中 | Bridge 不做 sync 等待，cycle 计数与直接 tick 模式对齐 |
| R4 | cf_plugin 框架层新增 `bridge/` 子目录，破坏 INTERFACE 库纯净性 | 低 | 低 | bridge 单独成 STATIC 库 (与 README §4 升级路径对齐) |

---

## 5. 反向决策（如果 D1 选 B）

如果用户更倾向 D1-B（Bundle 全面改造），则需配套：
- 修订 `cf::bundles::*` 定义（`ch_uint<N>` 字段，继承 `bundle_base`）
- 修订 `cf::bundles::*` 单元测试（移除 `cf::plugin::uint_t<N>` 断言）
- 修订 `storage.h` 接口（保持与 ch_mem 一致）
- 估算 1.3 上修到 7+ 天

**强烈建议不选 B**。除非有强制性能需求。

---

## 6. 等待用户决议

请用户在以下 3 个决策点表态：

| 决策 | 推荐 | 备选 |
|------|------|------|
| D1 Bridge vs Bundle 改造 | **Bridge (A)** | Bundle 改造 (B) |
| D2 Bridge 放在 `ip/` vs `src/cf_plugin/` | **`src/cf_plugin/bridge/` (B)** | `ip/cache/tlm/bridge/` (A) |
| D3 1.3 vs 1.3+1.4 同步 | **仅 1.3 (A)** | 1.3+1.4 同步 (B) |

确认后即开始 1.3a Bridge 实施。

---

*决策追溯：本次决策是 DECISION-2026-06-08-01 (Plugin 范式) 在 Phase 1.3 实施层的具象化，延续 D4 "Plugin 是范式" 原则。*
