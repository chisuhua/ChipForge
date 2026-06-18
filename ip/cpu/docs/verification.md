# CPU 验证测试

## 测试层级

### Level A - 单元测试
- Plugin 独立功能验证
- 指令解码正确性
- 流水线推进逻辑

### Level B - 集成测试
- CPU + Cache 联合仿真
- 中断响应时序
- 内存访问序列

### Level C - 系统测试
- riscv-tests 官方测试套件
- RISCOF 合规测试
- 基准程序（Dhrystone、CoreMark）

## 验证方法
- **Spike 对比**：与 Spike ISS 结果对比
- **COMPARE 模式**：TLM vs RTL 逐拍对比
- **覆盖收集**：指令覆盖、分支覆盖、异常覆盖

## 运行方式
```bash
# 运行单元测试
ctest --test-dir build -L unit

# 运行 riscv-tests
./scripts/run_riscv_tests.sh

# 运行 RISCOF 合规测试
./scripts/run_riscof.sh
```
