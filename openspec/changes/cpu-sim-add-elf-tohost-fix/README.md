# cpu-sim-add-elf-tohost-fix

修复 cpu_sim 跑 add.elf (RV32I, 无 .tohost section) 跑不到 tohost=1 的预先存在 FAIL — 让 pipeline 真正 tick + 加 tohost fallback 检测
