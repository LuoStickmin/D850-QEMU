# Boot ROM 尺寸与“只拿到 ROM”能走多远（估计）

> 状态：2026-11-16 的**静态估计**，不是实测。等真实 dump 到手后应回到本文更新。
> 结论按置信度标注；不要把“可能”读成“已确认”。

## 1. 先分清两个“boot ROM”

D850（E5）里至少有两个容易混在一起的东西：

| 名称 | 地址/形态 | 作用 | 能否单独拿到 |
| --- | --- | --- | --- |
| **0x40000000 代码/向量窗** | `0x40000000` 起，异常向量在 `0x4002DFC0..0x4002E12C`，交班点 `0x40028FF8` | 页表、异常 handler、boot/loader 代码、被拷贝的段 | 可通过 RAM dump / 服务模式拿到 |
| **物理 mask ROM** | 地址未知（E3 类比在 `0x000E0000..0x000FFFFF`） | 上电第一级 loader，可能把更大的镜像从 flash 拷到 SRAM/DRAM | 需要 decap/JTAG/漏洞，难度高 |

**二者大小不一定相同。** 物理 mask ROM 可能只有 128–256 KiB，而它装载的
`0x40000000` 运行窗可能是 512 KiB。只拿到其中一个，能做的事不同（见 §4/§5）。

## 2. 已知地址（真实字节）

| 地址 | 含义 | 来源 |
| --- | --- | --- |
| `0x40000000` | TTBR1 页表基址（init 写入） | `0x1035D45C` 的 `mcrr` |
| `0x40001000` | TTBR0 页表基址 | `0x1035D464` |
| `0x40028FF8` | init 最后的 `bx ip` 交班目标 | `0x1035D144` 字面量 |
| `0x4002DFC0` | undefined 向量 | 镜像向量表 |
| `0x4002E070` | SVC 向量 | 镜像向量表 |
| `0x4002E0E8` | prefetch abort 向量 | 镜像向量表 |
| `0x4002E0F8` | data abort 向量 | 镜像向量表 |
| `0x4002E12C` | FIQ 向量 | 镜像向量表 |
| `0x400E8450/54/58` | init 读写的 boot 参数/暂存寄存器 | 运行记录 |
| `0x4007C2E0/4` | init 写 `0x10`/`1` 的寄存器 | 运行记录 |
| `0x2E0014EC` | init 读改写 `0x80000000` | 运行记录 |

## 3. 尺寸估计

### 3.1 最低下限：约 184 KiB

最高已知向量是 `0x4002E12C`，加上 handler 本体，`0x40000000` 窗至少
**0x2E12C ≈ 184.3 KiB**。这是**高置信度**下限。

### 3.2 载荷内部还有一张段表：约 263 KiB

`b1330` 的 `0x1035F780` 与 `0x10383400` 各有一组 12 字节记录，关键边界值互相对应，
像一张“源 → 目标 → 长度”的装载/区域表：

```text
0x40000000 + 0x2F0D8  = 0x4002F0D8
0x4002F100 + 0x12BA8  = 0x40041CA8   <- 另一张表也给出同一边界
0x4007BE48 + 0x480    = 0x4007C2C8
0x4007C2C8 + 0x6C1AC  = 0x400E8474
0x400E8474 + 0x61F79C = 0x40707C10
```

前两段是 `0x40000000` 起、**首尾相接（中间 0x28 字节对齐间隙）到 `0x40041CA8`** 的区域；后三段从
`0x4007BE40` 起，更像寄存器/设备 aperture（`0x400E8450` 正好落在里面）。
如果这段表真是装载/区域表，那么 `0x40000000` 窗的**已用内容至少
0x41CA8 ≈ 263.2 KiB**。这是**中高置信度**（数值对齐很强，但消费方还未完全反推）。

### 3.3 最可能的抓取窗口：512 KiB

- `0x41CA8` 超过 256 KiB，所以 256 KiB 不够；
- 下一个自然大小是 **512 KiB**：`0x40000000..0x4007FFFF`；
- `0x4007BE40`/`0x4007C2C8` 的寄存器靠近 `0x4007Cxxx`，正好在这个窗口顶部；
- `0x400E8450` 在 512 KiB 之外的下一个设备 aperture，不影响窗口划分。

所以建议按 **512 KiB 窗口**抓取；如果只能抓一小段，先抓
`0x40000000..0x40042000`（264 KiB）。

### 3.4 物理 mask ROM：低置信度

E3（D5100/D7000/D800）的第一级 boot 在物理 `0x000E0000..0x000FFFFF`，约
128 KiB，是从 RAM dump 恢复的；E5 可能同量级（128–256 KiB），但这只是**类比**，
不是 D850 的测量。E5 的 mask ROM 也可能只是一个小 loader，真正的
`0x40000000` 代码来自 flash。

### 3.5 置信度汇总

| 结论 | 置信度 |
| --- | --- |
| `0x40000000` 窗至少 184 KiB（覆盖向量） | 高 |
| 已用内容至少 263 KiB（段表对齐） | 中高 |
| 运行窗按 512 KiB 抓取 | 中 |
| 物理 mask ROM 为 128–256 KiB | 低（仅 E3 类比） |

## 4. 只拿到 `0x40000000` 窗

能做的：

1. **替换夹具**：用真实 LPAE 页表替换 `--e5-tables`；用真实 handler 替换
   `0x4002xxxx` 的零页。
2. **验证 init**：从 `0x1035CE30` 起跑，确认真实页表/服务下是否仍 0 异常；
   观察 `0x400E8450/54/58` 的真实值如何改变分支。
