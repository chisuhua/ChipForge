# 决策记录：Phase 1.3 L1CachePlugin ↔ cpptlm::ModuleFactory 桥接方案

| 字段 | 值 |
|------|-----|
| 决策ID | DECISION-2026-06-10-02 (v2, 基于架构文档全面校准) |
| 决策日期 | 2026-06-10 |
| 决策状态 | **Accepted v2**（2026-06-10, 6 commit 落地 `26fe7d2`..`c8d1dd1`, 与 `roadmap-status.md` §2 Phase 1.3 v2 决策一致; D1=C POD+4 字段窄桥 / D1'=末尾调 pb.run / D1''=不实现 BundleMapper+drift 防护 / D2=B 框架层 / D3=A 仅最小 e2e 全部落地）|
| 提出方 | Sisyphus（基于 Phase 1.2 归档后的就绪度分析 + 架构文档校准） |
| 决策影响 | Phase 1.3 启动前提、cf_plugin 独立性边界、Plugin↔ChStreamModuleBase 协作契约 |
| 关联文档 | `docs/roadmap/phases/phase-1-tlm-foundation.md` §1.3, `docs/architecture/overview.md:125`, `docs/architecture/interface-design.md`, `docs/architecture/declarative-hybrid-framework.md` §4.8/§5.5/§6.3, `docs/architecture/plugin-framework.md` §4.1, `docs/architecture/adr.md` ADR-024/ADR-037, `bundles/README.md`, `include/cf/plugin/storage.h` (Phase 1.3+ 注释), `docs/lessons/phase-1.2-l1cacheplugin.md` |

---

## 0. v2 修订说明（相对 v1 的关键变化）

v1 草案基于"Phase 1.2 实施经验"推导出 D1-A（Pure Adapter Wrapper）为推荐。**v2 基于架构文档全面校准后调整了 D1 推荐**：

| 决策点 | v1 推荐 | v2 推荐 | 变化原因 |
|--------|---------|---------|---------|
| **D1** | A (Pure Adapter) | **C (Phase 1.3 保持 POD + 4 字段窄桥)** | v1 误以为"BundleMapper 可立即实现"；实际 `adr.md:118` ADR-024 状态 ⚠️、canonical 设计明确推迟到 Phase 5/6 |
| **D1'** | (未涉及) | **Bridge `tick()` 末尾调用 `plugin_->pb.run()`** | 显式回答 `declarative-hybrid-framework.md:443-447` §4.8 开放问题 1 |
| **D1''** | (未涉及) | **不在 Phase 1.3 实现 BundleMapper + 加 `verify_adr.sh` drift 防护** | 严格遵循 canonical phase 切分 |

---

## 1. 决策背景

Phase 1.2 落地后 (`e8deacc`)，对 Phase 1.3 就绪度做体检，发现 **2 个关键架构障碍** 使 spec 估算的 1 天实施不可行。

### 障碍 #1：Bundle 类型不兼容

| Bundle | CppTLM SoC JSON 期望 | cf_plugin 现有 |
|--------|----------------------|----------------|
| 请求 | `bundles::CacheReqBundle` (`bundle_base`+`ch_uint<N>`) | `cf::bundles::CacheReq` (POD, `cf::plugin::uint_t<N>`) |
| 字段集 | 完整 AXI: `transaction_id/parent_id/fragment_id/fragment_total/address/size/is_write/data` | 简化: `address/data/is_write/op/id` |
| 命名空间 | `bundles::` (cpptlm 全局) | `cf::bundles::` |

### 障碍 #2：类层次不匹配

```
CppTLM 模块继承链:                 cf::plugin 模块继承链:
  SimObject                           PluginBase (cf::plugin)
   └─ ChStreamModuleBase                  └─ L1CachePlugin
       └─ CacheTLM/MemoryTLM
```

`L1CachePlugin` 不在 `ChStreamModuleBase` 体系内 → `ModuleFactory::instantiateAll()` 无法识别为可流模块。

---

## 2. v1 错误的根因（v2 必须避免）

v1 推荐 D1-A（Pure Adapter Wrapper）的依据是"adapter 简单、不破坏 cf_plugin"。但 v1 **漏读了 3 份关键文档**：

