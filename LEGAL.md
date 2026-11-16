# 法律声明 / Legal Notice

> **本文档是项目维护者的合规与风险说明，不是法律意见，也不保证任何具体使用合法。**
> 在下载、使用、修改、再分发或依赖本项目之前，请咨询你所在司法辖区的律师，并自行判断。
>
> **This document is the maintainers' compliance and risk note. It is not legal
> advice and does not guarantee that any particular use is lawful.** Consult a
> lawyer in your jurisdiction and make your own assessment before use.

本文适用于本仓库的代码、文档、测试、运行证据，以及由本项目生成的任何分析产物。
This notice applies to the code, documentation, tests, run evidence and any
analysis output of this repository.

---

## 1. 非官方项目、商标与关联 / No affiliation, trademarks

本项目是独立的互操作性/固件验证研究项目，**与 Nikon Corporation 及其关联公司
没有任何关联，未获其授权、赞助、认可或背书**。Nikon、D850、EXPEED 以及相关机型
名称是各自权利人的商标，仅用于**指名性说明**研究对象与兼容性（nominative fair
use），不表示商标权属，也不表示任何官方关系。本仓库不使用 Nikon 的 Logo、图标、
字体或 UI 素材。

This is an independent interoperability and firmware-validation research project.
It is **not affiliated with, authorized by, sponsored by, or endorsed by Nikon
Corporation** or its affiliates. Nikon, D850, EXPEED and related model names are
trademarks of their respective owners and are used only descriptively to identify
the studied hardware and compatibility. No trademark ownership or official
relationship is claimed. No Nikon logos, icons, fonts or UI assets are included.

**本仓库不授予任何 Nikon 固件、商标、专利、商业秘密或其他第三方权利的许可。**
**Nothing here grants a license to Nikon firmware, trademarks, patents, trade
secrets, or any other third-party rights.**

## 2. 固件版权、EULA 与获取 / Firmware copyright, EULA, acquisition

相机固件镜像的版权归 Nikon Corporation 或其许可方所有，并可能受最终用户许可协议
（EULA）、更新服务条款、保修条款或其他合同约束。**本仓库不包含、也不下载任何
Nikon 固件镜像、解密后模块、密钥、签名材料、boot ROM、flash dump 或改后镜像。**
`.gitignore` 排除了运行目录；`evidence/` 只保存偏移、地址、异常与 MMIO 摘要等
事实性数据，不含固件字节。

Camera firmware images are copyrighted by Nikon Corporation or its licensors and
may be subject to end-user license agreements (EULA), update-service terms,
warranty terms, or other contracts. **This repository contains and downloads no
Nikon firmware image, decrypted module, key, signature material, boot ROM, flash
dump, or modified image.** `runs/` is ignored; `evidence/` contains only factual
data such as offsets, addresses, exception and MMIO summaries, not firmware bytes.

你只能使用你**合法取得、且有权研究**的镜像。固件 EULA 可能限制逆向工程、解密或
修改；是否违反取决于你与 Nikon 之间的合同及你所在司法辖区的法律。**本项目不
鼓励、不协助、也不提供任何违反 EULA 或规避技术保护措施的步骤。**

Use only images you obtained lawfully and are entitled to study. A firmware EULA
may restrict reverse engineering, decryption, or modification; whether a given
act breaches it depends on your contract with Nikon and your local law. **This
project does not encourage, assist with, or provide steps to breach a EULA or
circumvent technical protection measures.**

## 3. 逆向工程、解密与反规避 / Reverse engineering, decryption, anti-circumvention

固件分析、解密、修改和重新封装可能涉及你所在司法辖区的著作权、反规避、计算机
访问、反不正当竞争、出口管制或其他法律。以下仅是**示例，不是穷举，也不表示任何
例外一定适用于你**：

- 美国《数字千年版权法》（DMCA）第 1201 条，及其中的互操作性例外
  （17 U.S.C. §1201(f)）；该例外有严格条件，不是概括许可；
- 美国著作权法中的合理使用（17 U.S.C. §107）；
- 欧盟《软件指令》2009/24/EC 第 5(3) 条（观察、研究、测试）与第 6 条
  （为互操作性进行的反编译），两者均有条件；
- 日本《不正競争防止法》（不正当竞争防止法）中关于技术限制手段规避的规定，以及《著作权法》
  第 30 条之 4（分析）与第 47 条之 4（程序分析）；
- 中国《著作权法》关于技术措施与合理使用的规定、《反不正当竞争法》及
  其他相关法规；
- 其他国家和地区的类似规定。

