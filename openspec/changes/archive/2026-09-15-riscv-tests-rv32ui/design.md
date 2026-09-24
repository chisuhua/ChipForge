# Design: `riscv-tests-rv32ui` 实施细节

> **Schema**: spec-driven
> **Date**: 2026-09-15
> **Status**: PROPOSED
> **Companion to**: `proposal.md`

## 1. Context

### 1.1 背景与现状

`riscv-tests-rv32ui` 的动机已详述于 §1。本节只补充"如何实施" 所需的设计上下文：

**当前 ELF 加载链**（`tools/cpu_sim/main.cpp:117-198`）：
```
[CLI: --elf PATH] → main.cpp:147
    → cf::tools::load_elf_text(path, base_addr)
       → elf_loader.h:32-113
         • 解析 ELF header（仅 ELF32）
         • 跳过 e_entry（line 63-64）
         • 遍历 section headers：遇首个 PROGBITS 即 break（line 99-105）
         • 返回 vector<uint8_t> + base_addr (sh_addr)
    → mem.load_binary(data, size, base_addr)
       → picolibc_host_memory.h:30-43（base + size bounds check）
    → CpuFactory::build_cpu(config, &mem)
    → 循环: pb->run(); if (mem.exited()) break;
       (cycle 0 fetch 从 PC=0 开始，payload 默认值)
```

**当前 PicolibcHostMemory 接口**（`picolibc_host_memory.h:30-107`）：
```cpp
class PicolibcHostMemory {
  static constexpr uint64_t kMemorySize = 64 * 1024;  // 64KB
  static constexpr uint64_t kTohostAddr = 0x0;        // hardcoded
  
  uint8_t mem_[kMemorySize];                          // flat array
  
  // 写
  void write_byte(uint64_t addr, uint8_t val);   // bounds: addr < size
  void write_word(uint64_t addr, uint32_t val);  // bounds: addr+3 < size;
                                                 // check_tohost(addr, val & 0xFF)
                                                 // ⚠️ BUG: 仅查低字节
  // 缺 write_half ← riscv-tests 大量用 sh
  
  // 读
  uint8_t read_byte(uint64_t addr) const;       // bounds: addr < size
  uint32_t read_word(uint64_t addr) const;      // bounds: addr+3 < size
  
  // exit
  void check_tohost(uint64_t addr, uint8_t val); // addr == kTohostAddr?
                                                 // tohost_ = (addr==kTohostAddr) ? val : tohost_
                                                 // exit_code_ = (val == 1) ? 0 : 1
  bool exited() const;                            // tohost_ != 0
  uint8_t exit_code() const;
};
```

**约束**：
- D4 / ADR-040 Tier-1 #4：at_stage 无早返
- D4 / ADR-040 Tier-1 #5：`ip/*/tlm/` 无 ch_mem/ch_reg/ch_uint 渗透（本 change 不涉及，仅 `lib/`)
- ADR-045 canonical ordering：MMUPlugin 在 IBusPlugin 前（Wave 1 无 MMU，不影响）
- 工具链：riscv32-unknown-elf-gcc 已在 `/workspace/project/opt/riscv/bin/`，Wave 1 vendored ELF 离线即可用

## 2. Goals / Non-Goals

**Goals**：
- 41 个 riscv-tests rv32ui-p ELF 在 cpu_sim 下能跑，PASS/FAIL 信号基于 tohost 编码
- ELF loader 通用化支持多 PROGBITS SHF_ALLOC section + e_entry + .tohost sh_addr
- PicolibcHostMemory 支持 base address window（base=0 / base=0x80000000 双实例共存）
- 修复 4 个隐藏 bug（write_word partial-byte, write_half 缺失, 单 PROGBITS loader, RV64 拒绝）
- Catch2 macro 循环 41 TEST_CASE + JUnit reporter
- CSV matrix 产物供 Wave 2 消费
- 5 pre-existing RISC-V failures 因 ELF loader 升级 + vendored ELF 副效果转 green
- 4 architecture gates 全 PASS

**Non-Goals**（重申 proposal §3）：
- ❌ 不实现 ecall / 不实装 trap delivery
- ❌ 不修复 riscv-tests 真 bug（暴露即可，fix 归 Wave 2）
- ❌ 不改 pipe_builder stall loop
- ❌ 不改 int_alu.h execute 范围
- ❌ 不加 CSR file
- ❌ 不启用 SoC demo

## 3. Decisions

### Decision 1: PicolibcHostMemory base address window（实现细节）

