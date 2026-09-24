# Phase 6d Prerequisites — Design

> 详细设计文档

## Prereq #1: riscv64-unknown-elf-gcc 工具链安装 设计

### 安装方案对比

| 方案 | 优点 | 缺点 | 推荐 |
|------|------|------|------|
| **A: apt install** (Ubuntu 22.04+) | 简单, 1 行命令 | Ubuntu 20.04 源可能缺失, 需加 PPA | ✅ 推荐 |
| **B: 源码 build** (riscv-gnu-toolchain) | 最新版 | build 30+ 分钟, 依赖多 | ❌ 不推荐 |
| **C: 预编译包** (riscv-collab GitHub Release) | 跨平台一致 | 下载 100MB+, 需解压配置 PATH | ⚠️ 备选 |

### apt 方案详细步骤

```bash
# Ubuntu 22.04+ (jammy) / 24.04+ (noble)
sudo apt update
sudo apt install gcc-riscv64-unknown-elf

# 验证
which riscv64-unknown-elf-gcc
riscv64-unknown-elf-gcc --version  # GNU 14+ for jammy, GNU 13+ for focal
```

### CI 集成

```yaml
# .github/workflows/architecture-gates.yml (扩展)
- name: Install RISC-V toolchain
  if: matrix.target == 'chmem'
  run: |
    sudo apt-get update
    sudo apt-get install -y gcc-riscv64-unknown-elf
    riscv64-unknown-elf-gcc --version
```

## Prereq #2: Verilator + Yosys + iverilog 安装 设计 (Oracle 修正: 按发行版分支)

### 发行版兼容性 (Oracle 2026-09-20 验证)

| 发行版 | Verilator | Yosys | iverilog | gcc-riscv64-unknown-elf |
|--------|-----------|-------|----------|--------------------------|
| **Ubuntu 22.04 jammy** | 4.038 ❌ (< 5.020) | 0.23 ❌ (< 0.30) | 12 ✓ | GCC 12.x ✓ |
| **Ubuntu 24.04 noble** | 5.020 ✓ | 0.40 ✓ | 12 ✓ | GCC 13+ ✓ |

**当前 build env 实测 (Metis 2026-09-20)**:
- riscv64-unknown-elf-gcc: 已预装 GNU 16.1.0 (不需 apt)
- Verilator: 已预装 5.052 ✓
- yosys: ❌ 未装
- iverilog: ❌ 未装

### 安装命令 (按发行版分支)

**Ubuntu 24.04 noble (apt 方案)**:
```bash
sudo apt update
sudo apt install -y verilator yosys iverilog

# 验证
verilator --version      # 5.020 ✓
yosys --version          # 0.40 ✓
iverilog -V | head -3    # 12 ✓
```

**Ubuntu 22.04 jammy (源码 build 方案)**:
```bash
# Verilator 5.020+ 需源码 build (apt 仅 4.038)
git clone https://github.com/verilator/verilator
cd verilator
git checkout v5.020
autoconf && ./configure && make -j$(nproc) && sudo make install
verilator --version  # 5.020 ✓

# Yosys ≥ 0.30 也需源码 build
git clone https://github.com/YosysHQ/yosys
cd yosys
make -j$(nproc) && sudo make install
```

**前置检测** (`lsb_release -rs`):
```bash
UBUNTU_VER=$(lsb_release -rs)
if [[ "$UBUNTU_VER" == "22.04" ]]; then
  echo "WARNING: Ubuntu 22.04 - 需源码 build Verilator ≥5.020 + Yosys ≥0.30"
elif [[ "$UBUNTU_VER" == "24.04" ]]; then
  echo "OK: Ubuntu 24.04 - apt install 即可"
fi
```

### CI 集成

```yaml
# .github/workflows/architecture-gates.yml (扩展)
- name: Install Verilator toolchain
  if: matrix.target == 'chmem'
  run: |
    sudo apt-get install -y verilator yosys iverilog
    verilator --version
    yosys --version
    iverilog -V | head -3
```

## Prereq #3: ADR-037 v2.0 修订 设计

### 文档结构

```markdown
# ADR-037: Plugin 作为设计范式（D4 范式）— v2.0

## v1 → v2 翻转摘要 (2026-09-20, Phase 6c 落地)

### 7 大借鉴点 (SpinalHDL/VexRiscv/CppHDL → cf::plugin)
1. `uint_t<N> = ch::core::ch_uint<N>` (CH_MEM 双模)
2. `PayloadStore` cell 装 ch 代理
3. `PipeBuilder::elaborate()` 替代 `pb.run()` (TLM deprecated)
4. Stage plumbing 自动插 ch_reg (M2S)
5. VexRiscv service 系统简化版
6. `CtrlLink` ch_bool 化 + per-stage OR-merge
7. `array_store` 后端切 `ch_mem` 编译开关

### elaboration 纪律 (8 项 CI 检查)
- `check_plugin_portability.sh` v2.0 8/8 PASS
- `verify_plugin_decision.sh` 3+4/3 PASS
- `verify_adr.sh` 31 PASS / 0 FAILED

### CH_MEM 模式契约
- 双文件分离: `<name>.h` (TLM) + `<name>_chmem.h` (CH_MEM)
- `uint_t<N>` 双模: TLM=POD, CH_MEM=`ch::core::ch_uint<N>`
- `array_store` 双缓冲: TLM 单缓冲, CH_MEM 双缓冲 commit swap
- `CtrlLink` 双模: TLM=`std::function<bool()>`, CH_MEM=`ch_bool` + OR-merge
```

### cross-reference 更新

```markdown
# docs/architecture/adr.md ADR-037 行更新
| ADR-037 | Plugin 作为设计范式（D4） | ✅ v2.0 Accepted (Phase 6c M5 落地, 2026-09-20) — v1.0 由 Phase 1 提案升级 |
|        |                            | v2.0: D4 在 elaboration 语义下兑现, 7 大借鉴点 + 8 项 CI 检查 + CH_MEM 模式契约 |
|        |                            | 关联 ADR-040 v2.0 (TLM→HDL 移植性约束), ADR-046 (多周期 FSM 豁免) |
```

## Prereq #4 + #5: PoC follow-up fixes 协调 设计

详见独立 OpenSpec change [`openspec/changes/poc-follow-up-fixes/`](../poc-follow-up-fixes/)。

**协调机制**: 此 prerequisites change 在 `phase-6d-prerequisites/tasks.md` 中显式列出 "5.4 通知 phase-6d-prerequisites change: PoC follow-up 已 archive", 强制依赖顺序。

## DEVELOPMENT_SETUP.md 更新 设计

```markdown
## 工具链依赖 (v0.3.x, 2026-09-20 更新)

### RISC-V 工具链 (Phase 6d 6d.4 riscv-tests 依赖)
\`\`\`bash
sudo apt install gcc-riscv64-unknown-elf
riscv64-unknown-elf-gcc --version  # ≥ GNU 14
\`\`\`

### 综合验证工具链 (Phase 6d 6d.5 Verilator 依赖)
\`\`\`bash
sudo apt install verilator yosys iverilog
verilator --version  # ≥ 5.020
\`\`\`

### 当前 build env 缺失警告
- riscv64 工具链: 缺失, 阻塞 Phase 6d.4
- Verilator 工具链: 缺失, 阻塞 Phase 6d.5
- 见 `openspec/changes/phase-6d-prerequisites/` 跟踪
```