3. **执行真实交班**：`0x40028FF8` 不再是零，可以看到 stage-2 的第一批指令、
   MMIO、SVC/异常。
4. **补服务实现**：`0x1035D17C/180/184` 三个 `bx lr` 槽可以用真实 ROM 代码
   或跳转目标替换，init 不再“空调用”。

不能做的：

- 仍然没有**上电第一级**：入口/栈/页表还是主机侧辅助，不是自然 boot；
- 如果这个窗只是从 `b1330` 拷来的运行时代码，它**不含 loader**，自然上电仍缺；
- 仍缺 DRAM/时钟/存储/PMIC 的真实初始化序列。

## 5. 只拿到物理 mask ROM

能做的：

1. **静态**：找 reset 向量、boot 协议、从 flash 读什么、校验/签名、把哪些段
   拷到 `0x40000000`/DRAM、错误/恢复路径。
2. **QEMU 起跑**：把 ROM 放到它的物理地址，CPU 从 reset 向量起跑；它会先访问
   时钟/DRAM/flash。QEMU 会记录第一条未建模 MMIO，按 ROM 代码逐项建模，而不是
   猜寄存器。
3. **接上载荷**：如果 ROM 从 flash 读 `b1330`，把解密镜像放到它期望的位置；
   如果它校验容器/签名，可能失败——那需要原始容器或进一步分析，本项目不提供
   绕过。
4. **拿回真实入口上下文**：ROM 会设置 r4/SP/LR/页表，然后跳 `0x1037CBA8`；
   这才是 payload reset 字的真实调用环境。

不能做的：

- 如果 ROM 只是 loader，真正的 `0x40000000` 代码在 flash 里，你仍然缺这一段；
  但 `b1330` 里已经出现目的地址为 `0x40000000`/`0x4002F100` 的段表，可能能
  从载荷本身补上（待验证）。
- 没有整片 flash/配套模块，仍不知道分区、校准、M-core、传感器参数；
- 没有板级外设模型，ROM 会停在第一条未建模访问。

## 6. 建议的抓取范围（按优先级）

1. **`0x40000000..0x40080000`（512 KiB）** —— 首选；覆盖向量、交班点、页表、
   已用段和顶部寄存器。
2. **`0x40000000..0x40042000`（264 KiB）** —— 只能抓一小段时的最低要求。
3. **`0x4007B000..0x400E9000`** —— `0x4007C2E0/4`、`0x400E8450/54/58` 等
   寄存器/暂存区，用于还原 init 的输入。
4. **`0x40700000..0x40708000`** —— 段表里的高地址 aperture 末端，可选。
5. 如果能拿到物理 mask ROM：先找 reset 向量；E3 类比可试
   `0x000E0000..0x00100000`，但**不要**把它当成 D850 的结论。
6. **启动后的 RAM dump 比 flash dump 更有价值**：它直接给出映射后的页表、
   代码和 ROM 写过的参数。

> boot ROM 是 Nikon 版权材料。只在你合法拥有的设备上提取，本地分析，
> **不要提交到仓库、issue 或 PR**（见 [LEGAL.md](../LEGAL.md)）。

## 7. 拿到后 QEMU 里怎么接

最小改动顺序：

1. 新增 `--rom FILE` / `-machine rom=FILE`，把 dump 装到 `0x40000000`；
2. 关闭 `--e5-tables`（用真实页表）；
3. 先 `entry=0x1035CE30` 验证真实页表/服务，再 `entry=0x1037CBA8` 试真实 reset；
4. 记录第一条未建模 MMIO，按 ROM 代码补模型；
5. 如果 ROM 自己会从 flash 拷贝，再加 `--flash` 或把 `b1330` 放到它期望的位置。

## 8. 一句话

**按现有字节，`0x40000000` 窗按 512 KiB 抓最稳；最低 264 KiB 能覆盖已知代码/
向量/交班点。物理 mask ROM 可能只有 128–256 KiB，且可能只是 loader——它和
512 KiB 运行窗不是同一个东西。只拿到运行窗，能把 init 从“夹具”推进到“真实
页表/真实交班”；只拿到 mask ROM，能把启动推进到“真实 reset → loader → 载荷”，
但两者都不足以单独完成整机自然启动。**

---

### English summary

The `0x40000000` window is at least 184 KiB (highest known vector
`0x4002E12C`), at least ~263 KiB if the in-image region table
(`0x40000000+0x2F0D8`, `0x4002F100+0x12BA8 = 0x40041CA8`) is a real load map, and
most likely a **512 KiB** aperture (`0x40000000..0x4007FFFF`). The physical mask
ROM is a different thing and may be only 128–256 KiB (E3 analogy, low
confidence).

With only the `0x40000000` window: replace the LPAE/zero-page fixtures, run the
payload init on real tables/handlers, execute the real `0x40028FF8` stage-2 code,
and supply the three boot-service slots. You still lack a natural reset path,
DRAM/clock/storage/PMIC init, and possibly the loader itself.

With only the physical mask ROM: recover the reset vector, boot protocol, flash
load map and validation path; run it in QEMU and model the first unmodeled MMIO;
it can provide the real reset context for the payload's reset word. You may still
lack the `0x40000000` runtime image and the rest of the flash/board state.

Dump priority: 512 KiB at `0x40000000`, then the `0x4007B000..0x400E9000`
register/scratch area, then a post-boot RAM dump. Do not publish the dump.