1. `docs/architecture/overview.md:125` 明确说 Bundle 基于 `ch_uint<N>`+`bundle_base<Self>` 构建
2. `docs/architecture/declarative-hybrid-framework.md:534-545` §5.5 已**设计** `BundleMapper<CacheReqPOD, CacheReqBundle>`
3. `docs/architecture/adr.md:118` ADR-024 状态 **⚠️** "核心已实现 + Mapper 模板未实现"；验证命令 `[[ ! -e core/bundle/bundle_mapper.h ]]` 明确说"预期缺失"

v1 误以为 "Mapper 模板未实现 = 还没设计"，实际是"**已设计但显式推迟到 Phase 5/6**"。

### 进一步校准（v2 完整阅读后）

| 文档引用 | 关键论据 |
|----------|---------|
| `adr.md:118` ADR-024 | "Mapper 模板未实现" = 当前正确状态，**不应**在 Phase 1.3 实现 |
| `adr.md:1064-1097` ADR-037 | Phase 1 业务逻辑用 Plugin-style 不可逆 |
| `plugin-framework.md:129` | Phase 0 不包含 BundleMapper；推迟到 Phase 6 |
| `plugin-framework.md:626-669` §4.1 | Plugin ↔ ChStreamModuleBase **正交**关系（Plugin 不替代 ChStreamModuleBase） |
| `plugin-framework.md:470-476` §2.6 | `cf::plugin::uint_t<N>` 是 ch_uint 的"位宽语义占位 typedef" |
| `bundles/README.md:102` | "**Phase 5** (RTL) \| BundleMapper 转换为 ch_uint<N> + bundle_base<T>" |
| `bundles/README.md:5` §5 | 显式禁止 `bundles/` 字段用 ch_uint<>（D4 静态检查） |
| `declarative-hybrid-framework.md:660` §6.3 | "L1CacheTlm:ChStreamModuleBase {ch_stream cpu_req_;}" 模式**完全虚构** |
| `declarative-hybrid-framework.md:431-447` §4.8 | 关系约束：Plugin 必须挂载到 ChStreamModuleBase 子类；Phase 1a 仅 TLM 模式生效；4.8 开放问题 1 需 Phase 1a 回答 |

---

## 3. 决策点 D1：Bundle 统一方案

**推荐：选项 C — Phase 1.3 保持 `cf::bundles::*` POD 不动，Bridge 做 4 字段窄桥**

### 候选方案对照（按 canonical 设计重写）

| 方案 | 含义 | 与 canonical 关系 | 评价 |
|------|------|------------------|------|
| A. 纯 Adapter Wrapper | Bridge 手工逐字段转换 | ❌ 等价于"Phase 5 工作提前 Phase 1.3" | 不推荐 |
| A2. Bridge + BundleMapper | 实现 `BundleMapper<cf::bundles::*, bundles::*>` | ❌ 违反 `plugin-framework.md:129` "BundleMapper 推迟 Phase 6" + `bundles/README.md:102` "Phase 5 才做" | 不推荐 |
| B. Bundle 全面改造 | `cf::bundles::*` 改 `ch_uint<N>`+`bundle_base<Self>` | ❌ 违反 `bundles/README.md:5` 显式禁止 + 破坏 Phase 1.1 单元测试断言 | 不推荐 |
| **C. 渐进迁移** | Bridge 内部只对接 4 个最小字段（addr/data/is_write/id），其他字段走 default | ✅ 严格遵循 canonical：POD 内部表示 + BundleMapper 推迟 Phase 5/6 | **推荐** |

### 关键依据

- `adr.md:118` ADR-024 状态 ⚠️ 显式 "Mapper 模板未实现；bundle_mapper.h 预期缺失" → 当前"未实现"是**正确状态**
- `bundles/README.md:102` 显式 Phase 5 才做 BundleMapper 转换
- `bundles/README.md:5` 显式禁止 `bundles/` 字段用 ch_uint<>
- Phase 1.1 单元测试 9/9 PASS 显式断言 `cf::plugin::uint_t<N>` 字段，D 改造会破坏

