# 构建与运行

## 1. 依赖

- QEMU **11.1.0** 源码树（已实测）；
- C 工具链与 QEMU 的常规依赖：`build-essential ninja-build pkg-config
  libglib2.0-dev libpixman-1-dev zlib1g-dev libfdt-dev`；
- Python 3.10+（`prepare_source.py`、`tools/run.py`、`tests/`）。

QEMU 10.2.4 有 `CharFrontend` → `CharBackend` 的文本适配，但没有在本仓库里
编译验证；遇到问题请先回到 11.1.0。

## 2. 安装 overlay

```sh
cd /path/to/qemu-11.1.0
python3 /path/to/D850-QEMU/qemu/prepare_source.py .
```

脚本会：

- 复制 `nikon-d850-lab.c/.h` 到 `hw/arm/`；
- 在 `hw/arm/meson.build` 追加一行（按 QEMU 版本选 `arm_common_ss` 或
  `arm_ss`）；
- 在 `hw/arm/Kconfig` 追加 `config NIKON_D850_LAB`；
- 在 `configs/devices/arm-softmmu/default.mak` 追加 `CONFIG_NIKON_D850_LAB=y`。

重复执行是幂等的：第二次运行不会再改任何文件。不支持版本会直接拒绝
（可用 `--allow-unsupported` 强行尝试）。

## 3. 编译

```sh
ninja -C build qemu-system-arm
```

只重编受影响的目标；新增一个板级文件通常 1–2 分钟。

验证机器注册：

```sh
build/qemu-system-arm -M help | grep nikon
```

应看到：

```text
nikon-d850           Nikon D850 validation prototype (EXPEED 5 / E5, boot-ROM experiments)
```

## 4. 运行

固件需自行准备：解密后的 `b1330_131.bin`（D850 C:1.31，25,671,414 B）。

```sh
cd /path/to/D850-QEMU
python3 tools/run.py \
  --qemu /path/to/qemu-11.1.0/build/qemu-system-arm \
  --payload /path/to/b1330_131.bin \
  --out runs/reset --timeout 3
```

默认：

- `payload-base=0x10000000`
- `entry=0x1037CBA8`
- `flash-size=0x2000000`
- `cpu=cortex-a15`、`ram=128M`、`smp=1`

其他入口/夹具：

```sh
# 镜像内 init（无页表夹具）
python3 tools/run.py --payload "$FW/b1330_131.bin" --init --out runs/init

# 加 LPAE identity 夹具
python3 tools/run.py --payload "$FW/b1330_131.bin" \
  --init --e5-tables --out runs/init-lpae

# A9 对照（应在 mcrr 上 undefined）
python3 tools/run.py --payload "$FW/b1330_131.bin" \
  --init --cpu cortex-a9 --out runs/init-a9 --timeout 1
```

## 5. 输出

`--out` 目录包含：

| 文件 | 内容 |
| --- | --- |
| `command.json` | 实际 QEMU 命令行、入口 |
| `qemu.log` | QEMU 诊断日志（`d850-probe:` / `d850-halt:` / 异常） |
| `uart.log` | 串口字节（目前始终为空） |
| `session.json` | 状态、异常计数、MMIO 汇总、halt 记录 |

`session.json` 的 `status`：

- `timeout`：到时被终止（init 滑行/循环的常态）；
- `halt`：到达 `0xFF0` 的 halt 窗口且返回状态干净；
- `halt-unexpected`：到达 halt 但 `return_ok=false`（reset sled 的预期结果）；
- `strict-stop`：`--strict` 下第一条未建模访问后停止（退出码 7）。

## 6. 直接使用 QEMU

不使用 `run.py` 也可以：

```sh
qemu-system-arm \
  -M nikon-d850,payload-base=0x10000000,entry=0x1037CBA8,flash-size=0x2000000 \
  -cpu cortex-a15 -m 128M -display none -monitor none -serial null \
  -kernel b1330_131.bin -no-reboot -nic none
```

加 `--init` 等价于 `entry=0x1035ce30`；加 `--e5-tables` 等价于
`e5-tables=on`。

## 7. 测试

```sh
python3 tests/run_tests.py
```

11 个主机侧用例：`prepare_source` 幂等/版本适配、`run.py` 的机器参数、
异常/MMIO/halt 解析、Markdown 链接完整性。测试不需要 QEMU 构建。

## 8. 常见问题

**`-M help` 里没有 `nikon-d850`**：机器是编译期注册的，stock QEMU 不能运行时加载。
必须用本仓库的 overlay 重新编译 `qemu-system-arm`。

**`payload-base ... outside both the RAM and flash windows`**：`payload-base`
必须落在 `[0x90000000, 0x90000000+ram)` 或
`[0x10000000, 0x10000000+flash-size)`；D850 用后者。

**`e5-tables needs aux2-base=0x40000000 ...`**：不要同时手工指定不兼容的
`aux2-base`；让 `--e5-tables` 自己映射即可。

**编译警告 `CharFrontend`/`CharBackend`**：10.x 源码树请用
`prepare_source.py` 自动改写，不要手工交换。

## 9. 贡献、法律与安全

- 贡献规则、DCO 与禁止提交的内容：[CONTRIBUTING.md](../CONTRIBUTING.md) /
  [CONTRIBUTING.en.md](../CONTRIBUTING.en.md)
- 法律与合规说明（非法律意见）：[LEGAL.md](../LEGAL.md)
- 许可、版权、上游与第三方：[NOTICE.md](../NOTICE.md)
- 漏洞报告与安全边界：[SECURITY.md](../SECURITY.md)
- 行为准则：[CODE_OF_CONDUCT.md](../CODE_OF_CONDUCT.md)
