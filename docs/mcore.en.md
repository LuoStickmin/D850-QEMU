# D850 M-core (Firmware A / a1330) validation prototype

> Status: 2026-11-18. Experimental prototype, not a full-machine emulator,
> and it does not claim that the M-core "completed boot". All conclusions
> come from a locally supplied a1330 image and QEMU run logs.

## 1. Input and image

- Image: `D850_6006432_MPU.bin` (1,048,576 bytes, SHA-256 `d23e6260...`),
  from the Tornado MPU dump in the WL-D850-FILES repository; this session
  reconstructed it packet-by-packet from `d850_tornado_camera.pcap`, and the
  result is byte-identical to the repository file.
- Identity: `D850   Firmware A Copyright Nikon Corp.` (0x1f08), version
  block `01 03 03`.
- Vector table at 0x0, **32-bit big-endian**: MSP=`0x2000fa30`,
  reset=`0x0000c20d`.
- The instruction stream is **little-endian Thumb**: BE8 (LE instruction
  fetch, BE data accesses).
- Compared with stock `a1330_010303.bin`: 27,138 of 1,048,576 bytes differ,
  concentrated in `0x2000-0x8fff` (947 B) and `0xf0000-0xfffff`
  (26,191 B); the rest is byte-identical. The differences look like
  runtime-written/calibration data; unverified.

## 2. Machine model

```sh
qemu-system-arm -M nikon-d850-mcore \
  -kernel /path/to/D850_6006432_MPU.bin
```

| Item | Value |
| --- | --- |
| CPU | `cortex-m4` (VFP enabled); `-cpu` can override |
| Flash | 1 MiB @ `0x00000000`, a1330 loaded directly |
| SRAM | 256 KiB @ `0x20000000` (MSP at `0x2000fa30`) |
| Diagnostics | 4 GiB catch-all; unmodelled accesses log PC/address/size/direction |
| Default entry | vector table: SP=`0x2000fa30`, PC=`0x0000c20d` |

### 2.1 QEMU's M-profile vector fetch is LE

QEMU's `arm_cpu_reset_hold` and exception entry read the vector table with
`ldl_p` (little-endian); a1330's table is big-endian, so a direct read
yields `0x30fa0020/0x0dc20000`. When writing the image into flash, the
machine word-swaps only the first `0x40` bytes (16 entries, BE to LE) for
the CPU; the original image and the rest of the 1 MiB are untouched. This
is a **QEMU fetch fixture**, not a modification of the dump.

### 2.2 BE8 vs the LE NVIC

QEMU models the ARMv7-M NVIC/SCB as `DEVICE_LITTLE_ENDIAN`. When a BE8
guest writes `VTOR=0x2000fc00` and `CPACR=0x00f00000`, the NVIC receives
`0x00fc0020` and `0x0000f000`:

- VTOR becomes `0x00fc0000`, so exception entry reads the wrong address;
- the CP10/CP11 bits in CPACR are cleared, and the first FPU instruction
  (`mrc p10,...` at `0x19b9e`) raises a NOCP UsageFault.

The machine adds a 0x100-byte **big-endian SCB window** at `0xE000ED00`,
handling only `VTOR` (0x08) and `CPACR` (0x88) and writing the values into
`CPUARMState`'s `v7m.vecbase`/`v7m.cpacr`, which is what the CPU actually
uses. This is a BE8 fixup, not an SCB model.

### 2.3 First peripherals

The reset path touches two blocks first:

- `0x400F2000`: read, clear bit 7, write back; `0x400F2004 = 0xB1`.
- `0x400F3000`: `0x400F3004` is a command/status handshake - write
  `0xC35D0100 | (low 20 bits read back)`, poll until the top 12 bits match,
  then set/poll. A register file that returns the last written value gets
  past the handshake.

## 3. Observed boot trace

