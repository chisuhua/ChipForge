## Tasks

> **执行顺序**：1 → 2 → 3 → 4 → 5 → 6。
> 每个 Task 的 verify 必须是**可执行命令**。

- [x] ### Task 1: 升级上游依赖（前置）
  - **action**:
    - 上游 CppHDL 已提交 `c27543d` / `5b37b7d` / `eccf58f` / `cba0571`。
      用 `build-jit`（JIT OFF / Verilator OFF / PIC ON）重建 lib 并 install 到
      本仓库 `build/_deps/install`，再重建两个测试 binary。
    - ⚠️ 上游默认 `CH_JIT_ENABLE=ON` 会导致本仓库链接失败，**必须用 `build-jit`**。
  - **verify**:
    ```bash
    cd ../CppHDL && cmake --build build-jit --target cpphdl -j$(nproc)
    cmake --install build-jit --prefix ../ChipForge/build/_deps/install
    cd ../ChipForge && cmake --build build --target chipforge_tests chipforge_tests_chmem -j$(nproc)
    ```

- [x] ### Task 2: drift pin 反转（钉死缺陷 → 锁住修复）
  - **action**:
    - `tests/cpu/test_cpu_chmem_mem_read_drift.cpp`：
      - `mismatches == kTicks` → `mismatches == 0`（每拍观测值 == 当前地址内容）
      - `off_by_one == kTicks` → `off_by_one == 0`（观测值 != 上一拍地址内容，
        排除"恰好等于上一拍"的巧合 ⇒ 幅度断言强度保留）
      - test case 名 `chmem_mem_read_drift_aread_lags_one_cycle` →
        `chmem_mem_read_drift_aread_has_no_skew`（语义已变，名字须同步）
      - 文件头注释整段重写：不再是"钉死 1 拍滞后"，而是"锁住 0-cycle 对齐"，
        并引用上游 `c27543d` 与 `sim-mem-read-timing-align-2026-10`
  - **verify**:
    ```bash
    ./build/bin/chipforge_tests_chmem "[mem-drift]"
    ```
    MUST PASS。

- [x] ### Task 3: drift pin 方向敏感性（变异对照）
  - **action**:
    - 证明该 pin 对"上游退回 posedge 求值"敏感：在上游把 async 读端口改回
      sequential（或等价的最小变异），本测试 MUST FAIL；变异后 MUST 完整还原。
    - 与既有 pin 的做法一致（`chmem-alu-unit-semantics` 用 3 个变异对照自证强度）。
  - **verify**:
    ```bash
    # 变异 → 本测试 MUST FAIL → 还原 → MUST PASS
    ./build/bin/chipforge_tests_chmem "[mem-drift]"
    ```

- [x] ### Task 4: 文档台账更新
  - **action**:
    - `AGENTS.md`：
      - known-issues 的 `⚠️ CH_MEM dmem 读路径 1-cycle 滞后` 条目 → 移入「已解决」，
        写明上游 commit（`c27543d` + `5b37b7d`/`eccf58f`/`cba0571`）、
        方案 B 被实测确认为唯一零改动解法、实测数据（10 FAIL → 0、47 ELF 全 PASS）
      - `[chmem]` family 计数 `72/73 PASS` → `73/73 PASS`
      - 删除该条目中已被推翻的"违约方是 ChipForge"表述（真因在上游契约）
    - `.rddf/roadmap/features/feat-phase-6d-rtl-verification.md`：
      「已知问题（owned）」段关闭；E13 阻塞解除并写明解除依据
  - **verify**:
    ```bash
    bash tools/doc_link_check.sh
    bash tools/roadmap-r9-check.sh
    ```

- [x] ### Task 5: 归档 + 主 spec 修订
  - **action**:
    - `openspec validate chmem-dmem-read-latency-aligned --strict` MUST 通过
    - `openspec archive chmem-dmem-read-latency-aligned -y`（MUST NOT 用 `--skip-specs`）
    - archive 后 MUST 补写主 spec 的 `## Purpose`（CLI 会留 TBD 骨架）
    - **必须删除主 spec Purpose 段的 "Known deviation" 警示块** —— 该 deviation
      已随上游修复不存在，留着就是过期且误导的告警
    - 主 spec 内 MUST NOT 残留 change 名引用
  - **verify**:
    ```bash
    openspec validate --specs --strict
    grep -rn "chmem-dmem-read-latency-aligned" openspec/specs/   # MUST 命中 0 行
    grep -rn "Known deviation" openspec/specs/chmem-dmem-read-latency/  # MUST 命中 0 行
    ```

- [x] ### Task 6: 全量回归
  - **action**:
    - 跑 TLM 全套 + CH_MEM 全套 + 5 架构 gate
  - **verify**:
    ```bash
    ./build/bin/chipforge_tests                       # 480/481（唯一失败 = 已知 7stage node_count drift）
    ./build/bin/chipforge_tests_chmem "[chmem]"       # 73/73 PASS
    ./build/bin/chipforge_tests_chmem "[verilator],[mmu-verilator]"
    for g in verify_adr verify_plugin_decision check_plugin_portability doc_link_check roadmap-r9-check; do bash tools/$g.sh; done
    ```