Firmware analysis, decryption, modification and repacking may implicate copyright,
anti-circumvention, computer-access, unfair-competition, export-control or other
laws in your jurisdiction. The list below is **illustrative, not exhaustive**, and
does not imply that any exception applies to you:

- Section 1201 of the U.S. Digital Millennium Copyright Act (DMCA), including the
  interoperability exception at 17 U.S.C. §1201(f), which has strict conditions
  and is not a blanket permission;
- fair use under 17 U.S.C. §107;
- Article 5(3) (observation, study, testing) and Article 6 (decompilation for
  interoperability) of the EU Software Directive 2009/24/EC, both conditional;
- Japan's Unfair Competition Prevention Act provisions on circumvention of
  technical restriction measures, and Copyright Act Article 30-4 (analysis) and
  Article 47-4 (program analysis);
- China's Copyright Law provisions on technical measures and fair use, the
  Anti-Unfair Competition Law, and related regulations;
- similar provisions elsewhere.

本项目的目的是**在自己合法拥有的设备与本地取得的固件上进行互操作性、格式与
启动链研究**。它不提供盗版、付费功能绕过、签名/校验绕过、刷机绕过或任何规避
技术保护措施的教程。**维护者不对任何具体使用的合法性作出陈述或保证。**

The project is intended for **interoperability, format and boot-chain research on
devices you lawfully own and firmware you obtained locally**. It provides no
piracy assistance, paid-feature bypass, signature/verification bypass, flashing
bypass, or tutorial for circumventing technical protection measures. **The
maintainers make no representation or warranty about the legality of any
particular use.**

## 4. 本项目不提供什么 / What this project does not provide

本仓库明确**不包含、不提供**：

1. Nikon 固件、更新包、解密模块或其中任何片段；
2. 解密密钥、K8 密钥、T1/T2/T3 表或类似密钥材料；
3. boot ROM、SPI flash dump、相机 RAM dump 或整片镜像；
4. 可刷写的改后镜像、签名工具、校验绕过或更新器绕过；
5. 相机 USB/PTP 刷机工具或任何实机写入流程；
6. Nikon 的字体、图标、UI 文本、菜单资源、示例照片或相机截图；
7. 从泄露源码、盗版渠道或无权来源获得的代码/素材；
8. 针对第三方设备的攻击、破坏或未授权访问方法。

This repository explicitly **does not contain or provide**:

1. Nikon firmware, update packages, decrypted modules, or any fragment thereof;
2. decryption keys, K8 keys, T1/T2/T3 tables, or similar key material;
3. boot ROM, SPI flash dumps, camera RAM dumps, or full images;
4. flashable modified images, signing tools, verification bypass, or updater bypass;
5. camera USB/PTP flashing tools or any real-device write procedure;
6. Nikon fonts, icons, UI text, menu resources, sample photos, or camera screenshots;
7. code or assets from leaks, piracy channels, or sources you are not entitled to use;
8. attack, damage, or unauthorized-access methods against third-party devices.

如果你发现仓库历史、issue、PR 或 release 中意外包含了上述内容，请立即通过
[SECURITY.md](SECURITY.md) 联系维护者，维护者会尽快移除。

If you find any of the above accidentally present in history, issues, pull
requests or releases, contact the maintainers via [SECURITY.md](SECURITY.md); the
maintainers will remove it promptly.

## 5. 无担保与责任限制 / No warranty and limitation of liability

代码按 **GPL-2.0-or-later** 的“AS IS”条款提供，不附带任何明示或默示担保，包括
但不限于适销性、特定用途适用性、权利完整性、不侵权和准确性。**在法律允许的
最大范围内**，维护者与贡献者不对以下事项承担责任：相机损坏、变砖、数据/照片/
视频丢失、保修或服务失效、更新失败、利润损失、业务中断、第三方索赔、法律后果，
以及任何间接、附带、特殊或后果性损害。

The code is provided "AS IS" under **GPL-2.0-or-later**, without any express or
implied warranty, including merchantability, fitness for a particular purpose,
title, non-infringement, or accuracy. **To the maximum extent permitted by law**,
the maintainers and contributors are not liable for camera damage, bricking,
loss of data/photos/videos, voided warranty or service, failed updates, lost
profits, business interruption, third-party claims, legal consequences, or any
indirect, incidental, special or consequential damages.

修改固件、刷入改后镜像、或依赖本项目结论，均**风险自负**。本项目不提供可刷写
镜像，也不保证任何重建产物会被相机接受。刷机前请保留原厂固件与数据备份，并
确认存在恢复路径。

