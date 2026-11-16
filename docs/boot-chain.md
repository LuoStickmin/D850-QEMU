# D850（E5）启动链：从 `b1330_131.bin` 的字节到 QEMU 里的一次运行

基址 `0x10000000`。文件偏移 = 地址 − `0x10000000`。
镜像 `b1330_131.bin`：25,671,414 字节（`0x187B6F6`）。

## 1. 向量表（文件偏移 0）

```text
0x10000000  e59ff018  ldr pc,[pc,#0x18]
0x10000004  e59ff018
0x10000008  e59ff018
0x1000000c  e59ff018
0x10000010  e59ff018
0x10000014  e320f000  nop
0x10000018  ea00000d  b +0x3c
0x1000001c  e59ff018
0x10000020  1037cba8  <- reset
0x10000024  4002dfc0  <- undefined
0x10000028  4002e070  <- SVC
0x1000002c  4002e0e8  <- prefetch abort
0x10000030  4002e0f8  <- data abort
0x1000003c  4002e12c  <- FIQ
```

两个直接结论：

1. `reset` 在**镜像内**（`0x1037CBA8`），但其余异常向量在 `0x4002xxxx`——
   这是一个**独立的 boot ROM/handler bank**，不在镜像文件里；
2. `0x4002DFC0/0x4002E070/...` 在全镜像中只出现一次（就在向量表里），
   镜像内没有把代码拷到 `0x40000000` 的循环。拷贝者只能是 boot ROM。

## 2. reset 字 `0x1037CBA8` 是服务 sled

```text
0x1037cba8  020000ea  b     0x1037cbb8
0x1037cbac  00d2...   ldrb  r0,[r0,#0xd2]      <- 分支旁边是数据/桩
0x1037cbb0  ...       b     0x1037cbb8
0x1037cbb4  ...       ldrb  r0,[r0,#0x111]
0x1037cbb8  0084c4e5  str   r0,[r4]
0x1037cbbc  000000ea  b     0x1037cbc4
0x1037cbc0  ff00a0e3  mov   r0,#0xff
0x1037cbc4  0100a0e1  mov   r0,r1
0x1037cbc8  7080bde8  pop   {r4,r5,r6,pc}
```

它不是函数序言，也没有设置栈：`str r0,[r4]` 需要 r4，`pop` 需要 boot ROM
预先压好的返回地址。QEMU 的辅助启动给的是全零栈，`pop` 得到 PC=0，
随后在低地址零页滑到 halt 窗口（见 [status.md](status.md) §2）。

**这不是“装错基址”**：如果把镜像装到别的地址，`0x1037CBA8` 和它的
`pop` 都不会变；缺的是 boot ROM 的调用上下文。

## 3. 镜像内 init 安装器 `0x1035CE30`

2026-11-09 的 CP15 扫描已定位到这段代码（`0x1035CE30..0x1035D040`）。
按执行顺序：

### 3.1 读 boot 参数并重定位两个指针表

```text
0x1035ce30  rsb   r1, r0, r1
0x1035ce34  ldr   r3, [pc,#0x270]     ; 0x400E8450
0x1035ce38  ldr   r3, [r3]            ; = 0（未建模时为 0）
0x1035ce3c  add   r1, r3, r1
0x1035ce44  mov   r4, #0x40000000
0x1035ce54  ldr   r4, [pc,#0x28c]     ; 0x10381C80
0x1035ce58  rsb   r0, r0, r4
0x1035ce5c  add   r0, r3, r0
0x1035ce74  ldrd  r6, r7, [r0]
0x1035ce78  orr   r6, r6, r1          ; r1 = 0x40000000（入参为 0 时）
0x1035ce80  strd  r6, r7, [r0]
```

同样的循环在 `0x1035CE90` 再做一次，目标 `0x10384FF8`。
也就是说：**镜像内有两张 64 位指针表，值需要被 OR 上 `0x40000000`，
改指向 handler bank。** 真机 boot ROM 装载镜像后应完成这一步；
QEMU 的辅助入口从 r0=r1=0 开始，只能按同一公式算。

`0x400E8450` 在 QEMU 里返回 0（未建模），init 把它当作 RAM 偏移使用；
真机上它应该由某个 boot/系统寄存器提供。具体语义未知。

