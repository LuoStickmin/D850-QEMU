# 真实 reset + MIDR/display stub 的推进记录（2026-11-22）

> 目标：用固件自己的 `0x1037CBA8` reset 走到 `0x40028FF8`。当前已过 MIDR gate
> 并执行到显示/时钟块，但停在 boot ROM/CPU1 的握手等待，尚未进入 stage-2，
> 也还没有固件画面。

## 1. 已加入的固件路径夹具

- `payload-base=0x10020000`：Fenestra 的 b1330 基址。
- `bootrom=runs/bootrom_correct.bin`：修正后的 512 KiB 窗。
- `0x40708000` 映射 64 KiB boot RAM：让 `0x1037CCC0` 自己建 LPAE 表。
- `0x400E8000` 映射 boot-arg 页。
- `0x51340008` 自由计数器：让 reset 的 `>0x30` / `>0x18` 延时循环结束。
- `0x54000000..0x571FFFFF` 读返回 `0xffffffff`：GDU/display 状态/ready 轮询。
- `-cpu cortex-a15,midr=0x410FC0F4`：通过 `0x1037CC0C` 的 MIDR gate。
- assisted entry 打开 VFP/CPACR。

## 2. 命令

```sh
qemu-system-arm \
  -M nikon-d850,payload-base=0x10020000,entry=0x1037cba8,\
flash-size=0x2000000,bootrom=runs/bootrom_correct.bin \
  -cpu cortex-a15,midr=0x410FC0F4 -m 128M -display none -monitor none \
  -serial null -kernel b1330_131.bin -no-reboot -nic none
```

## 3. 结果

```text
exceptions: 0
PC: 0x1037CBA8 -> 0x1037CBB0 -> ... -> 0x1037E9xx / 0x1037CC88
```

- 已通过 MIDR gate，不再在 `0x1037D040` WFI；
- 观测到显示/时钟块的写：
  - `0x512D0000`、`0x5E0014EC`
  - `0x543001E4/.../5430039C`
  - `0x55061400/55040400/55041400/55020400/55021400`
- 当前停点：`0x1037CC88` 读 `[0x800E845C]` / `[0x800E8460]`，等非零；
  或 `0x1037E948` 等 `0x57100220` 的 bit3（已由 stub 覆盖后继续到前一个）。

## 4. 现有显示线索

已有 D850 显示研究脚本（`patch_qemu_gfx.py`、`apply_gdu_display_fix.py`）给出：

- GDU/display 状态块：`(addr & 0x0f000000) == 0x04000000`（含 `0x54xxxxxx`）
  全部返回 `0xffffffff`；
- 候选 framebuffer：`0x83dfe000`，640×480，ARGB8888，stride 2560；
  另一个候选 `0x86da0400`（1024×768 等）；
- GDU 写地址包括 `0x04007000` / `0x05027000` / `0x54000000..0x55000000`。

这些说明 **显示/GPU 路径在固件里**，但当前 reset 还没走到 framebuffer 写像素。

## 5. 下一步

1. 映射 `0x80000000..0x90000000`（DRAM/alias + surface），让 `0x800E845C`
   的握手页可写；或明确它由 boot ROM/CPU1 写。
2. 解决 `0x1037CC88` 的 peer 标志（boot ROM 或第二核），再继续到 `bx ip`。
3. 过 `0x40028FF8` 后处理 `0x400034B8` 的 Thumb 指针。
4. 用 `0x83dfe000` + 640×480 ARGB 把 surface dump 成 PNG；这一步才是“看见
   固件画面/GUI”。

## 6. 边界

- 当前是大量 host stub（timer/status/MIDR），不是自然上电。
- 0x54xxxxxx 全部返回 `0xffffffff` 只满足轮询，不代表显示控制器已建模。
- 还没有任何固件 framebuffer 写入证据；现有 preview PNG 有 mock，不是固件渲染。

## 7. 2026-11-22 后续：peer stub 与 stage-2 的 VFP 阻塞

- 映射 `0x800E8000` 的 peer 页，`0x800E845C/60` 读返回 1 后，reset 过了
  `0x1037CC88` 的握手，继续写 `0x51200010/14/18`，并在 `0x1037F34C`
  结束/等待；仍未调用 `0x40028FF8`。
- host-assisted stage-2 修正后，`0x4000094C` 的 Thumb `bl 0x400034B8` 实际执行成功，
  返回 `0x4000095A`，说明前一个“Thumb 指针”问题不存在；真正的 undef 在
  `0x10235618` 的 `mcrr2 p1,#0,r2,r4,c0`（VFP 双寄存器传送）。
- QEMU Cortex-A15 不支持/未使能这条指令。把 `file 0x235618` 的两个 Thumb 字节
  改成 NOP 后，能继续到 `0x10205150 -> 0x10235622 -> 0x101fc7a8`，随后
  data abort/undef（`0x10020010`）。
- 结论：GUI 之前还有两类真实阻塞：QEMU 的 VFP `mcrr2` 支持，以及后续
  framebuffer/服务初始化；不是单纯入口地址问题。下一步优先实现该 VFP
  指令（或精确 patch），再映射 `0x83dfe000` surface 做 dump。
