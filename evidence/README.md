# 证据索引（2026-11-16）

所有会话均由 `tools/run.py` 生成；固件为本地提供的 `b1330_131.bin`
（D850 C:1.31，SHA-256 `97E1DA12…204003`），仓库不附带固件。

| 文件 | 内容 | 关键结果 |
|---|---|---|
| [d850-reset.md](d850-reset.md) | 从 reset 字 `0x1037CBA8` 起跑 | 0 异常、0 MMIO；PC 弹回 0 并滑到 halt |
| [d850-init-no-fixture.md](d850-init-no-fixture.md) | 辅助入口 `0x1035CE30`，无页表夹具 | MMU 打开后 prefetch abort；表遍历读 `0x40000000`/`0x40001000` size=8 |
| [d850-init-lpae.md](d850-init-lpae.md) | 同一入口 + `--e5-tables` | **0 异常**；init 跑完并 `bx ip` 到 `0x40028FF8` |
| [d850-cpu-a9.md](d850-cpu-a9.md) | `-cpu cortex-a9` 对照 | `mcrr`（TTBR1）undefined；证明走 LPAE |
| [lcd-testcard.md](lcd-testcard.md) | 主机写入 320×240 ARGB 测试卡 + LCD dump | 管线通；**不是固件画面** |
| [bootrom-alt-stage2.md](bootrom-alt-stage2.md) | ALT 重建 + `--bootrom --boot-tables` | 进入 `0x40028FF8` 真代码，但入口/r2 不对 → data abort；**偏移高了 0x20000，已被下一行修正** |
| [bootrom-correct-stage2.md](bootrom-correct-stage2.md) | 修正到 Fenestra live delta 的映射 | `0x40028FF8` 是 ARM 入口，stage-2 写 `0xC0111450/...`；之后 Thumb 指针 `0x400034B8` undef |
| [real-reset-progress.md](real-reset-progress.md) | 真实 reset + MIDR/boot RAM/display stub | 过 MIDR，执行到显示/时钟块；停在 `0x800E845C` peer 握手 |
| [mmio-coverage.md](mmio-coverage.md) | 未建模 MMIO 统计 | stage-2 34 个、真实 reset 42 个、并集 76 个唯一地址 |
| [full-firmware-scope.md](full-firmware-scope.md) | 全固件 TOC/模块扫描 | a1330/wma/tpj/b1330 均无 `0x90001046` 直接初始化；更像 boot ROM |

`lpae-table.bin` 是 `--e5-tables` 写入的 8 KiB 夹具（两张 4 项 LPAE level-1 表），
由 `tools/run.py`/机器模型生成，用于让缺失的 boot ROM 页表不再阻断翻译；
它不包含任何 Nikon 代码。
