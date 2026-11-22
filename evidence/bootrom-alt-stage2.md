# ALT 重建 + QEMU `--bootrom` 实跑（2026-11-22）

> **已修正/已过时：** 本文使用的文件偏移高了 `0x20000`（应按 Fenestra live
> delta 用 `0x178E290`/`0x17BD380`）。修正后的结果见
> [bootrom-correct-stage2.md](bootrom-correct-stage2.md)；本文保留作对照。

> 这是“重建内容能执行”的第一次实跑，不是“stage-2 已跑通”。结论是：
> ALT 映射的代码确实在 `0x40028FF8` 开始执行，但入口上下文/重定位仍不对，
> 随后 data abort。

## 1. 输入

- 固件：`b1330_131.bin`（SHA-256 `97E1DA12…204003`）
- 重建：`bootrom_alt.bin`（512 KiB）
  - `0x40000000 ← b1330[0x17AE290] len 0x2F0D8`
  - `0x4002F100 ← b1330[0x17DD380] len 0x12BA8`
- 机器：`nikon-d850`，新增：
  - `bootrom=FILE`：映射 512 KiB 到 `0x40000000` 并装入；
  - `bootrom-tables=on`：在 `0x40708000` 建 LPAE identity 表，并预置
    `[0x400E8450] = 0x00708000`；
  - `bootrom-thumb=on`（默认）：把 payload 里 `0x35D144` 的
    `0x40028FF8` 改成 `0x40028FF9`，模拟 boot ROM 的 Thumb 交接补丁。

## 2. 命令

```sh
python3 tools/run.py \
  --qemu /path/to/qemu-system-arm \
  --payload "$FW/b1330_131.bin" --init \
  --bootrom runs/bootrom_alt.bin --boot-tables --bootrom-thumb \
  --out runs/alt-stage2-thumb --timeout 3 --trace in_asm
```

## 3. 结果

```text
status=timeout  exit=0
exceptions: data_abort=3242
MMIO: 5 events
```

PC 轨迹：

```text
0x1035CE30 ... 0x1035D018
0x40028FF8        <- 进入重建窗
0x40028FCC        <- Thumb 循环（ldm r2!, {r1}; ...）
0x10020010        <- 跳到 payload 的 0x10020010
```

首条 data abort：

```text
Taking exception 4 [Data Abort]
...with ESR 0x25/0x96000021
...with DFSR 0x221 DFAR 0x2
```

之后 3 秒内重复 3242 次，DFAR 出现 `0x2` / `0xffffffff`。

## 4. 解读

- **MMU 已通过**：init 用 `[0x400E8450] = 0x00708000` 找到
  `0x40708000` 的 LPAE 表，0 异常跑完 init。
- **stage-2 代码是真的**：`0x40028FF8` 不再是零页，执行到
  `0x40028FCC` 的 Thumb 循环。
- **但入口不对**：`0x40028FF8` 落在 `0x40028FCC` 循环中间，不是函数入口；
  init 给的 `r2=2` 被循环当成源指针，`ldm r2!` 去读地址 `0x2`，触发 data abort。
  这说明真实 boot ROM 还会**重定位/修补 `0x40028FF8` 字面量和 r2/r3 寄存器**，
  仅重建内容不够。
- 作为对照，`bootrom_win.bin` 在 `0x40028FF8` 是 `e5 06 01 12…`，
  ARM/Thumb 都解不出干净指令；ALT 至少执行到了真 Thumb 代码。

## 5. 边界

- 这是 `entry=0x1035CE30` 的 host-assisted init，不是自然上电。
- `0x40708000` 页表和 `[0x400E8450]=0x00708000` 是按 plan/Fenestra 线索的
  夹具；真机值仍需 trace 确认。
- `0x40028FF8` 的入口上下文（r0-r4、重定位 delta）未知；当前 data abort
  是“缺 boot ROM 交接”的直接证据，不是 ALT 内容全错。
- 不包含/提交任何 boot ROM 或固件；`bootrom_alt.bin` 只在本地 `runs/` 中。

## 6. 下一步

1. 找 boot ROM 对 `0x40028FF8` 字面量/寄存器的补丁（Fenestra trace 或
   relocation 表）；
2. 或从 `0x40028FCC` 反推函数真正入口，尝试在 QEMU 里改 `entry`/寄存器；
3. 若拿到 `0x40708000` 附近的真实 SRAM/trace，再验证页表基址。
