# 错误处理与异常管理

## 1. 设计目标

建立统一的错误处理框架，确保 TLM 和 RTL 层面的异常行为一致，支持可观测性和调试。

## 2. 错误分类

### 2.1 硬件异常
| 异常类型 | 来源 | 处理方式 |
|---------|------|---------|
| 指令页错误 | MMU | trap 到 M/S 模式 |
| 加载页错误 | MMU | trap 到 M/S 模式 |
| 存储页错误 | MMU | trap 到 M/S 模式 |
| 非法指令 | 解码器 | trap + mcause |
| 地址未对齐 | LSU | trap + mtval |
| 断点 | Debug | 进入调试模式 |
| 环境调用 | ECALL | trap（系统调用） |

### 2.2 总线错误
| 错误类型 | 场景 | Bundle 表示 |
|---------|------|-----------|
| 地址译码失败 | 访问无效地址 | MemRespBundle.status = ERROR |
| 访问权限违例 | PMP 检查失败 | MemRespBundle.status = PERMISSION_ERROR |
| 超时 | 从设备无响应 | MemRespBundle.status = TIMEOUT |
| 数据损坏 | ECC 校验失败 | MemRespBundle.status = DATA_ERROR |

### 2.3 仿真错误
| 错误类型 | 场景 | 处理方式 |
|---------|------|---------|
| 配置无效 | JSON 参数越界 | 启动时断言失败 |
| 死锁检测 | ch_stream 无推进 | 超时告警 + 仿真终止 |
| 断言违例 | 内部状态不一致 | 日志 + 可选终止 |
| TLM/RTL 不一致 | COMPARE 模式差异 | 记录差异 + 报告 |

## 3. ch_stream 错误信号设计

### 3.1 MemRespBundle 错误字段
```cpp
struct MemRespBundle {
    uint64_t data;
    enum Status {
        OK = 0,
        ERROR = 1,
        PERMISSION_ERROR = 2,
        TIMEOUT = 3,
        DATA_ERROR = 4
    } status;
    uint32_t error_info;  // 附加错误信息
};
```

### 3.2 错误传播机制
```
CPU → Cache → Memory
         ↓ (error)
CPU ← Cache ← Memory (error response)
  ↓
trap handler (if hardware exception)
```

## 4. 异常在 TLM/RTL 间的对齐

### 4.1 TLM 异常模型
- 使用 C++ 异常或返回码表示
- EventQueue 调度异常处理事件
- 统计收集器记录异常频次

### 4.2 RTL 异常模型
- 使用 CppHDL 信号表示（Wire\<ExceptionBundle\>）
- 异常优先级仲裁
- 精确异常语义保证

### 4.3 COMPARE 模式下的异常验证
- TLM 和 RTL 在相同输入下必须产生相同异常
- 异常发生时间允许偏差（TLM 可能合并周期）
- 异常原因码必须完全一致

## 5. 调试支持

### 5.1 日志系统
```cpp
// 分级日志
LOG_ERROR("Cache") << "ECC error at addr=" << addr;
LOG_WARN("CPU") << "Unaligned access at PC=" << pc;
LOG_DEBUG("Bus") << "Decode failed for addr=" << addr;
```

### 5.2 波形追踪
- TLM 模式：EventQueue 事件追踪（JSON/VCD）
- RTL 模式：CppHDL 波形导出（VCD/FST）
- COMPARE 模式：差异点自动标注

### 5.3 断言检查
```cpp
// 可配置断言（仿真 vs 综合）
CH_ASSERT(cache_state != INVALID, "Unexpected invalid state");
CH_ASSUME(req.addr % 4 == 0, "Aligned access expected");
```

## 6. 错误注入框架

支持主动注入错误以验证容错能力：
- 内存 bit-flip 注入
- 总线超时注入
- 缓存一致性违例注入

## 7. CppTLM 框架错误机制

基于 CppTLM 实际代码（`CppTLM/include/`），框架提供以下错误处理基础设施：

### 7.1 配置错误检测

基于 `core/param_errors.hh`，框架提供参数校验异常类：

```cpp
// cpptlm::ParamValidationError — 继承 std::invalid_argument
class ParamValidationError : public std::invalid_argument {
public:
    std::string module_name;    // 出错模块名
    std::string param_name;     // 出错参数名
    std::string rule_violated;  // 违反的校验规则描述

    ParamValidationError(const std::string& module,
                         const std::string& param,
                         const std::string& reason);
};
```

**检测能力：**
- **参数类型校验**：通过 C++ 类型系统和运行时检查确保参数类型正确
- **必填参数检查**：模块实例化时校验必需参数是否提供
- **范围验证**：通过 `rule_violated` 字段描述具体违反的约束条件

**使用方式：**
```cpp
// 模块构造时抛出配置错误
throw cpptlm::ParamValidationError(
    "L1Cache",          // module_name
    "cache_size_kb",    // param_name
    "Value must be power of 2, got 48"  // rule_violated
);
```

### 7.2 分层错误码体系

基于 `core/error_category.hh`，框架定义了分层错误分类系统：

