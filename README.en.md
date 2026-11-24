# D850-QEMU

A QEMU validation prototype for Nikon D850 (EXPEED 5 / E5) firmware research:
one `nikon-d850` board overlay, a runner/evidence tool, and boot-chain
conclusions derived from the real bytes of `b1330_131.bin`.

> M-core (Firmware A / a1330) track: [English](docs/mcore.en.md) | [中文](docs/mcore.md)

> This repository contains **no full Nikon firmware, key or modified image**; the docs quote only short instruction words/addresses needed for explanation.
> Supply your own decrypted firmware; the tools only read it.
>
> License: **GPL-2.0-or-later** (code and docs) · unofficial project · not
> affiliated with Nikon · read the [Legal Notice](LEGAL.md) before use.

## Core goal

Load a locally supplied, already decrypted D850 main image at the inferred base
and answer four ordered questions:

1. **D0** Can the reset word `0x1037CBA8` run? What does it need from the boot
   ROM?
2. **D1** What does the in-image init at `0x1035CE30` do, and what is missing
   after it enables the MMU?
3. **D2** Once the missing page tables are supplied, does the init complete and
   hand off?
4. **D3** After the hand-off to `0x40028FF8`, which code is still missing?

Out of scope: CMOS/ISP/storage/UI emulation, repacking, signatures, upgrade
containers, and any claim of natural power-on. Entry, stack and page tables are
host-assisted and labelled as such.

## Status (2026-11-24)

| Item | Status |
| --- | --- |
| `nikon-d850` machine | done; builds and runs on QEMU 11.1.0 |
| Image load | `b1330_131.bin` (25,671,414 B); real reset uses `0x10020000` (Fenestra base), host-assisted init can still use `0x10000000` |
| Reset word `0x1037CBA8` | real reset passes MIDR and display/clock polling; now stops at `0x1037CD3C` (`r0=0`, `DFAR=0`) |
| In-image init `0x1035CE30` | without tables: prefetch abort after MMU enable; table walks read `0x40000000`/`0x40001000` (LPAE) |
| LPAE fixture `--e5-tables` | **0 exceptions**; init completes and `bx ip` to `0x40028FF8` |
| Reconstructed bootrom | corrected mapping makes `0x40028FF8` an ARM entry; stage-2 runs, but is a CPU/boot-service setup, not a GUI entry |
| Real-reset run | passes the `0x1037F34C` display/clock poll and enters `0x1037CCC0`; blocked at `0x1037CD3C` with `DFAR=0` |
| Unmodelled MMIO | stage-2 path: **0**; real reset: **552 unique**, 546 of them in `0x80000000..0x8070E7xx` (page tables/DRAM), 6 clock/display registers |
| CPU | LPAE; real reset needs `midr=0x410FC0F4` to pass the MIDR gate |
| M-core `nikon-d850-mcore` (a1330) | boots through reset/clock/command blocks to idle `0xC05E`; no exception, no console TX found |
| Display code (static) | `b1330` contains `PLL_LCD`/`TFT`/`EGL`/`DMP`/`HDMI`/menu strings |
| GUI / LCD | no firmware framebuffer writes; QEMU has only a host-side PPM dump (test card: [docs/display.md](docs/display.md)) |
| UART output | none observed for A-core (`uart=0B`); no TX path found for M-core |
| Real camera test | none; this prototype produces no flashable image |

In one sentence: **the entry and stage-2 are no longer the main issue; the real
reset is now blocked by the unmapped `0x80000000` page-table/DRAM region, and a
firmware GUI still needs display init and framebuffer evidence afterwards.**

Details: [docs/status.md](docs/status.md), [docs/bootrom.md](docs/bootrom.md),
and [evidence/real-reset-progress.md](evidence/real-reset-progress.md).

## Quick start

Build (QEMU 11.1.0 tree, Linux/WSL):

```sh
cd /path/to/qemu-11.1.0
python3 /path/to/D850-QEMU/qemu/prepare_source.py .
ninja -C build qemu-system-arm
```

The overlay only adds `hw/arm/nikon-d850-lab.c` and its registration lines; it
does not patch upstream QEMU. Re-running `prepare_source.py` is idempotent.
QEMU 10.2.4 has a `CharBackend` text adapter but is not build-verified here.

Run:

