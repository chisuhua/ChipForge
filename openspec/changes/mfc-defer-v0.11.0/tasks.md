---
initiative: wave5-isa-coverage-and-bp
priority: P0
version_target: v0.11.0
---

# Tasks — mfc-defer-v0.11.0

## 1. Handoff Doc 落地 (本 change 实施)

- [x] 1.1 写真实 evidence (引用 commit hashes + baseline matrix + Oracle 双复审结论)
- [x] 1.2 v0.11.0 启动 checklist 写入 (mfc 重启 entry point: advance_fsm + fetch stall + archive)
- [x] 1.3 Out of scope 明确 (不修 advance_fsm / fetch stall / BTB 在本 change)

## 2. mfc 当前 partial advance 状态保留 (47/60)

- [x] 2.1 A' commit 722eca6: negotiate fix → div_fsm tohost=1 PASS
- [x] 2.2 C' commit 76ac53f + c4035e5: 100M cap → DMIPS 0.0351 (livelock 确认)
- [x] 2.3 B2 fix commit b8f4769: at_stage 闭包 state_==IDLE 守卫
- [x] 2.4 v1 archive commit 4429896 (cpu-pipeline-mmufault-handler)

## 3. v0.11.0 mfc 重启 prep (deferred, 仅在 v0.11.0 launch 时启动)

- [ ] 3.1 创建 follow-up change `mfc-extract-fsm-restore` 引用本文档 (v0.11.0 launch 触发)
- [ ] 3.2 实装 advance_fsm 真 radix-2 iterative (1-2 周, 32 cycle quotient bit-by-bit)
- [ ] 3.3 实装 fetch stall framework 扩展 (1-2 周, 类似 HazardPlugin RAW stall 模式)
- [ ] 3.4 跑 vendor rv32um 8/8 PASS (Phase E.5 实成)
- [ ] 3.5 跑 Dhrystone DMIPS/MHz ≥1.4 (受限于 BTB, 可能需要 wave5-bp 协同)
- [ ] 3.6 archive mfc-cpu-pipeline-multi-cycle-fsm 当前 47/60 partial advance

## 4. v0.10.0 launch 范围确认 (本 change 不阻塞)

- [x] 4.1 mfc 不阻塞 v0.10.0 launch (mfc 是 wave5 主变更, 非 v0.10.0 launch gate)
- [x] 4.2 v0.10.0 launch 仍按既定 scope (v1 + v2 等其他 wave5 changes + 不依赖 mfc)
- [x] 4.3 honesty audit (v0100-bootstrap.sh §honesty_audit) 写真实 mfc partial 状态, 不宣称 v0.10.0 launch 通过 mfc