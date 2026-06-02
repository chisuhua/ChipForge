# Phase 2：Bare-metal 测试套件

> **Status**: Not Started
> **Milestone**: M2 - ISA 全覆盖
> **Depends on**: Phase 1

**目标**：完整的 RISC-V ISA 验证与官方合规认证

---

## 任务清单

### 1. riscv-tests 集成

- [ ] rv64ui — 用户级整数指令（80+ 测试）
- [ ] rv64um — 乘除法
- [ ] rv64ua — 原子操作
- [ ] rv64uf — 单精度浮点
- [ ] rv64ud — 双精度浮点
- [ ] rv64uc — 压缩指令
- [ ] rv64si — 特权级指令（S-mode）
- [ ] rv64mi  机器级指令（M-mode）

### 2. RISCOF 合规认证

- [ ] 编写 DUT Plugin（调用 TLM 平台仿真）
- [ ] 对接 Sail/Spike 参考模型
- [ ] 生成 HTML 合规报告

### 3. 自定义功能测试

- [ ] CSR 完整访问测试（`mstatus`, `mtvec`, `mepc` 等）
- [ ] PMP（物理内存保护）区域测试
- [ ] 虚拟内存 Sv39 / Sv48 测试
- [ ] 中断/异常嵌套测试
- [ ] 原子操作正确性测试

### 4. 基准测试

- [ ] Dhrystone（整数性能指标）
- [ ] CoreMark（嵌入式基准）

### 5. 自动化测试驱动

```python
# scripts/run_tests.py
class TestRunner:
    def run_isa_tests(self, arch="rv64gc"):
        # 编译 ELF -> 加载平台 -> 监控 tohost -> 汇总
        ...
    def run_compliance(self):
        # RISCOF 框架调用
        ...
    def generate_report(self):
        # HTML / JSON 测试报告
        ...
```

### 6. 设计空间探索基础

- [ ] 实现 `tools/dse/sweep_driver.py` 参数扫描驱动
- [ ] 编写 `configs/sweep/cache_sweep.json` 示例扫描配置
- [ ] 支持多基准测试 x 多参数组合的批量执行
- [ ] 实现基本的 CSV/JSON 结果汇总