**API 签名**：
```cpp
class PicolibcHostMemory {
public:
  struct Config {
    uint64_t base_addr = 0;          // RAM window 起点（default 兼容 add.elf）
    uint64_t size     = 64 * 1024;   // RAM window 大小（default 64KB）
    uint64_t tohost_addr = 0;        // tohost 绝对地址（set by ELF loader）
  };
  
  explicit PicolibcHostMemory(Config cfg = {});
  // ... 兼容旧构造函数 PicolibcHostMemory() = default（base=0, size=64KB, tohost=0)
  
  // 索引统一改为：offset = addr - base_addr_（uint64_t 算术）
  // bounds check: if (offset < size_)
  
  // check_tohost 改为：if (addr == tohost_addr_)
  
  // write_word 修复：分别对 addr/+1/+2/+3 触发 check_tohost（每个字节）
  //                  + 新增 write_half(addr, uint16_t val)：addr/+1 各触发
  
  // Loader 调用的额外接口：
  void set_tohost_addr(uint64_t addr);  // ELF loader 解析 .tohost 后调用
};
```

**为什么用 Config struct**：让 default 构造零成本兼容 add.elf，explicit Config 路径支持 riscv-tests base=0x80000000。

**为什么 not constexpr base**：base_addr 必须运行时设置（不同 ELF 不同 base）。

**地址算术安全性**（`addr - base_addr` underflow）：
```cpp
// 安全模式（推荐）：
uint64_t offset;
if (addr < base_addr_) return;        // 早判 underflow
offset = addr - base_addr_;
if (offset >= size_) return;          // bounds check
// access mem_[offset]
```

**内存大小上限**：riscv-tests 单测试 <8KB，41 个都不超 8KB。base window 默认 64KB 完全够。**不引入更大内存（避免 flat array 浪费）**。

### Decision 2: ELF loader 升级（multi-section + e_entry + .tohost）

**新签名**：
```cpp
namespace cf::tools {
struct ElfLoadResult {
  std::vector<std::pair<uint64_t, std::vector<uint8_t>>> sections;  // sh_addr -> bytes
  uint64_t entry_addr;     // e_entry
  uint64_t tohost_addr;    // .tohost section sh_addr（UINT64_MAX if absent）
};

ElfLoadResult load_elf_full(const std::string& path);
}  // namespace cf::tools
```

**实现要点**：
1. 解析 ELF32 header，校验 ELFCLASS32（不再 throw RV64 — 但仍校验 ELFCLASS32）
2. 解析 program headers（PT_LOAD）— 备选路径（riscv-tests 用 sections 就够，但 PT_LOAD 更通用）
3. 遍历 section headers：
   - **对所有 SHT_PROGBITS + SHF_ALLOC**：收集 (sh_addr, sh_offset, sh_size)
   - **找名为 `.tohost` 的 section**（需 shstrtab 解析）：记录其 sh_addr → tohost_addr
   - 不再 break 早返
4. 读取 e_entry：单独返回（不做加载）

**shstrtab 解析**（~15 行）：
- 找 .shstrtab section（e_shstrndx → section header）
- 遍历所有 section header，sh_name 偏移到 shstrtab 取名
- 匹配 `.tohost\0`

**段冲突处理**：
```cpp
// 加载前：检查所有 section 区间 [sh_addr, sh_addr+sh_size) 无重叠
// 若有重叠：throw runtime_error("ELF section overlap: [a,b) vs [c,d)")
// 避免沉默覆盖
```

**base_addr_对齐**：ELF 加载到 PicolibcHostMemory 时，base_addr_ = e_entry & ~(size_ - 1)（window 对齐），即 base 通常 = 0x80000000 与 ELF e_entry 对齐。

### Decision 3: e_entry PC 写入时机（caller-side 唯一）

**写入路径**（caller-side only，不在 build_cpu 内）：
```
[load_elf_full → ElfLoadResult{entry_addr, tohost_addr, sections}]
]
    → PicolibcHostMemory(Config{base=entry_addr & ~(64KB-1), size=64KB, tohost=result.tohost_addr})
       mem.set_tohost_addr(result.tohost_addr)  // 兼容 entry 与 tohost 不同窗
    → mem.load_section(sh_addr, bytes) for each (sh_addr, bytes) in result.sections  // 多段加载（vs 旧 load_binary）
]
    → cf::cpu::CpuFactory::build_cpu(config, &mem)
       (build_cpu 不动 entry；fetch 节点 PC 仍为默认 0)
]
    → caller 显式写：
         pb->node_of_logic_stage("fetch")->operator()(KeyType::PC) = entry_addr;
         // caller: tools/cpu_sim/main.cpp（CLI 路径）+ tests/cpu/integration/test_rv32ui_runner.cpp（fixture 路径）
]
    → pb->run()  // cycle 0 fetch 从 PC=entry_addr 开始
```

