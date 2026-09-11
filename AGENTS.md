# ChipForge — Agent 简明手册

本项目是 CppTLM + CppHDL 上的 RISC-V 虚拟验证平台。使用声明式 Plugin 范式（D4）构建硬件 IP。

---

## 构建与测试

```bash
# 标准配置 + 构建（首次自动 ExternalProject build CppTLM/CppHDL 到 build/_deps/install/）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# 跑全部测试（单二进制 chipforge_tests）
ctest --test-dir build --output-on-failure

# 按 family tag 过滤
./build/bin/chipforge_tests "[framework]"       # Plugin 框架
./build/bin/chipforge_tests "[cache]"            # L1Cache IP
./build/bin/chipforge_tests "[cpu-integration]"  # RISC-V 集成
./build/bin/chipforge_tests "~[mmu]"             # 排除某 family
```

### 已知测试状态

- **MMU 测试临时排除**（`tests/CMakeLists.txt` 中 `list(REMOVE_ITEM)`）：5 个 `tests/mmu/` 测试因骨架阶段库代码问题（`TLBEntry::tag_type` 缺失等）暂不编译。恢复时间：`mmu-tlb-ptw-impl` 完成后。
- **5 个 RISC-V 仿真测试预先存在失败**（`test_*stage_riscv` + `test_cpu_sim_real_tohost`）：因 RISC-V 工具链配置（tohost 字符串、riscv64 assembler path），与代码无关。

### 构建模式

| 场景 | 命令 | 说明 |
|------|------|------|
| 默认（快速） | `cmake -B build && cmake --build build` | CppTLM/CppHDL 已 install 时跳过 build |
| 重 build 依赖 | `-DCHIPFORGE_REBUILD_DEPS=ON` | CppTLM/CppHDL 源码修改后需要 |
| 源码调试 | `-DCHIPFORGE_SOURCE_DEPS=ON` | add_subdirectory 嵌入模式，改 API 即时生效 |
| ASan | `-DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug` | 调试 |
| 完全重置 | `rm -rf build/_deps` | 从零开始重建 deps |

---

## 架构核心约束（D4 / ADR-040）

**所有业务 Plugin 必须遵守**（CI 强制执行，`tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh`）：

1. **无 `void tick()`** — `PluginBase::tick() = delete`，bridge 适配层例外（`src/cf_plugin/bridge/`）
2. **无状态机** — 禁止 `enum class State` + `switch(state_)`；控制流通过 `at_stage()` 声明
3. **Bundle 字段用 `cf::plugin::uint_t<N>`** — 禁止 `ch_uint<N>`、裸 `uint64_t`
4. **`at_stage` 回调内无 `if (cond) return;` 早返** — 用 `if (cond) { ... }` 包裹主逻辑
5. **`ip/{name}/tlm/` 无 `ch_mem`/`ch_reg`/`ch_uint`/`ch::core`** 渗透
6. **`Plugin::build()` 内不调 `pb.run()`** — 由 `PipeBuilder` 调度
7. **存储优先用 `cf::plugin::storage::array_store`** — 替代裸 `std::array`（ADR-040 Tier-2 推荐）

---

## 代码组织

### 项目骨架

| 目录 | 内容 | 状态 |
|------|------|------|
| `ip/{cpu,cache,mmu,memory,...}/` | 硬件 IP，每个独立，lib/ + tlm/ 双层切分 | 不同 IP 不同阶段 |
| `include/cf/plugin/` | Plugin 框架头文件（PluginBase/Payload/PipeNode/PipeBuilder/CtrlLink） | ✅ Phase 0 完成 |
| `src/cf_plugin/bridge/` | Bridge 适配层（CppTLM ↔ Plugin 桥接） | ✅ L1Cache 完成 |
| `tests/{framework,cache,cpu,mmu,soc,bundles}/` | 测试按 family 分目录，非按源码位置 | |
| `bundles/` | 共享 Bundle 定义（`mem_bundles.h`, `tlb_bundles_extension.h`） | |
| `soc/` | SoC 级 JSON 配置 + 系统架构文档（`soc/cpu/docs/architecture.md` + `soc/cpu/docs/roadmap/`） | |
| `docs/` | 架构文档、ADR、流程图、框架级路线图（Phase 0/6） | |
| `openspec/changes/` | OpenSpec 变更工作区 | |
| `openspec/specs/` | OpenSpec 跨 change 规范 | |
| `.omo/` | 历史决策草案、实施计划 | |
| `build/` | **CppTLM/CppHDL deps 在 `build/_deps/install/` 下** | |

### 测试家族

| Family tag | 目录 | 内容 |
|-----------|------|------|
| `[framework]` | `tests/framework/` | Plugin 基础组件测试（8 个） |
| `[cache]` | `tests/cache/` | L1CachePlugin + Bridge + Adapter（5 个） |
| `[cpu]` | `tests/cpu/` | CPU Plugin 单元测试 |
| `[cpu-integration]` | `tests/cpu/integration/` | RISC-V 多 stage 集成（4 个） |
| `[soc]` | `tests/soc/` | SoC JSON 拓扑 |
| `[bundles]` | `tests/bundles/` | Bundle 定义测试 |

