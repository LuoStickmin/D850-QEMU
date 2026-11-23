# Changelog

## 0.2.4 — 2026-11-23

Real reset reaches the page-table loop; MMU context still missing.

- `0x21100D00` virtual → physical `0x54000D00`; return 0 for offset `0xD00`.
- Real reset now passes `0x1037F34C` and enters `0x1037CCC0`.
- It aborts at `0x1037CD3C` `ldrd r6,r7,[r0]` with `DFAR=0`.
- CPU regs: `R00=0`, `R01=0x40001000`, `R04=0x10380000`.
- Bootarg IO logging shows no access to `0x400E8450` in this path; presetting
  it to `0x40708000` does not help.

## 0.2.3 — 2026-11-23

Boot-context fixture `0x400A761C`.

- The actual stage-2 context literal at `0x10204CF4` is `0x400A7614`;
  `[0x400A761C]` is the flag read before the `blx r0` callback.
- Added `boot-context=on` / `--boot-context`, presetting `[0x400A761C]=1`.
- Result: no data abort, 491 TB, 0 MMIO, halt at `0xFF0` with
  `LR=0x4002BD18`; the stack/LR context is still incomplete.

## 0.2.2 — 2026-11-23

Corrected a1330 extraction and re-scan.

- a1330 payload starts at `0xD0` (after the 32-byte module header), not `0xB0`.
- Re-scan confirms no direct `0x90001046` in a1330; its nine `0x90001040` raw
  hits are instruction bytes (e.g. inside `add.w`), not reliable pointers.
- The missing stage-2 context remains most likely boot-ROM-supplied.

## 0.2.1 — 2026-11-23

Full-firmware scope scan.

- Parsed the full `D850_0131_stock.dec` TOC: a1330 at `0xB0`, wma at
  `0x1000B2`, tpj at `0x1D79DD`, b1330 at `0x1E59DF`.
- No module other than b1330 contains a direct `0x90001046` literal; a1330's
  `0x90001040` hits are mostly instruction bytes, not reliable pointers.
- The missing stage-2 context is therefore more likely boot-ROM-supplied than
  a ready-made module we can load.

## 0.2.0 — 2026-11-23

Worklist MMIO modeling: stage-2 path reaches zero unmodelled accesses.

### Added

- RAM-backed `0x30001000` and `0xC0110000` register/tables blocks.
- Boot RAM moved to `0x40700000..0x4070FFFF`; LPAE tables still at
  `0x40708000`.
- Bootrom window expanded to 7 MiB (`0x40000000..0x406FFFFF`) so code/data
  references at `0x400A7xxx` and `0x404Axxxx` are mapped.
- Low scratch `0x00700000` and clock/PMU `0x2E001000`-style page.

### Result

- `runs/worklist5`: **`mmio_events=0`** on the stage-2 path.
- A single data abort remains: `0x10204D00` `blx r0` jumps to `0x10224918`
  (data), DFAR=`0x1022494D`. This is a function-pointer/context problem, not
  an unmodelled MMIO.

## 0.1.9 — 2026-11-23

Unmodelled MMIO coverage snapshot.

- stage-2 path: 34 unique addresses / 43 accesses.
- real-reset path: 42 unique addresses / 47 accesses.
- union: 76 unique addresses, concentrated in `0x30001000..0x3000108C` (36)
  and `0xC0111400..0xC01118FF` (24); about 7 device blocks remain.
- No `0x83DFE000` framebuffer write yet.

## 0.1.8 — 2026-11-22

CPU-model and cp1 probes.

- Cortex-A7/A9/A15 all fail at `0x10235618`; A9 stops earlier at `0xFE8`.
- Ignoring cp1 in `MCR/MRC/MCRR/MRRC` does not help: the Thumb `0xfc44 0x2100`
  encoding is an EXPEED custom coprocessor, not standard VFP.
- Patching `file 0x235618` to NOP reaches `0x101fc7a8`, then data abort
  (`DFAR=0xE`) because the skipped cp1 did not initialize the pointer.
