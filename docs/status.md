# D850-QEMU 状态（2026-11-16）

固件：`b1330_131.bin`（D850 C:1.31），25,671,414 字节，
SHA-256 `97E1DA12A91C9B08E7857C38A01090E770B7309407DE9A8E9BED2D5A81204003`。
仓库不附带该文件。

运行环境：WSL Ubuntu、gcc 15.2、QEMU 11.1.0（本仓库 overlay 增量编译），
`-cpu cortex-a15`、`-m 128M`、`-smp 1`，除非另注。

## 1. 结论速览

| 实验 | 入口 | 夹具 | 异常 | 终点 |
| --- | --- | --- | --- | --- |
| reset | `0x1037CBA8` | 无 | 0 | PC=0 → 滑到 `0xFF0` halt |
| init，无页表 | `0x1035CE30` | 无 | prefetch abort ≈1.8k / 2 s（数量随 timeout 变化） | `IFAR=0x1035D5D4`，随后 `0x0C` 递归 |
| init + 短描述符表 | `0x1035CE30` | 32-bit identity 表 | prefetch abort ≈1.8k / 2 s | 仍在 `0x1035D5D4` |
| **init + LPAE 表** | `0x1035CE30` | `--e5-tables` | **0** | `bx ip` → `0x40028FF8`，零页滑行 |
| init，A9 | `0x1035CE30` | 无 | undefined ×1 | `mcrr` @ `0x1035D45C` |

异常计数随 timeout 变化，比较时应看**首条异常地址**和**分支终点**，不要看总数。

## 2. reset 字 `0x1037CBA8`

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" \
  --out runs/d850-reset --timeout 3
```

```text
status=halt-unexpected  exit=0  wall=0.144 s
exceptions: 0
MMIO: 0
PC: 0x1037CBA8 -> 0x1037CBB8 -> 0x1037CBC4 -> 0x00000000
    0x000001E8 -> ... -> 0x00000FF0
d850-halt: pc=0x00000ff0 ... sp=0x97ffff10 lr=0x00000ff0 return_ok=false
```

`0x1037CBA8` 是 boot-service sled：`b/str/b/mov/pop {r4,r5,r6,pc}`。
辅助启动的栈全零，`pop` 回到 0。这个结果**符合预期**，说明：
镜像装对了、CPU 能取指，但 reset 字需要 boot ROM 的 r4/栈/返回地址。

## 3. init `0x1035CE30`：无夹具

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" --init \
  --out runs/d850-init --timeout 2
```

```text
status=timeout  exceptions: prefetch_abort≈1.8k
MMIO: 512 events
```

关键序列：

```text
d850-probe: pc=0x1035d5d4 rd addr=0x40000000 size=8   <- LPAE 表遍历
Taking exception 3 [Prefetch Abort]
  IFSR 0x205  IFAR 0x1035d5d4
d850-probe: pc=0x0000000c rd addr=0x40001000 size=8   <- 异常向量再次翻译
Taking exception 3 ...
  IFSR 0x206  IFAR 0xc
```

解读：

- init 在 `0x1035D424` 写 `TTBCR=0xB1003104`、`TTBR1=0x40000000`、
  `TTBR0=0x40001000`，再写 `SCTLR=0x1005`；
- 下一条取指（`0x1035D5D4`）走 LPAE 表，表基址正好是上面两个地址；
- QEMU 以 8 字节读表项，读不到有效描述符 → translation fault；
- `VBAR` 此时仍是 0，prefetch abort 向量在 `0x0C`，而取 `0x0C` 也要翻译，
  于是递归 abort。2 秒内上千次。

## 4. init + `--e5-tables`：0 异常

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" --init --e5-tables \
  --out runs/d850-init-lpae --timeout 2
