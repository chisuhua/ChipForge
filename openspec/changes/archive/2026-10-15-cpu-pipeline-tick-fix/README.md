# cpu-pipeline-tick-fix

Fix 5 cpu_sim CLI FAIL tests by 4 minimal product code changes (F1+F2+F3a+F3b).

**Successor to** `openspec/changes/archive/2026-10-15-cpu-pipeline-tick-rca/` (P2 实测确诊复合根因后启动).

**Outcome (实测)**:
- 5 cpu_sim CLI tests: baseline FAIL → PASS, cycles 1000 → 7
- 0 product code change 超出 4 处 (cpu_factory.h 不动, D4 sentinel 维持)
- 架构 gate (verify_adr 33/33 + verify_plugin_decision 8/8 + check_plugin_portability 12/12) 0 FAIL

**Naming caveat**: directory name "tick-fix" 沿用 predecessor cpu-pipeline-tick-rca 命名; 真实目标是修 "fetch 永 stall" (pb->run() 持续推进, decode/memory payload 全 0).