**为什么 caller-side**：`build_cpu` 签名（`config, mem=nullptr`）已被既有 spec 锁死，引入 `entry_addr` 第三参数会破坏 ABI 兼容 + 既有 4 个 call site。PC 写入语义简单（一个 KeyType::PC 赋值），caller-side 重复一次代码量 < 5 行 × 2 处。

**add.elf 兼容**：add.elf e_entry=0x0，caller 写 `PC=0` 与默认 0 一致，行为 byte-identical。

### Decision 4: Catch2 fixture 模式（macro 循环）

**Fixture 设计**：
```cpp
// tests/cpu/integration/test_rv32ui_runner.cpp
#define RV32UI_P_TEST(NAME) \
    TEST_CASE("rv32ui-p-" #NAME, "[cpu-integration][riscv-tests]") { \
        run_rv32ui_test("rv32ui-p-" #NAME); \
    }

// 41 个（excl fence_i, ma_data）
RV32UI_P_TEST(add) RV32UI_P_TEST(addi) RV32UI_P_TEST(and)
// ... (40+ 个, 总 41)

// 共享 helper：
static void run_rv32ui_test(const std::string& name) {
    const auto elf_path = std::string(TEST_RV32UI_ELF_DIR) + "/" + name;
    REQUIRE(fs::exists(elf_path));
    
    // 1. load ELF
    auto elf = cf::tools::load_elf_full(elf_path);
    REQUIRE_FALSE(elf.tohost_addr == UINT64_MAX);
    
    // 2. PicolibcHostMemory base=0x80000000
    cf::cpu::PicolibcHostMemory mem({
        .base_addr = 0x80000000,
        .size = 64 * 1024,
        .tohost_addr = elf.tohost_addr
    });
    
    // 3. load sections
    for (auto& [sh_addr, bytes] : elf.sections) {
        mem.load_section(sh_addr, bytes);
    }
    
    // 4. build_cpu + set PC
    auto pb = cf::cpu::CpuFactory<uint32_t>::build_cpu(cfg, &mem);
    pb->node_of_logic_stage("fetch")->operator()(KeyType::PC) = elf.entry_addr;
    
    // 5. run with cycle cap
    const uint64_t MAX_CYCLES = 10000;
    for (uint64_t i = 0; i < MAX_CYCLES; ++i) {
        pb->run();
        if (mem.exited()) break;
    }
    REQUIRE(mem.exited());  // 必须 exit 否则 timeout
    REQUIRE(mem.exit_code() == 0);  // 0=PASS, 1=FAIL
    INFO("cycles=" << actual_cycles);  // JUnit output 包含
}
```

**Cycle cap**：10,000 是 generous 上限（riscv-tests 单测试 ~100 cycles）。超 cap = timeout → catch2 INFO 标记 fail。

**JUnit 输出**（CI 集成）：
```bash
./build/bin/chipforge_tests "[riscv-tests]" --reporter JUnit::out=build/rv32ui-baseline-junit.xml
```

### Decision 5: Triage CSV 产物（fixture 副 action）

**生成路径**（同 fixture，CSV 写入在 REQUIRE 之前）：
```cpp
// test_rv32ui_runner.cpp helper（每个 TEST_CASE 在 REQUIRE 前调）
static std::once_flag g_csv_init;
static void init_csv_once() {
    std::call_once(g_csv_init, []() {
        fs::create_directories(fs::path(RV32UI_CSV_PATH).parent_path());
        std::ofstream f(RV32UI_CSV_PATH, std::ios::trunc);
        f << "elf,status,fail_stage,category,cycles,notes\n";
    });
}

static void append_to_csv(const std::string& name, const std::string& status,
                          const std::string& fail_stage,
                          const std::string& category,
                          uint64_t cycles,
                          const std::string& notes = "") {
    init_csv_once();
    std::ofstream f(RV32UI_CSV_PATH, std::ios::app);
    f << name << "," << status << "," << fail_stage << "," << category 
      << "," << cycles << "," << notes << "\n";
}

// 使用模式（每个 TEST_CASE 必须先 append 再 REQUIRE）：
TEST_CASE("rv32ui-p-add", "[cpu-integration][riscv-tests]") {
    uint64_t cycles = 0;
    int exit_code = 1;  // 默认 FAIL（runner 没跑到 = FAIL）
    bool exited = false;
    
    // setup + run
    // ...
    
    append_to_csv("rv32ui-p-add", exited && exit_code == 0 ? "PASS" : "FAIL",
                  exited && exit_code != 0 ? "execute" : "",
                  exited && exit_code == 0 ? "" : "真 bug",
                  cycles);
    
    REQUIRE(exited());  // runner-mechanics 硬 REQUIRE
    // exit_code 仅作 CSV signal，不 REQUIRE（Wave 2 fix 候选）
}
```

