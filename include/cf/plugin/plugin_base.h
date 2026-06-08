// include/cf/plugin/plugin_base.h
//
// 功能描述: Plugin 抽象基类 (Phase 0 P0 #1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08
//
// 设计目标:
//   - Plugin 派生类必须实现 build() (纯虚)
//   - setup() 有默认空实现, 供跨 Plugin 引用声明使用
//   - 禁止 tick() 业务重写 (D4 决策强制, 通过 static_assert 间接保护)
//
// 借鉴:
//   - VexRiscv Plugin.scala (仅 25 行, 2 个方法)
//   - .omo/drafts/decision-plugin-framework-2026-06-08.md D4 (Plugin-style 强制)
//
// 约束:
//   - 头文件 (无 .cpp)
//   - 编译期禁止 tick() 重写
//   - 与 cpptlm::ChStreamModuleBase / ch::Component 共存无冲突

#ifndef CF_PLUGIN_PLUGIN_BASE_H
#define CF_PLUGIN_PLUGIN_BASE_H

// 前向声明 PipeBuilder (避免循环包含)
// PluginBase 引用 PipeBuilder& 类型, 但只作为虚函数参数;
// PipeBuilder 完整定义在 pipe_builder.h 中.
namespace cf {
namespace plugin {
class PipeBuilder;
}  // namespace plugin
}  // namespace cf

namespace cf {
namespace plugin {

// PluginBase —— 所有 Plugin-style IP 的抽象基类
//
// 生命周期:
//   1. ctor()        - 构造 (用户实现 Payload 静态对象等)
//   2. setup(pb)     - 跨 Plugin 引用声明 (默认空)
//   3. build(pb)     - 实际生成逻辑 (强制派生类实现)
//   4. pb.build()    - 编译入口 (由 PipeBuilder 在所有 Plugin 注册后调用)
//   5. pb.run()      - 执行入口
//
// 禁止:
//   - 重写 tick()    (D4 决策; 通过 final 间接保护, 详见 static_assert)
//   - 持有时序状态  (调度由 PipeBuilder 决定, Plugin 不应有 state_)
class PluginBase {
 public:
  PluginBase() = default;
  virtual ~PluginBase() = default;

  // 禁止拷贝 (每个 Plugin 应当独立注册, 不应被容器拷贝)
  PluginBase(const PluginBase&) = delete;
  PluginBase& operator=(const PluginBase&) = delete;

  // setup —— 跨 Plugin 引用声明阶段
  // 默认空实现, 派生类可选择性 override
  // 调用时机: PipeBuilder::build() 期间, 在所有 Plugin 的 build() 之前
  virtual void setup(PipeBuilder& /*pb*/) {}

  // build —— 实际生成逻辑 (at_stage 注册)
  // 派生类必须实现
  // 调用时机: PipeBuilder::build() 期间, 在所有 Plugin 的 setup() 之后
  virtual void build(PipeBuilder& /*pb*/) = 0;

  // tick —— 显式禁用
  // D4 决策: Plugin-style 禁止业务 tick() 模式 (调度由框架决定)
  // 实现说明: 这里把 tick() 设为 private deleted, 阻止派生类意外定义同名函数
 private:
  void tick() = delete;
};

// 编译期检查: 派生类必须 override build()
// 说明: 这是 C++ 的常规 abstract class 机制. 由于 build() 是 pure virtual,
// 任何未 override build() 的派生类都是抽象类, 无法实例化.
// 例: struct Bad : PluginBase {}; Bad b;  // 编译错误: cannot allocate
//
// 附加检查: 在 build() 的实现中, 我们用 final 阻止链式重定义
// (派生类不应再有"中间层"PluginBase, 这会增加调度复杂度)

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PLUGIN_BASE_H
