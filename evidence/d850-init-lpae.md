# D850 init + E5 LPAE 页表 fixture：0 异常，交接到 0x40028FF8

- 日期：2026-11-16
- 固件：`b1330_131.bin`（D850 C:1.31），25,671,414 字节
- SHA-256：`97E1DA12A91C9B08E7857C38A01090E770B7309407DE9A8E9BED2D5A81204003`
- 机器：`nikon-d850`（payload @ `0x10000000`，flash 32 MiB，CPU `cortex-a15`）
- 会话目录：`runs/d850-init-lpae/`（`command.json`、`qemu.log`、`session.json`）
- 结果：status=`timeout`，exit=0，timed_out=True，
  wall=2.318 s


## 命令

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" --init --e5-tables \
  --out runs/d850-init-lpae --timeout 2
```

`--e5-tables` 在 `0x40000000` 映射 256 KiB handler RAM，并写入两张 LPAE
level-1 表（各 4 项、1 GiB block、identity），分别位于 `0x40000000`
（TTBR1）和 `0x40001000`（TTBR0）。这是**夹具**，不是真机 boot ROM 的页表。

## 结果

- 异常：**0**
- 未建模 MMIO 事件：654

未建模 MMIO 汇总：

- rd 0x400e8450 x7（first pc 0x1035ce38）
- rd 0x400e8454 x2（first pc 0x1035cf68）
- rd 0x2e0014ec x2（first pc 0x1035d6f4）
- wr 0x400e8454 x1（first pc 0x1035cf54）
- wr 0x400e8458 x1（first pc 0x1035cf84）
- wr 0x4007c2e0 x1（first pc 0x1035d60c）
- wr 0x4007c2e4 x1（first pc 0x1035d61c）
- wr 0x2e0014ec x1（first pc 0x1035d6fc）
- rd 0x400e8458 x1（first pc 0x1035d024）
- rd 0x40040000 x1（first pc 0x40040000）
- …其余 40 组（多为零页滑行的取指读）

### 起始 PC 序列

```text
  0x1035ce30
  0x1035ce38
  0x1035ce3c
  0x1035ce74
  0x1035ce90
  0x1035cea0
  0x1035cea4
  0x1035cedc
  0x1035cef8
  0x1035cefc
  0x1035cf00
  0x1035d17c
```

### 交接点

```text
  0x1035d6c0
  0x1035d6c8
  0x1035d6cc
  0x1035cfbc
  0x1035cfcc
  0x1035d018
  0x1035d01c
  0x1035d020
  0x1035d024
  0x1035d028
  0x40028ff8
  0x40029000
  0x400291e8
  0x40029400
  0x400295e8
```

`0x1035D028` 所在的 translation block 一直执行到 `0x1035D03C`：
`ldr ip,[pc,#0x108]` 取到 `0x40028FF8`，然后 `bx ip`。`0x40028FF8` 在
handler RAM 里是全零，于是 PC 从 `0x40028FF8` 开始每 4 字节滑行，
直到 `0x40040000` 进入诊断窗口。

## 解读

只要补上**格式正确**的 LPAE 页表，init 自己的控制流可以完整跑完：

1. 读 `0x400E8450`，写 `0x400E8454 = 0x00009000`、`0x400E8458 = 0x42400000`；
2. 调用 `0x1035D17C`/`0x1035D184`（镜像内都是 `bx lr`，即 boot-ROM 服务占位）；
3. 写 TTBCR/TTBR0/TTBR1，开 MMU，写 `VBAR = 0x10020000`；
4. 写 `0x4007C2E0 = 0x10`、`0x4007C2E4 = 1`、`0x2E0014EC = 0x80000000`；
5. `bx ip` 到 `0x40028FF8`。

缺的不是 init，而是 `0x40028FF8` 处的 boot ROM 代码和三个服务函数的真实实现。
换句话说：**E5 的第一级 boot 代码在 0x40000000 区，不在 b1330 镜像里**。

## 对照：用 32 位短描述符表无效

把同一组地址写成 4 GiB identity 的 32 位 1 MiB section 表（放在
`0x40000000`/`0x40001000`）后仍然 prefetch abort（1,802 次 / 2 s）：
LPAE 模式下描述符是 64 位，短描述符表会被解释成无效项。这也反向确认
init 走的是 LPAE。
