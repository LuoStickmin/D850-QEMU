# SPDX-License-Identifier: GPL-2.0-or-later
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RUNS = ROOT / "runs"
EV = ROOT / "evidence"
EV.mkdir(exist_ok=True)

FW_SIZE = 25671414
FW_SHA = "97E1DA12A91C9B08E7857C38A01090E770B7309407DE9A8E9BED2D5A81204003"


def session(name):
    return json.loads((RUNS / name / "session.json").read_text(encoding="utf-8"))


def log(name):
    return (RUNS / name / "qemu.log").read_text(encoding="utf-8",
                                                errors="replace")


def header(title, name, extra=""):
    s = session(name)
    return f"""# {title}

- 日期：2026-11-16
- 固件：`b1330_131.bin`（D850 C:1.31），{FW_SIZE:,} 字节
- SHA-256：`{FW_SHA}`
- 机器：`nikon-d850`（payload @ `0x10000000`，flash 32 MiB，CPU `cortex-a15`）
- 会话目录：`runs/{name}/`（`command.json`、`qemu.log`、`session.json`）
- 结果：status=`{s['status']}`，exit={s['exit']}，timed_out={s['timed_out']}，
  wall={s['wall_seconds']} s
{extra}
"""


def write(name, text):
    (EV / name).write_text(text, encoding="utf-8")
    print(name, len(text))


# ---- reset -------------------------------------------------------------
rtrace = log("d850-reset-trace")
rpcs = [m.group(1) for m in re.finditer(r"^(0x[0-9a-f]+):\s*$", rtrace, re.M)]
rfirst = "\n".join(f"  {p}" for p in rpcs[:6])
rlast = "\n".join(f"  {p}" for p in rpcs[-4:])
text = header("D850 reset word：0x1037CBA8 是 boot-service sled", "d850-reset")
text += f"""
## 命令

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" \\
  --out runs/d850-reset --timeout 3
```

## 结果

```text
PC 起始：
{rfirst}
...
PC 末尾：
{rlast}
exceptions: 0
MMIO: 0
""" + [l for l in log("d850-reset").splitlines() if "d850-halt:" in l][0] + """
```

## 解读

`0x1037CBA8` 不是复位入口，而是一段 8 字节的 boot-ROM 服务 sled：
`b 0x1037CBB8; str r0,[r4]; mov r0,r1; pop {r4,r5,r6,pc}`。辅助启动给的栈是全零，
`pop` 把 PC 弹回 0，随后在低地址零页滑到 `0xFF0` 的 halt 窗口。**0 异常、0 MMIO**
说明只靠“从 reset 字起跑”无法进入固件主流程；缺的是 boot ROM 提供的 r4/栈/返回地址。
"""
write("d850-reset.md", text)

# ---- init without fixture ---------------------------------------------
s = session("d850-init")
lines = log("d850-init").splitlines()
first_exc = next(i for i, l in enumerate(lines) if "Taking exception" in l)
exc_block = "\n".join(lines[first_exc:first_exc + 4])
probe_lines = [l for l in lines if "d850-probe:" in l]
seen = []
for line in probe_lines:
    if line not in seen:
        seen.append(line)
probes = "\n".join(seen[:15])
if len(seen) > 15:
    probes += f"\n...（共 {len(probe_lines)} 条，去重后 {len(seen)} 种）"

text = header("D850 镜像内 init（0x1035CE30）：MMU 打开后缺 LPAE 页表", "d850-init")
text += f"""
## 命令

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" --init \\
  --out runs/d850-init --timeout 2
```

## 结果

- 异常：**prefetch abort = {s['exceptions']['counts']['prefetch_abort']}**（2 s 内）
- 未建模 MMIO 事件：{s['mmio']['events']}

触发的第一条异常（表遍历读 `0x40000000`，size=8，即 LPAE 描述符）：

```text
{exc_block}
```

MMIO 记录（节选）：

```text
{probes}
```

## 解读

init 在 `0x1035D424` 里先写 `TTBCR = 0xB1003104`（EAE=1，T0SZ=4），
`TTBR1 = 0x40000000`、`TTBR0 = 0x40001000`，然后在 `0x1035D5D0` 写
`SCTLR = 0x1005`（M=1）。下一条取指需要走 LPAE 页表，但
`0x40000000`/`0x40001000` 都没有页表：QEMU 的 LPAE 表遍历以 size=8 读这两个地址
（被诊断窗口记下），随后 prefetch abort；`VBAR` 此时仍为 0，异常向量又落在
`0x0C`，而它同样需要翻译 → 递归 abort。所以 2 秒内出现上千次
prefetch abort，PC 停在 `0x0C`。
"""
write("d850-init-no-fixture.md", text)

