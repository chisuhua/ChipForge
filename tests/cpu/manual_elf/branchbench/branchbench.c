/* tests/cpu/manual_elf/branchbench/branchbench.c
 *
 * 分支微基准 (chmem-branch-microbench) —— 4 档**可控预测率**负载.
 *
 * 设计要点 (对应 plan 的 AC-1/AC-2/AC-3):
 *   ① **指令数完全相同**: 分支用 inline asm 的显式 `beq`, 两条路径各 1 条 `mv`
 *      => 无论分支走向, 每迭代执行指令数恒等 => cycle 差**只**来自分支预测
 *      (flush 代价), 不来自工作量差异.
 *   ② **可预测性由模式决定** (编译期 -DBENCH_MODE=N), 其余计算逐位相同:
 *        0 always_taken     恒 taken
 *        1 always_not_taken 恒 not-taken
 *        2 pattern_6of8     6/8 taken (周期性 => BTB/GShare 可学习)
 *        3 pseudo_random    LCG 伪随机 (理想预测器也难命中)
 *   ③ **结果校验放在宿主侧** (关键设计):
 *      程序恒写 `sum | 1` 到 tohost —— LSB 置 1 让 runner 的 done 位
 *      (`tohost_raw & 0x1`) 恒成立 => cycle 计数是**真实完成时刻**;
 *      同时 tohost 携带校验和 (低 31 位) => 宿主脚本拿它与
 *      **基线 CSV 里的期望值**比对 => 结果正确性由宿主判定.
 *      => **golden 不进二进制** (若进 .text, 4 档机器码就不同 => 工作量
 *      差异污染 cycle 对比 —— 实测踩过) => .text 逐字节一致, 唯一差异是
 *      数据里的 mode 字.
 *
 * 无 libc 依赖: 自供栈初始化 (crt0) + 无 printf (结果经 tohost 回传).
 */
#include <stdint.h>

extern char tohost;              /* link_branchbench.ld 锚定 @ 0x80001000 */

#ifndef BENCH_MODE
#define BENCH_MODE 0
#endif
#ifndef BENCH_ITERATIONS
#define BENCH_ITERATIONS 2000
#endif

/* tohost PASS marker: 写 1 后死循环 (与 dhrystone 同一约定).
 * 必须 extern "C": crt0 的 `tail tohost_pass` 是汇编引用, 需未修饰符号名
 * (与 dhrystone.c 的 extern "C" 块同构). */
extern "C" void tohost_pass(void) {
  volatile int *p = (volatile int *)&tohost;
  *p = 1;
  while (1);
}

/* 显式 beq 分支: cond==0 -> a (taken 路径); cond!=0 -> b.
 * 两条路径指令数相同 => 工作量恒定. */
static inline uint32_t branch_sel(uint32_t cond, uint32_t a, uint32_t b) {
  uint32_t r;
  __asm__ volatile(
      "bne %[c], zero, 1f\n\t"
      "mv  %[r], %[a]\n\t"
      "j   2f\n\t"
      "1:\n\t"
      "mv  %[r], %[b]\n\t"
      "2:\n\t"
      : [r] "=&r"(r)
      : [c] "r"(cond), [a] "r"(a), [b] "r"(b)
      : "memory");
  return r;
}

/* ★模式值放在**数据**里 (volatile 全局), 不放编译期常量:
 *   - 放编译期常量 => 编译器会把下面的 select 折叠掉 => 各档生成的**指令流不同**
 *     => cycle 差混入「工作量差异」, 基准失效 (实测踩过: mode2/3 明显更慢).
 *   - 放 volatile 数据 => 运行时读取, **4 档生成的机器码完全相同**,
 *     唯一差异是内存里的 mode 值 => cycle 差**只**来自分支方向 (flush 代价).
 *   - 4 个 ELF 因此只是「同一份代码 + 不同的一个数据字」. */
volatile const uint32_t bench_mode = (uint32_t)BENCH_MODE;

/* 分支方向: 四种候选全部算出, 再用**无分支**的 select 选一个.
 *   => 无论 mode 取何值, 执行的指令序列完全相同 (select 走 cmov 或
 *      loop-invariant 分支, 后者被完美预测 => 代价各档一致). */
static inline uint32_t branch_cond(uint32_t i, uint32_t w) {
  const uint32_t mode = bench_mode;
  const uint32_t c0 = 0u;                          /* always_taken */
  const uint32_t c1 = 1u;                          /* always_not_taken */
  const uint32_t c2 = ((i & 7u) >= 6u) ? 1u : 0u;  /* pattern_6of8 */
  const uint32_t c3 = w & 1u;                       /* pseudo_random */
  return (mode == 0u) ? c0 : ((mode == 1u) ? c1 : ((mode == 2u) ? c2 : c3));
}

extern "C" void _cstart(void) {
  uint32_t acc = 0x12345678u;
  uint32_t x = 0x9E3779B9u;
  uint32_t i;

  for (i = 0; i < (uint32_t)BENCH_ITERATIONS; ++i) {
    /* 固定工作量: LCG + 乘法 + 移位 + 异或 (逐位决定, 与模式无关) */
    x = x * 1664525u + 1013904223u;
    uint32_t w = x ^ (x >> 13);
    w = w * 2654435761u;                    /* mul (rv32im) */
    w ^= (w >> 16);
    /* 唯一的模式相关量: 分支方向 */
    uint32_t sel = branch_sel(branch_cond(i, w), w + 1u, w + 2u);
    acc += sel;
  }

  /* 校验和: 全 uint32 运算 (无 UB) => 逐位确定, 与优化级别/模式无关地稳定.
   * 写 (sum | 1): LSB=1 => runner 的 done 位恒成立 (cycle 计数 = 真实完成时刻);
   * 同时携带校验和 => 宿主脚本与基线 CSV 的期望值比对 => 结果正确性宿主判定. */
  uint32_t sum = acc ^ (i * 0x9E3779B9u) ^ 0x5BF03635u;
  volatile int *p = (volatile int *)&tohost;
  *p = (int)(sum | 1u);
  while (1);
}
