# feat-dhrystone-dmips-gate

Dhrystone benchmark harness repair (3 layers: H1 tohost out-of-window + H2 .text@0x0 dropped + H3 no crt0 sp init) + standard DMIPS formula (N*1e6/(1757*cycles), was 1757*N/cycles off by 3.087x) + -O2 build flag → 真 baseline **1.43 DMIPS/MHz** (was 伪 0.0351).

**Successor to** `openspec/changes/archive/2026-10-16-cpu-pipeline-tick-fix/` (cpu_sim harness 修绿, 测量底座解锁)

**Outcome (实测, commit f95b2d0)**:
- `[cpu-integration][dhrystone]` 1 FAIL → 3/3 PASS (legacy + fsm_dmips_mhz + fsm_metric)
- DMIPS/MHz: 0.0351 (伪) → 1.43 (真) — **40.84× 增长**, 1.4 hard gate 兑现
- 0 regression: [cpu] 129/129 + [riscv-tests] 48/48 + [mmu] 59/59 + [cpu-integration][mul-div-fsm-integration] 4/4 + [cpu-l1-mmu-demo] 6/6
- 架构 gate 0 FAIL (33+8+12)

**Naming caveat**: directory name "tick-fix" → "dmips-gate" 切换; 真实目标是修 harness contract violation (H1+H2+H3) + 公式量纲错误 (3.087× 宽) + 缺 -O2 (B1+B2+B3), 不是修 "DMIPS 计算"
