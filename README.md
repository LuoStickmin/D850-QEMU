# D850-QEMU

> [English](README.en.md)

Nikon D850（EXPEED 5 / E5）固件研究用的 QEMU 验证原型：一个 `nikon-d850`
板级 overlay、一个运行/取证工具，以及从 `b1330_131.bin` 真实字节得出的启动链结论。

> 本仓库**不包含完整 Nikon 固件、密钥或改后镜像**；文档只引用说明所需的短指令字/地址。固件由你本地提供，工具只读。
>
> 许可：**GPL-2.0-or-later**（代码与文档）· 非官方项目 · 与 Nikon 无关联 ·
> 使用前请阅读 [法律声明](LEGAL.md)。

## 核心目标（先读）

**目标**：把已解密的 D850 主镜像按推断基址装进 QEMU，回答四个有顺序的问题：

1. **D0** reset 字 `0x1037CBA8` 能不能起跑？它需要 boot ROM 提供什么？
2. **D1** 镜像内 init（`0x1035CE30`）做了什么，MMU 打开后缺什么？
3. **D2** 补上缺失的页表后，init 能不能跑完并交接？
4. **D3** 交接到 `0x40028FF8` 之后，还缺哪段代码？

**边界**：

- 不模拟 CMOS/ISP/存储/UI，不做整机 emulation；
- 不提供签名、重打包或升级容器处理；
- 不是自然上电启动：入口、栈、页表夹具都是主机侧辅助，结论按“辅助启动”标注。

## 当前状态（2026-11-16）

| 项目 | 状态 |
| --- | --- |
| 机器 `nikon-d850` | ✅ 已写并在 QEMU 11.1.0 上编译、实跑 |
| 镜像装入 | ✅ `b1330_131.bin`（25,671,414 B）@ `0x10000000`，flash 窗口 32 MiB |
| reset 字 `0x1037CBA8` | ✅ 0 异常、0 MMIO；但它是 boot-service sled，`pop` 回 PC=0 |
| 镜像内 init `0x1035CE30` | ⚠️ 无页表夹具时：MMU 打开后 prefetch abort；表遍历读 `0x40000000`/`0x40001000`（size=8，LPAE） |
| LPAE 页表夹具 `--e5-tables` | ✅ **0 异常**；init 跑完并 `bx ip` 到 `0x40028FF8` |
| `0x40028FF8` 之后的 boot ROM | ❌ 缺失；handler RAM 全零，PC 从 `0x40028FF8` 滑到 `0x40040000` |
| CPU 假设 | ✅ LPAE（`mcrr` 写 TTBR）；Cortex-A9 在 `0x1035D45C` undefined |
| UART / 串口输出 | ❌ 未观测到（`uart=0B`） |
| 真机验证 | ❌ 本原型不产生可刷写镜像，也没有实机记录 |

一句话：**D850 的镜像内 init 已经能在 QEMU 里完整跑完；缺的是 `0x40000000`
区的 boot ROM 代码和三个服务函数，不在 `b1330` 镜像里。**

细节：[docs/status.md](docs/status.md)、[docs/boot-chain.md](docs/boot-chain.md)；
原始运行证据在 [evidence/](evidence/README.md)。

## 快速开始

### 构建

需要 QEMU 11.1.0 源码树和 C 工具链（Linux / WSL）：

```sh
cd /path/to/qemu-11.1.0
python3 /path/to/D850-QEMU/qemu/prepare_source.py .
ninja -C build qemu-system-arm
```

overlay 只增加 `hw/arm/nikon-d850-lab.c` 和注册行，不改上游 QEMU 代码；
重复执行 `prepare_source.py` 是幂等的。QEMU 10.2.4 有 `CharBackend` 文本适配，
但本仓库只在 11.1.0 上编译、实跑过。详见 [docs/build.md](docs/build.md)。

### 运行

