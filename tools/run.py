# SPDX-License-Identifier: GPL-2.0-or-later
"""Run the Nikon D850 QEMU prototype and record evidence.

The launcher never modifies the firmware.  It maps the decrypted b1330 image
at 0x10000000, starts the CPU at the D850 reset word (or at --entry), and
leaves a session directory with the exact command, guest serial bytes, the
QEMU diagnostic log, and parsed MMIO/exception summaries.

Examples:
    python tools/run.py --payload b1330_131.bin
    python tools/run.py --payload b1330_131.bin --entry 0x1035CE30 --trace in_asm
    python tools/run.py --payload b1330_131.bin --strict --timeout 2
"""
from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent

DEFAULT_ENTRY = 0x1037CBA8          # D850 reset word target
DEFAULT_INIT_ENTRY = 0x1035CE30     # in-image MMU/init installer
DEFAULT_PAYLOAD_BASE = 0x10000000
DEFAULT_FLASH_SIZE = 0x2000000      # 32 MiB
DEFAULT_CPU = "cortex-a15"          # init uses LPAE (mcrr TTBR0/TTBR1)
MACHINE = "nikon-d850"

PROBE_RE = re.compile(
    r"d850-(?P<src>probe|a1010-io): pc=(?P<pc>0x[0-9a-f]+) "
    r"(?P<rw>rd|wr) addr=(?P<addr>0x[0-9a-f]+) size=(?P<size>\d+)"
    r"(?: value=(?P<value>0x[0-9a-f]+))?"
)
HALT_RE = re.compile(
    r"d850-halt: pc=(0x[0-9a-f]+) r0=(0x[0-9a-f]+) r1=(0x[0-9a-f]+)"
    r" sp=(0x[0-9a-f]+) lr=(0x[0-9a-f]+)"
    r"(?: return_ok=(true|false) mode=(0x[0-9a-f]+))?"
)
STRICT_RE = re.compile(
    r"d850-strict: pc=(0x[0-9a-f]+) (rd|wr) addr=(0x[0-9a-f]+) size=(\d+)"
    r"(?: value=(0x[0-9a-f]+))?"
)
EXC_RE = re.compile(r"Taking exception (\d)")
EXC_NAMES = {1: "undef", 2: "svc", 3: "prefetch_abort", 4: "data_abort",
             5: "irq", 6: "fiq"}


def find_qemu(explicit: str | None = None) -> Path | None:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    env = os.environ.get("D850_QEMU")
    if env:
        candidates.append(Path(env))
    for pattern in (
        ".build/build/qemu-system-arm.exe",
        ".build/build/qemu-system-arm",
        ".build/src/*/build/qemu-system-arm.exe",
        ".build/src/*/build/qemu-system-arm",
    ):
        candidates.extend(sorted(PROJECT.glob(pattern)))
    found = shutil.which("qemu-system-arm")
    if found:
        candidates.append(Path(found))
    for cand in candidates:
        if cand and Path(cand).is_file():
            return Path(cand)
    return None


def machine_supported(qemu: Path) -> tuple[bool, str]:
    try:
        out = subprocess.run([str(qemu), "-M", "help"], capture_output=True,
                             text=True, timeout=20)
    except (OSError, subprocess.TimeoutExpired) as exc:
        return False, str(exc)
    return (MACHINE in out.stdout), out.stdout


def sha256_file(path: Path) -> str | None:
    if not path.is_file():
        return None
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_size(text: str) -> int:
    """Parse a size with an optional K/M/G suffix (e.g. 32M, 0x2000000)."""
    t = str(text).strip().lower()
    mult = 1
    for suffix, value in (("g", 1 << 30), ("m", 1 << 20), ("k", 1 << 10)):
        if t.endswith(suffix):
            mult = value
            t = t[:-1]
            break
    return int(t, 0) * mult


