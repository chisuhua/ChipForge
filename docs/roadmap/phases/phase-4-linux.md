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

