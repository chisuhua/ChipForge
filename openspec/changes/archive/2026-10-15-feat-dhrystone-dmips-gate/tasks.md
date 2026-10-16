## Tasks

- [x] ### Task 1: H1+H2 — 新建 `link_dhrystone.ld` + 改 `dhrystone.c` 的 `tohost_pass` (commit `f95b2d0` [F1 H1+H2])
- [x] ### Task 2: H3 — `_start` 加 sp init (crt0 替代) (commit `f95b2d0` [F1 H3] + `tests/cpu/manual_elf/dhrystone/dhrystone_crt0.S` 新建)
- [x] ### Task 3: 测试 window_base 修 + 公式修为标准量纲 + gate 真值 (commit `f95b2d0` [F1 B1 part c] + [F2 B2] + [N2])
- [x] ### Task 4: CMakeLists.txt add_custom_command 加 dhrystone target (commit `f95b2d0` [B3])
- [x] ### Task 5: 跑 [cpu-integration][dhrystone] 2 TEST_CASE 验证 (target #1 + gate #1.4) (实测 1.43 DMIPS/MHz ≥ 1.4 硬门禁, 3/3 PASS)
- [x] ### Task 6: CSV 重写 (N1) + spec-delta (AC11) (commit `f95b2d0` [N1] + `openspec/specs/dhrystone-benchmark/spec.md` archive commit `c1cb8ec`)
- [x] ### Task 7: 架构 gate 0 FAIL (AC13) (实测 33+8+12 全 PASS, 0 FAIL)
- [x] ### Task 8: 1 commit per CONTRIBUTING.md:25 prefix `fix(dhrystone):` 或 `chore(dhrystone):` (commit `f95b2d0` 实装 + commit `c1cb8ec` archive chore, 2 commits 全部按 prefix)
