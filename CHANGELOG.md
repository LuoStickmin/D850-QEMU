# Changelog

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