### Bridge 与 cpptlm Bundle 的最小接口面

```
L1CacheTLMBridge 内部只转发 4 个字段（足够跑最小 e2e）：
  cf::bundles::CacheReq.addr    → bundles::CacheReqBundle.address
  cf::bundles::CacheReq.data    → bundles::CacheReqBundle.data
  cf::bundles::CacheReq.is_write→ bundles::CacheReqBundle.is_write
  cf::bundles::CacheReq.id      → bundles::CacheReqBundle.transaction_id
其他字段（burst_len, parent_id, fragment_*）走 default

反向：CacheResp/MemResp 同理（hit/data/id/error）
```

**Phase 5/6 切换路径**：届时 `BundleMapper` 落地，Bridge 内 4 行 field-to-field 赋值替换为 `BundleMapper::to_external(...)`，业务代码 0 改动。

---

## 4. 决策点 D1'：Bridge ↔ Plugin 挂载契约

**推荐：Bridge `tick()` 末尾调用 `plugin_->pb.run()`**

### 回答 `declarative-hybrid-framework.md:443-447` §4.8 开放问题 1

> Plugin 如何与 ChStreamModuleBase 子类的 tick() 协作？

**答案：在 tick() 末尾触发**。理由：

1. **Plugin 是"横切关注点"**（`plugin-framework.md:626-669` §4.1）—— 观察模块的 tick 副作用后做 transform/post-process
2. **L1CachePlugin 的语义就是 post-process**：`lookup` 阶段判定 hit/miss，`refill` 阶段在 miss 时把 MemResp 写入 storage
3. **符合 VexRiscv Plugin 模式**：VexRiscv 的 `dbusCachedPlugin`/`iCachePlugin` 都在 stage 末尾触发

### Bridge tick() 伪代码

```cpp
void L1CacheTLMBridge::tick() override {
  // 1. 协议转换: cpptlm::Packet → cf::bundles::CacheReq (写入 payload_node)
  if (req_in_.valid()) {
    auto& p = req_in_.data();
    payload_node_->put(g_addr, p.address);
    payload_node_->put(g_is_write, p.is_write);
    payload_node_->put(g_id, p.transaction_id);
  }
  
  // 2. 触发 Plugin (lookup + refill 在一次 pb.run() 内完成)
  plugin_->pb.run();
  
  // 3. 协议转换: cf::bundles::CacheResp → cpptlm::Packet
  auto resp = read_response();
  resp_out_.write({resp.id, resp.data, resp.hit, 0, true, true});
}
```

---

## 5. 决策点 D1''：BundleMapper 在 Phase 1.3 的范围

**推荐：不在 Phase 1.3 实现 BundleMapper**

### 依据

- `plugin-framework.md:129` 显式列 BundleMapper 为 Phase 6 范围
- `bundles/README.md:102` 显式列 BundleMapper 为 Phase 5 范围
- `adr.md:118` ADR-024 状态 ⚠️ 表示"未实现"是当前正确状态

### 但：必须加 drift 防护

Phase 1.3 完成后，**追加 `tools/verify_adr.sh` 检查**：

```bash
# 防止 BundleMapper 被提前实现（破坏 phase boundary）
[[ ! -e bundles/bundle_mapper.h ]] || {
  echo "DRIFT: BundleMapper 提前到 Phase 1.3 实现 (应推迟到 Phase 5)"
  exit 1
}
```

这样 ADR-024 验证脚本会**主动拒绝**任何 Phase 1.3 期间引入的 BundleMapper。

---

## 6. 决策点 D2：Bridge 路径

**推荐：选项 B — `src/cf_plugin/bridge/` (框架层)**

### 依据

- `declarative-hybrid-framework.md:431-447` §4.8 框架层定位
- 不在 `ip/` = 不受 D4 静态检查约束
- Bridge 逻辑本质是"框架适配器"，与 `coexistence.cpp` 同属框架层

### 路径

```
src/cf_plugin/bridge/
├── l1_cache_bridge.h       # L1CacheTLMBridge : public ChStreamModuleBase
└── l1_cache_bridge.cpp     # 4 字段窄桥 + Bridge tick() 末尾 pb.run()
```

