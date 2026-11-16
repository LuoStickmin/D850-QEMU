## 变更 / What changed

<!-- 简要说明改了什么、为什么。 -->

## 验证 / Verification

<!-- 命令、输出、测试结果；日志请先脱敏。 -->

```text
$ python tests/run_tests.py
...
```

## 检查清单 / Checklist

- [ ] 不含 Nikon 固件、密钥、K8/T1/T2/T3、boot ROM、flash/RAM dump、
      解密模块或可刷写改后镜像 / no firmware, keys, dumps or flashable images
- [ ] 不含泄露代码、Nikon 字体/图标/UI 素材、个人信息或无权授权的素材 /
      no leaked code, Nikon assets, personal data or unlicensed material
- [ ] `python tests/run_tests.py` 通过 / tests pass
- [ ] 行为变化已更新 `docs/` 与 `CHANGELOG.md` / docs and changelog updated
- [ ] 新增文件带 SPDX 头（代码）/ SPDX headers on new code files
- [ ] commit 含 `Signed-off-by:`（DCO 1.1）/ DCO sign-off present
- [ ] 新增链接通过 `tests/test_links.py` / links resolve
- [ ] 我已阅读并同意 [CONTRIBUTING.md](../CONTRIBUTING.md) 与
      [LEGAL.md](../LEGAL.md) / I have read CONTRIBUTING and LEGAL

## 备注 / Notes

<!-- 可选：假设、已知限制、后续工作。 -->
