# CPU 证据：Cortex-A9 在 mcrr（64 位 TTBR）上 undefined

- 日期：2026-11-16
- 固件：`b1330_131.bin`（D850 C:1.31），25,671,414 字节
- SHA-256：`97E1DA12A91C9B08E7857C38A01090E770B7309407DE9A8E9BED2D5A81204003`
- 机器：`nikon-d850`（payload @ `0x10000000`，flash 32 MiB，CPU `cortex-a15`）
- 会话目录：`runs/d850-init-a9/`（`command.json`、`qemu.log`、`session.json`）
- 结果：status=`halt-unexpected`，exit=0，timed_out=False，
  wall=0.149 s


## 命令

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" --init \
  --cpu cortex-a9 --out runs/d850-init-a9 --timeout 1
```

## 结果

```text
d850-probe: pc=0x1035cefc rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cf10 rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cf24 rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cf54 wr addr=0x400e8454 size=4 value=0x00009000 (unmodelled write)
d850-probe: pc=0x1035cf5c rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cf68 rd addr=0x400e8454 size=4 (unmodelled read, returning 0)
d850-probe: pc=0x1035cf84 wr addr=0x400e8458 size=4 value=0x42400000 (unmodelled write)
d850-probe: pc=0x1035cf8c rd addr=0x400e8450 size=4 (unmodelled read, returning 0)
Taking exception 1 [Undefined Instruction] on CPU 0
...from EL3 to EL3
...with ESR 0x0/0x2000000
d850-halt: pc=0x00000ff0 r0=0x40000000 r1=0x00000000 sp=0x00000000 lr=0x1035d460 return_ok=false mode=0x1b unexpected state (exception or wild branch reached the trampoline)
d850-halt: pc=0x00000ff0 r0=0x40000000 r1=0x00000000 sp=0x00000000 lr=0x1035d460 return_ok=false mode=0x1b unexpected state (exception or wild branch reached the trampoline)
```

异常发生前 PC 已执行到 `0x1035D45C` 的
`mcrr p15, #1, r0, r1, c2`（TTBR1 的 64 位写）；Cortex-A9 不支持 LPAE，
该指令 undefined，LR=`0x1035D460`。这确认 init 需要 LPAE 核（A7/A15 类），
也解释了为什么 A15 + 正确 LPAE 表能继续、A9 不能。