**CSV 路径**：`RV32UI_CSV_PATH` 由 CMake 注入 `${CMAKE_SOURCE_DIR}/soc/cpu/docs/dse/rv32ui-baseline-matrix.csv`（绝对路径，避免 ctest 工作目录在 build/ 的相对路径问题）。

**CSV schema**：
```
elf,status,fail_stage,category,cycles,notes
rv32ui-p-add,PASS,,,5,
rv32ui-p-jalr,FAIL,execute,真 bug,52,
test_3stage_riscv,PASS,,,5,loader upgrade
...
```

**5 pre-existing classification**（`test_3stage_riscv` × 4 + `test_cpu_sim_real_tohost`）— 在同 CSV 跑，**预期全 PASS**（loader 升级副效果），归 `category=toolchain（升级后转 green）`。

### Decision 6: ELF vendor 工作流（一次性 30 分钟工作）

**来源**：riscv-software-src/riscv-tests commit `XXXXXXX` (commit hash 锁定)

**build 脚本**（`tests/cpu/riscv_tests/build_rv32ui.sh`）：
```bash
#!/bin/bash
set -e
RV32_TC=/workspace/project/opt/riscv/bin/riscv32-unknown-elf-gcc
SRC=/path/to/riscv-tests/isa/rv32ui
DEST=$(dirname "$0")/elf

mkdir -p "$DEST"

for src in $(ls "$SRC"/*.S); do
    name=$(basename "$src" .S)
    case "$name" in
        fence_i|ma_data) echo "Skip $name"; continue ;;
    esac
    "$RV32_TC" -march=rv32i -mabi=ilp32 -nostdlib -static \
        -Wl,-Ttext=0x80000000 \
        -I "$SRC/../env/p" \
        -o "$DEST/rv32ui-p-$name" "$src" 2>&1 || echo "FAIL: $name"
done
```

**README**（`tests/cpu/riscv_tests/README.md`）：
```markdown
# riscv-tests rv32ui-p ELF vendor

Pre-built 41 ELF binaries for RV32I compliance baseline (Wave 1 of Phase 1.5).

**Source**: https://github.com/riscv-software-src/riscv-tests @ COMMIT_HASH
**Build container**: riscv-gnu-toolchain docker image SHA256:xxxxx
**Build command**: ./build_rv32ui.sh (in this directory)
**License**: BSD-3-Clause (compatible)

**Excluded tests**: fence_i (requires Zifencei extension), ma_data (misaligned access)
**Total size**: ~270KB

To rebuild:
1. `git clone https://github.com/riscv-software-src/riscv-tests`
2. `cd riscv-tests && git submodule update --init --recursive`
3. `git checkout COMMIT_HASH`
4. cd back to this directory and run `./build_rv32ui.sh`
```

**Plain git 决策**：单文件 <8KB，41 文件 ~270KB，远低于 LFS 阈值（5MB），plain git 即可。

## 4. Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| `addr - base_addr_` underflow（C++ 无符号回绕） | 显式 `if (addr < base_addr_) return;` 早判（Decision 1 §3） |
| 多个 PROGBITS section 加载顺序影响 | 按 sh_addr 升序加载（避免后加载覆盖前加载） |
| `.tohost` section 解析失败（shstrtab 缺） | loader 显式 throw，fixture REQUIRE 验证 |
| base=0x80000000 与 4-byte 字序 | ELF linked at 0x80000000 (rv32ui-p), test ELF linked at 0x0 (add.elf) — 两者互斥运行 |
| `cpu_default.json` 默认 MMU=true 与 riscv-tests 冲突 | fixture 显式 `enable_mmu=false`（Decision 4 §3） |
| RV64 ELF 拒绝逻辑（`elf_loader.h:48-50` 旧逻辑） | 保留拒绝（riscv-tests 是 RV32），但加注释 `// Wave 1 only: RV64 ELFs are rv64ui-p-* (not in this change)` |
| `write_word` partial-byte bug 修复后旧 add.elf 行为差异 | add.elf 写 0 地址 4 字节全 0x04 — 高字节=0，check_tohost 不会被错误触发；回归测 add.elf PASS |
| CSV append 模式多次跑会重复 | fixture SETUP 阶段 `truncate` CSV 头部；每次跑覆盖 |
| 41 ELF vendor 引入新依赖 | README 明示构建容器 + commit hash；CI 不依赖网络/工具链 |
| `e_entry` 写入 PC 时机 | caller-side：cpu_sim main.cpp 或 fixture helper 在 `build_cpu(cfg, &mem)` 返回后、首次 `pb->run()` 前写入 fetch 节点的 `KeyType::PC`（不入 build_cpu，避免破坏既有 4 个 call site 的 ABI 兼容） |
| JUnit reporter 输出格式 vs Catch2 默认输出冲突 | `--reporter JUnit::out=...` 写到文件，stdout 仍用 console reporter（multi-reporter 语法） |