def build_machine_opts(args: argparse.Namespace) -> str:
    """Build the -machine string for a run (kept pure for tests)."""
    opts = [MACHINE,
            f"payload-base=0x{args.payload_base:x}",
            f"entry=0x{args.entry:x}",
            f"flash-size=0x{args.flash_size:x}"]
    if args.aux2_size:
        opts.append(f"aux2-base=0x{args.aux2_base:x}")
        opts.append(f"aux2-size=0x{args.aux2_size:x}")
    if args.e5_tables:
        opts.append("e5-tables=on")
    if args.bootrom:
        opts.append("bootrom=" + Path(args.bootrom).as_posix())
        if args.boot_tables:
            opts.append("bootrom-tables=on")
            opts.append(f"bootrom-tables-base=0x{args.boot_tables_base:x}")
            opts.append(f"bootrom-arg=0x{args.boot_arg:08x}")
    if args.strict:
        opts.append("strict=on")
    return ",".join(opts)


def build_command(qemu: Path, args: argparse.Namespace, run_dir: Path) -> list[str]:
    debug = ",".join(["guest_errors", "int", *args.trace])
    cmd = [
        str(qemu), "-M", build_machine_opts(args),
        "-cpu", args.cpu,
        "-m", args.ram,
        "-smp", str(args.smp),
        "-display", "none",
        "-monitor", "none",
        "-serial", "file:" + (run_dir / "uart.log").as_posix(),
        "-D", str(run_dir / "qemu.log"),
        "-d", debug,
        "-no-reboot",
        "-nic", "none",
        "-kernel", str(Path(args.payload).resolve()),
    ]
    return cmd


def summarize_probe_log(path: Path) -> dict:
    events: list[dict] = []
    if path.is_file():
        for match in PROBE_RE.finditer(
                path.read_text(encoding="utf-8", errors="replace")):
            events.append({
                "source": "a1010" if match.group("src") == "a1010-io" else "probe",
                "pc": match.group("pc"),
                "rw": match.group("rw"),
                "addr": match.group("addr"),
                "size": int(match.group("size")),
                "value": match.group("value") or "0x00000000",
            })
    counts: dict[tuple[str, str], dict] = {}
    for ev in events:
        key = (ev["addr"], ev["rw"])
        rec = counts.setdefault(key, {"addr": ev["addr"], "rw": ev["rw"],
                                      "count": 0, "first_pc": ev["pc"],
                                      "size": ev["size"],
                                      "first_value": ev["value"]})
        rec["count"] += 1
    return {
        "events": len(events),
        "by_address": sorted(counts.values(), key=lambda r: r["count"],
                             reverse=True)[:50],
    }


def summarize_exceptions(path: Path) -> dict:
    counts = {name: 0 for name in EXC_NAMES.values()}
    if not path.is_file():
        return {"counts": counts, "total": 0}
    for match in EXC_RE.finditer(
            path.read_text(encoding="utf-8", errors="replace")):
        name = EXC_NAMES.get(int(match.group(1)))
        if name:
            counts[name] += 1
    return {"counts": counts, "total": sum(counts.values())}


def summarize_halt(path: Path) -> dict | None:
    if not path.is_file():
        return None
    match = HALT_RE.search(path.read_text(encoding="utf-8", errors="replace"))
    if not match:
        return None
    return_ok = None
    if match.group(6) is not None:
        return_ok = match.group(6) == "true"
    return {
        "pc": match.group(1), "r0": match.group(2), "r1": match.group(3),
        "sp": match.group(4), "lr": match.group(5),
        "return_ok": return_ok, "mode": match.group(7),
        "suspicious": return_ok is False,
    }


def summarize_strict(path: Path) -> dict | None:
    if not path.is_file():
        return None
    match = STRICT_RE.search(path.read_text(encoding="utf-8", errors="replace"))
    if not match:
        return None
    return {
        "pc": match.group(1), "rw": match.group(2), "addr": match.group(3),
        "size": int(match.group(4)), "value": match.group(5) or "0x00000000",
    }