```sh
python3 tools/run.py \
  --qemu /path/to/qemu-11.1.0/build/qemu-system-arm \
  --payload /path/to/b1330_131.bin \
  --out runs/d850-reset --timeout 3
```

默认 profile：`payload-base=0x10000000`、`entry=0x1037CBA8`、
`flash-size=0x2000000`、CPU `cortex-a15`。

三个复现实验：

```sh
# 1. reset 字：0 异常、0 MMIO，PC 弹回 0 并滑到 halt
python3 tools/run.py --payload "$FW/b1330_131.bin" --out runs/reset

# 2. 镜像内 init：无页表夹具 -> prefetch abort 循环
python3 tools/run.py --payload "$FW/b1330_131.bin" --init --out runs/init

# 3. init + LPAE 页表夹具：0 异常，交接到 0x40028FF8
python3 tools/run.py --payload "$FW/b1330_131.bin" --init --e5-tables \
  --out runs/init-lpae
```

每次运行生成 `runs/<名字>/`：`command.json`、`qemu.log`、`uart.log`、
`session.json`（异常计数、MMIO 汇总、halt 记录）。`session.json` 里的
`status=halt-unexpected` 对 reset 字是**预期结果**：sled 的 `pop` 把 PC 弹回 0，
随后滑到 `0xFF0` 的 halt 窗口；它不是一次“干净函数返回”。

## 关键发现（都来自真实字节）

### 1. reset 字是 boot-service sled，不是入口

`b1330` 的向量表在 `0x10000000`，reset 指针是 `0x1037CBA8`：

```text
1037cba8  b     0x1037cbb8
1037cbb8  str   r0, [r4]
1037cbbc  b     0x1037cbc4
1037cbc4  mov   r0, r1
1037cbc8  pop   {r4, r5, r6, pc}
```

辅助启动给的栈是全零，`pop` 直接回到 PC=0。**它需要 boot ROM 预置 r4、栈和
返回地址**；这就是“从 reset 字自然起跑”失败的原因，而不是镜像装错了地址。

### 2. 镜像内 init 会自己建表、开 MMU、设 VBAR

`0x1035CE30..0x1035D040` 是一段完整的 init 安装器：

- 读 `0x400E8450`，写 `0x400E8454 = 0x9000`、`0x400E8458 = 0x42400000`；
- 调用 `0x1035D17C`/`0x1035D184`——镜像里这两个函数都是 `bx lr`，是
  **boot-ROM 服务占位**；
- 在 `0x1035D424` 写 `TTBCR = 0xB1003104`（EAE=1）、
  `TTBR1 = 0x40000000`、`TTBR0 = 0x40001000`；
- 写 `SCTLR = 0x1005`（M=1），然后写 `VBAR = 0x10020000`；
- 写 `0x4007C2E0 = 0x10`、`0x4007C2E4 = 1`、`0x2E0014EC = 0x80000000`；
- 最后 `ldr ip,[pc,#0x108]` 取到 `0x40028FF8`，`bx ip` 交班。

### 3. 缺的是 LPAE 页表，不是 init

`0x40000000`/`0x40001000` 没有页表时，MMU 打开后的第一次取指就
translation fault：QEMU 的 LPAE 表遍历以 **size=8** 读这两个地址，随后
`IFSR=0x205/0x206`、`IFAR=0x1035D5D4`；`VBAR` 还是 0，异常向量又落在 `0x0C`，
于是递归 abort。2 秒内可看到上千次 prefetch abort。

`--e5-tables` 在 `0x40000000` 映射 256 KiB handler RAM，并写入两张 4 项
LPAE level-1 表（1 GiB block、identity）——分别对应 TTBR1 和 TTBR0。
结果 **0 异常**，init 完整跑完并交接到 `0x40028FF8`。这是**夹具**，不是真机
boot ROM 的页表；它只证明“init 自身没有缺页表逻辑”，并把缺口精确推到
`0x40000000` 区。

### 4. CPU 必须是 LPAE 核

