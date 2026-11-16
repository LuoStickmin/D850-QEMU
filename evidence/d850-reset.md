# D850 reset word：0x1037CBA8 是 boot-service sled

- 日期：2026-11-16
- 固件：`b1330_131.bin`（D850 C:1.31），25,671,414 字节
- SHA-256：`97E1DA12A91C9B08E7857C38A01090E770B7309407DE9A8E9BED2D5A81204003`
- 机器：`nikon-d850`（payload @ `0x10000000`，flash 32 MiB，CPU `cortex-a15`）
- 会话目录：`runs/d850-reset/`（`command.json`、`qemu.log`、`session.json`）
- 结果：status=`halt-unexpected`，exit=0，timed_out=False，
  wall=0.269 s


## 命令

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" \
  --out runs/d850-reset --timeout 3
```

## 结果

```text
PC 起始：
  0x1037cba8
  0x1037cbb8
  0x1037cbc4
  0x00000000
  0x000001e8
  0x00000400
...
PC 末尾：
  0x00000fe4
  0x00000fe8
  0x00000fec
  0x00000ff0
exceptions: 0
MMIO: 0
d850-halt: pc=0x00000ff0 r0=0x00000000 r1=0x00000000 sp=0x97ffff10 lr=0x00000ff0 return_ok=false mode=0x13 unexpected state (exception or wild branch reached the trampoline)
```

## 解读

`0x1037CBA8` 不是复位入口，而是一段 8 字节的 boot-ROM 服务 sled：
`b 0x1037CBB8; str r0,[r4]; mov r0,r1; pop {r4,r5,r6,pc}`。辅助启动给的栈是全零，
`pop` 把 PC 弹回 0，随后在低地址零页滑到 `0xFF0` 的 halt 窗口。**0 异常、0 MMIO**
说明只靠“从 reset 字起跑”无法进入固件主流程；缺的是 boot ROM 提供的 r4/栈/返回地址。
