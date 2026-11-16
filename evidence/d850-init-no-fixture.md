# D850 镜像内 init（0x1035CE30）：MMU 打开后缺 LPAE 页表

- 日期：2026-11-16
- 固件：`b1330_131.bin`（D850 C:1.31），25,671,414 字节
- SHA-256：`97E1DA12A91C9B08E7857C38A01090E770B7309407DE9A8E9BED2D5A81204003`
- 机器：`nikon-d850`（payload @ `0x10000000`，flash 32 MiB，CPU `cortex-a15`）
- 会话目录：`runs/d850-init/`（`command.json`、`qemu.log`、`session.json`）
- 结果：status=`timeout`，exit=0，timed_out=True，
  wall=2.144 s


## 命令

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" --init \
  --out runs/d850-init --timeout 2
```

## 结果

- 异常：**prefetch abort = 1852**（2 s 内）
- 未建模 MMIO 事件：512

触发的第一条异常（表遍历读 `0x40000000`，size=8，即 LPAE 描述符）：

```text
Taking exception 3 [Prefetch Abort] on CPU 0
...from EL3 to EL3
...with ESR 0x21/0x86000005
...with IFSR 0x205 IFAR 0x1035d5d4
```

MMIO 记录（节选）：

```text
d850-probe: pc=0x1035ce38 rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cea0 rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cefc rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cf10 rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cf24 rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cf54 wr addr=0x400e8454 size=4 value=0x00009000 (unmodelled write)
d850-probe: pc=0x1035cf5c rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cf68 rd addr=0x400e8454 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cf84 wr addr=0x400e8458 size=4 value=0x42400000 (unmodelled write)
d850-probe: pc=0x1035cf8c rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035d5d4 rd addr=0x40000000 size=8 (unmodelled read, returning 0)
d850-probe: pc=0x0000000c rd addr=0x40001000 size=8 (unmodelled read, returning 0)
```

## 解读

init 在 `0x1035D424` 里先写 `TTBCR = 0xB1003104`（EAE=1，T0SZ=4），
`TTBR1 = 0x40000000`、`TTBR0 = 0x40001000`，然后在 `0x1035D5D0` 写
`SCTLR = 0x1005`（M=1）。下一条取指需要走 LPAE 页表，但
`0x40000000`/`0x40001000` 都没有页表：QEMU 的 LPAE 表遍历以 size=8 读这两个地址
（被诊断窗口记下），随后 prefetch abort；`VBAR` 此时仍为 0，异常向量又落在
`0x0C`，而它同样需要翻译 → 递归 abort。所以 2 秒内出现上千次
prefetch abort，PC 停在 `0x0C`。