def summarize_uart(path: Path, tail: int = 4096) -> dict:
    if not path.is_file():
        return {"bytes": 0, "tail": ""}
    data = path.read_bytes()
    return {"bytes": len(data),
            "tail": data[-tail:].decode("utf-8", errors="replace")}


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--payload", required=True,
                        help="decrypted main image (b1330_131.bin)")
    parser.add_argument("--qemu", help="qemu-system-arm with the nikon-d850 machine")
    parser.add_argument("--cpu", default=DEFAULT_CPU,
                        choices=("cortex-a15", "cortex-a7", "cortex-a9"))
    parser.add_argument("--ram", default="128M")
    parser.add_argument("--smp", type=int, default=1, choices=(1, 2))
    parser.add_argument("--entry", type=lambda s: int(s, 0),
                        default=DEFAULT_ENTRY,
                        help=f"initial PC (default 0x{DEFAULT_ENTRY:x}); "
                             f"--init selects 0x{DEFAULT_INIT_ENTRY:x}")
    parser.add_argument("--init", action="store_true",
                        help="host-assisted entry at the in-image init installer "
                             f"(0x{DEFAULT_INIT_ENTRY:x})")
    parser.add_argument("--payload-base", type=lambda s: int(s, 0),
                        default=DEFAULT_PAYLOAD_BASE,
                        help=f"load address (default 0x{DEFAULT_PAYLOAD_BASE:x})")
    parser.add_argument("--flash-size", type=parse_size,
                        default=DEFAULT_FLASH_SIZE,
                        help="flash window size (default 32M)")
    parser.add_argument("--aux2-base", type=lambda s: int(s, 0),
                        default=0x40000000,
                        help="handler/boot RAM base for --aux2-size")
    parser.add_argument("--aux2-size", type=parse_size, default=0,
                        help="map a handler RAM bank at --aux2-base (e.g. 256K)")
    parser.add_argument("--e5-tables", action="store_true",
                        help="install the LPAE identity fixture at "
                             "0x40000000/0x40001000 and map handler RAM "
                             "(needed to get past the init's MMU enable)")
    parser.add_argument("--bootrom", type=Path,
                        help="reconstructed 512 KiB 0x40000000 window to load")
    parser.add_argument("--boot-tables", action="store_true",
                        help="with --bootrom: install LPAE tables at "
                             "--boot-tables-base and preset [0x400E8450]")
    parser.add_argument("--boot-tables-base", type=lambda s: int(s, 0),
                        default=0x40708000,
                        help="LPAE table base for --boot-tables "
                             "(default 0x40708000)")
    parser.add_argument("--boot-arg", type=lambda s: int(s, 0),
                        default=0x00708000,
                        help="value written to [0x400E8450] for --boot-tables "
                             "(default 0x00708000)")
    parser.add_argument("--trace", action="append", default=[],
                        help="extra -d item, e.g. --trace in_asm (repeatable)")
    parser.add_argument("--strict", action="store_true",
                        help="stop at the first unmodelled MMIO access")
    parser.add_argument("--timeout", type=float, default=10.0,
                        help="run time in seconds")
    parser.add_argument("--out", type=Path,
                        help="session directory (default runs/<timestamp>)")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)
    if args.init:
        args.entry = DEFAULT_INIT_ENTRY
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    payload = Path(args.payload)
    if not payload.is_file():
        print(f"error: payload not found: {payload}", file=sys.stderr)
        return 2
    if args.boot_tables and not args.bootrom:
        print("error: --boot-tables requires --bootrom", file=sys.stderr)
        return 2
    if args.bootrom and not Path(args.bootrom).is_file():
        print(f"error: bootrom not found: {args.bootrom}", file=sys.stderr)
        return 2
    qemu = find_qemu(args.qemu)
    if qemu is None:
        print("error: qemu-system-arm not found; build the overlay "
              "(docs/build.md) or pass --qemu", file=sys.stderr)
        return 2
    supported, _ = machine_supported(qemu)
    if not supported:
        print(f"error: {qemu} has no '{MACHINE}' machine.", file=sys.stderr)
        print("A stock QEMU cannot load a machine at runtime; build the "
              "overlay first (docs/build.md).", file=sys.stderr)
        return 3

    stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = Path(args.out or (PROJECT / "runs" / stamp))
    run_dir.mkdir(parents=True, exist_ok=True)

    cmd = build_command(qemu, args, run_dir)
    (run_dir / "command.json").write_text(
        json.dumps({"argv": cmd, "entry": f"0x{args.entry:08x}"}, indent=2)
        + "\n", encoding="utf-8")
    if args.dry_run:
        print(" ".join(cmd))
        return 0

    payload_path = Path(args.payload).resolve()
    started = time.time()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    session: dict = {
        "argv": cmd,
        "entry": f"0x{args.entry:08x}",
        "payload": str(payload_path),
        "payload_sha256": sha256_file(payload_path),
        "payload_size": payload_path.stat().st_size if payload_path.is_file() else None,
        "started": _dt.datetime.now().isoformat(timespec="seconds"),
    }
    try:
        proc.wait(timeout=args.timeout)
        session["exit"] = proc.returncode
        session["timed_out"] = False
    except subprocess.TimeoutExpired:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        session["exit"] = proc.returncode
        session["timed_out"] = True

    session["wall_seconds"] = round(time.time() - started, 3)
    session["qemu_output_tail"] = (proc.stdout.read()[-4000:]
                                   if proc.stdout else "")
    session["uart"] = summarize_uart(run_dir / "uart.log")
    session["mmio"] = summarize_probe_log(run_dir / "qemu.log")
    session["exceptions"] = summarize_exceptions(run_dir / "qemu.log")
    session["halt"] = summarize_halt(run_dir / "qemu.log")
    session["strict"] = summarize_strict(run_dir / "qemu.log")

    if session["timed_out"]:
        status = "timeout"
    elif session["strict"] is not None:
        status = "strict-stop"
    elif session["halt"] is not None:
        status = "halt-unexpected" if session["halt"]["suspicious"] else "halt"
    elif session["exit"] == 0:
        status = "ran"
    else:
        status = f"exit-{session['exit']}"
    session["status"] = status
    (run_dir / "session.json").write_text(
        json.dumps(session, indent=2) + "\n", encoding="utf-8")

    print(f"session: {run_dir}")
    print(f"status={status}  entry 0x{args.entry:08x}  exit={session['exit']}  "
          f"timed_out={session['timed_out']}  uart={session['uart']['bytes']}B  "
          f"mmio_events={session['mmio']['events']}")
    exc = session["exceptions"]
    exc_text = " ".join(f"{k}={v}" for k, v in exc["counts"].items() if v)
    print("exceptions: " + (exc_text or "none"))
    if session["halt"]:
        halt = session["halt"]
        print(f"halt: pc={halt['pc']} r0={halt['r0']} r1={halt['r1']} "
              f"sp={halt['sp']} lr={halt['lr']} return_ok={halt['return_ok']}")
    if session["strict"]:
        stop = session["strict"]
        print(f"strict: {stop['rw']} {stop['addr']} size={stop['size']} "
              f"(pc {stop['pc']})")
    if session["mmio"]["by_address"]:
        print("unmodelled MMIO:")
        for rec in session["mmio"]["by_address"][:10]:
            print(f"  {rec['rw']} {rec['addr']} x{rec['count']} "
                  f"(first pc {rec['first_pc']})")
    if session["uart"]["tail"]:
        print("uart tail:")
        print(session["uart"]["tail"][-1200:])

    if status == "strict-stop":
        return 7
    if session["timed_out"] or session["exit"] in (0, None):
        return 0
    return session["exit"]


if __name__ == "__main__":
    sys.exit(main())
