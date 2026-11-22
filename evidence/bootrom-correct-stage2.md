# 修正映射后的 stage-2：入口 `0x40028FF8` 是 ARM 代码

> 2026-11-22。Fenestra 的 live delta 把 b1330 的 VA 基址钉在 `0x10020000`，
> 之前 `bootrom_alt.bin` 用的文件偏移整体高了 `0x20000`。修正后，
> `0x40028FF8` 是干净的 ARM 入口，不是 Thumb 循环中间。

## 1. 映射修正

Fenestra HANDOFF 的 live read：

```text
live 0x40000000 / 0x40020000 / 0x40028000 / 0x4002C000
    -> file delta 0x3E871D70
live 0x40030000 / 0x40044800
    -> file delta 0x3E871D80
```

因此：

```text
0x40000000 <- file 0x178E290  (0x2F0D8 bytes)
0x4002F100 <- file 0x17BD380  (0x12BA8 bytes)
0x40028FF8 <- file 0x17B7288
```

`file 0x178E290 + 0x10020000 = VA 0x117AE290`，与段表里的源地址一致。
之前用的 `0x17AE290/0x17DD380` 是 base `0x10000000` 的算法，偏了 `0x20000`。

## 2. 修正后的入口字节

```text
0x40028FF8  00 60 a0 e3   mov r6, #0
0x40028FFC  b0 8f 10 ee   mrc p15,0,r8,c0,c0,5   ; MPIDR
0x40029000  03 80 08 e2   and r8,r8,#3
0x40029004  04 00 58 e1   cmp r8,r4
0x40029008  33 00 00 8a   bhi 0x400290DC
...
0x40029024  d1 f0 2f e3   msr cpsr_fsxc,#0xd1
0x4002903c  04 d0 a0 e1   mov sp,r4
0x4002907c  55 75 01 e3   movw r7,#0x1555
0x40029080  10 7f 03 ee   mcr p15,0,r7,c3,c0,0  ; DACR
```

这是一个**按 MPIDR 给各模式设栈、设 DACR 的启动/次级核设置函数**，不是 Thumb
循环。`bx ip` 的 `0x40028FF8`（偶数）与 ARM 状态一致，**不需要 Thumb fix**。

## 3. 实跑

命令：

```sh
python3 tools/run.py \
  --payload "$FW/b1330_131.bin" --init \
  --bootrom runs/bootrom_correct.bin --boot-tables \
  --out runs/correct-stage2-vfp --timeout 3 --trace in_asm
```

结果：

```text
status=timeout  exit=0
exceptions: undef=4167
MMIO: 19 events
```

关键 PC：

```text
0x1035D018 -> 0x40028FF8
0x40029000 -> ... -> 0x400290C8
0x4000094C        ; 写 0xC0111450/54/58/5C
0x400034B8        ; 之后进入 Thumb 形态的函数指针，但 CPU 仍在 ARM
```

已观测的新 MMIO：

- `wr 0xC0111880` / `wr 0xC0111884`（PC `0x400290B4`）
- `wr 0xC0111450/54/58/5C`（PC `0x4000094C`）

这是第一次看到 stage-2 自己的外设写，不再是 init 的 `0x400E8450` 那几组。

## 4. 仍未闭合的点

- `0x400034B8` 是 Thumb 代码（`ldr r0,[pc,#0]; bx lr`），但入口状态仍是 ARM；
  某个函数指针缺 bit0（或 boot ROM 会重定位/补 Thumb 位）。这导致 undefined，
  随后异常向量 `VBAR=0x10020000` 的 `+4` 又是数据，递归 undef。
- `0x400E8450` 的 live 值是 `0x40708000`，而 init 代码有
  `[0x400E8450] + 0x40000000`；本次夹具用 `0x00708000` 才落到
  `0x40708000` 的 LPAE 表。真实算术仍需按 `0x1037CCC0` 的表页初始化
  重新核对。
- 这是 `entry=0x1035CE30` 的 host-assisted init，不是自然上电。

## 5. 真实 reset 的额外发现

用 `payload-base=0x10020000`、`entry=0x1037CBA8` 跑真实 reset：

```text
status=timeout  exceptions: 0  MMIO: 0
PC: 0x1037CBA8 -> 0x1037CBB0 -> ... -> 0x1037CC0C -> 0x1037D040 (wfi)
```

`0x1037CBA8` 是 `ldr pc,[pc,#0x4f0]` → `0x1037CBB0`，不是之前 base
`0x10000000` 下的 sled。`0x1037CC0C` 读 MIDR：

```text
mrc p15,0,r0,c0,c0,0
and r1,r0,#0xf00000
cmp r1,#0
bne 0x1037D040      ; WFI
and r1,r0,#0xf
cmp r1,#4
bne 0x1037D040
```

Cortex-A15 的 MIDR 不满足这个 gate，所以真实 reset 停在 WFI，没走到
`0x40028FF8`。host-assisted init 绕过了这个 CPU-ID gate。

## 6. 结论

- **入口就是 `0x40028FF8`，ARM 代码，源文件偏移 `0x17B7288`**。
- 之前 alt 的 data abort 是 `0x20000` 偏移错误，不是入口本身错。
- 现在 stage-2 已执行并写 `0xC0111450/...`，下一个阻塞是 Thumb 函数指针
  `0x400034B8` 的状态位，以及真实 reset 的 MIDR gate。