```sh
python3 tools/run.py \
  --qemu /path/to/qemu-11.1.0/build/qemu-system-arm \
  --payload /path/to/b1330_131.bin \
  --out runs/reset --timeout 3
```

Defaults: `payload-base=0x10000000`, `entry=0x1037CBA8`,
`flash-size=0x2000000`, CPU `cortex-a15`.

Three reproducible experiments:

```sh
# 1. reset word: 0 exceptions, 0 MMIO, PC pops to 0 and slides to halt
python3 tools/run.py --payload "$FW/b1330_131.bin" --out runs/reset

# 2. in-image init: no tables -> prefetch-abort loop
python3 tools/run.py --payload "$FW/b1330_131.bin" --init --out runs/init

# 3. init + LPAE fixture: 0 exceptions, hand-off to 0x40028FF8
python3 tools/run.py --payload "$FW/b1330_131.bin" --init --e5-tables \
  --out runs/init-lpae
```

Each run writes `command.json`, `qemu.log`, `uart.log` and `session.json`
(exception counts, MMIO summary, halt record). `status=halt-unexpected` is the
expected result for the reset word: the sled's `pop` returns to PC=0 and the
CPU slides into the `0xFF0` halt window.

## Key findings

### 1. The reset word is a boot-service sled

The vector table is at `0x10000000`; the reset pointer is `0x1037CBA8`:

```text
1037cba8  b     0x1037cbb8
1037cbb8  str   r0, [r4]
1037cbbc  b     0x1037cbc4
1037cbc4  mov   r0, r1
1037cbc8  pop   {r4, r5, r6, pc}
```

With the assisted all-zero stack, `pop` returns to PC=0. It needs the boot ROM
to preload r4, a stack and a return address; the image base is therefore not
the problem.

### 2. The in-image init builds tables, enables the MMU and sets VBAR

`0x1035CE30..0x1035D040`:

- reads `0x400E8450`, writes `0x400E8454 = 0x9000`,
  `0x400E8458 = 0x42400000`;
- calls `0x1035D17C`/`0x1035D184`; both are `bx lr` in the image — boot-ROM
  service slots;
- at `0x1035D424` writes `TTBCR = 0xB1003104` (EAE=1),
  `TTBR1 = 0x40000000`, `TTBR0 = 0x40001000`;
- writes `SCTLR = 0x1005` (M=1), then `VBAR = 0x10020000`;
- writes `0x4007C2E0 = 0x10`, `0x4007C2E4 = 1`, `0x2E0014EC = 0x80000000`;
- finally loads `ip = 0x40028FF8` and executes `bx ip`.

### 3. What is missing is the LPAE tables, not the init

With no tables at `0x40000000`/`0x40001000`, the first fetch after MMU enable
translation-faults: QEMU's LPAE walk reads both addresses with **size 8**, then
`IFSR=0x205/0x206`, `IFAR=0x1035D5D4`. `VBAR` is still 0, so the abort vector
at `0x0C` itself needs translation and the CPU recurses.

`--e5-tables` maps 256 KiB of handler RAM at `0x40000000` and writes two
4-entry LPAE level-1 tables (1 GiB blocks, identity): `0x40000000` for TTBR1 and
`0x40001000` for TTBR0. Result: **0 exceptions**, init completes and hands off
to `0x40028FF8`. This is a fixture, not the real boot ROM table; it proves the
init's own control flow and localises the gap to the `0x40000000` bank.

### 4. The CPU must support LPAE

With `-cpu cortex-a9`, the `mcrr p15,#1,r0,r1,c2` at `0x1035D45C` (64-bit
TTBR1 write) is undefined and `LR=0x1035D460`. This rules out A9 and agrees
with `TTBCR.EAE=1`; the default is `cortex-a15` (A7-class is equivalent).
The boot ROM at `0x40028FF8` is still absent — handler RAM is zero, so PC
slides from `0x40028FF8` to `0x40040000` and into the diagnostic window.

## Machine model

