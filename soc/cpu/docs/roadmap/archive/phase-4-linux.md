> **⚠️ ARCHIVED 2026-09-26**：本文档作为**历史参考**保留（Linux 启动链 + DTS 模板任务清单溯源）。当前主控路线图是 [`./execution-roadmap.md`](./execution-roadmap.md)（v0.10.0 → v1.3.0）。原 Phase 4 内容覆盖映射：
>
> | 原 Phase 4 任务 | 对应 execution-roadmap.md 章节 |
> |-----------------|-------------------------------|
> | Sv39 MMU + S/U mode + PLIC S-mode + DRAM 128MB + VirtIO | v1.0.0 §3.2（S/U + medeleg）+ v1.2.0 §3.2（Linux-sim SOFT gate）+ v1.3.0 §3.2（Linux-on-FPGA HARD） |
> | OpenSBI FW_PAYLOAD + Linux Kernel 配置 + Buildroot rootfs | v1.2.0 资产 + v1.3.0 PoC 配套 |
> | DTS 设备树模板 | v1.2.0 资产（CLINT 0x2000000 / PLIC 0xc000000 / UART 0x10000000 / VirtIO 0x10001000 布局） |
>
> **不再作为执行依据。**

# Phase 4：Linux 启动支持

> **Status**: Not Started
> **Milestone**: M5 - Linux 启动
> **Depends on**: Phase 3

**目标**：TLM 平台完整引导 Linux，Shell 可交互

---

## 任务清单

### 1. 硬件模型完整化

- [ ] ISS 支持 Sv39 虚拟内存（`satp` CSR，页表遍历 PTW）
- [ ] ISS 支持 S-mode/U-mode 完整特权级
- [ ] PLIC 支持 S-mode 外部中断（Linux 驱动依赖）
- [ ] DRAM 模型 >= 128 MB，DMI 加速大块访问
- [ ] VirtIO Block：用于挂载 rootfs
- [ ] VirtIO Net（可选）：网络功能

### 2. Linux 启动链

```
复位向量 (0x2000_0000)
    |
    v
OpenSBI FW_PAYLOAD (M-mode)
    +-- 初始化 hart（物理核）
    +-- 设置 PMP 保护内核区域
    +-- mret -> S-mode
        |
        v
Linux Kernel (S-mode)
    +-- 解析 DTB（设备树）
    +-- 初始化 UART、PLIC、CLINT 驱动
    +-- 挂载 VirtIO Block rootfs
    +-- 启动 /sbin/init
```

- [ ] OpenSBI 编译与适配
- [ ] Linux Kernel 配置与编译
- [ ] 设备树编写
- [ ] rootfs 构建（Buildroot）

### 3. 设备树（DTS）

```dts
// soc/chipforge_virt.dts
/dts-v1/;
/ {
    #address-cells = <2>;
    #size-cells    = <2>;
    compatible     = "chipforge,virt";

    cpus { cpu@0 { compatible = "riscv"; riscv,isa = "rv64imafdcsu"; }; };

    memory@80000000 { reg = <0x0 0x80000000 0x0 0x20000000>; };  // 512MB

    clint@2000000 {
        compatible = "riscv,clint0";
        reg = <0x0 0x2000000 0x0 0x10000>;
    };
    plic@c000000 {
        compatible = "riscv,plic0";
        reg = <0x0 0xc000000 0x0 0x4000000>;
        riscv,ndev = <31>;
    };
    uart@10000000 {
        compatible = "ns16550a";
        reg = <0x0 0x10000000 0x0 0x100>;
        interrupts = <10>;
    };
    virtio_block@10001000 {
        compatible = "virtio,mmio";
        reg = <0x0 0x10001000 0x0 0x1000>;
        interrupts = <1>;
    };
};
```