- Conclusion: GUI requires modeling/deriving the custom cp1, not another
  address patch.

## 0.1.7 — 2026-11-22

Peer handshake and stage-2 VFP boundary.

### Added

- `0x800E8000` peer page: reads at `0x45C/0x460` return 1, so the reset
  handshake at `0x1037CC88` completes.
- `0x51100000` status page (GDU/display ack).

### Result

- Real reset now passes `0x1037CC88` and writes `0x51200010/14/18`; it still
  does not call `0x40028FF8`.
- Host-assisted stage-2 passes the Thumb `bl 0x400034B8` and reaches
  `0x10235618`, where QEMU undefines the VFP `mcrr2 p1,#0,r2,r4,c0`.
  Patching it to NOP reaches `0x10205150 -> 0x101fc7a8`, then a data abort.

## 0.1.6 — 2026-11-22

Real-reset fixtures and display-block probe.

### Added

- Boot RAM at `0x40708000` (64 KiB) and boot-arg page at `0x400E8000` for the
  real `0x1037CCC0` table-page setup.
- Diagnostic free-running counter at `0x51340008` and all-ones GDU/display
  status at `0x54000000..0x571FFFFF`.
- Assisted entry enables VFP/CPACR; `-cpu ...,midr=0x410FC0F4` passes the
  `0x1037CC0C` gate.
- `evidence/real-reset-progress.md`.

### Result

- Real reset now runs past MIDR and writes display/clock registers
  (`0x5502xxxx`/`0x5504xxxx`/`0x5506xxxx`).
- It stops at `0x1037CC88` waiting for `[0x800E845C]/[0x800E8460]`, i.e. the
  boot-ROM/peer handshake; no framebuffer pixels yet.

## 0.1.5 — 2026-11-22

Corrected boot ROM mapping and entry inference (Fenestra live delta).

### Fixed

- The reconstructed window was shifted by `0x20000`. Fenestra live reads give
  `0x40000000 <- file 0x178E290` and `0x4002F100 <- file 0x17BD380`; the hand-off
  entry `0x40028FF8` is file `0x17B7288` and is **ARM**, not the Thumb loop used
  by the earlier ALT build.
- `bootrom-thumb` now defaults off (only for genuinely Thumb windows).

### Added

- VFP/CPACR enable in the assisted entry (the real boot ROM does this).
- `evidence/bootrom-correct-stage2.md`: corrected stage-2 writes
  `0xC0111450/54/58/5C` and `0xC0111880/84`, then undefines on a Thumb pointer
  at `0x400034B8`.
- Real reset trace: `0x1037CBA8` -> `0x1037CBB0`; `0x1037CC0C` is a MIDR gate
  that sends Cortex-A15 to WFI at `0x1037D040`.

## 0.1.4 — 2026-11-22

Reconstructed boot ROM loader and first stage-2 run.

### Added

- Machine options `bootrom=FILE`, `bootrom-tables=on`,
  `bootrom-tables-base=`, `bootrom-arg=`, and `bootrom-thumb=on`.
- `--bootrom` / `--boot-tables` / `--boot-tables-base` / `--boot-arg` in
  `tools/run.py`.
- `evidence/bootrom-alt-stage2.md`: ALT window (from `b1330` offsets
  `0x17AE290` + `0x17DD380`) executes real Thumb code at `0x40028FF8`.

### Result

- init runs with 0 exceptions using `[0x400E8450]=0x00708000` and LPAE tables at
  `0x40708000`;
- stage-2 enters `0x40028FF8 -> 0x40028FCC -> 0x10020010`, then data abort
  (`DFAR=0x2`): the window content is plausible, but the boot-ROM hand-off
  entry/relocation (and `r2`) is still missing.

## 0.1.3 — 2026-11-20

Display/GUI boundary and M-core status.

### Added

- `docs/display.md`: current execution ceiling (`0x40028FF8`), static display
  code evidence (`PLL_LCD`/`TFT`/`EGL`/`DMP`/`HDMI`/menu strings), the fact that
  D780's display blocks have zero hits in `b1330`, and what the host-side LCD
  dump can and cannot prove.