| Item | Value | Evidence |
| --- | --- | --- |
| CPU | 1× Cortex-A15 (`-cpu` A7/A9) | LPAE `mcrr`; A9 faults |
| Image | `b1330_131.bin` at `0x10000000`, 32 MiB flash | vector table at the load base |
| Initial PC | `0x1037CBA8` (`--init` → `0x1035CE30`) | reset word / init entry |
| Handler RAM | `0x40000000`, 256 KiB; optional LPAE tables | exception vectors `0x4002xxxx`, TTBR1 |
| RAM (assisted stack) | `0x90000000`, 128 MiB | **inherited E4 hypothesis**, not a D850 claim |
| Low memory | `0x0..0xFFFF`, halt window at `0xFF0` | assisted return |
| Diagnostics | 4 GiB catch-all; `strict=on` stops at the first unmodelled access | — |
| Observed MMIO | `0x400E8450/54/58`, `0x4007C2E0/4`, `0x2E0014EC` | this repository's runs |

## Relationship to D750-QEMU

This is a D850-focused split of the
[D750-QEMU](https://github.com/LuoStickmin/D750-QEMU) method. It keeps the
overlay shape, the user-supplied-firmware boundary, and the
"log PC/address instead of returning zero" diagnostic rule. D850-specific work:
the E5 `0x10000000`/`0x40000000` layout, the LPAE init analysis, the
`--e5-tables` fixture and the `0x40028FF8` hand-off finding.

## What is still needed

1. A full D850 SPI flash dump (CH341A/clip) — most likely to contain the
   `0x40000000` boot stage or handler code.
2. A `0x40000000..0x40040000` RAM dump — directly supplies `0x40028FF8` and the
   three service functions.
3. A Nikon service/recovery boot image, if one can be obtained.
4. A hardware UART boot log — to confirm which segments the boot ROM copies.
5. A D5 (same E5 generation) dump — to interpret `0x400E8450/54/58`.

See [docs/bootrom.md](docs/bootrom.md) for the size estimate: the
`0x40000000` window is at least ~184 KiB (vectors), at least ~263 KiB if the
in-image region table is a load map, and a **512 KiB**
(`0x40000000..0x4007FFFF`) dump is recommended. The physical mask ROM may be
smaller and is not necessarily the same object.

Already available: the decrypted `b1330_131.bin`, the vector/exception tables,
the init disassembly, the QEMU fixture and the `0x40028FF8` hand-off point.

> Those firmware/dump inputs are for local analysis only. **Do not attach
> firmware, keys, boot ROM, flash/RAM dumps or modified images to the repository,
> issues or PRs.** If discussion is needed, share only factual summaries without
> code/data (addresses, offsets, hashes, exception records).

## License, legal and contributing

- **Code and documentation**: GPL-2.0-or-later (GNU GPL v2 or any later version).
  The full GPLv2 text is in [LICENSE](LICENSE); SPDX headers and
  [NOTICE.md](NOTICE.md) grant the "or later" option.
- **Copyright**: Copyright (C) 2026 LuoStickmin and D850-QEMU contributors;
  portions of the method and board code derive from the author's
  [D750-QEMU](https://github.com/LuoStickmin/D750-QEMU) (GPL-2.0-or-later).
- **No firmware/keys**: the repository contains no full Nikon firmware, decrypted
  module, K8/T1/T2/T3 material, boot ROM, flash/RAM dump, flashable modified
  image, or Nikon font/icon/UI asset.
- **Unofficial**: not affiliated with, authorized by, sponsored by, or endorsed
  by Nikon; "Nikon", "D850" and "EXPEED" are marks of their owners, used only
  descriptively.
- **Not legal advice**: reverse engineering, anti-circumvention (e.g. DMCA §1201,
  EU 2009/24/EC), EULA, export-control and warranty questions vary by
  jurisdiction; consult a lawyer before use.
- **No warranty**: provided "AS IS" under the GPL; no liability for camera damage,
  bricking, data loss, voided warranty or legal consequences. QEMU is not a
  security boundary; run it in an isolated environment.

| File | Purpose |
| --- | --- |
| [LICENSE](LICENSE) | full GPLv2 text |
| [NOTICE.md](NOTICE.md) | copyright, license scope, upstream and third parties |
| [LEGAL.md](LEGAL.md) | legal/compliance note (bilingual, not legal advice) |
| [CONTRIBUTING.md](CONTRIBUTING.md) / [CONTRIBUTING.en.md](CONTRIBUTING.en.md) | contribution rules, DCO, prohibited content |
| [SECURITY.md](SECURITY.md) | vulnerability reporting and security boundary |
| [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) | code of conduct |