# ---- init with LPAE fixture -------------------------------------------
s = session("d850-init-lpae")
trace = log("d850-init-lpae-trace")
pcs = [m.group(1) for m in re.finditer(r"^(0x[0-9a-f]+):\s*$", trace, re.M)]
idx = next(i for i, p in enumerate(pcs) if int(p, 16) >= 0x40000000)
trail = "\n".join(f"  {p}" for p in pcs[:12])
trans = "\n".join(f"  {p}" for p in pcs[max(0, idx - 10):idx + 5])
mmio = "\n".join(
    f"- {r['rw']} {r['addr']} x{r['count']}（first pc {r['first_pc']}）"
    for r in s["mmio"]["by_address"][:10])
if len(s["mmio"]["by_address"]) > 10:
    mmio += f"\n- …其余 {len(s['mmio']['by_address']) - 10} 组（多为零页滑行的取指读）"

text = header("D850 init + E5 LPAE 页表 fixture：0 异常，交接到 0x40028FF8",
              "d850-init-lpae")
text += f"""
## 命令

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" --init --e5-tables \\
  --out runs/d850-init-lpae --timeout 2
```

`--e5-tables` 在 `0x40000000` 映射 256 KiB handler RAM，并写入两张 LPAE
level-1 表（各 4 项、1 GiB block、identity），分别位于 `0x40000000`
（TTBR1）和 `0x40001000`（TTBR0）。这是**夹具**，不是真机 boot ROM 的页表。

## 结果

- 异常：**0**
- 未建模 MMIO 事件：{s['mmio']['events']}

未建模 MMIO 汇总：

{mmio}

### 起始 PC 序列

```text
{trail}
```

### 交接点

```text
{trans}
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
"""
write("d850-init-lpae.md", text)

# ---- CPU evidence ------------------------------------------------------
lines = log("d850-init-a9").splitlines()
first_exc = next(i for i, l in enumerate(lines) if "Taking exception" in l)
ctx = "\n".join(lines[max(0, first_exc - 8):first_exc + 6])
halt = next(l for l in lines if "d850-halt:" in l)

text = header("CPU 证据：Cortex-A9 在 mcrr（64 位 TTBR）上 undefined",
              "d850-init-a9")
text += f"""
## 命令

```sh
python3 tools/run.py --payload "$FW/b1330_131.bin" --init \\
  --cpu cortex-a9 --out runs/d850-init-a9 --timeout 1
```

## 结果

```text
{ctx}
{halt}
```

异常发生前 PC 已执行到 `0x1035D45C` 的
`mcrr p15, #1, r0, r1, c2`（TTBR1 的 64 位写）；Cortex-A9 不支持 LPAE，
该指令 undefined，LR=`0x1035D460`。这确认 init 需要 LPAE 核（A7/A15 类），
也解释了为什么 A15 + 正确 LPAE 表能继续、A9 不能。
"""
write("d850-cpu-a9.md", text)

# ---- evidence index ----------------------------------------------------
idx_text = """# 证据索引（2026-11-16）

所有会话均由 `tools/run.py` 生成；固件为本地提供的 `b1330_131.bin`
（D850 C:1.31，SHA-256 `97E1DA12…204003`），仓库不附带固件。

| 文件 | 内容 | 关键结果 |
|---|---|---|
| [d850-reset.md](d850-reset.md) | 从 reset 字 `0x1037CBA8` 起跑 | 0 异常、0 MMIO；PC 弹回 0 并滑到 halt |
| [d850-init-no-fixture.md](d850-init-no-fixture.md) | 辅助入口 `0x1035CE30`，无页表夹具 | MMU 打开后 prefetch abort；表遍历读 `0x40000000`/`0x40001000` size=8 |
| [d850-init-lpae.md](d850-init-lpae.md) | 同一入口 + `--e5-tables` | **0 异常**；init 跑完并 `bx ip` 到 `0x40028FF8` |
| [d850-cpu-a9.md](d850-cpu-a9.md) | `-cpu cortex-a9` 对照 | `mcrr`（TTBR1）undefined；证明走 LPAE |

`lpae-table.bin` 是 `--e5-tables` 写入的 8 KiB 夹具（两张 4 项 LPAE level-1 表），
由 `tools/run.py`/机器模型生成，用于让缺失的 boot ROM 页表不再阻断翻译；
它不包含任何 Nikon 代码。
"""
write("README.md", idx_text)