```

`--e5-tables` 做两件事：

1. 在 `0x40000000` 映射 256 KiB handler RAM（覆盖异常向量 `0x4002xxxx`
   和 init 的交班目标 `0x40028FF8`）；
2. 写入两张 4 项 LPAE level-1 表（1 GiB block、identity、AF/SH/AP 有效）：
   `0x40000000`（TTBR1）和 `0x40001000`（TTBR0）。

结果：

```text
status=timeout  exceptions: 0  MMIO: ≈650–750 events（随 timeout 变化）
```

观察到的 MMIO：

| 地址 | 方向 | 值 | 首次 PC |
| --- | --- | --- | --- |
| `0x400E8450` | rd | — | `0x1035CE38`（共 7 次） |
| `0x400E8454` | wr | `0x00009000` | `0x1035CF54` |
| `0x400E8454` | rd | — | `0x1035CF68` |
| `0x400E8458` | wr | `0x42400000` | `0x1035CF84` |
| `0x4007C2E0` | wr | `0x00000010` | `0x1035D60C` |
| `0x4007C2E4` | wr | `0x00000001` | `0x1035D61C` |
| `0x2E0014EC` | rd/wr | `0x80000000` | `0x1035D6F4` / `0x1035D6FC` |

PC 交接：

```text
0x1035D028 -> ... -> 0x1035D03C  ldr ip,[pc,#0x108] ; 0x40028FF8
0x40028FF8 -> 0x40029000 -> ... -> 0x40040000（诊断窗口）
```

`0x40028FF8` 处没有代码（handler RAM 全零），所以 PC 每 4 字节滑行；
`0x40040000` 之后由诊断窗口返回 0，继续滑行。**init 自身已经跑完，
缺的是 boot ROM 在 `0x40000000` 区提供的代码和三个服务函数。**

> 注意：这是“辅助入口 + 页表夹具”，不是自然上电。夹具只满足翻译，
> 不提供 boot ROM 的代码、服务、时钟/复位语义。

## 5. 负结果：32 位短描述符表无效

把 4 GiB identity 的 32 位 1 MiB section 表（`0x0C0E` 描述符）放到
`0x40000000`/`0x40001000` 后，仍然在 `0x1035D5D4` prefetch abort
（2 s 内 1,802 次）。原因：init 走的是 LPAE，描述符是 64 位；
32 位表被解释成无效项。这反证了 LPAE 结论。

## 6. CPU 证据：A9 的 `mcrr` undefined

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" --init \
  --cpu cortex-a9 --out runs/d850-init-a9 --timeout 1
```

```text
exceptions: undef=1
halt: pc=0x00000ff0 ... sp=0x00000000 lr=0x1035d460 return_ok=false
```

`LR=0x1035D460` 指向 `0x1035D45C` 的 `mcrr p15,#1,r0,r1,c2`（TTBR1 的
64 位写）。Cortex-A9 不支持 LPAE，直接 undefined。因此默认 CPU 用
`cortex-a15`；A7 同类也具备 LPAE。

## 7. 机器选项

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `payload` / `-kernel` | — | 解密后的 b1330 主镜像 |
| `payload-base` | `0x10000000` | 镜像装入地址 |
| `entry` | `0x1037CBA8` | 初始 PC |
| `flash-size` | 32 MiB | flash 窗口（≥ 镜像大小） |
| `boot-mode` | `entry` | `stub` 是旧 E4 路径，D850 不用 |
| `e5-tables` | off | LPAE identity 夹具 + handler RAM |
| `aux2-base` / `aux2-size` | off | 手工映射 handler RAM |
| `virtual-boot-rom` | off | 旧 E5 `bx lr` 桩实验；与 `e5-tables` 互斥 |
| `strict` | off | 第一条未建模访问后停止 |

## 8. 下一步

1. 拿 `0x40000000..0x40040000` 的 RAM dump 或整片 flash dump；
   尺寸与优先级见 [bootrom.md](bootrom.md)；
2. 给 `0x400E8450/54/58` 建寄存器模型，观察 init 的分支；
3. 在 `0x40028FF8` 放 `bx lr`/`b .` 桩，记录返回契约；
4. 把服务函数 `0x1035D17C`/`0x1035D184` 的真实实现接进来。

这些都需要新输入；在没有新输入时，不会用“填零/填假值”冒充 boot ROM。
