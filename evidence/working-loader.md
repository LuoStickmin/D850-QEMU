# D850 working loader（短分析）

## 指纹

- 文件：`D850_0131_FLASHED_WORKING_LOADER.bin`
- 大小：27,660,549 B
- SHA-256：`b24aad103fa2f075af8b18a2a3be5af8a25406eb170e585accbab68631d08c42`
- 与 `D850_0131_FENESTRA_SIGNED_v3.bin` **完全相同**
- 对应明文 `D850_0131_FENESTRA_SIGNED_v3.dec`
  - SHA-256：`07dcfcfa281ef00bed172e722280c4671debc0e18d263a3773717cfac23b7e06`

## 与 stock 的差异

`v3.dec` 对 `D850_0131_stock.dec`：约 **221 B / 20 段**差异。

- 文件头/签名/CRC；
- `0x2689BB` 的 4 B；
- 主要注入在 **a1330（M-core）** 的 full offset `0x545A1F` 起；
- `0x546E07/0x546E0F` 的 2 B 表项；
- 文件尾 2 B。
- **b1330（A-core）没有差异。**

所以 Fenestra loader 补丁在 a1330，不在 b1330。

## Loader 关键代码（a1330，Thumb）

入口 full offset `0x545A1F`（相对 a1330 模块头 0x54596F）：

```text
0x545A25  ldr r0,=0xE0103800
0x545A27  ldr r1,[r0]
0x545A29  cmp r1,#1
0x545A2B  beq 0x545AA9        ; once 标志已置位则跳过
0x545A2D  ldr r0,=0x00103800 ; 读缓冲
0x545A31  movs r2,#0x20
0x545A33  ldr r3,=0xEF0000D8
0x545A35  blx r3             ; 服务调用 A
0x545A43  ldr r1,=0x00103800
0x545A45  ldr r2,=0x5700000F
0x545A4B  blx 0x1910232B     ; 服务调用 B
0x545A53  blx 0x1D10232B     ; 服务调用 C
0x545A5B  cmp r5,#0x40       ; 读回长度 >= 64
0x545A61  ldr r1,[0x00103800]
0x545A65  cmp r1,#0x5700000F ; 头 magic
0x545A69  ldr r1,[0x00103828]
0x545A6D  cmp r1,#0x50314D4C ; "LM1P"（WLM1P 的一部分）
0x545A71  ldr r2,[0x00103830]
0x545A73  cmp r2,#0x40       ; 大小 >= 0x40
0x545A77  cmp r2,r5           ; 大小 < 读回长度
```

字符串：

```text
0x545AC3  WLM1P
0x545ADF  B:\FENESTRA\CORE.WLM
```

## 日志插桩建议

**在 a1330/M-core 侧插桩，A-core b1330 日志看不到它。**

建议断点/记录：

| full offset | 记录 |
|---|---|
| `0x545A1F` | 入口 r0-r4、SP；是否已加载 |
| `0x545A25/27` | `[0xE0103800]` 的 once 值 |
| `0x545A35` | 服务 A 的目标、r0/r1/r2、返回 r0 |
| `0x545A4B` | 服务 B 的目标、r1/r2、返回 r0 |
| `0x545A53` | 服务 C 的返回 |
| `0x545A5B` | 读回长度 `[sp]` |
| `0x545A61/69/71` | `0x103800`、`0x103828`、`0x103830` 三个头字段 |
| `0x545ADF` | 路径字符串 |
| `0x546E07/0F` | 被改表项 |

判断“已加载”的最小证据：

1. `[0xE0103800] == 1`（或从 0 变 1）；
2. `0x00103800` 读缓冲里出现 `0x5700000F`；
3. `0x00103828 == 0x50314D4C`；
4. `0x00103830` 是合理的大小（>=0x40 且小于读回长度）；
5. 之后 A-core 共享标志（HANDOFF 的 `0x103800DC`）变化。

## 未确定

- a1330 的 M-core 运行时基址到 full offset 的精确换算；
- 三个 `blx` 目标（`0xEF0000D8`、`0x1910232B`、`0x1D10232B`）是服务 ID、SVC 还是函数指针；
- `0xE0103800` 是共享寄存器还是内存别名；
- CORE.WLM 被复制/跳转到的最终地址。