- `evidence/lcd-testcard.md` + `evidence/lcd-testcard.png`: a 320×240 ARGB test
  card proves the RAM→PPM path; it is host-written, not firmware UI.
- README/status now list the M-core track and the GUI/LCD boundary.

### Noted

- The M-core track (`nikon-d850-mcore`, `docs/mcore.md`/`.en.md`, 2026-11-18)
  was added after 0.1.2 without a changelog entry; it boots a1330 to idle
  `0xC05E` but has no found console TX or display path.

## 0.1.2 — 2026-11-16

Boot ROM size estimate and acquisition plan.

### Added

- `docs/bootrom.md`: distinguishes the `0x40000000` runtime window from the
  physical mask ROM; estimates ≥184 KiB from the vectors, ≥263 KiB from the
  in-image region table (`0x40000000+0x2F0D8`, `0x4002F100+0x12BA8 =
  0x40041CA8`), recommends a 512 KiB dump, and lists what each artifact alone
  can and cannot reach.
- README/docs now link the estimate and the recommended dump ranges.

## 0.1.1 — 2026-11-16

Open-source and legal documentation completion.

### Added

- `LEGAL.md`: bilingual legal/compliance notice (non-affiliation, firmware
  copyright/EULA, reverse engineering and anti-circumvention, no warranty,
  export control, privacy, third parties, notifications).
- `CONTRIBUTING.md` and `CONTRIBUTING.en.md`: prohibited-content list, DCO 1.1
  sign-off, testing and privacy rules.
- `SECURITY.md`: scope, private reporting, response expectations, good-faith
  research and the QEMU security-boundary warning.
- `CODE_OF_CONDUCT.md`, `.github/PULL_REQUEST_TEMPLATE.md` and issue templates
  with firmware/key warnings.
- Explicit copyright and GPL-2.0-or-later statements in `NOTICE.md` and the
  READMEs.

### Changed

- Expanded `NOTICE.md` with license scope, upstream attribution, third-party
  components and trademark/firmware boundaries.
- README license sections now link the legal, contribution and security docs.

## 0.1.0 — 2026-11-16

Initial D850 split from the D750-QEMU method.

### Added

- `nikon-d850` QEMU machine (QEMU 11.1.0 overlay), compiled and run.
- D850 profile defaults: `payload-base=0x10000000`,
  `entry=0x1037CBA8`, 32 MiB flash window, `cortex-a15`.
- Diagnostic handler RAM at `0x40000000` and the `e5-tables` LPAE identity
  fixture (two 4-entry level-1 tables at `0x40000000`/`0x40001000`).
- `tools/run.py` runner with exception/MMIO/halt summaries and
  `--init` / `--e5-tables` experiment switches.
- Host-side tests for the overlay installer and the runner parsers.
- `docs/status.md`, `docs/boot-chain.md`, `docs/build.md`, and four
  reproducible evidence pages in `evidence/`.

### Findings

- The D850 reset word `0x1037CBA8` is a boot-service sled, not a reset
  handler; with the assisted stack it pops to PC=0.
- The in-image init at `0x1035CE30` programs `TTBCR=0xB1003104` (EAE=1),
  `TTBR1=0x40000000`, `TTBR0=0x40001000`, enables the MMU
  (`SCTLR=0x1005`) and sets `VBAR=0x10020000`.
- Without boot-ROM LPAE tables the next fetch faults at `0x1035D5D4`;
  QEMU's table walk reads `0x40000000`/`0x40001000` with size 8.
- A correct LPAE identity fixture yields **0 exceptions**; the init runs to
  `bx ip` at `0x40028FF8`. The hand-off target is empty handler RAM, so the
  missing piece is the boot ROM code/services in the `0x40000000` bank.
- Cortex-A9 faults with an undefined instruction on the 64-bit TTBR `mcrr`
  at `0x1035D45C`, confirming the LPAE requirement.