---

## 7. 决策点 D3：Phase 1.3 范围

**推荐：选项 A — 仅 Phase 1.3 最小 e2e**

- spec 阶段切分原则（小步快跑，每次提交可独立验收）
- 1.4 baseline 对比独立 session

---

## 8. 工作量重估（基于 D1=C）

| 任务 | 工时 |
|------|------|
| 1.3a `L1CacheTLMBridge` 4 字段窄桥 + D1' 末尾挂载 | 1.5 天 |
| 1.3b `soc/l1_cache_minimal.json` | 0.5 天 |
| 1.3c `ip/cache/configs/params_schema.json` | 0.5 天 |
| 1.3d `test_l1_cache_plugin_e2e.cpp` (1000+ tx) | 1 天 |
| 1.3e `verify_adr.sh` 加 BundleMapper drift 防护 | 0.5 天 |
| 1.3f `ip/cache/README.md` 用户指南 | 0.5 天 |
| **Phase 1.3 合计** | **4.5 天** |

差距来自"spec 假设 `L1CachePlugin` 直接派生 `ChStreamModuleBase`"（`declarative-hybrid-framework.md:660` 已订正为"虚构"），差距是**真实认知矫正**，不是过度工程。

---

## 9. 启动顺序

```
Step 1: 用户确认 D1/D1'/D1''/D2/D3（本决策）
        ↓
Step 2: 更新 roadmap-status.md (Phase 1.3 估算 1→4.5 天)
        ↓
Step 3: 1.3a 实施 Bridge (1.5 天)
        ↓
Step 4: 1.3b + 1.3d soc JSON + e2e 测试 (1.5 天)
        ↓
Step 5: 1.3c + 1.3e + 1.3f 文档+drift 防护 (1.5 天)
        ↓
Step 6: Phase 1.3 归档 + 沉淀 lessons
        ↓
Step 7: Phase 1.4 启动 (cpptlm::CacheTLM baseline 对比)
```

---

## 10. v1 → v2 决策表对照（便于审计）

| 维度 | v1 推荐 | v2 推荐 | 修正依据 |
|------|---------|---------|----------|
| D1 方案 | A (Pure Adapter) | **C (POD + 4 字段窄桥)** | canonical 推迟 BundleMapper 到 Phase 5/6 |
| D1' 挂载契约 | (未涉及) | **Bridge tick() 末尾** | §4.8 开放问题 1 显式回答 |
| D1'' BundleMapper 范围 | (未涉及) | **不实现 + drift 防护** | 严格遵循 phase 切分 |
| D2 路径 | B (`src/cf_plugin/bridge/`) | B（不变） | §4.8 框架层定位 |
| D3 范围 | A (仅 1.3) | A（不变） | spec 阶段切分 |
| 工作量 | 4.5 天 | 4.5 天 | 不变（v1 估算 1 天是 spec 偏差） |
| BundleMapper 实施期 | 1.3（v1 隐含） | **Phase 5/6** | canonical phase 切分 |

---

## 11. 等待用户决议（5 项）

| 决策 | 推荐 | 一句话 |
|------|------|--------|
| **D1** | **C** | Phase 1.3 保持 `cf::bundles::*` POD 不动；Bridge 做 4 字段窄桥 |
| **D1'** | **末尾** | Bridge `tick()` 末尾调用 `plugin_->pb.run()` |
| **D1''** | **不实现** | BundleMapper 推迟到 Phase 5/6；加 `verify_adr.sh` drift 防护 |
| **D2** | **B** | Bridge 在 `src/cf_plugin/bridge/`，不在 `ip/` |
| **D3** | **A** | 仅 Phase 1.3 最小 e2e；1.4 baseline 留到下次 session |

---

*决策追溯：v2 是 DECISION-2026-06-10-02 v1 在架构文档全面校准后的修订版；与 DECISION-2026-06-08-01 D4（Plugin 范式）一致；与 ADR-024（Bundle 三层分层 ⚠️）一致；与 ADR-037（Plugin 作为设计范式）一致。*