Modifying firmware, flashing modified images, or relying on this project's
findings is **at your own risk**. This project provides no flashable image and
does not guarantee that any rebuilt artifact will be accepted by a camera. Keep
an original firmware copy and data backup, and verify a recovery path before
flashing anything.

QEMU 是功能模拟器，**不是安全边界**。运行来路不明或畸形的固件可能触发 QEMU
自身的缺陷。请在没有敏感数据的隔离环境中运行，并及时更新 QEMU。

QEMU is a functional emulator, **not a security boundary**. Running untrusted or
malformed firmware may trigger bugs in QEMU itself. Run it in an isolated
environment without sensitive data, and keep QEMU updated.

## 6. 出口管制与制裁 / Export control and sanctions

密码分析、逆向工程和固件工具可能受出口管制、再出口限制、进口限制或制裁法规
约束（例如美国 EAR、欧盟双用途条例、日本外汇及外贸法，以及其他国家/地区的
类似规定）。**维护者不对本项目的出口分类或制裁合规作出任何陈述。** 你应自行
确认下载、使用、再出口或再分发本项目不违反适用于你的法律。

Cryptanalysis, reverse-engineering and firmware tools may be subject to export
controls, re-export or import restrictions, or sanctions (for example the U.S.
EAR, the EU dual-use Regulation, Japan's Foreign Exchange and Foreign Trade Act,
and similar rules elsewhere). **The maintainers make no representation about the
export classification or sanctions compliance of this project.** You are
responsible for confirming that downloading, using, re-exporting or
redistributing it does not violate laws applicable to you.

## 7. 隐私与本地数据 / Privacy and local data

本项目不收集、不上传、不包含遥测。`tools/run.py` 在本机运行，但会在
`runs/<时间戳>/session.json` 中记录：

- 固件的**绝对路径**、大小与 SHA-256；
- 实际 QEMU 命令行；
- `qemu.log`/`uart.log` 与解析后的异常、MMIO 摘要。

这些内容可能暴露本机用户名、目录结构或研究材料。`.gitignore` 已忽略 `runs/`；
**不要在 issue、PR 或截图中直接粘贴未脱敏的 `session.json` 或完整日志**。
`evidence/` 页面使用相对路径与固件哈希，不包含本机绝对路径。

This project collects no telemetry and uploads nothing. `tools/run.py` runs
locally but writes to `runs/<timestamp>/session.json`:

- the firmware's **absolute path**, size and SHA-256;
- the exact QEMU command line;
- `qemu.log`/`uart.log` and parsed exception/MMIO summaries.

These may reveal local usernames, directory layout or research material. `runs/`
is git-ignored. **Do not paste unredacted `session.json` or full logs into issues,
pull requests, or screenshots.** `evidence/` pages use relative paths and the
firmware hash, not local absolute paths.

通过 GitHub 访问或参与本仓库时，GitHub 按其自身隐私政策处理数据。
When you access or participate through GitHub, GitHub processes data under its
own privacy policy.

## 8. 第三方、上游与许可 / Third parties, upstream, licensing

- 本仓库的板级 overlay、工具、测试与文档按 **GPL-2.0-or-later** 授权；
  完整 GPLv2 文本见 [LICENSE](LICENSE)。SPDX 头与本声明共同给出“或任何更晚
  版本”的选择权。