```text
Loaded reset SP 0x2000fa30 PC 0xc20d
0xc2b0  rd 0x400f2000          ; clear bit 7
0xc2b6  wr 0x400f2000
0xc2be  wr 0x400f2004 = 0xb1
0x177c8 rd 0x400f3004          ; command/status handshake
0x177d2 wr 0x400f3004
0x177d4 rd 0x400f3004 (x N)
...
0x166aa rd 0x440ce000          ; pin/clock block (not a UART)
0x166b0 wr 0x440ce000 = 0x80000000
0x16724 rd 0x440ce004
0x16732 wr 0x440ce004 = 0x40000000
0x16754 rd 0x440ce008
0x1675c wr 0x440ce008 = 0
0x1673c rd 0x440ce004
0x1674a wr 0x440ce004 = 0x03000000
```

After that there are no further unmodelled accesses; QMP at 3 s reads:

```text
R15=0000c05e  R14=0000c05f  R01=e000ef00  R13=2000fa28
XPSR=61000000  T priv-thread
```

That is, it stops at `0xc05e` (the `bx lr` of the function at `0xc044`),
which had just written to `0xE000EF00` (NVIC STIR). There is no exception
and no lockup; this is a stable idle/loop state, not a crash.

## 4. Current boundary

- `0x440CE000` is a **pin/clock/GPIO block, not a UART** (see 4.1); the
  boot path writes only eight control registers, never a serial data
  register.
- `0x440CE004/008/010` are its global config; `0x24 + 0x10*n` (n=0..7) are
  the eight channel/pin config registers; `0x166b4/0x166f0` read/write them
  by index.
- The real UART candidates are `0x440C4000..0x440C4900` (eight instances,
  0x100 stride; the base table is at 0x1f218; 0x129bc initializes
  `0x440C4600` and reads a counter). The boot path does not touch this range.
- The STIR IRQ number (observed `0x8f`) exceeds the current `num-irq=96`;
  interrupt behavior is not modelled.
- Exception entry still reads the SRAM vector table at `VTOR=0x2000fc00`;
  that table is BE and QEMU reads it LE. No exception has occurred yet.
- `0x400F2000`/`0x400F3000` are register files, not hardware models.

### 4.1 Why there are no TX bytes

I checked the "build a UART model at 0x440CE000" assumption by
disassembling the block; the assumption is wrong:

- `0x166aa`: read `[0x440CE000]`, set bit 7; `0x16724/0x1673c`: modify bits
  0/1/3/6 of `[0x440CE004]`; `0x16754`: modify the low 7 bits of
  `[0x440CE008]`; `0x16760/0x16774`: modify bits 6/7 of `[0x440CE010]`.
  This is a pin/clock enable sequence, not TX.
- `0x166b4/0x166f0` use `tbb` to select one of eight channels
  (`0x24/0x34/.../0x94`) and read/write 16-bit config values; `0x166ec`
  reads `[0x440CE010]` status.
- The UART config functions are `0x16576/0x16598/0x165a8/0x165c0`, with
  `0x440C4xxx` bases; `0x16570` reads a 16-bit counter at `[base+0x1c]`
  (more like a baud/clock counter). `0x129bc` (the `0x440C4600`
  initializer) has no direct callers; it may be in a function table.
- The boot path is `0xC20D -> 0xC2AE -> 0x177C4 -> 0x83A0/0x1CE34/0x2B4`;
  the trace contains no `0x440C4xxx` access.

So there are **no TX bytes to capture** on this boot path; a UART model at
0x440CE000 would only see control writes. To get output, the console path
must first be found or triggered.

## 5. Next steps

1. Find the console init/print path: callers of `0x129bc`/`0x12a3c` (maybe
   in a function table/RTOS task), or check the 21 callers of `0x16576` for
   a putc.
2. If the console lives in an RTOS task, model STIR/IRQ and a timer first so
   the idle loop can continue.
3. Handle the BE/LE of the SRAM vector table at `VTOR=0x2000fc00` and verify
   the exception path.
4. Once M-core output confirms "boot complete", consider the handoff to the
   A-core (who releases it, who supplies the `0x400E8450` parameters).

> All a1330/flash content is local input; the repository does not include the
> image. The fixtures above are only for QEMU control flow, and they do not
> represent real register semantics.
