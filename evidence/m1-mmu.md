# M1：真实 reset 过 0x1037CD3C（2026-11-28）

## 结论

真实 reset 已通过 `0x1037CD3C`，并进入 `0x40028FF8` stage-2。

```text
D850DBG pc=1037cd3c cpsr=000000d3
r0=40708000 r1=80709000 r3=4 r4=10380000
ttbr0_3=0000000010389000 ttbr1_3=0000000010380000
tcr3=00000000b1003100 sctlr3=0000000000001005
```

随后：

```text
0x1037D018 -> 0x40028FF8 -> 0x40029000 ...
```

1647 个 TB，stage-2 已命中；最后仍有 1 个 data abort（`DFAR=0xC`，`0x10020010`），但已不再是 `0x1037CD3C` 的 `r0=0/DFAR=0`。

## 根因

安全态（EL3）页表：

- `TTBR0_S = 0x10389000`
- `TTBR1_S = 0x10380000`
- `TTBCR = 0xB1003100`（LPAE）
- `TTBR0_S` 的 level-1 entry[1] = `0x80000701`
  - 虚拟 `0x40000000..0x7FFFFFFF` → 物理 `0x80000000`

因此虚拟 `0x400E8450` 实际访问的是**物理 `0x800E8450`**，不是 `0x400E8450`。

之前把 bootarg 放在物理 `0x400E8000`，所以：

- 固件写 `[0x400E8450]` 写到 `0x800E8450`；
- 我们的 bootarg IO 没看到；
- 后续读到 0 → `r0=0` → `DFAR=0`。

## 修复

在 `0x800E8000` 增加 bootarg 别名 IO 页（大小 0x458，priority 2，避开 peer `0x800E845C`），复用 bootarg 读写日志：

```c
memory_region_init_io(&s->bootarg_hi, ..., &d850_bootarg_ops, s,
                      "nikon-d850.bootarg-hi", 0x458);
memory_region_add_subregion_overlap(sysmem, 0x800E8000,
                                    &s->bootarg_hi, 2);
```

日志确认：

```text
0x1037CCD8 wr [0x400E8450] = 0x40708000
0x1037CD00 rd [0x400E8450] = 0x40708000
```

于是 `0x1037CD3C` 的 `r0=0x40708000`，不再 abort。

## 当前边界

- M1 已过；
- 真实 reset 进入 stage-2；
- 仍有 1 个 data abort（DFAR=0xC，`0x10020010`），不是 M1 阻塞；
- 剩余未建模：`0x51000020`、`0x51200010/14/18`、`0x512D0000`、`0x5E0014EC`；
- 仍无 framebuffer。

## 备注

- `target/arm` 的 D850DBG 打印是临时诊断，不在仓库；
- 机器侧的 bootarg 别名是实际修复；
- 下一阶段 M2：追 stage-2 之后的 T-Kernel 入口/内核装载。
