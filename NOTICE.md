# NOTICE

**D850-QEMU** — Nikon D850 (EXPEED 5 / E5) firmware boot-chain validation
prototype for QEMU.

Copyright (C) 2026 LuoStickmin and D850-QEMU contributors.
Portions derived from D750-QEMU, Copyright (C) 2026 LuoStickmin and D750-QEMU contributors.

SPDX-License-Identifier: **GPL-2.0-or-later**

---

## 1. 许可 / License

除另有明确标注外，本仓库的原创内容——`qemu/` 下的板级 overlay、`tools/`、
`tests/`、`docs/`、`evidence/` 与根目录文档——按
**GNU General Public License v2.0 或任何更晚版本（GPL-2.0-or-later）** 授权。
完整 GPLv2 文本见 [LICENSE](LICENSE)；SPDX 头与本声明给出“或更晚版本”的选择权。

Except where explicitly marked otherwise, the original content of this
repository — the board overlay under `qemu/`, `tools/`, `tests/`, `docs/`,
`evidence/` and the root documents — is licensed under the **GNU General Public
License version 2.0 or any later version (GPL-2.0-or-later)**. The full GPLv2
text is in [LICENSE](LICENSE); the SPDX headers and this notice grant the
"or later" option.

本仓库不内嵌 QEMU 源码；QEMU 按你构建所用源码树的上游许可（整体 GPLv2）分发。
This repository does not vendor QEMU sources; QEMU is distributed under the
license of the source tree you build against (GPLv2 overall).

## 2. 上游与致谢 / Upstream and credits

- 方法、板级代码骨架与工具约定源自作者自己的
  [D750-QEMU](https://github.com/LuoStickmin/D750-QEMU)
  （GPL-2.0-or-later）。
- 组织方式参考 WonderLantern 的
  [z7-qemu](https://github.com/WonderLantern/z7-qemu) 与
  [z9-qemu](https://github.com/WonderLantern/z9-qemu)；仅参考方法，未复制代码。
- D850 专属的向量/异常表分析、`0x1035CE30` init 分析、`--e5-tables` LPAE
  夹具、`0x40028FF8` 交接结论、文档与证据，是在本仓库完成的。
- The method, board-code skeleton and tooling conventions derive from the
  author's own [D750-QEMU](https://github.com/LuoStickmin/D750-QEMU)
  (GPL-2.0-or-later).
- Organisation is inspired by WonderLantern's
  [z7-qemu](https://github.com/WonderLantern/z7-qemu) and
  [z9-qemu](https://github.com/WonderLantern/z9-qemu); method only, no code
  copied.
- The D850-specific vector/exception analysis, the `0x1035CE30` init analysis,
  the `--e5-tables` LPAE fixture, the `0x40028FF8` hand-off finding, docs and
  evidence were produced for this repository.

## 3. 第三方组件 / Third-party components

- 工具与测试只使用 **Python 标准库**；不捆绑 NumPy、Capstone 或其他第三方包。
- C overlay 在构建时链接 QEMU；QEMU 及其子项目保留各自的版权与许可。
- 仓库中的第三方名称、链接与项目归其各自作者所有；引用不代表背书。
- The tools and tests use only the **Python standard library**; no NumPy,
  Capstone or other third-party packages are bundled.
- The C overlay links against QEMU at build time; QEMU and its subprojects retain
  their own copyright and licenses.
- Third-party names, links and projects belong to their authors; references are
  not endorsements.

## 4. 固件、密钥与商标 / Firmware, keys and trademarks

- 本仓库**不包含完整** Nikon 固件、解密模块、密钥、K8/T1/T2/T3 表、boot ROM、
  flash/RAM dump、可刷写改后镜像、Nikon 字体/图标/UI 素材；文档只引用说明所需的
  短指令字/地址。
- 固件版权归 Nikon Corporation 或其许可方；你只能使用合法取得且有权研究的镜像。
- “Nikon”“D850”“EXPEED”等是各自权利人的商标，仅用于指名性说明研究对象。
- 本项目与 Nikon 无关联，未获授权、赞助或背书。
- This repository **contains no full** Nikon firmware, decrypted module, key,
  K8/T1/T2/T3 table, boot ROM, flash/RAM dump, flashable modified image, or
  Nikon font/icon/UI asset; the documentation quotes only short instruction
  words/addresses needed for explanation.
- Firmware is copyrighted by Nikon Corporation or its licensors; use only images
  you lawfully obtained and are entitled to study.
- "Nikon", "D850", "EXPEED" and related marks belong to their owners and are used
  only descriptively.
- This project is not affiliated with, authorized, sponsored or endorsed by Nikon.

## 5. 无担保与责任 / No warranty and liability

代码按 GPL-2.0-or-later 的“AS IS”条款提供，不附带任何明示或默示担保。在法律
允许的最大范围内，维护者与贡献者不对设备损坏、变砖、数据丢失、保修失效、更新
失败、第三方索赔、法律后果或任何间接/后果性损害承担责任。详见
[LEGAL.md](LEGAL.md) 与 GPLv2 第 15、16 条。

The code is provided "AS IS" under GPL-2.0-or-later, without express or implied
warranty. To the maximum extent permitted by law, the maintainers and
contributors are not liable for device damage, bricking, data loss, voided
warranty, failed updates, third-party claims, legal consequences, or any
indirect or consequential damages. See [LEGAL.md](LEGAL.md) and GPLv2 §§15–16.

## 6. 法律与贡献边界 / Legal and contribution boundary

- 本文不是法律意见；逆向工程、解密、反规避、出口管制与 EULA 的适用因司法辖区
  而异，请咨询律师。见 [LEGAL.md](LEGAL.md)。
- 贡献按 GPL-2.0-or-later 授权，并要求 DCO 1.1 签名；不得提交固件、密钥、
  dump、泄露代码或无权授权的素材。见 [CONTRIBUTING.md](CONTRIBUTING.md)。
- 安全问题请走私密渠道。见 [SECURITY.md](SECURITY.md)。
- This is not legal advice; reverse engineering, decryption, anti-circumvention,
  export control and EULA questions vary by jurisdiction—consult a lawyer. See
  [LEGAL.md](LEGAL.md).
- Contributions are licensed GPL-2.0-or-later and require DCO 1.1 sign-off; do
  not submit firmware, keys, dumps, leaked code, or material you cannot license.
  See [CONTRIBUTING.md](CONTRIBUTING.md).
- Report security issues privately. See [SECURITY.md](SECURITY.md).

## 7. 联系 / Contact

许可、版权或本声明的问题：通过仓库 issue 联系维护者，并先阅读
[LEGAL.md](LEGAL.md)。安全问题请使用 [SECURITY.md](SECURITY.md) 的私密渠道。

License, copyright or notice questions: contact the maintainers via the
repository issues after reading [LEGAL.md](LEGAL.md). For security, use the
private channel in [SECURITY.md](SECURITY.md).