## 5. Migration Plan

### 5.1 Commit 序列（5 commits，TDD 5 步）

```
commit A: PicolibcHostMemory base address window + write_half + write_word partial-byte fix
         + 5 framework tests (test_picolibc_memory_base_window.cpp)
         
commit B: ELF loader 升级 + e_entry PC init + .tohost sh_addr 解析
         + 4 framework tests (test_elf_loader_full.cpp)
         
commit C: cpu_sim --base-addr flag + entry_addr PC write
         + 3 framework tests (test_cpu_sim_base_addr.cpp)
         
commit D: 41 ELF vendor + Catch2 fixture + JUnit reporter
         + 41 riscv-tests-rv32ui tests
         + 5 pre-existing 转 green
         + CSV triage 产物
         
commit E: ADR (optional) + CHANGELOG v0.2.0 + roadmap sync + STATUS update + archive
```

### 5.2 验证检查点

- 每次 commit 后：`cmake --build build && ctest --test-dir build --output-on-failure`
- commit D 后额外：`bash tools/run_chipforge_tests.sh "[riscv-tests]"` + 检查 JUnit XML + 检查 CSV 完整
- 每次 commit 后：`bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh && bash tools/doc_link_check.sh`

### 5.3 回滚策略

- 单 commit 失败：`git revert <commit>`
- 整体回滚：`git revert A..E`（依赖 openspec 归档在独立目录）
- ELF vendor 失败：`rm -rf tests/cpu/riscv_tests/elf/`（不影响主分支其他文件）

### 5.4 文档同步（commit E 内）

- `CHANGELOG.md`：`## v0.2.0 (2026-09-XX) - riscv-tests-rv32ui`
- `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md`：§2 Wave 1 估时 1 周→1.5 周；§11 风险表：Wave 1 缓解了 §11.2 (scoreboard lifecycle) 中"未暴露" 一栏
- `ip/cpu/STATUS.md`：新增"RV32I 合规模性已建立 (v0.2.0)"段落
- `ip/cpu/picolibc_host_memory.h`：Doxygen 注释更新 base window 使用契约
- `tools/cpu_sim/elf_loader.h`：Doxygen 注释更新多 section 加载契约

## 6. Open Questions

| Question | Owner | Resolution |
|----------|-------|------------|
| 41 ELF vendor 来源（容器 vs docker image vs 本地 toolchain build） | dev | 决策点（§3 Decision 6 README）：**本地 toolchain build + README 记录 commit hash**；若 toolchain 不可用则改 docker image（需在 commit A 之前确认） |
| Cycle cap 10000 是否够（含多写 store） | Wave 2 fix | 若不够，triage CSV `category=timeout`，Wave 2 fix 候选 |
| `simple.S` 是否真的通过（含 alignment check） | Wave 1 实测 | 第一波跑过看结果；若 fail，归 `feature stub` |
| 41 ELF vendor commit hash 锁定时机 | dev | build 前必须 git checkout 到 riscv-tests 某 stable commit（如 `master` HEAD 或带 tag 的 commit）；建议 v0.2.0 时用 `master` HEAD + commit hash 写入 README |
| `load_elf_full` 是否引入新 `cf::tools::` 头文件路径 | Wave 1 design | 决策点：放 `tools/cpu_sim/elf_loader.h` 内（不引入新头文件），inline 实现 |
| JUnit reporter 是否引入 build 系统依赖 | Catch2 v3.7 vendor 已含 | 验证：`grep -i junit tests/catch2/catch_amalgamated.hpp` 确认 |