### 3.2 调用三个 boot-ROM 服务

```text
0x1035cef8  ldr   r0, [0x400E8450]
0x1035cf00  ldr   r1, =0x40000000
0x1035cf04  ldr   r2, =0x4002F0D8
0x1035cf08  bl    0x1035D17C

0x1035cf0c  ldr   r0, [0x400E8450]
0x1035cf14  ldr   r1, =0x4007BE40
0x1035cf18  ldr   r2, =0x4007C2C8
0x1035cf1c  bl    0x1035D17C

0x1035cf20  ldr   r0, [0x400E8450]
0x1035cf28  ldr   r1, =0x10380000
0x1035cf2c  ldr   r2, =0x10389000
0x1035cf30  sub   r1, r2, r1          ; 0x9000
0x1035cf38  add   r0, r0, r1
0x1035cf3c  mov   r1, #0
0x1035cf40  ldr   r3, =0x40000000
0x1035cf44  ldr   r4, =0x40041CA8
0x1035cf48  sub   r4, r4, r3          ; 0x41CA8
0x1035cf4c  bl    0x1035D184
```

而 `0x1035D17C`、`0x1035D180`、`0x1035D184` 在镜像里都只有一条：

```text
0x1035d17c  1eff2fe1  bx lr
0x1035d180  1eff2fe1  bx lr
0x1035d184  1eff2fe1  bx lr
```

这是**服务槽**，不是实现。真机 boot ROM 必须在运行前把这三个槽改成
真实函数（或让它们指向 `0x4002xxxx`）。QEMU 里它们是空调用：
什么也没拷，`0x40028FF8` 最终是零。

### 3.3 写 boot 参数、开 MMU、设 VBAR

```text
0x1035cf50  ldr   r3, =0x400E8454
0x1035cf54  str   r0, [r3]            ; 0x400E8454 <- 0x00009000
...
0x1035cf84  str   r0, [r1]            ; 0x400E8458 <- 0x42400000
...
0x1035cf9c  bl    0x1035D424          ; r0 = 0x40000000, r1 = 0
0x1035cfa0  ldr   r0, =0x10020000
0x1035cfa4  bl    0x1035D5D8          ; VBAR = 0x10020000
```

`0x1035D5D8` 就是 `VBAR = r0 & ~0x1F`。注意 VBAR 指向**镜像内**
（`0x10020000`），但 reset 之外的异常向量字仍指向 `0x4002xxxx`；
两者并不矛盾：VBAR 是向量表基址，表项内容是 boot ROM 地址。

### 3.4 MMU 开关 `0x1035D424`

```text
0x1035d424  ldr   r2, [pc,#0x24c]     ; 0x00C50078
0x1035d428  mcr   p15,0,r2,c1,c0,0    ; SCTLR
...
0x1035d444  ldr   r2, [pc,#0x234]     ; 0xB1003104
0x1035d448  mcr   p15,0,r2,c2,c0,2    ; TTBCR  (EAE=1, T0SZ=4)
0x1035d44c  ldr   r2, [pc,#0x230]     ; 0xFFFFFC00
0x1035d450  and   r0, r0, r2          ; r0 = 0x40000000
0x1035d454  mov   r2, #0xff
0x1035d458  and   r1, r1, r2
0x1035d45c  mcrr  p15,#1,r0,r1,c2     ; TTBR1 = 0x40000000
0x1035d460  add   r0, r0, #0x1000
0x1035d464  mcrr  p15,#0,r0,r1,c2     ; TTBR0 = 0x40001000
...
0x1035d5cc  movw  r2, #0x1005
0x1035d5d0  mcr   p15,0,r2,c1,c0,0    ; SCTLR = 0x1005 (M=1)
0x1035d5d4  bx    lr
```

三个可检验的推论：

1. **LPAE**：`mcrr p15,...,c2` 写 64 位 TTBR；`TTBCR.EAE=1`。
   Cortex-A9 实测在该指令 undefined（[status.md](status.md) §6）。
2. **页表必须在 `0x40000000`/`0x40001000`**：表基址不是镜像内地址。
   真机 boot ROM 提供这两张表；QEMU 里没有，所以第一条
   `bx lr` 的取指就 translation fault。