同一入口用 `-cpu cortex-a9`：`0x1035D45C` 的 `mcrr p15,#1,r0,r1,c2`
（64 位 TTBR1 写）直接 undefined，LR=`0x1035D460`。这排除了 A9 假设，
与 init 的 `TTBCR.EAE=1` 一致；默认 CPU 因此是 `cortex-a15`（A7 同类）。
`0x40028FF8` 处的 boot ROM 代码仍然缺失——handler RAM 全零，PC 从
`0x40028FF8` 每 4 字节滑行到 `0x40040000`，之后进入诊断窗口。

## 机器模型（一句话版）

| 项目 | 取值 | 依据 |
| --- | --- | --- |
| CPU | 1× Cortex-A15（`-cpu` 可换 A7/A9） | init 使用 LPAE `mcrr`；A9 实测 undefined |
| 主镜像 | `b1330_131.bin` @ `0x10000000`，flash 32 MiB | 向量表在 `0x10000000`，reset/异常指针都在镜像基址之上 |
| 初始 PC | `0x1037CBA8`（`--init` 改 `0x1035CE30`） | 向量表 reset 字 / init 首地址 |
| handler RAM | `0x40000000`，可映射 256 KiB；`--e5-tables` 额外铺 LPAE 表 | 异常向量 `0x4002xxxx`、init 的 TTBR1 基址 |
| RAM（辅助栈） | `0x90000000`，128 MiB | **继承 E4 假设**，没有 D850 证据；只用于栈/halt |
| 低内存 | `0x0..0xFFFF`，`0xFF0` halt 窗口 | 辅助启动返回地址 |
| 诊断 | 4 GiB catch-all；`strict=on` 第一条未建模访问后停止 | — |
| 已观测 MMIO | `0x400E8450/54/58`、`0x4007C2E0/4`、`0x2E0014EC` | 本仓库运行记录 |

机器**诊断优先**：未建模访问记 `d850-probe:`（PC/地址/宽度/方向），
`strict=on` 时记 `d850-strict:` 后停止。不会先填一堆零把 boot 骗过去。

## 目录

```text
.
├── qemu/
│   ├── nikon-d850-lab.c/.h     板级模型（payload/异常向量/handler RAM/LPAE 夹具/诊断）
│   └── prepare_source.py       把 overlay 装进 QEMU 源码树（幂等）
├── tools/
│   └── run.py                  启动、取证、异常/MMIO 汇总
├── tests/                      主机侧测试（不需要 QEMU 构建）
├── docs/
│   ├── status.md               当前状态、三次实验的完整命令与结果
│   ├── boot-chain.md           字节级启动链分析（向量表/reset/init/交接）
│   ├── bootrom.md              boot ROM 尺寸估计、抓取范围与“只拿到 ROM”能走多远
│   └── build.md                构建与运行说明
├── evidence/                   运行证据（Markdown + LPAE 夹具二进制）
├── README.md / README.en.md
├── CHANGELOG.md
├── LEGAL.md                    法律与合规声明（中英双语）
├── NOTICE.md                   版权/许可/上游/第三方
├── CONTRIBUTING.md / .en.md    贡献规则与 DCO
├── SECURITY.md                 漏洞报告与安全边界
├── CODE_OF_CONDUCT.md          行为准则
└── LICENSE                     GPL-2.0（or later）
```

## 与 D750-QEMU 的关系

