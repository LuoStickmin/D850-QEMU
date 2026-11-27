# M2：T-Kernel 入口初步（2026-11-28）

## 结论

M1 之后，真实 reset 已经进入 **0x40000000 重建窗**，但停在 boot ROM 的
输入/命令循环，而不是 T-Kernel 主入口。

当前最后 PC：

```text
0x400022F8 ... 0x40002302
```

`0x400022F8` 是 Thumb 函数：

```text
0x40002302  adr r0, #0x154
0x40002304  bl  0x400021EE
0x4000230A  movs r4, #0
0x40002310  strb r8, [r6]
0x40002314  movs r0, #1
0x40002316  bl  0x40002104
0x4000231A  cbz r0, ...
0x4000231C  cmp r0, #-1
0x40002320  beq 0x40002302
0x40002324  cmp r5, #0xD
0x40002328  cmpne r5, #0xA
0x4000232C  cmp r5, #8
```

它比较 `0xD`/`0xA`/`8`，很像 **命令/输入循环**（boot monitor/shell），
不是 T-Kernel 任务启动。

## T-Kernel 在 0x40000000 窗内

bootrom 窗里有明确 T-Kernel 字符串：

```text
0x40004150  : module initialize error
0x400044E3  @!ERROR! init_kernel
0x400044FC  !ERROR! init_system_slave
0x4000451B  @!ERROR! start_system
0x40006033  @!ERROR! SMP Initialize
0x4001A0EC  T-Kernel/OS SVC in execution
0x4002FF43  @FirstLoad1 Start
0x4002FF58  FirstLoad1 End
0x4002FF68  FirstLoad2 Start
0x4002FF7C  FirstLoad2 End
```

这说明 **T-Kernel 本体在重建的 0x40000000 窗内**，不是只在 b1330 的模块表里。
之前 b1330 的 `T-Kernel Version 1.02.01` 只是镜像里的副本/元数据。

相关代码：

- `0x4002FE44` 起是 FirstLoad 相关函数；
- `0x40004400` 起是 init/start_system 序列；
- `0x400022F8` 是当前停在的 boot monitor 循环。

## 当前阻塞

- 真实 reset 进入 boot monitor 后等待输入/命令；
- QEMU 没有输入/命令模型，所以停在 `0x40002302`；
- 还有一个 data abort：`DFAR=0xC`，PC `0x10020010`；
- 剩余 6 个未建模地址：`0x51000020`、`0x51200010/14/18`、`0x512D0000`、`0x5E0014EC`；
- 没有 framebuffer。

## 下一步

M2 继续只需要二选一：

1. **找 boot monitor 的启动命令**：什么输入/标志会从 `0x400022F8` 走到
   `0x400044E3`/`start_system`；
2. **直接跳到 T-Kernel 入口**：在 QEMU 里把 PC 置到 `0x40004400` 附近的
   init/start 序列，补最小上下文，验证能否进入 T-Kernel SVC/任务。

在此之前，不应再猜显示/MMU；T-Kernel 入口已经在 0x40000000 窗内。
