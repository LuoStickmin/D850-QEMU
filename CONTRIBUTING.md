# 贡献指南 / Contributing

> 本文是项目规则与法律边界，**不是法律意见**。如果你计划分发、刷机或在商业环境
> 使用，请咨询你所在司法辖区的律师。完整风险说明见 [LEGAL.md](LEGAL.md)。

## 1. 项目是什么

本仓库是独立的 D850（EXPEED 5 / E5）固件互操作性与启动链验证研究：一个 QEMU
overlay 机器、一个运行/取证工具，以及基于真实字节的启动链结论。它与 Nikon、
WonderLantern 均无隶属、授权或背书关系。

## 2. 绝对不要提交的内容（硬性禁止）

无论以附件、patch、测试 fixture、issue 粘贴还是 Git 历史的形式，以下内容
**一律不接受**：

1. **Nikon 固件**：`.bin`、更新包、解密后的模块、完整镜像或可运行的大段代码；
2. **密钥/密钥表**：`key_all.json`、K8、T1/T2/T3、解密器输出等；
3. **boot ROM、SPI flash dump、相机 RAM dump**，或其中的代码/数据/校验表；
4. **Nikon 版权素材**：字体、图标、UI 文本/翻译、菜单资源、示例照片、相机截图；
5. **反编译/反汇编得到的成段专有代码**，或明显复制自泄露源码的代码；
6. **可直接刷机的改后镜像**、签名/校验绕过工具、更新器绕过方法；
7. **个人数据**：序列号、相机内部 ID、用户照片、位置、账号信息；
8. 任何违反当地法律（包括但不限于 DMCA §1201、EU 2009/24/EC、日本/中国相关
   法规）的内容，或针对第三方设备的攻击/未授权访问方法。

> 本仓库的工具只读取**你自己提供的本地文件**，不下载固件、不附带密钥、不生成
> 可刷写镜像。提交代码时也不要附带这些输入。

## 3. 可以提交的内容

- 你自己编写的 C/Python/文档/测试；
- 地址、偏移、哈希、短小的数值常量等**事实性**信息；
- 基于公开规格、可观察行为或你自己设备上的实验得出的分析；
- 不含固件内容的 bug 报告、复现步骤、日志（先删除个人信息与绝对路径）；
- 对文档、构建、测试、可移植性的改进。

## 4. 许可与 DCO

- 除文件头另有说明外，贡献按 **GPL-2.0-or-later** 授权。
- 提交 PR 即表示你同意
  [Developer Certificate of Origin 1.1](https://developercertificate.org/)，
  请在 commit message 中加入：

  ```text
  Signed-off-by: Your Name <you@example.com>
  ```

- 不要提交你无权授权的代码、文档或素材；不要移除或修改上游版权头。
- 贡献不得附带额外的法律限制；GPL 授予的权利必须保持完整。

## 5. 开发与测试

```sh
python tests/run_tests.py
```

要求：

- 新增/修改 Python 需兼容 **3.10+**，只使用标准库；
- 新增/修改 C 需在 QEMU 11.1.0 上能通过
  `python qemu/prepare_source.py <qemu> && ninja -C <qemu>/build qemu-system-arm`；
- 行为变化需更新 `docs/` 与 `CHANGELOG.md`；
- 新增链接需能通过 `tests/test_links.py`；
- 保持 `ruff`/`flake8` 级别的基本整洁（无未使用导入、无裸 `except` 吞错）。

## 6. 证据与隐私

- 不要提交 `runs/`、`session.json` 或未脱敏的 `qemu.log`；
- 截图前删除本机用户名、绝对路径、序列号、IP 等；
- 需要展示日志时，使用 `evidence/` 的格式：命令、结果、必要摘录、解读；
- 大二进制先讨论；仓库目标是保持轻量、可审计。

## 7. 贡献者声明

提交贡献即表示你确认：

1. 你有权按 GPL-2.0-or-later 授权该贡献；
2. 贡献是原创的，或来自许可兼容的来源并已正确署名；
3. 贡献不含固件、密钥、dump、泄露代码、个人信息或无权授权的素材；
4. 你理解本项目不提供法律意见，也不会因合并贡献而承担你的法律风险。

## 8. 行为与安全

- 参与即表示同意 [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)；
- 发现安全问题时不要公开可复现的破坏步骤，走 [SECURITY.md](SECURITY.md)；
- 维护者可以以“法律/证据不足/范围不符”为由拒绝贡献，无需给出完整解释。

## 9. 参考

- [LEGAL.md](LEGAL.md) — 法律与合规说明
- [NOTICE.md](NOTICE.md) — 许可、上游、第三方
- [SECURITY.md](SECURITY.md) — 漏洞报告
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) — 行为准则
- [README.md](README.md) — 项目与运行方式