- 本仓库的方法与部分板级代码源自作者自己的
  [D750-QEMU](https://github.com/LuoStickmin/D750-QEMU)，同样为
  GPL-2.0-or-later；D850 专属的分析、夹具与结论是在此基础上的新工作。
- QEMU 本身按其上游许可（整体 GPLv2）分发；本仓库**不内嵌** QEMU 源码，
  只提供 overlay 与安装脚本。
- 本仓库不捆绑第三方 Python 包；工具只使用 Python 标准库。
- WonderLantern 的 z7-qemu/z9-qemu 等仅作为方法参考，未复制其代码。
- 第三方链接、名称与项目归其各自作者所有；链接不代表背书。

- The board overlay, tools, tests and documentation are licensed under
  **GPL-2.0-or-later**; the full GPLv2 text is in [LICENSE](LICENSE). The SPDX
  headers and this notice grant the "or any later version" option.
- The method and parts of the board code derive from the author's own
  [D750-QEMU](https://github.com/LuoStickmin/D750-QEMU), also
  GPL-2.0-or-later; the D850-specific analysis, fixtures and findings are new
  work on top of it.
- QEMU is distributed under its upstream license (GPLv2 overall). This
  repository **does not vendor** QEMU sources; it only supplies an overlay and
  an installer.
- No third-party Python packages are bundled; the tools use only the standard
  library.
- WonderLantern's z7-qemu/z9-qemu are referenced for method only; no code was
  copied.
- Third-party links, names and projects belong to their authors; links are not
  endorsements.

### 8.1 专利 / Patents

本仓库**不授予任何专利许可**，也不保证任何实现不侵犯第三方专利。GPL-2.0-or-later
的专利条款（如适用）仅来自该许可证本身及贡献者；维护者不对专利状态、自由实施
（freedom to operate）或产品认证作任何陈述。

No patent license is granted by this repository, and no representation is made
that any implementation avoids third-party patents. Any patent terms come solely
from GPL-2.0-or-later (if applicable) and the contributors; the maintainers make
no statement about patent status, freedom to operate, or product certification.

**就受 GPL 许可的代码而言，如果本声明与 [LICENSE](LICENSE) 冲突，以 LICENSE
为准。** 关于固件、商标、专利和第三方材料的限制独立于代码许可，不因 GPL 而
改变。

**For code licensed under the GPL, [LICENSE](LICENSE) controls if this notice
conflicts with it.** Restrictions concerning firmware, trademarks, patents and
third-party material are independent of the code license and are not changed by
the GPL.

## 9. 贡献与 DCO / Contributions and DCO

提交贡献即表示你有权按 GPL-2.0-or-later 授权该贡献，并同意
[Developer Certificate of Origin 1.1](https://developercertificate.org/)；
请在 commit 中加入 `Signed-off-by: Name <email>`。**不要提交固件、密钥、
dump、解密模块、可刷写镜像、泄露代码、个人信息或无权授权的素材。**
详见 [CONTRIBUTING.md](CONTRIBUTING.md)。

By contributing, you represent that you are entitled to license the contribution
under GPL-2.0-or-later and agree to the
[Developer Certificate of Origin 1.1](https://developercertificate.org/); add
`Signed-off-by: Name <email>` to your commits. **Do not submit firmware, keys,
dumps, decrypted modules, flashable images, leaked code, personal data, or
material you are not entitled to license.** See [CONTRIBUTING.md](CONTRIBUTING.md).

## 10. 通知、下架与联系 / Notices, takedown, contact

如果你是权利人，认为本仓库的某部分侵犯了你的权利，请通过仓库 issue 或
[SECURITY.md](SECURITY.md) 中的私密渠道联系维护者，说明具体文件、权利依据与
期望的处理方式。维护者会在合理时间内审阅，并在有依据时移除或修改相关内容、
保留处理记录。**提交通知不等于承认侵权；维护者保留反驳与恢复内容的权利。**

If you are a rights holder and believe part of this repository infringes your
rights, contact the maintainers through the repository issues or the private
channel in [SECURITY.md](SECURITY.md), identifying the specific material, the
basis of your claim, and the requested action. The maintainers will review in
reasonable time and, where warranted, remove or modify the material and keep a
record. **Submitting a notice is not an admission of infringement; the
maintainers reserve the right to contest and to restore material.**

许可、版权或本声明的问题，请先联系维护者，而不是直接向平台提交滥用报告。
For license, copyright or notice questions, contact the maintainers first rather
than filing a platform abuse report directly.

## 11. 可分割性与完整说明 / Severability and entire notice

如果本声明的任何部分被认定为不可执行，其余部分仍在法律允许的最大范围内有效。
本声明不改变、也不额外限制 GPL 授予你的代码权利；它只是风险与合规说明。本声明
不构成法律意见，也不构成对任何具体使用的许可或保证。本声明**不选择准据法**，
也不排除强制性法律：受 GPL 许可的代码由 GPL 管辖，其他事项适用你所在司法辖区
的法律；当地强制性法律优先于任何冲突表述。

If any part of this notice is held unenforceable, the remainder remains effective
to the maximum extent permitted by law. This notice does not change or further
restrict the rights the GPL grants you in the code; it is a risk and compliance
note. It is not legal advice and is not a license or guarantee for any specific
use. This notice **does not select a governing law** and does not exclude
mandatory law: GPL-licensed code is governed by the GPL, other matters are
governed by applicable law in your jurisdiction, and mandatory local law prevails
over any conflicting statement.

---

**版权 / Copyright:** Copyright (C) 2026 LuoStickmin and D850-QEMU contributors.
Portions derived from D750-QEMU, Copyright (C) 2026 LuoStickmin and D750-QEMU contributors.