3. **T0SZ=4**：TTBR0 只覆盖低 256 MiB；`0x1035D5D4`（>256 MiB）走 TTBR1。
   首次缺页时 QEMU 读的是 `0x40000000`（TTBR1），第二次读
   `0x40001000`（TTBR0，`VBAR=0` 的异常向量在低地址）——与观察一致。

### 3.5 外设写与交班

```text
0x1035cfa8  mov   r0, #0x100
0x1035cfac  mov   r1, #0x10
0x1035cfb0  bl    0x1035D5F8          ; 0x4007C2E0 <- 0x10
                                       ; 0x4007C2E4 <- 0x01
0x1035cfb4  bl    0x1035D6F0
0x1035cfb8  bl    0x1035D6C0
...
0x1035d018  ldr   r0, [0x400E8454]     ; 0x9000
0x1035d01c  ldr   r1, [0x400E8458]     ; 0x42400000
0x1035d028  mov   r2, #2
0x1035d02c  mov   r3, #0
0x1035d030  mov   r4, #1
0x1035d034  ldr   ip, [pc,#0x108]      ; 0x40028FF8
0x1035d038  mov   lr, pc
0x1035d03c  bx    ip
0x1035d040  wfi
0x1035d044  b     0x1035D040
```

`0x1035D6F0` 一带有 `0x2E0014EC` 的读改写：
`[0x2E0014EC] |= 0x80000000`。`0x1035D6C0` 是另一个轮询/读回。

**交班目标是 `0x40028FF8`**（`0x1035D144` 的字面量）。这个地址在
handler RAM 内、异常向量 `0x4002DFC0` 附近。真机上这里应是 boot ROM
的下一阶段代码；QEMU 里是零，因此 PC 从 `0x40028FF8` 滑到
`0x40040000`。

## 4. QEMU 观察对照表

| 观察 | 地址/值 | 含义 |
| --- | --- | --- |
| 首次 MMIO | `0x400E8450` rd ×7 | init 读 boot 参数槽 |
| 写参数 | `0x400E8454 ← 0x9000` | 大小/偏移，语义未知 |
| 写参数 | `0x400E8458 ← 0x42400000` | 常量，语义未知 |
| 服务槽 | `0x1035D17C/180/184 = bx lr` | 需要 boot ROM 补实现 |
| MMU | `TTBCR=0xB1003104`，TTBR1/0 | LPAE，表在 `0x40000000` 区 |
| 首次缺页 | `IFAR=0x1035D5D4`，表读 `0x40000000` size=8 | 没有 LPAE 表 |
| 递归缺页 | `IFAR=0x0C`，表读 `0x40001000` size=8 | VBAR=0，向量也要翻译 |
| 开 MMU 后 | `VBAR=0x10020000` | 向量表基址在镜像内 |
| 外设 | `0x4007C2E0/4`、`0x2E0014EC` | 未建模，已记录 |
| 交班 | `bx ip`，ip=`0x40028FF8` | boot ROM 下一阶段入口 |

## 5. 复现

```sh
FW=/path/to/D850_0131.dec.modules     # 含 b1330_131.bin
QEMU=/path/to/qemu-11.1.0/build/qemu-system-arm

python3 tools/run.py --qemu "$QEMU" --payload "$FW/b1330_131.bin" \
  --out runs/reset --timeout 3

python3 tools/run.py --qemu "$QEMU" --payload "$FW/b1330_131.bin" \
  --init --out runs/init --timeout 2

python3 tools/run.py --qemu "$QEMU" --payload "$FW/b1330_131.bin" \
  --init --e5-tables --out runs/init-lpae --timeout 2
```

`--e5-tables` 的页表内容见 [status.md](status.md) §4；它是 identity 映射，
只用于证明 init 的控制流，不代表真机页表。

## 6. 边界

- 入口、栈、页表都是主机侧辅助；不是自然上电。
- `0x400E8450/54/58`、`0x4007C2E0/4`、`0x2E0014EC` 的语义**未知**，
  本仓库只记录访问，不猜寄存器。
- “`0x40000000` 区是 boot ROM” 是强证据支持的结论（向量表、TTBR、服务槽、
  镜像内无拷贝循环），但仍需要整片 dump 或 RAM dump 最终确认。
- `0x40028FF8` 处的代码、三个服务函数的实现，均不在镜像内。
