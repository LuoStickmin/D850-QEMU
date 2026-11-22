# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import sys
import tempfile
import unittest
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "tools"))

import run  # noqa: E402


def args(**overrides) -> argparse.Namespace:
    base = dict(payload_base=0x10000000, entry=0x1037CBA8,
                flash_size=0x2000000, aux2_base=0x40000000,
                aux2_size=0, e5_tables=False, bootrom=None,
                boot_tables=False, boot_tables_base=0x40708000,
                boot_arg=0x00708000, strict=False)
    base.update(overrides)
    return argparse.Namespace(**base)


class ParseSizeTests(unittest.TestCase):
    def test_suffixes_and_hex(self):
        self.assertEqual(run.parse_size("32M"), 0x2000000)
        self.assertEqual(run.parse_size("256k"), 0x40000)
        self.assertEqual(run.parse_size("1G"), 0x40000000)
        self.assertEqual(run.parse_size("0x2000000"), 0x2000000)


class MachineOptsTests(unittest.TestCase):
    def test_default_profile(self):
        opts = run.build_machine_opts(args())
        self.assertTrue(opts.startswith("nikon-d850,"))
        self.assertIn("payload-base=0x10000000", opts)
        self.assertIn("entry=0x1037cba8", opts)
        self.assertIn("flash-size=0x2000000", opts)
        self.assertNotIn("aux2-base", opts)
        self.assertNotIn("strict=on", opts)

    def test_handler_bank_and_strict(self):
        opts = run.build_machine_opts(
            args(aux2_size=0x40000, strict=True))
        self.assertIn("aux2-base=0x40000000", opts)
        self.assertIn("aux2-size=0x40000", opts)
        self.assertIn("strict=on", opts)

    def test_e5_table_fixture(self):
        opts = run.build_machine_opts(args(e5_tables=True))
        self.assertIn("e5-tables=on", opts)

    def test_bootrom_fixture(self):
        opts = run.build_machine_opts(
            args(bootrom=Path("bootrom_alt.bin"), boot_tables=True))
        self.assertIn("bootrom=bootrom_alt.bin", opts)
        self.assertIn("bootrom-tables=on", opts)
        self.assertIn("bootrom-tables-base=0x40708000", opts)
        self.assertIn("bootrom-arg=0x00708000", opts)


class LogSummaryTests(unittest.TestCase):
    def test_probe_summary_groups_by_address(self):
        text = (
            "d850-probe: pc=0x1035ce38 rd addr=0x400e8450 size=4 "
            "(unmodelled read, returning 0)\n"
            "d850-probe: pc=0x1035cea0 rd addr=0x400e8450 size=4 "
            "(unmodelled read, returning 0)\n"
            "d850-probe: pc=0x1035cf54 wr addr=0x400e8454 size=4 "
            "value=0x00009000 (unmodelled write)\n"
        )
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "qemu.log"
            path.write_text(text, encoding="utf-8")
            summary = run.summarize_probe_log(path)
        self.assertEqual(summary["events"], 3)
        self.assertEqual(summary["by_address"][0]["addr"], "0x400e8450")
        self.assertEqual(summary["by_address"][0]["count"], 2)
        writes = [r for r in summary["by_address"] if r["rw"] == "wr"]
        self.assertEqual(writes[0]["first_value"], "0x00009000")

    def test_exception_summary(self):
        text = ("Taking exception 3 [Prefetch Abort] on CPU 0\n"
                "Taking exception 3 [Prefetch Abort] on CPU 0\n"
                "Taking exception 1 [Undefined Instruction] on CPU 0\n")
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "qemu.log"
            path.write_text(text, encoding="utf-8")
            summary = run.summarize_exceptions(path)
        self.assertEqual(summary["counts"]["prefetch_abort"], 2)
        self.assertEqual(summary["counts"]["undef"], 1)
        self.assertEqual(summary["total"], 3)

    def test_halt_record_marks_unexpected_return(self):
        text = ("d850-halt: pc=0x00000ff0 r0=0x00000000 r1=0x00000000 "
                "sp=0x97ffff10 lr=0x00000ff0 return_ok=false mode=0x13\n")
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "qemu.log"
            path.write_text(text, encoding="utf-8")
            halt = run.summarize_halt(path)
        self.assertTrue(halt["suspicious"])
        self.assertEqual(halt["pc"], "0x00000ff0")


if __name__ == "__main__":
    unittest.main()