### `ip/{name}/` 标准结构

```
ip/{name}/
├── README.md       # IP 总览
├── STATUS.md       # 当前阶段/状态
├── tlm/            # CppTLM Plugin 层（D4 强制）
├── rtl/            # CppHDL RTL 层（Phase 5+）
├── lib/            # 纯 C++ 算法层（与 Plugin 框架解耦）
├── configs/        # JSON 配置 + params_schema.json
├── docs/           # 设计文档
│   ├── README.md   # 文档索引
│   ├── architecture.md
│   ├── configuration.md
│   ├── integration.md
│   └── adr/        # IP 级 ADR
├── policies/       # 替换策略（mmu 特有）
└── test/           # 预留（实际在 tests/{name}/）
```

### lib/ vs tlm/ 严格切分（核心架构规则）

- `lib/` — 纯 C++ 算法，**0 引用** `cf::plugin::PluginBase`/`PipeBuilder`/`Payload`（唯一例外：`cf::plugin/uint_t.h`）
- `tlm/` — Plugin 框架集成，依赖 `cf::plugin::*`，持 `lib/` 算法为成员
- `lib/` → HDL 1:1 转换，`tlm/` → Phase 6 才转换

---

## 文档组织

### ADR 归属判断

> 删除 `ip/{name}/` 后如果该 ADR 仍有意义 → **框架级**（`docs/architecture/adr/`）  
> 删除后变成死链接 → **IP 级**（`ip/{name}/docs/adr/`）

### 关键文档源

| 文件 | 内容 |
|------|------|
| `docs/architecture/adr.md` | ADR 注册表（全局索引，44 条） |
| `docs/DEVELOPMENT_SETUP.md` | 开发环境搭建（symlink + 3 种 CMake 模式） |
| `README.md` | 项目概述 |
| `CONTRIBUTING.md` | 贡献流程、commit 规范、PR SLA |
| `docs/architecture/overview.md` | 架构总览 |
| `docs/architecture/plugin-framework.md` | Plugin 框架详细设计 |
| `docs/methodology/plugin-style-design-methodology-v1.md` | D4 方法学 |
| `tests/README.md` | 测试框架详情（Catch2 使用、已知问题） |
| `tools/README.md` | CI 验证脚本详情（verify_adr.sh 等） |

---

## 工作流

### OpenSpec 工作流（推荐用于多步变更）

```bash
# 探索 → 提案 → 实施 → 归档
# 技能: openspec-{explore|propose|apply|archive}
skill(name="openspec-explore")
skill(name="openspec-propose")
skill(name="openspec-apply-change")
skill(name="openspec-archive-change")
```

### 验证命令

```bash
bash tools/verify_adr.sh                           # ADR 漂移检查
bash tools/verify_plugin_decision.sh                # D4 业务代码检查（~7 项）
bash tools/check_plugin_portability.sh              # ADR-040 移植性检查（~4 项）
bash tools/doc_link_check.sh                        # 文档死链检查
bash tools/run_chipforge_tests.sh                   # 完整 ctest 运行
python tools/doc_checker.py --format text --verbose  # 文档健康检查
pre-commit run --all-files                          # 格式化/空白/JSON 检查
```

3 个验证脚本（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability`）作为 **PR 阻塞门禁**（`.github/workflows/architecture-gates.yml`，ADR-043）。

---

## CI 细节

| Workflow | 触发 | 阻塞？ |
|----------|------|--------|
| `architecture-gates.yml` | PR to main/develop | ✅ 3 脚本全阻塞 |
| `doc_check.yml` | PR + push（docs/ip/变更时） | ❌ smoke-only |

CI 会自动 checkout CppTLM/CppHDL 仓库（`${{ vars.CPPTLM_REPO || 'chisuhua/CppTLM' }}`）。

---

## 不显而易见的约定

- **`ip/cpu/` 是最老的 IP**，当前不完全遵守标准文档结构（无 `ip/cpu/docs/` 等）；新增 IP 必须对齐
- **`src/cf_plugin/tests/` 已删除**（CHANGE-005 empty-directory-cleanup），测试在 `tests/` 下
- **`ip/README.md` 中 mmu 行已存在两次**（STATUS 表 + IP 模块表），不再重复添加
- **`ip/mmu/STATUS.md`** 声明"TLB/PTW 算法 stub"，不可误用为生产
- **.cursorrules 与 AGENTS.md** 的 MCP 部分内容重复，维护时同步更新
- CppTLM/CppHDL 是 **独立仓库**（symlink 引入），git 操作需分别在各目录中执行
- 测试框架使用 **Catch2 v3.7.0**（vendored 在 `tests/catch2/`，与 CppTLM 同步）
- **`tests/CMakeLists.txt`** 用 `file(GLOB_RECURSE)` 自动发现测试文件，新增测试不需改 CMake
- C++17 项目，但 `chipforge_tests` target 需 C++20（因为链接了 CppHDL 头文件）