本仓库是从 [D750-QEMU](https://github.com/LuoStickmin/D750-QEMU) 的验证机方法中
拆出的 D850 专用工作区。沿用了它的三条原则：

- overlay 形态（一个板级 C 文件 + 幂等注册脚本）；
- 用户自带固件、仓库不发布固件/密钥；
- 未建模访问必须留 PC/地址证据，而不是静默返回零。

D850 专属部分：E5 的 `0x10000000`/`0x40000000` 版图、LPAE init 分析、
`--e5-tables` 夹具和 `0x40028FF8` 交接结论。D780/E6 的 `e6-prep` 等选项仍保留在
共享代码里，但不在 D850 验收范围内。

## 还需要什么（输入需求）

按优先级：

1. **D850 整片 SPI flash dump**（CH341A/测试夹）——最可能包含 `0x40000000`
   区的一级 boot 或 handler 代码；
2. **`0x40000000..0x40040000` 的 RAM dump**（从真机或服务模式读取）——
   直接补上 `0x40028FF8` 和三个服务函数；
3. **Nikon 服务/维修固件里的 recovery/boot 镜像**（如 Tornado NKN）；
4. **硬件 UART 启动日志**——确认 boot ROM 把哪些段拷到 `0x4002xxxx`；
5. 其他 E5 机型（D5）的同类 dump——用于判断 `0x400E8450/54/58` 的语义。

尺寸与抓取范围估计见 [docs/bootrom.md](docs/bootrom.md)：`0x40000000` 窗至少
~184 KiB（覆盖向量），按段表边界至少 ~263 KiB，建议按 **512 KiB**
（`0x40000000..0x4007FFFF`）抓取；物理 mask ROM 可能更小，且不一定是同一个东西。

已经具备、无需再提供：解密后的 `b1330_131.bin`、E5 的向量/异常表、
init 反汇编与 QEMU 夹具、`0x40028FF8` 交接点。

> 上述固件/dump 输入只用于你本地的分析。**不要把固件、密钥、boot ROM、flash/RAM
> dump 或改后镜像附到仓库、issue 或 PR**；如需讨论，请只提供不含代码/数据的事实性
> 摘要（地址、偏移、哈希、异常记录）。

没有新输入时仍可做：把 `0x400E8450/54/58` 建成寄存器模型，观察 init 在
不同返回值下的分支；或对 `0x40028FF8` 放最小 `bx lr`/`b .` 桩，记录返回契约。
这些都属于夹具，不会冒充 boot ROM。

## 许可、法律与贡献

- **代码与文档**：GPL-2.0-or-later（GNU GPL v2 或任何更晚版本）。完整 GPLv2
  文本见 [LICENSE](LICENSE)；SPDX 头与 [NOTICE.md](NOTICE.md) 给出“或更晚版本”
  的选择权。
- **版权**：Copyright (C) 2026 LuoStickmin and D850-QEMU contributors；
  部分方法与板级代码源自作者的 [D750-QEMU](https://github.com/LuoStickmin/D750-QEMU)
  （GPL-2.0-or-later）。
- **不含固件/密钥**：仓库不包含完整 Nikon 固件、解密模块、K8/T1/T2/T3、boot ROM、
  flash/RAM dump、可刷写改后镜像或 Nikon 字体/图标/UI 素材。
- **非官方**：与 Nikon Corporation 无关联，未获授权、赞助或背书；“Nikon”
  “D850”“EXPEED”是各自权利人的商标，仅用于指名性说明。
- **不是法律意见**：逆向工程、反规避（如 DMCA §1201、EU 2009/24/EC）、EULA、
  出口管制与保修问题因司法辖区而异；使用前请咨询律师。
- **无担保**：按 GPL 的“AS IS”条款提供；不对相机损坏、变砖、数据丢失、保修
  失效或法律后果负责。QEMU 不是安全边界，请在隔离环境运行。

| 文件 | 内容 |
| --- | --- |
| [LICENSE](LICENSE) | GPLv2 全文 |
| [NOTICE.md](NOTICE.md) | 版权、许可范围、上游与第三方 |
| [LEGAL.md](LEGAL.md) | 法律与合规说明（中英双语，非法律意见） |
| [CONTRIBUTING.md](CONTRIBUTING.md) / [CONTRIBUTING.en.md](CONTRIBUTING.en.md) | 贡献规则、DCO、禁止内容 |
| [SECURITY.md](SECURITY.md) | 漏洞报告与安全边界 |
| [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) | 行为准则 |
