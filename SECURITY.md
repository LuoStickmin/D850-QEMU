# 安全策略 / Security Policy

## 支持范围 / Supported versions

| 版本 | 支持 |
| --- | --- |
| `main` 分支 | ✅ 接受安全报告 |
| 历史 commit / fork | ❌ 不保证 |

Only the `main` branch is supported. Historical commits and forks are not.

## 范围 / Scope

**属于本项目范围：**

- `qemu/nikon-d850-lab.c/.h` 的内存映射、DMA、越界、整数溢出、死循环等缺陷；
- `qemu/prepare_source.py` 对 QEMU 源码树的写入/覆盖问题；
- `tools/run.py` 的命令注入、路径处理、日志泄露；
- 文档中会导致用户误操作或泄露数据的错误。

**不属于本项目范围：**

- Nikon 固件本身的漏洞或相机安全问题——请联系 Nikon；
- QEMU 上游的通用漏洞——请按 QEMU 的安全流程报告，同时可告知我们 overlay
  是否触发；
- 需要真实相机、刷机或物理访问才能验证的问题；
- 社会工程、DoS 压测、针对第三方系统的攻击。

**In scope:** memory-mapping, DMA, bounds, integer-overflow or hang bugs in the
board overlay; unsafe writes by `prepare_source.py`; command/path handling or log
leakage in `tools/run.py`; documentation that could cause data loss or exposure.

**Out of scope:** Nikon firmware/camera vulnerabilities (contact Nikon); generic
QEMU vulnerabilities (report upstream, then tell us if the overlay triggers them);
issues requiring a physical camera, flashing, or third-party systems.

## 报告方式 / Reporting

首选 GitHub 的 **私密漏洞报告**（仓库 Security 标签页 → Report a vulnerability）。
如果该入口不可用，请开一个**不含利用细节**的 issue，仅说明“需要私密渠道”，维护者
会回复联系方式。

请包含：

- 受影响文件/版本；
- 最小复现（命令、输入类型；**不要附固件/密钥**）；
- 影响与触发条件；
- 你希望公开的时间（如有）。

**不要**在公开 issue 中粘贴固件、密钥、序列号、绝对路径、用户照片或可复现的
破坏步骤。

Prefer GitHub private vulnerability reporting (Security tab → Report a
vulnerability). If unavailable, open an issue **without exploit details** asking
for a private channel. Include affected files/version, a minimal reproduction
(command, input type; **no firmware/keys**), impact, and any disclosure timeline.
Do not post firmware, keys, serial numbers, absolute paths, personal photos, or
reproducible destructive steps publicly.

## 响应 / Response

维护者按尽力而为处理：通常 7 天内确认收到，30 天内给出初步评估。修复时间取决于
复杂度与上游 QEMU。本项目没有漏洞赏金。修复后会在 CHANGELOG 或 Security
Advisory 中致谢（除非你要求匿名）。

Best-effort: acknowledgement usually within 7 days, initial assessment within 30
days. Fix timing depends on complexity and upstream QEMU. No bug bounty. We will
credit you in the CHANGELOG or a Security Advisory unless you ask to remain
anonymous.

## 善意研究 / Good-faith research

在下列前提下，维护者不会因善意安全研究而追究：

- 只测试你拥有或获授权的系统；
- 不访问、修改或删除他人数据；
- 不进行拒绝服务、社会工程或针对第三方的攻击；
- 给维护者合理的修复时间，不提前公开利用细节；
- 遵守适用法律。

**这不是法律意见，也不构成对任何第三方的豁免。** 如果你不确定，请先咨询律师。

The maintainers will not pursue good-faith research that: tests only systems you
own or are authorized to test; does not access, alter or delete others' data;
does not conduct DoS, social engineering or attacks on third parties; allows
reasonable fix time before disclosure; and complies with applicable law. **This
is not legal advice and does not bind any third party.** Consult a lawyer if
unsure.

## 安全边界 / Security boundary

QEMU 是功能模拟器，不是沙箱或安全边界。运行不可信固件可能触发 QEMU 自身缺陷。
请在隔离环境（无敏感数据、无网络、可丢弃）中运行，并及时更新 QEMU。

QEMU is a functional emulator, not a sandbox or security boundary. Running
untrusted firmware may trigger bugs in QEMU. Run it in a disposable, isolated
environment without sensitive data or network access, and keep QEMU updated.

## 相关 / See also

- [LEGAL.md](LEGAL.md) — 法律与合规
- [CONTRIBUTING.md](CONTRIBUTING.md) — 贡献边界
- [NOTICE.md](NOTICE.md) — 许可与第三方
