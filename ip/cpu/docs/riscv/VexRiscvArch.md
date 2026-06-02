# RV64I-Plugin-CPU 架构设计文档

**Architecture Design Document**  
**Version**: 1.0  
**Date**: 2026-05-08  
**Status**: Draft

## 1. 架构概述

### 1.1 设计哲学

本项目的核心设计哲学借鉴自 [VexRiscv](https://github.com/SpinalHDL/VexRiscv) 的 Plugin 架构：

> **指令行为与流水线机制完全解耦**

传统 CPU 仿真器将指令逻辑硬编码在流水线各阶段，导致：
- 添加新指令需要修改核心代码
- 无法灵活配置微架构参数
- 难以独立测试特定指令

Plugin 架构通过**回调机制**将指令逻辑封装为独立模块，实现：
- 添加新指令 = 添加新 Plugin（零修改核心代码）
- JSON 配置驱动微架构（发射数/流水级数/乱序）
- Plugin 可独立测试和组合

### 1.2 系统架构图

```
┌──────────────────────────────────────────────────────────────┐
│                     RV64ICore (Container)                     │
│                                                               │
│  ┌────────────────────────────────────────────────────────┐  │
│  │              Configurable Pipeline                     │  │
│  │  ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐ │  │
│  │  │  IF  │──▶│  ID  │──▶│  EX  │──▶│ MEM  │──▶│  WB  │ │  │
│  │  │ Fetch│   │Decode│   │Execute│  │Memory│   │Write │ │  │
│  │  └──────┘   └──────┘   └──────┘   └──────┘   └──────┘ │  │
│  └────────────────────────────────────────────────────────┘  │
│         │          │          │          │          │         │
│         ▼          ▼          ▼          ▼          ▼         │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                  Plugin Manager                       │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌─────────┐ │   │
│  │  │ on_fetch │ │ on_decode│ │on_execute│ │on_memory│ │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └─────────┘ │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐ │
│  │ ALU Plugin  │  │ Load/Store   │  │ Branch Plugin       │ │
│  │ (ADD/SUB/…) │  │ (LD/LW/SW/…) │  │ (BEQ/BNE/JAL/JALR)  │ │
│  └─────────────┘  └──────────────┘  └─────────────────────┘ │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │          Register File (x0-x31, 64-bit)               │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

### 1.3 核心组件

| 组件 | 职责 | 文件 |
|------|------|------|
| RV64ICore | 容器，管理流水线和 Plugin | `include/rv64i_core.hh` |
| ConfigurablePipeline | 可配置流水线引擎 | `include/pipeline.hh` |
| CpuPlugin | Plugin 基类 | `include/cpu_plugin.hh` |
| PluginManager | Plugin 注册和调度 | `include/plugin_manager.hh` |
| PipelineStageIF | 流水线阶段接口 | `include/pipeline_stage.hh` |
| RegisterFile | 寄存器文件 | `include/register_file.hh` |

## 2. Plugin 系统设计

### 2.1 Plugin 基类

```cpp
// include/cpu_plugin.hh
#ifndef CPU_PLUGIN_HH
#define CPU_PLUGIN_HH

#include <string>
#include <cstdint>

class RV64ICore;
class PipelineStageIF;
class PipelineControl;

/**
 * @brief CPU Plugin 基类
 * 
 * 所有 Plugin 必须从此类派生，实现特定阶段的回调方法。
 * 默认实现为空，Plugin 只需重写需要的方法。
 */
class CpuPlugin {
protected:
    RV64ICore* core_ = nullptr;
    std::string name_;

public:
    virtual ~CpuPlugin() = default;

    void set_core(RV64ICore* core) { core_ = core; }
    const std::string& name() const { return name_; }

    // ========== 流水线阶段回调 ==========

    /**
     * @brief Fetch 阶段回调
     * @param stage 流水线阶段接口
     * 
     * 典型用途：分支预测器在此阶段预测下一条指令地址
     */
    virtual void on_fetch(PipelineStageIF& stage) { (void)stage; }

    /**
     * @brief Decode 阶段回调
     * @param stage 流水线阶段接口
     * 
     * 典型用途：识别指令类型，设置有效标志
     */
    virtual void on_decode(PipelineStageIF& stage) { (void)stage; }

    /**
     * @brief Execute 阶段回调
     * @param stage 流水线阶段接口
     * 
     * 典型用途：执行 ALU 运算，计算分支目标
     */
    virtual void on_execute(PipelineStageIF& stage) { (void)stage; }

    /**
     * @brief Memory 阶段回调
     * @param stage 流水线阶段接口
     * 
     * 典型用途：Load/Store 指令访问内存
     */
    virtual void on_memory(PipelineStageIF& stage) { (void)stage; }

    /**
     * @brief Writeback 阶段回调
     * @param stage 流水线阶段接口
     * 
     * 典型用途：准备写回数据
     */
    virtual void on_writeback(PipelineStageIF& stage) { (void)stage; }

    // ========== 流水线控制回调 ==========

    /**
     * @brief 冒险检测回调
     * @param ctrl 流水线控制接口
     * 
     * 典型用途：检测 RAW 冒险，请求停顿
     */
    virtual void on_stall_check(PipelineControl& ctrl) { (void)ctrl; }

    /**
     * @brief 分支刷新回调
     * @param ctrl 流水线控制接口
     * 
     * 典型用途：分支预测误判时刷新流水线
     */
    virtual void on_flush_check(PipelineControl& ctrl) { (void)ctrl; }

    // ========== 生命周期回调 ==========

    virtual void on_init() {}
    virtual void on_reset() {}
};

#endif // CPU_PLUGIN_HH
```

### 2.2 Plugin 接口示例

**PipelineStageIF 接口**：

```cpp
// include/pipeline_stage.hh
class PipelineStageIF {
public:
    virtual ~PipelineStageIF() = default;

    // 指令字段
    virtual uint32_t instruction() const = 0;
    virtual uint64_t pc() const = 0;
    virtual uint8_t opcode() const = 0;
    virtual uint8_t rd() const = 0;
    virtual uint8_t rs1() const = 0;
    virtual uint8_t rs2() const = 0;
    virtual uint8_t funct3() const = 0;
    virtual uint8_t funct7() const = 0;
    virtual int64_t imm() const = 0;

    // 寄存器值
    virtual uint64_t rs1_value() const = 0;
    virtual uint64_t rs2_value() const = 0;

    // ALU 结果
    virtual uint64_t alu_result() const = 0;
    virtual void set_alu_result(uint64_t val) = 0;

    // 内存访问
    virtual uint64_t mem_addr() const = 0;
    virtual uint64_t mem_data_in() const = 0;
    virtual void set_mem_data_out(uint64_t val) = 0;

    // 写回
    virtual uint64_t writeback_value() const = 0;
    virtual void set_writeback_value(uint64_t val) = 0;
    virtual bool should_writeback() const = 0;
    virtual void set_should_writeback(bool val) = 0;

    // 分支控制
    virtual bool is_branch() const = 0;
    virtual bool branch_taken() const = 0;
    virtual void set_branch_taken(bool val) = 0;
    virtual uint64_t branch_target() const = 0;
    virtual void set_branch_target(uint64_t val) = 0;

    // 流水线控制
    virtual void stall() = 0;
    virtual void flush() = 0;
    virtual bool is_stalled() const = 0;
    virtual bool is_valid() const = 0;
    virtual void set_valid(bool val) = 0;
};
```

### 2.3 ALU Plugin 实现

```cpp
// include/plugins/alu_plugin.hh
#ifndef ALU_PLUGIN_HH
#define ALU_PLUGIN_HH

#include "../cpu_plugin.hh"

/**
 * @brief ALU 指令 Plugin
 * 
 * 处理 RV64I 所有整数运算指令：
 * - 寄存器运算：ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
 * - 立即数运算：ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI
 * - 高位指令：LUI, AUIPC
 */
class AluPlugin : public CpuPlugin {
public:
    AluPlugin() { name_ = "alu"; }

    /**
     * @brief Decode 阶段：识别 ALU 指令
     */
    void on_decode(PipelineStageIF& stage) override;

    /**
     * @brief Execute 阶段：执行 ALU 运算
     */
    void on_execute(PipelineStageIF& stage) override;

private:
    // RV64I 指令类型识别
    bool is_alu_reg(uint8_t opcode) const;
    bool is_alu_imm(uint8_t opcode) const;
    bool is_lui(uint8_t opcode) const;
    bool is_auipc(uint8_t opcode) const;
};

#endif // ALU_PLUGIN_HH
```

```cpp
// src/plugins/alu_plugin.cc
#include "../../include/plugins/alu_plugin.hh"
#include "../../include/pipeline_stage.hh"

void AluPlugin::on_decode(PipelineStageIF& stage) {
    uint8_t opcode = stage.opcode();

    // 识别 ALU 指令
    if (is_alu_reg(opcode) || is_alu_imm(opcode) || 
        is_lui(opcode) || is_auipc(opcode)) {
        stage.set_valid(true);
    }
}

void AluPlugin::on_execute(PipelineStageIF& stage) {
    uint8_t opcode = stage.opcode();

    // 获取操作数
    uint64_t rs1 = stage.rs1_value();
    uint64_t rs2 = stage.rs2_value();
    int64_t imm = stage.imm();
    uint8_t funct3 = stage.funct3();
    uint8_t funct7 = stage.funct7();

    uint64_t result = 0;

    // LUI: 加载高位立即数
    if (opcode == 0b0110111) {  // OP_IMM_32
        result = static_cast<uint64_t>(imm);
        stage.set_alu_result(result);
        stage.set_should_writeback(true);
        stage.set_writeback_value(result);
        return;
    }

    // AUIPC: PC 相对高位
    if (opcode == 0b0010111) {  // LUI
        result = static_cast<uint64_t>(imm) + stage.pc();
        stage.set_alu_result(result);
        stage.set_should_writeback(true);
        stage.set_writeback_value(result);
        return;
    }

    // 立即数运算
    if (is_alu_imm(opcode)) {
        rs2 = static_cast<uint64_t>(imm);
    }

    // 根据 funct3 执行运算
    switch (funct3) {
        case 0b000: // ADD/ADDI or SUB
            if (opcode == 0b0110011 && funct7 == 0b0100000) {
                result = rs1 - rs2;  // SUB
            } else {
                result = rs1 + rs2;  // ADD/ADDI
            }
            break;
        case 0b001: // SLL/SLLI
            result = rs1 << (rs2 & 0x3F);
            break;
        case 0b010: // SLT/SLTI
            result = static_cast<int64_t>(rs1) < static_cast<int64_t>(rs2) ? 1 : 0;
            break;
        case 0b011: // SLTU/SLTIU
            result = rs1 < rs2 ? 1 : 0;
            break;
        case 0b100: // XOR/XORI
            result = rs1 ^ rs2;
            break;
        case 0b101: // SRL/SRA/SRLI/SRAI
            if (funct7 == 0b0100000) {
                result = static_cast<int64_t>(rs1) >> (rs2 & 0x3F);  // SRA/SRAI
            } else {
                result = rs1 >> (rs2 & 0x3F);  // SRL/SRLI
            }
            break;
        case 0b110: // OR/ORI
            result = rs1 | rs2;
            break;
        case 0b111: // AND/ANDI
            result = rs1 & rs2;
            break;
    }

    stage.set_alu_result(result);
    stage.set_should_writeback(true);
    stage.set_writeback_value(result);
}

bool AluPlugin::is_alu_reg(uint8_t opcode) const {
    return opcode == 0b0110011;  // OP
}

bool AluPlugin::is_alu_imm(uint8_t opcode) const {
    return opcode == 0b0010011;  // OP_IMM
}

bool AluPlugin::is_lui(uint8_t opcode) const {
    return opcode == 0b0110111;  // LUI
}

bool AluPlugin::is_auipc(uint8_t opcode) const {
    return opcode == 0b0010111;  // AUIPC
}
```

### 2.4 Plugin 注册表

```cpp
// include/plugin_manager.hh
#ifndef PLUGIN_MANAGER_HH
#define PLUGIN_MANAGER_HH

#include "cpu_plugin.hh"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

/**
 * @brief Plugin 管理器
 * 
 * 负责 Plugin 的注册、调度和生命周期管理
 */
class PluginManager {
public:
    /**
     * @brief 注册 Plugin
     * @param plugin Plugin 实例
     */
    void register_plugin(std::unique_ptr<CpuPlugin> plugin);

    /**
     * @brief 批量注册 Plugin
     * @param plugins Plugin 列表
     */
    void register_plugins(std::vector<std::unique_ptr<CpuPlugin>> plugins);

    /**
     * @brief 调用所有 Plugin 的 on_fetch 回调
     */
    void on_fetch(PipelineStageIF& stage);

    /**
     * @brief 调用所有 Plugin 的 on_decode 回调
     */
    void on_decode(PipelineStageIF& stage);

    /**
     * @brief 调用所有 Plugin 的 on_execute 回调
     */
    void on_execute(PipelineStageIF& stage);

    /**
     * @brief 调用所有 Plugin 的 on_memory 回调
     */
    void on_memory(PipelineStageIF& stage);

    /**
     * @brief 调用所有 Plugin 的 on_writeback 回调
     */
    void on_writeback(PipelineStageIF& stage);

    /**
     * @brief 调用所有 Plugin 的 on_stall_check 回调
     */
    void on_stall_check(PipelineControl& ctrl);

    /**
     * @brief 调用所有 Plugin 的 on_flush_check 回调
     */
    void on_flush_check(PipelineControl& ctrl);

    /**
     * @brief 初始化所有 Plugin
     */
    void init_all();

    /**
     * @brief 复位所有 Plugin
     */
    void reset_all();

private:
    std::vector<std::unique_ptr<CpuPlugin>> plugins_;
};

#endif // PLUGIN_MANAGER_HH
```

## 3. 流水线设计

### 3.1 5 级标量顺序流水线

```
Cycle 1:  IF ──► ID ──► EX ──► MEM ──► WB
Cycle 2:        IF ──► ID ──► EX ──► MEM ──► WB
Cycle 3:              IF ──► ID ──► EX ──► MEM ──► WB
```

**阶段寄存器**：

```cpp
// include/pipeline.hh
struct PipelineReg {
    bool valid = false;          // 有效标志
    uint32_t instr = 0;          // 指令编码
    uint64_t pc = 0;             // 程序计数器
    uint64_t rs1_val = 0;        // rs1 寄存器值
    uint64_t rs2_val = 0;        // rs2 寄存器值
    uint64_t alu_result = 0;     // ALU 运算结果
    uint64_t mem_data = 0;       // 内存读取数据
    uint64_t wb_value = 0;       // 写回值
    uint8_t rd = 0;              // 目标寄存器
    uint8_t rs1 = 0;             // rs1 寄存器索引
    uint8_t rs2 = 0;             // rs2 寄存器索引
    bool should_wb = false;      // 是否需要写回
    bool branch_taken = false;   // 分支是否跳转
    uint64_t branch_target = 0;  // 分支目标地址
    bool is_branch = false;      // 是否分支指令
    bool stalled = false;        // 是否停顿
};

class ConfigurablePipeline {
private:
    RV64ICore* core_;
    PipelineReg if_id_;   // IF/ID 阶段寄存器
    PipelineReg id_ex_;   // ID/EX 阶段寄存器
    PipelineReg ex_mem_;  // EX/MEM 阶段寄存器
    PipelineReg mem_wb_;  // MEM/WB 阶段寄存器

    PluginManager plugin_mgr_;
    RegisterFile reg_file_;

    uint64_t pc_ = 0;           // 当前 PC
    uint64_t next_pc_ = 0;      // 下一条指令 PC
    bool flush_requested_ = false;
    bool stall_requested_ = false;

public:
    ConfigurablePipeline(RV64ICore* core);

    // 流水线推进
    void tick();

    // 刷新流水线
    void flush();

    // 请求停顿
    void request_stall() { stall_requested_ = true; }

    // 访问器
    uint64_t get_pc() const { return pc_; }
    void set_pc(uint64_t pc) { pc_ = pc; }
    RegisterFile& get_reg_file() { return reg_file_; }

private:
    void fetch_stage();
    void decode_stage();
    void execute_stage();
    void memory_stage();
    void writeback_stage();

    void update_pc();
    bool check_hazards();
};
```

### 3.2 流水线推进逻辑

```cpp
// src/pipeline.cc
void ConfigurablePipeline::tick() {
    // 从后往前推进，避免覆盖
    writeback_stage();
    memory_stage();
    execute_stage();
    decode_stage();
    fetch_stage();

    // 检查冒险
    if (check_hazards()) {
        stall_requested_ = true;
    }

    // 更新 PC
    if (!stall_requested_) {
        update_pc();
    } else {
        stall_requested_ = false;  // 清除停顿标志
    }

    // 清除刷新标志
    flush_requested_ = false;
}
```

### 3.3 RAW 冒险检测

```cpp
bool ConfigurablePipeline::check_hazards() {
    // 检测 ID/EX 阶段的 rs1/rs2 是否与 EX/MEM 或 MEM/WB 的 rd 冲突
    if (!id_ex_.valid) return false;

    // EX/MEM 阶段的目标寄存器
    if (ex_mem_.valid && ex_mem_.should_wb && ex_mem_.rd != 0) {
        if (id_ex_.rs1 == ex_mem_.rd || id_ex_.rs2 == ex_mem_.rd) {
            return true;  // RAW 冒险，需要停顿
        }
    }

    // MEM/WB 阶段的目标寄存器
    if (mem_wb_.valid && mem_wb_.should_wb && mem_wb_.rd != 0) {
        if (id_ex_.rs1 == mem_wb_.rd || id_ex_.rs2 == mem_wb_.rd) {
            return true;  // RAW 冒险，需要停顿
        }
    }

    return false;
}
```

## 4. 指令编码解析

### 4.1 RV64I 指令格式

```cpp
// include/instr_decode.hh
#ifndef INSTR_DECODE_HH
#define INSTR_DECODE_HH

#include <cstdint>

struct DecodedInstr {
    uint32_t raw;           // 原始指令
    uint8_t opcode;         // 操作码 (bits 6-0)
    uint8_t rd;             // 目标寄存器 (bits 11-7)
    uint8_t rs1;            // 源寄存器 1 (bits 19-15)
    uint8_t rs2;            // 源寄存器 2 (bits 24-20)
    uint8_t funct3;         // 功能码 3 (bits 14-12)
    uint8_t funct7;         // 功能码 7 (bits 31-25)
    int64_t imm;            // 立即数

    // 指令类型
    enum Type {
        R_TYPE,  // 寄存器-寄存器
        I_TYPE,  // 立即数/加载
        S_TYPE,  // 存储
        B_TYPE,  // 分支
        U_TYPE,  // 高位
        J_TYPE   // 跳转
    } type;
};

/**
 * @brief 解析 32 位 RISC-V 指令
 */
DecodedInstr decode_instruction(uint32_t instr);

#endif // INSTR_DECODE_HH
```

## 5. 测试策略

### 5.1 单元测试

```cpp
// test/test_alu_plugin.cc
#include "../include/plugins/alu_plugin.hh"
#include "../include/pipeline_stage.hh"
#include <cassert>
#include <iostream>

class MockPipelineStage : public PipelineStageIF {
    // ... 实现接口用于测试
};

void test_add_instruction() {
    AluPlugin plugin;
    MockPipelineStage stage;

    // 设置输入：x10 = 10, x11 = 20, ADD x12, x10, x11
    stage.set_rs1_value(10);
    stage.set_rs2_value(20);
    stage.set_funct3(0b000);
    stage.set_funct7(0b0000000);

    plugin.on_execute(stage);

    // 验证输出：x12 = 30
    assert(stage.alu_result() == 30);
    assert(stage.should_writeback() == true);
    assert(stage.writeback_value() == 30);
}

int main() {
    test_add_instruction();
    std::cout << "All ALU Plugin tests passed!" << std::endl;
    return 0;
}
```

### 5.2 集成测试

```cpp
// test/test_rv64i_integration.cc
#include "../include/rv64i_core.hh"
#include <cassert>
#include <vector>

void test_arithmetic_program() {
    // 测试程序：计算 10 + 20 = 30
    std::vector<uint32_t> program = {
        0x00A00513, // addi x10, x0, 10
        0x01400593, // addi x11, x0, 20
        0x00B50533, // add  x10, x10, x11
        0x0000006F, // jal  x0, 0 (halt)
    };

    RV64ICore core("test_core", nullptr);
    core.load_program(program);

    // 执行直到 halt
    while (core.get_pc() < program.size() * 4) {
        core.tick();
    }

    // 验证结果
    assert(core.get_reg_value(10) == 30);
}

int main() {
    test_arithmetic_program();
    std::cout << "All integration tests passed!" << std::endl;
    return 0;
}
```

## 6. 构建配置

### 6.1 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(RV64I_Plugin_CPU CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
set(SOURCES
    src/rv64i_core.cc
    src/pipeline.cc
    src/plugin_manager.cc
    src/instr_decode.cc
    src/plugins/alu_plugin.cc
    src/plugins/load_store_plugin.cc
    src/plugins/branch_plugin.cc
)

# 头文件
set(HEADERS
    include/rv64i_core.hh
    include/pipeline.hh
    include/plugin_manager.hh
    include/cpu_plugin.hh
    include/pipeline_stage.hh
    include/register_file.hh
    include/instr_decode.hh
    include/plugins/alu_plugin.hh
    include/plugins/load_store_plugin.hh
    include/plugins/branch_plugin.hh
)

# 库
add_library(rv64i_plugin ${SOURCES} ${HEADERS})
target_include_directories(rv64i_plugin PUBLIC include)

# 测试
enable_testing()
add_executable(test_alu_plugin test/test_alu_plugin.cc)
target_link_libraries(test_alu_plugin rv64i_plugin)

add_executable(test_integration test/test_rv64i_integration.cc)
target_link_libraries(test_integration rv64i_plugin)

add_test(NAME alu_plugin COMMAND test_alu_plugin)
add_test(NAME integration COMMAND test_integration)
```

## 7. 文件清单

```
RV64I-Plugin-CPU/
├── docs/
│   ├── PRD.md                    # 产品需求文档
│   └── proposal/VexRiscvArch.md           # 架构设计文档（本文件）
├── include/
│   ├── rv64i_core.hh             # RV64ICore 核心
│   ├── pipeline.hh               # 可配置流水线
│   ├── plugin_manager.hh         # Plugin 管理器
│   ├── cpu_plugin.hh             # Plugin 基类
│   ├── pipeline_stage.hh         # 流水线阶段接口
│   ├── register_file.hh          # 寄存器文件
│   ├── instr_decode.hh           # 指令解码
│   └── plugins/
│       ├── alu_plugin.hh         # ALU Plugin
│       ├── load_store_plugin.hh  # Load/Store Plugin
│       └── branch_plugin.hh      # Branch Plugin
├── src/
│   ├── rv64i_core.cc
│   ├── pipeline.cc
│   ├── plugin_manager.cc
│   ├── instr_decode.cc
│   └── plugins/
│       ├── alu_plugin.cc
│       ├── load_store_plugin.cc
│       └── branch_plugin.cc
├── test/
│   ├── test_alu_plugin.cc
│   ├── test_load_store_plugin.cc
│   ├── test_branch_plugin.cc
│   └── test_rv64i_integration.cc
├── samples/
│   └── README.md                 # 示例程序说明
└── CMakeLists.txt
```

## 8. 参考资料

- [RISC-V Unprivileged Spec](https://riscv.org/technical/specifications/)
- [VexRiscv Architecture](https://github.com/SpinalHDL/VexRiscv)
- [The VexRiscV CPU - A New Way To Design](https://tomverbeure.github.io/rtl/2018/12/06/The-VexRiscV-CPU-A-New-Way-To-Design.html)

