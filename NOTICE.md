# NOTICE

D850-QEMU is an experimental validation prototype for locally supplied,
already decrypted Nikon D850 firmware. It is **not** a Nikon product and is
not endorsed by Nikon Corporation.

## Upstream and lineage

- The board overlay, runner and documentation method are derived from
  [D750-QEMU](https://github.com/LuoStickmin/D750-QEMU)
  (`GPL-2.0-or-later`), which in turn follows the organisation of
  WonderLantern's `z7-qemu` / `z9-qemu` projects.
- The D850-specific constants, LPAE init analysis, `--e5-tables` fixture and
  the `0x40028FF8` hand-off finding were produced for this repository.
- QEMU itself remains under its own upstream licence; this repository does not
  vendor QEMU sources. See the QEMU tree you build against for its `COPYING`.

## Firmware and keys

- No Nikon firmware image, decryption key, K8 key, signature or modified image
  is included in this repository or its Git history.
- `tools/run.py` and the machine model only read the firmware path supplied by
  the user; they never modify it.
- "D850", "EXPEED" and other Nikon marks are trademarks of Nikon Corporation,
  used only to describe the compatibility target.

## Safety

- This repository provides **no flashing procedure** and no upgrade container
  handling. Running a guest firmware image in QEMU does not imply that a camera
  will accept it.
- The addresses and register accesses recorded here are hypotheses supported by
  observed bytes and runs; they are not a board schematic.

## Licence

The board overlay (`qemu/`), tools (`tools/`), tests (`tests/`) and this
repository's documentation are licensed under **GPL-2.0-or-later**. See
[LICENSE](LICENSE).
