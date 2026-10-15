# cpu-pipeline-tick-rca

诊断 cpu_sim CLI 路径 **fetch 永 stall** 的根因 (5 个 RISC-V 仿真测试 FAIL, C++ API 路径 48/48 PASS).
**"tick-rca" 是 commit `5403718` 命名前的目录名, 真实症状是 fetch stage 永 stall** (`pb->run()` 持续推进, decode/memory payload 全 0, tohost 永不写). **复合根因 3 层 + F4 待验证**(config 保真度 F1 + 测试 config 意图 F2 + MMU 瞬态语义 F3). **纯诊断 change, 不动 product code 修复** — 输出诊断报告 + 后续修复 change 建议.
