# Assisted GUI 冷调用路线（2026-11-26）

目标：不再等自然 reset/RTOS，参考 WonderLantern Z 机型 QEMU 的 assisted 思路，
直接调用 D850 的显示/菜单候选，看能否让固件自己写 framebuffer。

## 参考

- z7-qemu / z6-qemu / z5-qemu / z30-qemu：明确 assisted，host 提供状态后调用原厂函数。
- zf-qemu：功能 sensor/ISP/display 模型，不是自然上电；机制不完全相同。
- 共同点：都需要一个**已初始化的任务/菜单 checkpoint**，不是冷调任意 handler。

## D850 候选与实际结果

| 入口 | 结果 |
|---|---|
| `APPMENU 0x1025368C` | Thumb 冷调用 `r0=5,r1=2`：287 TB、0 异常、0 MMIO，但无 `0x83DFE000` 写入；它是菜单项状态 getter/setter，不是画屏入口 |
| `TFTBACK 0x1000EA56` | 背光/亮度曲线，不是 GUI |
| `TFTSERV 0x1000EDDA` | 需要 `r1` 命令结构；冷调用缺上下文 |
| `0x1033A3F4` | 真正的 LCD/clock 初始化序列，写 `0x543001E4..0x5430039C`；可独立调用，0 异常返回，但不写 framebuffer |

## 关键反汇编

`APPMENU`：

```text
0x1025368C APPMENU
0x102536A0 -> 0x10235AE4 -> 0x10235ADE
0x102536CC -> 0x10211C38
```

`0x10235AE4`/`0x10211C38` 是状态读取/设置，不是全屏绘制。

`0x1033A3F4` 是 LCD 初始化：

```text
ldr r1, =0x543001E4
ldr r0, [r1]
bic/orr r0, #0xc00/#0x400
str r0, [r1]
...
bx lr
```

## 结论

assisted **冷调用**这条路的尽头是：

- 能调用 LCD 初始化；
- 能调用菜单状态 handler；
- 但没有 T-Kernel 任务/菜单对象/回调表，handler 不会画屏；
- 没有 `0x83DFE000`/`0x86DA0400` framebuffer 写入；
- 因此不能把任何 host 结果当成固件 GUI。

要继续，需要的是 T-Kernel **任务 checkpoint**（或真实 reset 到应用就绪），
而不是继续挑一个函数冷调。当前没有该 checkpoint 的入口/状态。

## 未上传内容

本文件、0x80000000 RAM 映射、0x54300000 寄存器页和调试改动在本地；
没有把 mock 当 GUI，也没有提交固件。