```cpp
// 错误类别（高字节）
enum class ErrorCategory : uint8_t {
    SUCCESS     = 0x00,  // 成功
    TRANSPORT   = 0x01,  // 传输层错误
    RESOURCE    = 0x02,  // 资源层错误
    COHERENCE   = 0x03,  // 一致性层错误
    PROTOCOL    = 0x04,  // 协议层错误
    SECURITY    = 0x05,  // 安全层错误
    PERFORMANCE = 0x06,  // 性能层错误
    CUSTOM      = 0x10,  // 用户自定义
};

// 错误码格式：0x{category}_{code}
enum class ErrorCode : uint16_t {
    // 传输层 (0x01xx)
    TRANSPORT_INVALID_ADDRESS = 0x0100,
    TRANSPORT_TIMEOUT         = 0x0102,
    TRANSPORT_NO_ROUTE        = 0x0104,
    // 资源层 (0x02xx)
    RESOURCE_BUFFER_FULL      = 0x0200,
    RESOURCE_OUT_OF_MEMORY    = 0x0201,
    // 一致性层 (0x03xx)
    COHERENCE_DEADLOCK        = 0x0301,
    COHERENCE_DATA_INCONSISTENCY = 0x0303,
    // ... 更多错误码
};
```

**辅助函数：**
| 函数 | 功能 |
|------|------|
| `get_error_category(code)` | 从错误码提取类别 |
| `error_code_to_string(code)` | 错误码转可读字符串 |
| `is_fatal_error(code)` | 判断是否为致命错误（DEADLOCK/DATA_INCONSISTENCY/TAMPER/OOM） |
| `is_recoverable_error(code)` | 判断是否可恢复（BUFFER_FULL/TIMEOUT/STARVATION） |

### 7.3 错误上下文扩展

基于 `ext/error_context_ext.hh`，框架通过 TLM Extension 机制将错误信息附加到事务 payload：

```cpp
struct ErrorContextExt : public tlm::tlm_extension<ErrorContextExt> {
    // 核心错误信息
    ErrorCode error_code;
    ErrorCategory error_category;
    std::string error_message;
    std::string source_module;
    uint64_t timestamp;

    // 堆栈追踪
    struct StackFrame {
        std::string module;
        std::string function;
        std::string context;
    };
    std::vector<StackFrame> stack_trace;

    // 上下文数据（键值对）
    std::map<std::string, uint64_t> context_data;

    // 一致性特定字段
    CoherenceState expected_state;
    CoherenceState actual_state;
    std::vector<uint32_t> sharers;
};
```

**便捷 API：**
```cpp
// 获取 payload 上的错误上下文
ErrorContextExt* ext = get_error_context(payload);

// 创建并附加错误上下文
ErrorContextExt* ext = create_error_context(
    payload,
    ErrorCode::COHERENCE_STATE_VIOLATION,
    "Expected EXCLUSIVE but found SHARED",
    "L2Cache"
);

// 添加堆栈帧
ext->add_stack_frame("L2Cache", "handle_snoop", "line 142");

// 设置上下文数据
ext->set_context_data("address", 0x80001000);
ext->set_context_data("expected_sharers", 1);
```

### 7.4 运行时事务追踪

基于 `framework/transaction_tracker.hh`，框架提供事务生命周期追踪：

```cpp
auto& tracker = TransactionTracker::instance();
tracker.initialize();

// 创建事务记录
uint64_t tid = tracker.create_transaction(payload, "CPU", "READ");

// 记录经过的模块（hop）
tracker.record_hop(tid, "L1Cache", 2, "MISS");
tracker.record_hop(tid, "L2Cache", 8, "HIT");

// 完成事务
tracker.complete_transaction(tid);
```

**核心能力：**
- **事务生命周期追踪**：创建/跳转/完成，记录每个阶段时间戳
- **父子事务关联**：支持事务分裂（如 cache line fill 拆分为多个 burst）
- **分片事务追踪**：通过 `fragment_id` / `fragment_total` 追踪分片聚合
- **活跃事务查询**：`get_active_transactions()` 检测可能泄漏或超时的事务
- **粗/细粒度控制**：可独立开关父事务追踪和分片追踪

### 7.5 调试追踪器

基于 `framework/debug_tracker.hh`，框架提供错误记录与状态历史追踪：

```cpp
auto& dbg = DebugTracker::instance();
dbg.initialize(true, true, false);  // errors=on, state=on, stop_on_fatal=off

// 记录错误（自动关联 transaction_id）
uint64_t eid = dbg.record_error(
    payload,
    ErrorCode::COHERENCE_DEADLOCK,
    "Circular dependency detected",
    "DirectoryController"
);

// 记录一致性状态转换
dbg.record_state_transition(
    0x80001000,              // address
    CoherenceState::SHARED,  // from
    CoherenceState::INVALID, // to
    "INVALIDATE",            // event
    tid                      // transaction_id
);
```

**查询接口：**
| 方法 | 说明 |
|------|------|
| `get_error(error_id)` | 按错误 ID 查询 |
| `get_errors_by_transaction(tid)` | 按事务 ID 查询关联错误 |
| `get_errors_by_category(cat)` | 按错误类别查询 |
| `get_errors_by_module(module)` | 按模块查询 |
| `get_state_history(address)` | 获取地址的状态转换历史 |

### 7.6 错误处理流程

```
模块检测到异常
    ↓
创建 ErrorContextExt 附加到 payload
    ↓
DebugTracker.record_error() 记录错误
    ↓
判断 is_fatal_error()?
    ├─ 是 → stop_on_fatal 模式下终止仿真
    └─ 否 → 判断 is_recoverable_error()?
          ├─ 是 → 重试/降级（如 BUFFER_FULL → 反压等待）
          └─ 否 → 报告错误，继续仿真
```

## 8. 相关文档
- [接口设计详解](interface-design.md)
- [测试与 DSE 框架](testing-and-dse.md)
- [项目架构总览](overview.md)
