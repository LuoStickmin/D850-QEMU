# Contributing

> This document states project rules and legal boundaries. It is **not legal
> advice**. Consult a lawyer in your jurisdiction before redistributing,
> flashing, or using this project commercially. See [LEGAL.md](LEGAL.md).

## 1. What this project is

An independent D850 (EXPEED 5 / E5) firmware interoperability and boot-chain
validation study: a QEMU overlay machine, a runner/evidence tool, and conclusions
derived from real bytes. It is not affiliated with, authorized by, or endorsed by
Nikon or WonderLantern.

## 2. Never submit (hard rule)

The following are **not accepted** in attachments, patches, test fixtures, issue
pastebins, or Git history:

1. **Nikon firmware**: `.bin`, update packages, decrypted modules, or any full
   image fragment;
2. **keys/key tables**: `key_all.json`, K8, T1/T2/T3, decryptor output;
3. **boot ROM, SPI flash dumps, camera RAM dumps**, or their code/data/tables;
4. **Nikon copyrighted material**: fonts, icons, UI text/translations, menu
   resources, sample photos, camera screenshots;
5. **bulk proprietary code** from decompilation/disassembly, or code copied from
   leaked sources;
6. **flashable modified images**, signing/verification bypass tools, or updater
   bypass methods;
7. **personal data**: serial numbers, internal camera IDs, user photos,
   locations, account information;
8. anything violating local law (including but not limited to DMCA §1201, EU
   2009/24/EC, applicable Japanese/Chinese rules), or attack/unauthorized-access
   methods against third-party devices.

> The tools read only **local files you provide**; they do not download firmware,
> ship keys, or produce flashable images. Do not attach such inputs to code.

## 3. What is welcome

- Your own C/Python/docs/tests;
- factual data: addresses, offsets, hashes, short numeric constants;
- analysis based on public specifications, observable behaviour, or experiments on
  your own device;
- bug reports without firmware content, reproduction steps, sanitized logs;
- documentation, build, test and portability improvements.

## 4. License and DCO

- Unless a file header says otherwise, contributions are licensed
  **GPL-2.0-or-later**.
- Opening a PR means you agree to the
  [Developer Certificate of Origin 1.1](https://developercertificate.org/); add

  ```text
  Signed-off-by: Your Name <you@example.com>
  ```

  to your commits.
- Do not submit code/assets you cannot license; do not remove or alter upstream
  copyright headers.
- Contributions must not add legal restrictions; GPL-granted rights stay intact.

## 5. Development and tests

```sh
python tests/run_tests.py
```

Requirements:

- Python changes must support **3.10+** and use only the standard library;
- C changes must build on QEMU 11.1.0 via
  `python qemu/prepare_source.py <qemu> && ninja -C <qemu>/build qemu-system-arm`;
- behaviour changes update `docs/` and `CHANGELOG.md`;
- new links must pass `tests/test_links.py`;
- keep the code tidy (no unused imports, no bare `except` that swallows errors).

## 6. Evidence and privacy

- Do not commit `runs/`, `session.json`, or unredacted `qemu.log`;
- remove local usernames, absolute paths, serial numbers and IPs before posting
  screenshots;
- present logs in the `evidence/` style: command, result, minimal excerpt,
  interpretation;
- discuss large binaries first; the repository is meant to stay small and
  auditable.

## 7. Contributor representations

By contributing you confirm that:

1. you are entitled to license the contribution under GPL-2.0-or-later;
2. it is original, or comes from a license-compatible source with attribution;
3. it contains no firmware, keys, dumps, leaked code, personal data, or material
   you cannot license;
4. you understand this project gives no legal advice and does not assume your
   legal risk by merging a contribution.

## 8. Conduct and security

- Participation implies agreement to [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md);
- do not publish reproducible destructive steps for security issues; use
  [SECURITY.md](SECURITY.md);
- maintainers may decline contributions for legal, evidentiary or scope reasons
  without a full explanation.

## 9. References

- [LEGAL.md](LEGAL.md) — legal and compliance note
- [NOTICE.md](NOTICE.md) — license, upstream, third parties
- [SECURITY.md](SECURITY.md) — vulnerability reporting
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) — conduct
- [README.md](README.md) — project and usage
