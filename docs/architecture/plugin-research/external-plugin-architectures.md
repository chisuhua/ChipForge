# 外部 Plugin 架构研究 — ChipForge Phase0 设计参考

> 研究目标：从 4 个外部参考项目（VexRiscv、SpinalHDL、Bluespec/BlueAXI、Gem5）中提取
> Plugin 风格的设计模式，作为 ChipForge Phase0（Plugin 最小框架）的设计参考。
>
> 研究日期：2026-06-08
> 工作目录：`/workspace/project/ChipForge`
>
> **代码引用约定**：所有代码片段均以 `https://github.com/<owner>/<repo>/blob/<sha>/<path>#L<start>-L<end>` 形式给出 permalink，
> SHA 取自 `gh api repos/.../contents/...` 检索时的主分支 HEAD 提交。

---

## 1. VexRiscv 设计要点

VexRiscv 是一个用 SpinalHDL 编写的可配置 RISC-V CPU，**整个 CPU 就是一个由 40+ 个 Plugin 组合而成的产物**。
Plugin 体系是 VexRiscv 最大的架构特征。

### 1.1 `Plugin` trait — 极简接口（仅 25 行）

**证据**（[VexRiscv Plugin.scala](https://github.com/SpinalHDL/VexRiscv/blob/96d2bc68070f958cda29595d28ae2fcfc8954f68/src/main/scala/vexriscv/plugin/Plugin.scala#L9-L25)）：

```scala
trait Plugin[T <: Pipeline] extends Nameable {
  var pipeline : T = null.asInstanceOf[T]
  setName(this.getClass.getSimpleName.replace("$",""))

  // Used to setup things with other plugins
  def setup(pipeline: T) : Unit = {}

  //Used to flush out the required hardware (called after setup)
  def build(pipeline: T) : Unit

  implicit class implicitsStage(stage: Stage){
    def plug[T <: Area](area : T) : T = {area.setCompositeName(stage,getName()).reflectNames();area}
  }
  implicit class implicitsPipeline(stage: Pipeline){
    def plug[T <: Area](area : T) = {area.setName(getName()).reflectNames();area}
  }
}
```

**关键观察**：
- **只有 2 个生命周期方法**：`setup`（默认空实现）+ `build`（强制实现）
- Plugin 持有 `var pipeline: T` 反向引用，构造时不注入，调用时由 `Pipeline.build()` 注入
- `plug[Area]` 隐式类：把子硬件"挂载"到 Stage / Pipeline 的命名空间下

### 1.2 `Pipeline.build()` — 显式分阶段调度

**证据**（[VexRiscv Pipeline.scala](https://github.com/SpinalHDL/VexRiscv/blob/e9d93c24ad3adf71a6f4f48f37472f3143eb7c3a/src/main/scala/vexriscv/Pipeline.scala#L45-L161)）：

```scala
def build(): Unit = {
  // PHASE 1: inject pipeline ref
  plugins.foreach(_.pipeline = this.asInstanceOf[T])

  // PHASE 2: setup (cross-plugin references)
  plugins.foreach(_.setup(this.asInstanceOf[T]))

  // PHASE 3: name reflection
  plugins.foreach{ p =>
    p.parentScope = Component.current.dslBody
    p.reflectNames()
  }

  // PHASE 4: build (hardware generation)
  plugins.foreach(_.build(this.asInstanceOf[T]))

  // PHASE 5: INTERCONNECT — compute stage index per Stageable, auto-wire
  val inputOutputKeys = mutable.LinkedHashMap[Stageable[Data], KeyInfo]()
  // ... (auto-insert `RegNext` between stages per key)

  // PHASE 6: arbitration
  // (compute isStuck / isFlushed / isFiring per stage)

  Component.current.addPrePopTask(() => build())
}
```

**关键观察**：
- 调度是**显式、确定性的**（`plugins.foreach` 按注册顺序）
- 整个 `build()` 自身通过 `addPrePopTask` 延迟到 `Component.prePop` 阶段执行 — 即所有 `describe()` 体跑完之后
- 5/6 阶段都是"框架自动完成"，Plugin 作者只关心第 4 阶段（自己的 `build`）

### 1.3 `Stageable[T]` — 类型安全的 Stage payload key

**证据**（[VexRiscv Stage.scala](https://github.com/SpinalHDL/VexRiscv/blob/5275622fda7a889565e833b96d205192a40b1b0a/src/main/scala/vexriscv/Stage.scala#L10-L44)）：

```scala
class Stageable[T <: Data](_dataType : => T) extends HardType[T](_dataType) with Nameable {
  def dataType = apply()
  setWeakName(this.getClass.getSimpleName.replace("$",""))
}

class Stage() extends Area {
  def input[T <: Data](key : Stageable[T]) : T = { ... }   // 上一阶段输出
  def output[T <: Data](key : Stageable[T]) : T = { ... } // 本阶段输出
  def insert[T <: Data](key : Stageable[T]) : T = ...      // 注入点
  // ... arbitration state
}
```

**真实使用**（[VexRiscv.scala](https://github.com/SpinalHDL/VexRiscv/blob/96d2bc68070f958cda29595d28ae2fcfc8954f68/src/main/scala/vexriscv/VexRiscv.scala#L64-L80)）：

```scala
object INSTRUCTION extends Stageable(Bits(32 bits))
object PC          extends Stageable(UInt(32 bits))
object RS1         extends Stageable(Bits(32 bits))
object REGFILE_WRITE_DATA extends Stageable(Bits(32 bits))

// Stage 间共享（非 Stageable，作为 PipelineThing）
object MPP extends PipelineThing[UInt]
```

**关键观察**：
- `Stageable` 是"键（key）"，不是"信号（signal）"
- 同一个 key 可在多个 stage 上 `input(key)` / `output(key)`；框架在 `Pipeline.build()` 阶段**自动决定如何连线**（同一 stage 内用 `:=`，跨 stage 用 `RegNext`）
- `PipelineThing` 是 Stageable 的"穷人版"——只支持值共享，不参与自动流水线连线

### 1.4 Plugin 间通信：服务定位 + Stageable

**证据**（[Pipeline.scala](https://github.com/SpinalHDL/VexRiscv/blob/e9d93c24ad3adf71a6f4f48f37472f3143eb7c3a/src/main/scala/vexriscv/Pipeline.scala#L24-L43)）：

```scala
def service[T](clazz : Class[T]) = {
  val filtered = plugins.filter(o => clazz.isAssignableFrom(o.getClass))
  assert(filtered.length == 1, s"??? ${clazz.getName}")
  filtered.head.asInstanceOf[T]
}

def update[T](that : PipelineThing[T], value : T) : Unit = things(that) = value
def apply[T](that : PipelineThing[T]) : T = things(that).asInstanceOf[T]
```

**真实使用**（[BranchPlugin.scala](https://github.com/SpinalHDL/VexRiscv/blob/680756065e9e6fc50d8c3d6c58191a16e867d822/src/main/scala/vexriscv/plugin/BranchPlugin.scala#L92-L120)）：

```scala
def hasHazardOnBranch = if(earlyBranch)
  pipeline.service(classOf[HazardService]).hazardOnExecuteRS
else False

override def setup(pipeline: VexRiscv): Unit = {
  val decoderService = pipeline.service(classOf[DecoderService])
  decoderService.addDefault(BRANCH_CTRL, BranchCtrlEnum.INC)
  decoderService.add(Riscv.BEQ, List(
    BRANCH_CTRL       -> BranchCtrlEnum.B,
    SRC1_CTRL         -> Src1CtrlEnum.RS,
    SRC2_CTRL         -> Src2CtrlEnum.RS,
    HAS_SIDE_EFFECT   -> True
  ))
}
```

**关键观察**：
- **服务定位**：`pipeline.service[T](classOf[X])` — 强类型、按 class 唯一性 assert
- **接口契约**：`PredictionInterface`（trait）— 主动暴露给其他 plugin
- Plugin 通常既**消费**服务（在 `setup` 中调用 `service(...)`），又**实现**服务（`extends XService`）

### 1.5 与 ChipForge 的相关性

| VexRiscv 概念                | ChipForge 现状                  | 借鉴点                                      |
| :--------------------------- | :------------------------------ | :------------------------------------------ |
| `Plugin[T]` trait            | 无                              | **直接照搬**：`Plugin<P>` 最小接口（见 §6）  |
| `setup()` + `build()`        | `Component::describe()` 单方法  | **拆分**：增加 `setup()` 钩子               |
| `pipeline.service[T]()`      | 无（`ModuleFactory` 仅注册类型） | **必须新增**：`PluginContext` 持有注册表   |
| `Stageable[T]`               | 无                              | **可借鉴**：用类型化 Key 而非 raw signal    |
| `Pipeline.build()` 显式 5 阶段 | `Component::build()` 单步        | **必须新增**：`PluginRegistry::build_all()`  |
| `addPrePopTask` 延迟到 Component.pop 之后 | `Component::build_internal()` 同步 | 借鉴**延迟绑定**模式                     |

---

## 2. SpinalHDL Plugin 模型

SpinalHDL 的 plugin/fiber 系统比 VexRiscv 更通用 —— VexRiscv 的 Plugin trait 是 SpinalHDL `fiber.Fiber` 机制的应用层封装。

### 2.1 `fiber.Fiber` — 通用阶段化任务系统

**证据**（[SpinalHDL Fiber.scala](https://github.com/SpinalHDL/SpinalHDL/blob/cfe8f1b183286cbaaa89a52e275f50686f75b4be/core/src/main/scala/spinal/core/fiber/Fiber.scala#L26-L50)）：

```scala
object Fiber {
  def apply[T: ClassTag](orderId : Int)(body : => T) : Handle[T] = {
    GlobalData.get.elab.addTask(orderId)(body)
  }
  def setup[T: ClassTag](body : => T) : Handle[T]   = apply(ElabOrderId.SETUP)(body)
  def build[T: ClassTag](body : => T) : Handle[T]   = apply(ElabOrderId.BUILD)(body)
  def patch[T: ClassTag](body: => T): Handle[T]     = apply(ElabOrderId.PATCH)(body)
  def check[T: ClassTag](body: => T): Handle[T]     = apply(ElabOrderId.CHECK)(body)
  // ...
}
```

**ElabOrderId**（同文件 [L10-L24](https://github.com/SpinalHDL/SpinalHDL/blob/cfe8f1b183286cbaaa89a52e275f50686f75b4be/core/src/main/scala/spinal/core/fiber/Fiber.scala#L10-L24)）：

```scala
object ElabOrderId {
  val INIT  = -1000000
  val SETUP = 0
  val BUILD = 1000000
  val PATCH = 1500000
  val CHECK = 2000000
}
```

**关键观察**：
- 5 个**有序阶段**（INIT < SETUP < BUILD < PATCH < CHECK），由 `Fiber.await(id)` 同步
- 用户用 `Fiber build { ... }` 注册任务；多个任务在同一阶段并发执行，**阶段间**是同步屏障
- `Fiber.runSync()` 实际是"cooperative async runtime" — 看 `AsyncThread`/`Engine`（同目录其他文件）就是单线程协程

### 2.2 `Handle[T]` — 强类型 Future

**证据**（[SpinalHDL Handle.scala](https://github.com/SpinalHDL/SpinalHDL/blob/2c82ee2c3c083b588996d365117ce6c6c81ee141/core/src/main/scala/spinal/core/fiber/Handle.scala#L43-L92)）：

```scala
class Handle[T] extends Nameable {
  @dontName private var loaded = false
  @dontName private var value : T = null.asInstanceOf[T]

  def get : T = {
    if(loaded) return value
    val t = AsyncThread.current
    t.waitOn = this
    if(wakeups == null) wakeups = ArrayBuffer[() => Unit]()
    wakeups += {() => e.wakeup(t)}
    e.sleep(t)              // 协程挂起
    value
  }

  def load(value : T) = {
    applyName(value)
    loaded = true
    this.value = value
    if(wakeups != null) wakeups.foreach(_.apply())  // 唤醒所有等待者
    this
  }
}
```

**关键观察**：
- `Handle[T]` 是"未求值的 T"
- `get` 阻塞当前协程直到 `load(value)` 被调用 → 协程间**类型安全**的值传递
- `Handle[T]` 隐式转换 → `T`（`implicit def keyImplicit[T](key: Handle[T]): T = key.get`）— 用起来像普通值

### 2.3 `Component.addPrePopTask` — 延迟到 describe 全部结束

**证据**（[SpinalHDL Component.scala](https://github.com/SpinalHDL/SpinalHDL/blob/73436b3a82594aa3d33e35d5bfa2e8ee5c2205a1/core/src/main/scala/spinal/core/Component.scala#L122-L210)）：

```scala
case class PrePopTask(task : () => Unit) {
  val context = ScopeProperty.capture()
}
private[core] var prePopTasks = mutable.ArrayBuffer[PrePopTask]()

def prePop(): Unit = {
  while(prePopTasks.nonEmpty){
    val prePopTasksToDo = prePopTasks
    prePopTasks = mutable.ArrayBuffer[PrePopTask]()

    val ctx = ScopeProperty.captureNoClone()
    for(t <- prePopTasksToDo){
      t.context.restoreCloned()
      t.task()
    }
    ctx.restore()
  }
}

def addPrePopTask(task: () => Unit) = prePopTasks += PrePopTask(task)
def afterElaboration(body : => Unit) = addPrePopTask(() => body)
```

**关键观察**：
- **`addPrePopTask` 钩子是 SpinalHDL 的"类 plugin 回调"**
- 在 Component 描述（DSL）阶段注册回调，所有 describe 体跑完后统一执行
- `prePop()` 内部循环支持**任务注册新任务**（不会丢失）
- `PostInitCallback.postInitCallback()` 在 `prePop()` 之后调用 → 标准 4 阶段生命周期：
  1. 构造（构造时 DSL body 填充信号）
  2. `prePop`（所有 addPrePopTask 跑完）
  3. `postInitCallback`（final init）
  4. 真正进入代码生成（`Phase` 系统）

### 2.4 `Phase` 编译期生成 — 真正的 plugin 系统

**证据**（[SpinalHDL Phase.scala](https://github.com/SpinalHDL/SpinalHDL/blob/822278d2051403452bc952a74dfa9633e9815a3b/core/src/main/scala/spinal/core/internals/Phase.scala#L199-L217)）：

```scala
trait Phase {
  def impl(pc: PhaseContext): Unit
  def hasNetlistImpact: Boolean
}
trait PhaseNetlist extends Phase { override def hasNetlistImpact: Boolean = true }
trait PhaseMisc   extends Phase { override def hasNetlistImpact: Boolean = false }
trait PhaseCheck  extends Phase { override def hasNetlistImpact: Boolean = false }
```

**真实实现示例**（同文件 [L266-L294](https://github.com/SpinalHDL/SpinalHDL/blob/822278d2051403452bc952a74dfa9633e9815a3b/core/src/main/scala/spinal/core/internals/Phase.scala#L266-L294)）：

```scala
class PhaseApplyIoDefault(pc : PhaseContext) extends PhaseNetlist {
  override def impl(pc: PhaseContext): Unit = {
    pc.walkComponents { c =>
      c.getOrdredNodeIo.foreach { bt =>
        if (bt.dlcIsEmpty && bt.isInput) {
          SpinalError(s"$bt of ${c.getPath()} has no driver")
        }
      }
    }
  }
}
```

**关键观察**：
- **`Phase` 是 SpinalHDL 的真正 plugin 系统** —— 编译时遍历 AST
- `PhaseContext` 持有 `topLevel: Component`、`enums`、全局命名空间 → 访问全图
- 框架里有 **30+ 个** `Phase` 子类（`PhasePullClockDomains`、`PhaseMemBlackboxing`、`PhaseNameNodesByReflection` ...）
- 用户可以**自定义 Phase** 插入编译流程

### 2.5 编译期生成 RTL 的原理（Q7 关键答案）

SpinalHDL 的"编译期生成 Verilog"分两层：

| 层         | 机制                                                  | 触发时机                            |
| :--------- | :---------------------------------------------------- | :---------------------------------- |
| **表达层** | `Component` DSL body 在 Scala 构造时执行             | `new MyComponent()` 的瞬间          |
| **优化层** | `Phase.impl(pc)` 遍历 AST、推倒优化、改名、生成 RTL | `SpinalVerilog(component)` 调用时   |
| **输出层** | `ComponentEmitterVerilog` 渲染 Verilog 字符串         | 最后一个 Phase                      |

**关键公式**：`Scala 程序 = 硬件描述程序 + 编译期优化/生成阶段`。
ChipForge（C++17）的等价方案见 §5 Q7。

### 2.6 与 ChipForge 的相关性

| SpinalHDL 概念           | ChipForge 现状                                | 借鉴点                                            |
| :----------------------- | :-------------------------------------------- | :------------------------------------------------ |
| `Fiber` 协程 + 阶段 ID   | 无                                            | **可选借鉴**：Phase0 不需要，按阶段排序的 vector 即可 |
| `Handle[T]`              | 无                                            | **可选**：Phase0 暂用 `std::any` 即可             |
| `addPrePopTask`          | `Component::build_internal()` 同步             | **必须新增**：`add_post_build_task(closure)`      |
| `Phase` trait + 30+ 内置 | 无（手工 codegen）                           | **可借鉴**：`PipelinePhase` 接口（见 §6）         |
| `Component::prePop` 循环 | `Component::build()` 单次                    | **可借鉴**：支持 task 嵌套注册                    |

---

## 3. Bluespec / BlueAXI 模式

Bluespec 用 Haskell 风格的语言描述"原子动作 + 规则 + 模块 + 接口"，编译器 (bsc) 把 rules 静态调度成 FSM。

### 3.1 `interface` — 强类型方法集合

**证据**（[BlueAXI/AXI4_Master.bsv](https://github.com/esa-tu-darmstadt/BlueAXI/blob/d8f992ea61076cb71a6d3eedbf75bf3869a039bb/src/AXI4_Master.bsv#L22-L53)）：

```bluespec
(* always_ready, always_enabled *)
interface AXI4_Master_Rd_Fab#(numeric type addrwidth, numeric type datawidth,
                              numeric type id_width, numeric type user_width);
  method Bool arvalid;
  (*prefix = ""*)method Action parready((*port="arready"*)Bool a);
  method Bit#(id_width) arid;
  method Bit#(addrwidth) araddr;
  method UInt#(8) arlen;
  // ...

  method Bool rready;
  (*prefix = ""*)method Action prchannel((*port="rvalid"*)Bool v,
                                         (*port="rid"*)Bit#(id_width) id, ...);
endinterface

interface AXI4_Master_Rd#(addrwidth, datawidth, id_width, user_width);
  interface AXI4_Master_Rd_Fab#(addrwidth, datawidth, id_width, user_width) fab;
  interface Put#(AXI4_Read_Rq#(addrwidth, id_width, user_width)) request;
  interface Get#(AXI4_Read_Rs#(datawidth, id_width, user_width)) response;
endinterface
```

**关键观察**：
- 接口是一组**类型化方法**（`method Bool x` / `method Action y(Bit#(32) v)`）
- 方法分为 `method`（值） vs `method Action`（副作用）
- 接口**可继承 / 组合**：`AXI4_Master_Rd` 内嵌了 `AXI4_Master_Rd_Fab`
- 注释 `(*prefix = "")` / `(*port = "...")` 控制 Verilog 输出命名

### 3.2 `module mkXxx` — 单一构造点

**证据**（同文件 [L55-L130](https://github.com/esa-tu-darmstadt/BlueAXI/blob/d8f992ea61076cb71a6d3eedbf75bf3869a039bb/src/AXI4_Master.bsv#L55-L130)）：

```bluespec
module mkAXI4_Master_Rd#(Integer bufferIn, Integer bufferOut, Bool bram)
                       (AXI4_Master_Rd#(addrwidth, datawidth, id_width, user_width));

    let isRst <- isResetAsserted();

    FIFOF#(AXI4_Read_Rq) in = ?;
    if(bufferIn == 0) in <- mkBypassFIFOF();
    else if(bufferIn == 1) in <- mkPipelineFIFOF();
    else if(bram) in <- mkSizedBRAMFIFOF(bufferIn);
    else in <- mkSizedFIFOF(bufferIn);
    // ...

    rule deqIn if(!isRst && arreadyIn && in.notEmpty());
        in.deq();
    endrule

    rule forwardIn;
        warid <= in.first().id();
        // ...
    endrule
    // ...

    interface Put request = toPut(in);
    interface Get response = toGet(out);
    interface AXI4_Read_Rs snoop = out.first();
    interface AXI4_Master_Rd_Fab fab;
        interface parready = arreadyIn._write;
        // ...
endmodule
```

**关键观察**：
- `mkAXI4_Master_Rd` 是一个**工厂函数** — 接收配置参数，返回一个 `AXI4_Master_Rd` 接口实例
- 模块体分为：状态声明 + `rule`（条件触发动作）+ `interface` 绑定
- **`rule` 之间的并行性 = 编译期自动分析**：编译器把 rule 编译成 FSM，决定 cycle-accurate 时序
- **没有显式"plugin"或"stage"概念** —— 组合靠"接口嵌套 + mkXxx 工厂"

### 3.3 与 ChipForge 的相关性

| Bluespec 概念            | ChipForge 现状                       | 借鉴点                                                |
| :----------------------- | :----------------------------------- | :---------------------------------------------------- |
| `interface` (方法集合)   | `__io(Bundle)` (raw signal 集合)     | **可借鉴**：`ch_interface` 类持有 method-like 函数对象 |
| `module mkXxx#(...)`     | `class ch_module<T>`（`module.h`）   | **已对齐**；增加参数化支持                            |
| `rule` 自动调度         | 显式 `EventQueue` tick              | **不可直接借鉴**（C++ 无静态分析）；用 `EventQueue` 替代 |
| `FIFOF` / `FIFO` 内置     | 手动 `ch_stream` + ready/valid        | **已对齐**（chipforge 抽象为 `ch_stream<Bundle>`）   |
| 编译期生成 Verilog       | 暂无（直接 C++ 仿真）                | **可借鉴**：把 `ch_module` 描述转 AST → Verilog 文本  |

**核心洞见**：Bluespec 强调"声明式 = 编译器做调度"，ChipForge 是"命令式 + 显式握手"。两者**不直接兼容**，但 `interface` 的"嵌套组合"思路值得借鉴到 C++17 中（通过 CRTP / mixin）。

---

## 4. Gem5 SimObject 模式

Gem5 是**最接近 ChipForge** 的参考 —— 因为它也是 C++ 仿真框架，**且** 用 Python 做配置驱动。

### 4.1 `SimObject` 生命周期（关键设计文档）

**证据**（[gem5 sim_object.hh L67-L120](https://github.com/gem5/gem5/blob/047821d371b7824acb89589041b8caab193d4e0f/src/sim/sim_object.hh#L67-L120)）：

> SimObject initialization is controlled by the instantiate method in src/python/m5/simulate.py.
> After instantiation and connecting ports, simulate.py initializes the object using the following call sequence:
>
> 1. **SimObject::init()**
> 2. **SimObject::regStats()**
> 3. **SimObject::initState()** (if starting fresh) **/ loadState()** (if restoring)
> 4. **SimObject::resetStats()**
> 5. **SimObject::startup()**
> 6. **Drainable::drainResume()** (if resuming)
>
> "pre-order depth-first traversal" — **parent before children**

**C++ 关键虚函数**（[sim_object.hh L185-L298](https://github.com/gem5/gem5/blob/047821d371b7824acb89589041b8caab193d4e0f/src/sim/sim_object.hh#L185-L298)）：

```cpp
class SimObject : public EventManager, public Serializable, public Drainable {
  public:
    virtual ~SimObject();
    virtual void init();
    virtual void loadState(CheckpointIn &cp);
    virtual void initState();
    virtual void regProbePoints();
    virtual void regProbeListeners();
    virtual Port &getPort(const std::string &if_name, PortID idx=InvalidPortID);
    virtual void startup();
    virtual void memWriteback() {};
    virtual void memInvalidate() {};
};
```

### 4.2 Python 元类驱动 — `MetaSimObject` 自动化

**证据**（[gem5 SimObject.py L132-L196](https://github.com/gem5/gem5/blob/cb0810ee3421a3e883071e33ad0e4b620b9bdf5c/src/python/m5/SimObject.py#L132-L196)）：

```python
class MetaSimObject(type):
    init_keywords = {
        "abstract": bool,
        "cxx_class": str,
        "cxx_type": str,
        "cxx_header": str,
        "type": str,
        "cxx_base": (str, type(None)),
        "cxx_extra_bases": list,
        "cxx_exports": list,
        "cxx_param_exports": list,
        "cxx_template_params": list,
        "override_create": bool,
    }

    def __new__(mcls, name, bases, dict):
        # 1. 过滤：保留以 _ 开头的私有属性
        # 2. 把 Param.X 描述符存入 _value_dict
        # 3. 写入 allClasses[name] 注册表
        # ...
```

**`SimObject` 根类**（同文件 [L628-L647](https://github.com/gem5/gem5/blob/cb0810ee3421a3e883071e33ad0e4b620b9bdf5c/src/python/m5/SimObject.py#L628-L647)）：

```python
class SimObject(metaclass=MetaSimObject):
    type = "SimObject"
    abstract = True
    cxx_header = "sim/sim_object.hh"
    cxx_class = "gem5::SimObject"
    cxx_extra_bases = ["Drainable", "Serializable", "statistics::Group"]
    eventq_index = Param.UInt32(Parent.eventq_index, "Event Queue Index")

    cxx_exports = [
        PyBindMethod("init"),
        PyBindMethod("initState"),
        PyBindMethod("memInvalidate"),
        PyBindMethod("memWriteback"),
        PyBindMethod("regProbePoints"),
        PyBindMethod("regProbeListeners"),
        PyBindMethod("startup"),
    ]
```

### 4.3 `Port` / `RequestPort` / `ResponsePort` — 类型化接口

**证据**（[gem5 port_params.py L315-L374](https://github.com/gem5/gem5/blob/2adb095f6f414d982f8761d14ea8e853f7148d19/src/python/m5/params/port_params.py#L315-L374)）：

```python
class Port:
    @classmethod
    def compat(cls, role, peer):
        cls._compat_dict.setdefault(role, set()).add(peer)
        cls._compat_dict.setdefault(peer, set()).add(role)

    @classmethod
    def is_compat(cls, one, two):
        for port in one, two:
            if not port.role in Port._compat_dict:
                fatal("Unrecognized role '%s' for port %s\n", port)
        return one.role in Port._compat_dict[two.role]

    def __init__(self, role, desc, is_source=False):
        self.role = role
        self.is_source = is_source

    def makeRef(self, simobj):
        return PortRef(simobj, self.name, self.role, self.is_source)

    def connect(self, simobj, ref):
        self.makeRef(simobj).connect(ref)

# 角色配对规则
Port.compat("GEM5 REQUESTOR", "GEM5 RESPONDER")

class RequestPort(Port):
    def __init__(self, desc):
        super().__init__("GEM5 REQUESTOR", desc, is_source=True)

class ResponsePort(Port):
    def __init__(self, desc):
        super().__init__("GEM5 RESPONDER", desc)
```

**`connectPorts()`**（[SimObject.py L1341-L1345](https://github.com/gem5/gem5/blob/cb0810ee3421a3e883071e33ad0e4b620b9bdf5c/src/python/m5/SimObject.py#L1341-L1345)）：

```python
def connectPorts(self):
    # Sort the ports based on their attribute name to ensure the
    # order is the same on all hosts
    for attr, portRef in sorted(self._port_refs.items()):
        portRef.ccConnect()
```

### 4.4 与 ChipForge `Component` 直接对比

| 维度              | Gem5 `SimObject`                             | ChipForge `Component`（[component.h](https://github.com/east-gallifrey-stone/ChipForge/blob/PLACEHOLDER/include/component.h)） | 状态     |
| :---------------- | :------------------------------------------- | :----------------------------------------------------------- | :------- |
| **基类继承**      | `EventManager`, `Serializable`, `Drainable`  | `std::enable_shared_from_this<Component>`                   | 简单但够用 |
| **IO 声明**       | `RequestPort("desc")` 描述符                  | `virtual void create_ports() {}` 虚函数                      | **需扩展** |
| **Port 类型化**   | `RequestPort` / `ResponsePort` + `compat()`   | `ch_stream<Bundle>` (无方向标签)                             | **可借鉴 compat** |
| **Port 连接**     | `connectPorts()` 按名配对                     | `port.connect(other.port)` 手动                              | **可借鉴** |
| **生命周期**      | `init() / initState() / startup() / ...` 6 步 | `create_ports() → describe() → build()` 3 步                | **需扩展** |
| **配置驱动**      | Python 元类 + `cxx_class`/`cxx_header` 反射  | 手工 `REGISTER_MODULE` 宏 + JSON 解析                        | **够用** |
| **服务定位**      | 无 (`service[T](clazz)` 缺失)                | 无                                                           | **需新增** |
| **Stageable**     | 无                                            | 无                                                           | **可选** |

**可直接复用**：
- **`init → initState → startup` 3 步生命周期**（删除 gem5 的 `loadState/regProbe*` 这些 gem5-specific 项）
- **`Port.compat(role, role)` 配对表**（在 ChipForge 可作为 `StreamAdapter` 的运行时检查）
- **`connectPorts()` 的"按名连接"语义**（替代 ChipForge 当前的 `port.connect(other.port)` 手动调用）

**需要重写**：
- **服务定位器**（VexRiscv 风格 `pipeline.service[T](classOf[X])`）— gem5 没有
- **延迟构建回调**（SpinalHDL 风格 `addPrePopTask`）— gem5 缺
- **类型化 payload key**（VexRiscv 风格 `Stageable[T]`）— gem5 缺

---

## 5. 回答 7 个设计问题

### Q1: Plugin 接口最小集

**答**：照搬 VexRiscv 的极简风格（**仅 2 方法 + 1 反向引用**）：

```cpp
// 最小集
class PluginBase {
public:
  Pipeline* pipeline = nullptr;          // [VexRiscv] 反向引用
  virtual void setup(Pipeline* p) {}     // [VexRiscv] 跨插件协商（可选）
  virtual void build(Pipeline* p) = 0;   // [VexRiscv] 硬件生成（必须）
  virtual ~PluginBase() = default;
};
```

**理由**：
- VexRiscv 用这 2 个方法构建了 40+ 复杂 plugin，**证伪了"需要更多钩子"的假设**
- `setup` 解决"插件 A 想知道插件 B 提供了什么服务" —— 在 `build` 之前先建立连接
- `build` 是唯一强制方法 —— 编译期保证每个 plugin 都参与硬件生成
- 唯一补充：增加 `name()` 接口（用于诊断/调试） —— VexRiscv 用 `setName(this.getClass.getSimpleName)` 自动实现，C++ 中可用 `typeid(*this).name()`

**不引入**：
- ❌ `init()` / `startup()` 多阶段（gem5 风格） —— Phase0 不需要 checkpoint / drain
- ❌ `preBuild()` / `postBuild()` / `onCommit()`（VexRiscv 的 implicit 隐式类）—— 复杂度过高
- ❌ 模板插件 (`Plugin<T>`) —— 改为运行时类型擦除（service locator 间接实现类型安全）

---

### Q2: 阶段调度模型

**答**：**采用 (a) 显式顺序**，不用 (b) 数据流依赖分析，也不用 (c) 编译期生成。

| 模式            | 来自                | 优点                       | 缺点                                | ChipForge 选择  |
| :-------------- | :------------------ | :------------------------- | :---------------------------------- | :-------------- |
| (a) 显式顺序    | VexRiscv `Pipeline.build()` | 简单、可预测、零运行时开销 | 插件多时顺序难维护                  | ✅ **采用**       |
| (b) 数据流分析  | 编译器 (Halide)     | 自动最优                   | 静态分析复杂，需 plugin 声明 IO     | ❌ 复杂度太高     |
| (c) 编译期生成  | SpinalHDL `Phase`   | 可做深度优化               | 需要 AST，C++17 没有 Scala 那种宏系统 | ❌ Phase0 不必要  |

**实现建议**（照搬 VexRiscv [Pipeline.scala L45-L55](https://github.com/SpinalHDL/VexRiscv/blob/e9d93c24ad3adf71a6f4f48f37472f3143eb7c3a/src/main/scala/vexriscv/Pipeline.scala#L45-L55)）：

```cpp
// ChPipeline::build_all() — 显式 4 阶段
void ChPipeline::build_all() {
  // 阶段 1: 注入反向引用
  for (auto* p : plugins_) p->pipeline = this;

  // 阶段 2: setup（service locator 注册 + 跨插件协商）
  for (auto* p : plugins_) p->setup(this);

  // 阶段 3: build（硬件生成）
  for (auto* p : plugins_) p->build(this);

  // 阶段 4: interconnect（自动连线，类似 VexRiscv 的 Stageable 互连）
  interconnect_stages_();
}
```

**为什么不用 (b) 数据流依赖？**：
- C++17 没有 Scala 的 type class / 隐式转换
- 真正的数据流分析需要 LLVM-level IR，远超 Phase0 范围
- VexRiscv 的 Stageable 已经**显式表达了依赖**（在哪个 stage 上 `input` / `output`），不需要再做分析

**为什么不用 (c) 编译期生成？**：
- C++ 没有 Scala 的 `Fiber` 协程 + `Phase` 系统
- ChipForge 的目标是"动态组合 + 仿真"，不是"高级综合"
- 若需要 codegen（Q7），单独走 `codegen_verilog.h` 即可

---

### Q3: 类型安全 Payload

**答**：**采用 (c) 类型安全 payload key**（VexRiscv `Stageable[T]` 风格），**不采用** VexRiscv `PipelineThing` 的"运行时 typeid"风格，也**不采用** raw `ch_stream<Bundle>`（那是流接口，不是 payload key）。

**问题分析**：

VexRiscv 有两种 payload：
1. **`Stageable[T]`** —— 类型 + 弱命名，框架在 `Pipeline.build()` 阶段**自动连线**
2. **`PipelineThing[T]`** —— 仅类型 + 强命名，**仅做值共享，不参与自动连线**

两者在 C++17 中的对应：

| VexRiscv   | C++17 等价                                          | ChipForge 选择         |
| :--------- | :-------------------------------------------------- | :--------------------- |
| `Stageable[T]` | `template<typename T> struct Stageable { ... };`   | ✅ **必须采用**         |
| `PipelineThing[T]` | `std::any` + `std::type_index`                     | ❌ 过于动态，难调试     |
| raw `ch_stream<Bundle>` | 同名同概念                        | ✅ **流接口，独立存在**  |

**实现建议**（直接对照 VexRiscv [Stage.scala L10](https://github.com/SpinalHDL/VexRiscv/blob/5275622fda7a889565e833b96d205192a40b1b0a/src/main/scala/vexriscv/Stage.scala#L10-L13)）：

```cpp
// ChStageable — 类型安全的 pipeline payload key
template <typename T>
class ChStageable {
public:
    explicit ChStageable(std::string name) : name_(std::move(name)) {}
    const std::string& name() const { return name_; }
private:
    std::string name_;
};

// 静态注册（VexRiscv 风格：每个 stageable 是个 singleton object）
// 用法（参照 VexRiscv.scala L67）：
//   inline ChStageable<ChBits<32>> INSTRUCTION{"INSTRUCTION"};
```

**关键差异**：
- VexRiscv 的 `Stageable` 继承 `HardType[T]`，是 SpinalHDL 的"信号工厂"
- ChipForge 的 `ChStageable` 不需要"信号工厂"角色 —— 它**只是 key**，信号在 `ChStage` 的 `inputs_/outputs_/inserts_` 三张 map 中存
- ChipForge 的 `ChStageable<T>` 应当**支持嵌套**（如 `ChStageable<ChBundle<MyBundle>>`），方便 group 多个相关信号

---

### Q4: Plugin 间通信

**答**：**混合方案** —— 主体采用 (a) + (c) 的组合：
- **(a) 共享成员 / 服务定位** 为主（VexRiscv `pipeline.service[T]`）
- **(c) 类型安全 payload key** 为辅（VexRiscv `Stageable[T]`）
- **不采用** (b) 显式 stage output 协议（VexRiscv 已经把它"语言化"为 Stageable）

| 通信机制      | 来自                | 用途                       | ChipForge 角色       |
| :------------ | :------------------ | :------------------------- | :------------------- |
| (a) 服务定位  | VexRiscv `pipeline.service[T]` | 单例/单实现的服务（如 `DecoderService`） | **核心机制**         |
| (b) 显式 stage output | VexRiscv `Stageable` 的副作用 | pipeline 数据流 | **隐含在 (c) 中**     |
| (c) 类型安全 key | VexRiscv `Stageable[T]` | 多实例、强类型 payload | **辅助机制**         |

**实现建议** —— `ChPluginContext`：

```cpp
// ChPluginContext — 服务注册表（VexRiscv pipeline.service() 的 C++17 实现）
class ChPluginContext {
public:
    // (a) 服务定位：注册时要求唯一
    template <typename T>
    void provide(T* service) {
        const auto key = std::type_index(typeid(T));
        CHREQUIRE(services_.count(key) == 0, "duplicate service");
        services_[key] = service;
    }

    template <typename T>
    T* get() {
        const auto key = std::type_index(typeid(T));
        CHREQUIRE(services_.count(key) == 1, "service not found / ambiguous");
        return static_cast<T*>(services_.at(key));
    }

    template <typename T>
    bool has() const { return services_.count(std::type_index(typeid(T))) > 0; }

    // (c) payload 值共享（VexRiscv PipelineThing 风格，但用模板）
    template <typename T>
    void put(const ChStageable<T>& key, T value) {
        things_[&key] = std::move(value);   // 指针作 key（key 是 singleton）
    }

    template <typename T>
    T& get(const ChStageable<T>& key) {
        return std::any_cast<T&>(things_.at(&key));
    }

private:
    std::map<std::type_index, void*> services_;
    std::map<const void*, std::any> things_;
};
```

**为什么不只用 (a)？**：
- (a) 适合"服务"（单例、唯一），但不适合"阶段间流动数据"（每 stage 一份）
- (c) 适合"类型化键 → 值"，pipeline 数据流天然就是这模式

**为什么不只用 (c)？**：
- (c) 缺少"服务提供方 vs 消费方"的语义
- 比如 `DecoderService` 是一次性服务，整个 pipeline 共享；用 (c) 表达就丢失"唯一性"约束

---

### Q5: 生命周期

**答**：**采用 SpinalHDL 的 4 阶段模型**，**简化**为 3 阶段（去掉 `initState` —— ChipForge 无 checkpoint）：

```
[构造]            [pre-build]        [build]          [post-build]
new Component() → Component::setup  → Component::describe → Component::finalize
                     (create_ports)     (硬件生成)         (interconnect)
```

**Plugin 在每一阶段做什么**：

| 阶段          | Plugin 钩子       | 典型动作                                          | 对应 VexRiscv      |
| :------------ | :---------------- | :------------------------------------------------ | :----------------- |
| **构造**      | `Plugin()` 构造函数 | 接收参数、初始化成员                              | 同                  |
| **pre-build** | `setup(pipeline)` | 调用 `pipeline->context().get<X>()` 拿依赖；注册服务 | `setup()` 完全相同  |
| **build**     | `build(pipeline)` | 调用 `pipeline->new_stage()`、插 Stageable、生成信号 | `build()` 完全相同 |
| **post-build**| （框架自动）       | 互连 stages、仲裁                                 | `Pipeline.build()` 阶段 5/6 |

**错误处理策略**：
- **构造期错误**（参数不合法）→ 抛 `std::invalid_argument`（参考 VexRiscv `assert(earlyBranch || withMemoryStage, ...)`）
- **pre-build 错误**（服务未注册 / 重复）→ 抛 `std::runtime_error`，带 `plugin name`
- **build 错误**（Stageable 未注册）→ 抛 `ChStageable_missing` 异常（参考 VexRiscv `throw new Exception("Missing inserts : " + ...)`）
- **post-build 错误**（拓扑矛盾）→ 抛 `std::logic_error`

**关键设计**：
- **构造期绝不读 `pipeline`**（VexRiscv 同样，pipeline 在 setup 时才注入）
- **`setup` 阶段**是唯一允许"读其他 plugin"的阶段；`build` 阶段只允许"写自己的硬件"
- 强制这两阶段分离，避免"VexRiscv 早期"那种"A 写在 B 之前"导致死锁的问题

---

### Q6: Plugin 组合

**答**：**3 种组合都支持**，但通过**不同机制**实现：

| 组合模式      | VexRiscv 怎么做                                | ChipForge 怎么做                           |
| :------------ | :--------------------------------------------- | :----------------------------------------- |
| **(a) 顺序**（pipeline） | `newStage()` / `stage.input(key)` / `stage.output(key)` + 自动 interconnect | **完全照搬**：`ChPipeline::new_stage()` + `ChStageable` |
| **(b) 并行**（fork-join） | 多个 plugin `build()` 中都引用同一个 Stageable，框架用 KeyInfo 算交集 | **完全照搬**：在 `interconnect` 阶段收集所有 key 的引用 |
| **(c) 选择**（mux）       | 用 `Mux(cond, a, b)` 在 plugin 内手动选         | **照搬**：`ChMux2` 由 Plugin 自行实例化（这是普通 C++ 组件，不是 plugin 组合模式） |

**顺序组合（最常用）的例子**：

```cpp
// 用户组装：5 级流水线 CPU
ChPipeline pipe;
auto& fetch   = pipe.add_plugin<FetchPlugin>();
auto& decode  = pipe.add_plugin<DecodePlugin>();
auto& execute = pipe.add_plugin<ExecutePlugin>();
auto& mem     = pipe.add_plugin<MemoryPlugin>();
auto& wb      = pipe.add_plugin<WritebackPlugin>();

// 框架自动（照搬 VexRiscv Pipeline.scala L45-L161）：
// 1. setup: 收集所有 plugin 需要的 Stageable
// 2. build: 每 plugin 创建自己的 stage 和信号
// 3. interconnect: 对每个 Stageable，自动插 RegNext 连接相邻 stage
```

**并行组合（用于旁路/旁路 cache）**：

```cpp
// 例：cache plugin 与 bypass plugin 并行（都消费 PC，都输出 fetched data）
auto& cache = pipe.add_plugin<CachePlugin>();      // 创建 stage.insert(CACHE_HIT)
auto& bypass = pipe.add_plugin<BypassPlugin>();    // 创建 stage.output(FETCHED_DATA)
fetch_stage.input(PC)    // PC 是 Stageable
execute_stage.input(FETCHED_DATA)  // 框架自动选择 winner（按 plugin 注册顺序？）
```

**注意**：VexRiscv 的并行组合实际上**没有"自动选择"** —— 它的 Stageable 只是"自动布线"，选择是用 SpinalHDL 的 `Mux` 在 plugin 内手动做的。ChipForge 同样应如此：**plugin 组合只解决"线怎么连"，选择逻辑由 plugin 自己写**。

---

### Q7: 编译期 RTL 生成

**答**：**采用 SpinalHDL 的两层模型**（Q2-Section 2.5）：

| 层         | SpinalHDL 机制                                  | ChipForge C++17 等价                                       |
| :--------- | :---------------------------------------------- | :--------------------------------------------------------- |
| **表达层** | `Component` DSL body 在 Scala 构造时执行       | `Component::describe()` 在 C++ 构造时执行                |
| **优化层** | `Phase.impl(pc)` 遍历 AST、推倒优化            | **`ChPipeline::finalize()` 单遍**，不做优化（不做综合）  |
| **输出层** | `ComponentEmitterVerilog` 渲染 Verilog         | **`codegen_verilog.h`** 渲染（已存在，Phase5 才用）      |

**为什么 ChipForge 不需要 SpinalHDL 那么深的 AST**：
- C++ 本身就是"运行期"语言，没有 Scala 的 type class / implicit conversion
- ChipForge 的目标是**仿真**（直接 C++17 执行），不是综合
- 只有在 **Phase 5**（codegen）时才需要 AST，Phase0-4 都不需要

**Phase0 编译期生成 RTL 的最小方案**：

```cpp
// ChPipeline::emit_verilog() — 最小 Verilog 输出（参考 SpinalHDL ComponentEmitter）
std::string ChPipeline::emit_verilog() const {
    std::ostringstream out;
    out << "module " << name_ << "(\n";
    for (const auto& [name, port] : ports_) {
        out << "  " << (port.is_input() ? "input" : "output")
            << "  " << port.width() << "  " << name << ",\n";
    }
    out << ");\n";
    // ... (按 DAG 顺序输出 assigns)
    out << "endmodule\n";
    return out.str();
}
```

**核心洞见**：**AST 不需要单独维护** —— `Component::describe()` 在执行时已经把信号都加进 `children_/signals_` map 了，直接遍历即可。SpinalHDL 维护独立 AST 是因为它**需要在多个 Phase 之间保留信息**；ChipForge 的 Phase 模型是单遍的，所以**不需要 AST**。

---

## 6. Phase0 "Plugin最小框架" 推荐设计

基于以上研究，给出 **5 个具体设计建议**，每个标注来源框架与优先级（P0/P1/P2）。

### 建议 1: 定义 `PluginBase` trait（接口最小集）— **P0**

**来源**：VexRiscv（[Plugin.scala L9-L25](https://github.com/SpinalHDL/VexRiscv/blob/96d2bc68070f958cda29595d28ae2fcfc8954f68/src/main/scala/vexriscv/plugin/Plugin.scala#L9-L25)）

```cpp
// include/chipforge/plugin_base.h
namespace ch {
class ChPipeline;  // forward

class ChPluginBase {
public:
    ChPipeline* pipeline = nullptr;
    virtual ~ChPluginBase() = default;

    /** 与其他 plugin 建立服务连接（在所有 build 之前调用 1 次）*/
    virtual void setup(ChPipeline* p) {}

    /** 生成硬件（必须实现）*/
    virtual void build(ChPipeline* p) = 0;

    /** 诊断用名称（默认 typeid）*/
    virtual const char* name() const {
        return typeid(*this).name();
    }
};
}  // namespace ch
```

**对照 VexRiscv**：1:1 对应（`setup` + `build` + `var pipeline`）。
**理由**：证伪了"需要更多钩子"的假设。40+ 复杂 plugin 只需要 2 个方法。

---

### 建议 2: 实现 `ChPipeline` 容器（阶段调度）— **P0**

**来源**：VexRiscv（[Pipeline.scala L45-L161](https://github.com/SpinalHDL/VexRiscv/blob/e9d93c24ad3adf71a6f4f48f37472f3143eb7c3a/src/main/scala/vexriscv/Pipeline.scala#L45-L161)）+ SpinalHDL（[Component.scala L122-L210](https://github.com/SpinalHDL/SpinalHDL/blob/73436b3a82594aa3d33e35d5bfa2e8ee5c2205a1/core/src/main/scala/spinal/core/Component.scala#L122-L210)）

```cpp
// include/chipforge/pipeline.h
namespace ch {
class ChPipeline {
public:
    template <typename PluginT, typename... Args>
    PluginT& add_plugin(Args&&... args) {
        auto p = std::make_unique<PluginT>(std::forward<Args>(args)...);
        auto& ref = *p;
        plugins_.push_back(std::move(p));
        return ref;
    }

    void build() {
        // 阶段 1: 注入 pipeline 引用
        for (auto& p : plugins_) p->pipeline = this;
        // 阶段 2: setup (服务注册)
        for (auto& p : plugins_) p->setup(this);
        // 阶段 3: build (硬件生成)
        for (auto& p : plugins_) p->build(this);
        // 阶段 4: interconnect (Stageable 自动连线, 可选 P1)
        interconnect_stages_();
    }

    ChPluginContext& context() { return context_; }

private:
    void interconnect_stages_();  // 留空，Phase1 实现

    std::vector<std::unique_ptr<ChPluginBase>> plugins_;
    ChPluginContext context_;
};
}  // namespace ch
```

**对照 VexRiscv**：阶段顺序、`addPrePopTask` 模式用 `ChPipeline::build()` 同步调用替代（不延迟）。
**对照 SpinalHDL**：`addPrePopTask` 模式 → 在 `Component::build_internal` 中**当前已同步**，借鉴意义不大。

---

### 建议 3: 引入 `ChPluginContext`（服务定位器）— **P0**

**来源**：VexRiscv（[Pipeline.scala L24-L40](https://github.com/SpinalHDL/VexRiscv/blob/e9d93c24ad3adf71a6f4f48f37472f3143eb7c3a/src/main/scala/vexriscv/Pipeline.scala#L24-L40)）+ VexRiscv 真实使用（[BranchPlugin.scala L92](https://github.com/SpinalHDL/VexRiscv/blob/680756065e9e6fc50d8c3d6c58191a16e867d822/src/main/scala/vexriscv/plugin/BranchPlugin.scala#L92)）

```cpp
// include/chipforge/plugin_context.h
namespace ch {
class ChPluginContext {
public:
    // (a) 服务定位：注册时要求唯一
    template <typename ServiceT>
    void provide(ServiceT* service) {
        const auto key = std::type_index(typeid(ServiceT));
        CHREQUIRE(services_.count(key) == 0, "duplicate service");
        services_[key] = service;
    }

    template <typename ServiceT>
    ServiceT* get() {
        const auto key = std::type_index(typeid(ServiceT));
        CHREQUIRE(services_.count(key) == 1,
                  "service not found: " + std::string(typeid(ServiceT).name()));
        return static_cast<ServiceT*>(services_.at(key));
    }

    template <typename ServiceT>
    bool has() const {
        return services_.count(std::type_index(typeid(ServiceT))) > 0;
    }

    // (c) 类型安全 payload 值共享
    template <typename T>
    void put(const ChStageable<T>* key, T value) {
        things_[key] = std::move(value);
    }

    template <typename T>
    const T& get(const ChStageable<T>* key) const {
        return std::any_cast<const T&>(things_.at(key));
    }

private:
    std::map<std::type_index, void*> services_;
    std::map<const void*, std::any>  things_;
};
}  // namespace ch
```

**对照 VexRiscv**：
- `service[T](classOf[X])` → `context.get<X>()`（C++ 没有 `classOf`，用 `typeid`）
- `PipelineThing` → `ChStageable<T>*`（用指针作 key，因为是 singleton）

**对照 Gem5**：Gem5 缺这个机制，**必须新增**。

---

### 建议 4: 引入 `ChStageable<T>` 类型安全 payload key — **P1**

**来源**：VexRiscv（[Stage.scala L10-L13](https://github.com/SpinalHDL/VexRiscv/blob/5275622fda7a889565e833b96d205192a40b1b0a/src/main/scala/vexriscv/Stage.scala#L10-L13)）

```cpp
// include/chipforge/stageable.h
namespace ch {
template <typename T>
class ChStageable {
public:
    explicit ChStageable(std::string name) : name_(std::move(name)) {}
    const std::string& name() const { return name_; }
private:
    std::string name_;
};

// 用法（参照 VexRiscv.scala L64-L80）：
//   inline ChStageable<ch_uint<32>> INSTRUCTION{"INSTRUCTION"};
//   inline ChStageable<ch_uint<32>> PC{"PC"};
}  // namespace ch
```

**P1 而非 P0 的理由**：
- Stageable 的**真正威力**在 `Pipeline::interconnect_stages()` 阶段（自动插 RegNext）
- Phase0 只用到"类型化值共享"（通过 `ChPluginContext`），可暂时不用 Stageable
- Phase1 再引入完整 Stageable + 自动 interconnect 机制

---

### 建议 5: 引入 `ChStage` + `ChPipeline::new_stage()`（pipeline 抽象）— **P1**

**来源**：VexRiscv（[Pipeline.scala L12-L43](https://github.com/SpinalHDL/VexRiscv/blob/e9d93c24ad3adf71a6f4f48f37472f3143eb7c3a/src/main/scala/vexriscv/Pipeline.scala#L12-L43) + [Stage.scala](https://github.com/SpinalHDL/VexRiscv/blob/5275622fda7a889565e833b96d205192a40b1b0a/src/main/scala/vexriscv/Stage.scala)）

```cpp
// include/chipforge/stage.h
namespace ch {
class ChStage {
public:
    template <typename T>
    T& input(const ChStageable<T>& key);

    template <typename T>
    T& output(const ChStageable<T>& key);

    template <typename T>
    T& insert(const ChStageable<T>& key);

private:
    std::map<std::type_index, std::any> inputs_;
    std::map<std::type_index, std::any> outputs_;
    std::map<std::type_index, std::any> inserts_;
    // ... arbitration state (Phase1+)
};
}  // namespace ch
```

**P1 而非 P0 的理由**：
- Phase0 重点是**验证 plugin 框架本身**（setup/build/服务定位）
- Stage 抽象对 plugin 作者来说**增加心智负担**（要理解 `input/output/insert` 三种语义）
- Phase1 在 cache / pipeline CPU 真正使用时再引入

---

## 7. 总结：Phase0 vs Phase1+ 边界

| 特性              | Phase0（最小）        | Phase1+                | 来源参考                |
| :---------------- | :-------------------- | :--------------------- | :---------------------- |
| `PluginBase` 接口 | ✅ 必选                | ✅                      | VexRiscv 25 行          |
| `ChPipeline` 调度 | ✅ 必选（同步 4 阶段） | ✅                      | VexRiscv `Pipeline.build()` |
| `ChPluginContext` 服务定位 | ✅ 必选                | ✅                      | VexRiscv `service[T]`   |
| `ChStageable<T>`  | ⏸ 可选                | ✅ 必选                 | VexRiscv `Stageable`    |
| `ChStage` 抽象    | ⏸ 可选                | ✅ 必选                 | VexRiscv `Stage`        |
| 自动 Stageable 互连 | ⏸ 可选                | ✅ 必选（核心价值）     | VexRiscv `Pipeline.build()` 阶段 5 |
| `addPrePopTask` 延迟 | ⏸ 可选                | ✅ 必选（codegen 时）   | SpinalHDL `Component`   |
| `emit_verilog()`  | ❌ 不必                | ✅ Phase5 必选          | SpinalHDL `ComponentEmitter` |
| Gem5 风格的 6 步生命周期 | ❌ 不必                | ⏸ 可选（Phase5+）      | Gem5 `SimObject`        |
| Bluespec 风格 `interface` 嵌套 | ⏸ 可选（用 CRTP） | ⏸ 可选                  | BlueAXI `interface`     |

**Phase0 最小实现文件清单**（建议 5 个新文件）：

```
include/chipforge/
├── plugin_base.h       # 建议 1：ChPluginBase（< 30 行）
├── pipeline.h          # 建议 2：ChPipeline（< 100 行）
├── plugin_context.h    # 建议 3：ChPluginContext（< 80 行）
├── stageable.h         # 建议 4：ChStageable（< 40 行，可选）
└── stage.h             # 建议 5：ChStage（< 100 行，可选）
```

**Phase0 测试用例**（参考 VexRiscv [VexRiscv.scala L131-L158](https://github.com/SpinalHDL/VexRiscv/blob/96d2bc68070f958cda29595d28ae2fcfc8954f68/src/main/scala/vexriscv/VexRiscv.scala#L131-L158)）：

```cpp
// tests/test_pipeline_minimal.cpp
struct CounterPlugin : ChPluginBase {
    ChStageable<int> count{ "count" };
    int value = 0;
    void build(ChPipeline* p) override { value = 42; }
};

struct ConsumerPlugin : ChPluginBase {
    ChPluginContext::ServiceT<CounterPlugin>* counter = nullptr;
    void setup(ChPipeline* p) override {
        counter = p->context().get<CounterPlugin>();  // 服务定位
        CHREQUIRE(counter->value == 42, "build order wrong");
    }
    void build(ChPipeline* p) override {}
};

int main() {
    ChPipeline pipe;
    pipe.add_plugin<CounterPlugin>();
    pipe.add_plugin<ConsumerPlugin>();
    pipe.build();
    // 验证：服务定位正确，build/setup 顺序正确
}
```

---

## 8. 引用清单（Permalinks）

| 文件                                              | SHA         | 说明                          |
| :------------------------------------------------ | :---------- | :---------------------------- |
| [VexRiscv/Plugin.scala](https://github.com/SpinalHDL/VexRiscv/blob/96d2bc68070f958cda29595d28ae2fcfc8954f68/src/main/scala/vexriscv/plugin/Plugin.scala#L9-L25) | `96d2bc68` | Plugin trait (25 行) |
| [VexRiscv/Pipeline.scala](https://github.com/SpinalHDL/VexRiscv/blob/e9d93c24ad3adf71a6f4f48f37472f3143eb7c3a/src/main/scala/vexriscv/Pipeline.scala#L45-L161) | `e9d93c24` | Pipeline.build() 6 阶段 |
| [VexRiscv/Stage.scala](https://github.com/SpinalHDL/VexRiscv/blob/5275622fda7a889565e833b96d205192a40b1b0a/src/main/scala/vexriscv/Stage.scala#L10-L44) | `5275622f` | Stageable + Stage |
| [VexRiscv.scala](https://github.com/SpinalHDL/VexRiscv/blob/96d2bc68070f958cda29595d28ae2fcfc8954f68/src/main/scala/vexriscv/VexRiscv.scala#L64-L80) | `96d2bc68` | 默认 Stageable 集合 |
| [VexRiscv/BranchPlugin.scala](https://github.com/SpinalHDL/VexRiscv/blob/680756065e9e6fc50d8c3d6c58191a16e867d822/src/main/scala/vexriscv/plugin/BranchPlugin.scala#L54-L120) | `68075606` | 真实 plugin 示例（service + setup） |
| [SpinalHDL/Fiber.scala](https://github.com/SpinalHDL/SpinalHDL/blob/cfe8f1b183286cbaaa89a52e275f50686f75b4be/core/src/main/scala/spinal/core/fiber/Fiber.scala#L26-L50) | `cfe8f1b1` | Fiber + ElabOrderId |
| [SpinalHDL/Handle.scala](https://github.com/SpinalHDL/SpinalHDL/blob/2c82ee2c3c083b588996d365117ce6c6c81ee141/core/src/main/scala/spinal/core/fiber/Handle.scala#L43-L92) | `2c82ee2c` | Handle[T] (类型安全 Future) |
| [SpinalHDL/Component.scala](https://github.com/SpinalHDL/SpinalHDL/blob/73436b3a82594aa3d33e35d5bfa2e8ee5c2205a1/core/src/main/scala/spinal/core/Component.scala#L122-L210) | `73436b3a` | addPrePopTask + prePop 循环 |
| [SpinalHDL/Phase.scala](https://github.com/SpinalHDL/SpinalHDL/blob/822278d2051403452bc952a74dfa9633e9815a3b/core/src/main/scala/spinal/core/internals/Phase.scala#L199-L294) | `822278d2` | Phase trait + 3 个实现 |
| [BlueAXI/AXI4_Master.bsv](https://github.com/esa-tu-darmstadt/BlueAXI/blob/d8f992ea61076cb71a6d3eedbf75bf3869a039bb/src/AXI4_Master.bsv#L22-L130) | `d8f992ea` | interface + module mkXxx |
| [BlueAXI/GenericAxi4Master.bsv](https://github.com/esa-tu-darmstadt/BlueAXI/blob/47572c9f37d44c9ff0cb70b12cf79d6ab2f05ca7/src/GenericAxi4Master.bsv#L243-L433) | `47572c9f` | 嵌套 interface + 多个 rule |
| [BlueAXI/AXI4_Types.bsv](https://github.com/esa-tu-darmstadt/BlueAXI/blob/d8f992ea61076cb71a6d3eedbf75bf3869a039bb/src/AXI4_Types.bsv#L1-L80) | `d8f992ea` | 类型 + DefaultValue instance |
| [gem5/sim_object.hh](https://github.com/gem5/gem5/blob/047821d371b7824acb89589041b8caab193d4e0f/src/sim/sim_object.hh#L67-L298) | `047821d3` | SimObject 6 阶段生命周期 |
| [gem5/SimObject.py](https://github.com/gem5/gem5/blob/cb0810ee3421a3e883071e33ad0e4b620b9bdf5c/src/python/m5/SimObject.py#L132-L757) | `cb0810ee` | MetaSimObject 元类 + SimObject 根类 |
| [gem5/SimObject.py connectPorts](https://github.com/gem5/gem5/blob/cb0810ee3421a3e883071e33ad0e4b620b9bdf5c/src/python/m5/SimObject.py#L1341-L1345) | `cb0810ee` | connectPorts 按名配对 |
| [gem5/port_params.py](https://github.com/gem5/gem5/blob/2adb095f6f414d982f8761d14ea8e853f7148d19/src/python/m5/params/port_params.py#L315-L374) | `2adb095f` | Port / RequestPort / ResponsePort |

---

## 9. ChipForge 现状评估

调研时实际查看的 ChipForge 文件（用于"借鉴 vs 保留"决策）：

- **[/workspace/project/CppHDL/include/component.h](file:///workspace/project/CppHDL/include/component.h)**（100 行）
  - 已有：`create_ports()` + `describe()` 虚函数 + `create_child<T>()` 模板
  - 缺：`setup` 钩子、服务定位器、Stageable 抽象
- **[/workspace/project/CppHDL/include/module.h](file:///workspace/project/CppHDL/include/module.h)**（97 行）
  - 已有：`ch_module<T>` 工厂模式，借鉴了 Bluespec `mkXxx`
  - 缺：参数化（模板参数）、多实例组合
- **[/workspace/project/CppHDL/include/ch.hpp](file:///workspace/project/CppHDL/include/ch.hpp)**（33 行）
  - 头文件聚合器，已包含 core / ast / device / module
  - 需要新增 `chipforge/plugin_base.h` 等

**结论**：ChipForge 已有 `Component` 基底（功能上 ≈ Gem5 SimObject），但**完全没有 VexRiscv 风格的 plugin 框架**。Phase0 正是要补齐这一块。

---

> **下一步建议**：
> 1. 在 `include/chipforge/` 新建 `plugin_base.h` / `pipeline.h` / `plugin_context.h`（3 个 P0 文件）
> 2. 在 `tests/test_plugin_minimal.cpp` 加 CounterPlugin + ConsumerPlugin 测试
> 3. 在 `examples/minimal_plugin/` 加一个 RISC-V FetchPlugin 简化版，验证端到端
> 4. Phase1 再加 `stageable.h` / `stage.h`（P1 文件）和 `interconnect_stages()` 自动连线
